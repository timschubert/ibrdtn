/*
 * USBConvergenceLayer.cpp
 *
 * Copyright (C) 2017 IBR, TU Braunschweig
 *
 * Written-by: Tim Schubert <tim.schubert@tu-bs.de>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "USBConvergenceLayer.h"

namespace dtn
{
	namespace net
	{
		const std::string USBConvergenceLayer::TAG = "USBConvergenceLayer";

		USBConvergenceLayer::USBConvergenceLayer(uint16_t vendor, uint16_t product, int interfaceNum, uint8_t endpointIn, uint8_t endpointOut)
		 : dtn::daemon::IndependentComponent()
		 , _config(daemon::Configuration::getInstance().getUSB())
		 , _run(false)
		 , _endpointIn(endpointIn)
		 , _endpointOut(endpointOut)
		 , _interfaceNum(interfaceNum)
		 , _vendor_id(vendor)
		 , _product_id(product)
		 , _service(usbconnector::get_instance())
		 , _frame_length(1000)
		{
		}

		USBConvergenceLayer::~USBConvergenceLayer()
		{
		}

		dtn::core::Node::Protocol USBConvergenceLayer::getDiscoveryProtocol() const
		{
			return dtn::core::Node::CONN_USB;
		}

		void USBConvergenceLayer::queue(const dtn::core::Node &n, const dtn::net::BundleTransfer &job)
		{
			/* check if destination supports USB convergence layer */
			const std::list<dtn::core::Node::URI> uri_list = n.get(dtn::core::Node::CONN_USB);
			if (uri_list.empty())
			{
				dtn::net::TransferAbortedEvent::raise(n.getEID(), job.getBundle(), dtn::net::TransferAbortedEvent::REASON_UNDEFINED);
				return;
			}

			{
				MutexLock l(_connectionsLock);
				if (_connection && _connection->match(n))
				{
					_connection->queue(job);
				}
			}

			dtn::net::TransferAbortedEvent::raise(n.getEID(), job.getBundle(), dtn::net::TransferAbortedEvent::REASON_UNDEFINED);
		}

		void USBConvergenceLayer::onAdvertiseBeacon(const vinterface &iface, const DiscoveryBeacon &beacon) throw()
		{
			/* broadcast beacon over USB */
			IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "Sending beacon over USB." << IBRCOMMON_LOGGER_ENDL;

			if (_connection)
			{
				MutexLock l(_connectionsLock);
				(*_connection) << beacon;
			}
		}

		void USBConvergenceLayer::onUpdateBeacon(const vinterface &iface, DiscoveryBeacon &beacon) throw(NoServiceHereException)
		{
			std::stringstream ss;
			ss << "usb="
			   << "host-mode";
			DiscoveryService srv(dtn::core::Node::Protocol::CONN_USB, ss.str());

			IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 90) << "Adding USB information to discovery beacon." << IBRCOMMON_LOGGER_ENDL;
			beacon.addService(srv);
		}

		void USBConvergenceLayer::resetStats()
		{
			// TODO calculate packet loss
		}

		void USBConvergenceLayer::getStats(ConvergenceLayer::stats_data &data) const
		{
			// TODO get packet loss
		}

		void USBConvergenceLayer::componentUp() throw()
		{

			try
			{
				usbconnector::get_instance().register_device_cb(this, _vendor_id, _product_id);

				{
					MutexLock l(_interfacesLock);
					_interface = usbconnector::get_instance().open(_vendor_id, _product_id, _interfaceNum);
					_interface.set_up();
				}

				usbsocket *sock = new usbsocket(_interface, _endpointIn, _endpointOut, 1000);
				sock->up();

				usbstream stream(sock);
				Node nonode = Node();

				MutexLock l(_connectionsLock);
				_connection = new USBConnection(stream, nonode, *this);
			}
			catch (ibrcommon::Exception &e)
			{
				IBRCOMMON_LOGGER_TAG(TAG, warning) << "Failed to set USB CL up: " << e.what() << IBRCOMMON_LOGGER_ENDL;
			}

			IBRCOMMON_LOGGER_TAG(TAG, info) << "Set up USB CL " << IBRCOMMON_LOGGER_ENDL;
		}

		void USBConvergenceLayer::componentDown() throw()
		{
			try
			{
				usbconnector::get_instance().unregister_device_cb(this, _vendor_id, _product_id);

				if (_connection)
				{
					MutexLock l(_connectionsLock);
					delete _connection;
				}

				try
				{
					this->stop();
					this->join();
				}
				catch (const ThreadException &e)
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 90) << e.what() << IBRCOMMON_LOGGER_ENDL;
				}

				MutexLock i(_interfacesLock);
				_interface.set_down();
			}
			catch (ibrcommon::Exception &e)
			{
				IBRCOMMON_LOGGER_TAG(TAG, warning) << "Failed to set USB CL down: " << e.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void USBConvergenceLayer::componentRun() throw()
		{
			_run = true;

			try
			{
				while (_run)
				{
					if (!_connection)
					{
						IBRCOMMON_LOGGER_TAG(TAG, warning) << "no connections" << IBRCOMMON_LOGGER_ENDL;
						Thread::sleep(1000);
					}
					else
					{
						MutexLock l(_connectionsLock);
						_connection->processInput();

						if (_connection->isDown())
						{
							__processError();
						}
					}
				}
			}
			catch (std::exception &e)
			{
				/* ignore all other errors */
			}
		}

		void USBConvergenceLayer::__processError()
		{
			if (_connection)
			{
				MutexLock l(_connectionsLock);
				delete _connection;
			}
		}

		void USBConvergenceLayer::__cancellation() throw()
		{
			_run = false;
		}

		const std::string USBConvergenceLayer::getName() const
		{
			return TAG;
		}

		void USBConvergenceLayer::device_discovered(usbdevice &dev)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("USBConvergenecLayer", 80) << "new device discovered" << IBRCOMMON_LOGGER_ENDL;

			try
			{
				/* swap the interface new sockets will be opened on */
				if (dev == _interface.get_device())
				{
					return;
				}

				{
					MutexLock l(_interfacesLock);
					_interface = usbinterface(dev, _interfaceNum);
					_interface.set_up();
				}

				dtn::core::BundleCore::getInstance().getDiscoveryAgent().registerService(_interface, this);

				usbsocket *sock = new usbsocket(_interface, _endpointIn, _endpointOut, _frame_length);
				sock->up();

				usbstream stream(sock);

				MutexLock c(_connectionsLock);
				if (_connection)
				{
					delete _connection;
				}

				Node node;
				_connection = new USBConnection(stream, node, *this);
			}
			catch (std::exception &e)
			{
				IBRCOMMON_LOGGER_TAG("USBConvergenecLayer", warning) << "failed to open new device: " << e.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void USBConvergenceLayer::device_lost(const usbdevice &dev)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("USBConvergenecLayer", 80) << "device lost" << IBRCOMMON_LOGGER_ENDL;

			try
			{
				if (_interface.get_device() == dev)
				{
					MutexLock l(_interfacesLock);
					dtn::core::BundleCore::getInstance().getDiscoveryAgent().unregisterService(_interface, this);
					_interface.set_down();
				}

				MutexLock c(_connectionsLock);
				if (_connection)
				{
					delete _connection;
				}
			}
			catch (ibrcommon::Exception &e)
			{
				IBRCOMMON_LOGGER_TAG("USBConvergenecLayer", warning)
				  << "failed to set interface down after device was lost: " << e.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void USBConvergenceLayer::eventBundleReceived(Bundle &newBundle)
		{
			/* validate the bundle */
			try
			{
				dtn::core::BundleCore::getInstance().validate(newBundle);
			}
			catch (dtn::data::Validator::RejectedException &)
			{
				throw InvalidDataException("Bundle was rejected by validator");
			}

			/* create a filter context */
			dtn::core::FilterContext context;
			context.setProtocol(dtn::core::Node::CONN_USB);

			/* push bundle through the filter routines */
			context.setBundle(newBundle);
			BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().filter(dtn::core::BundleFilter::INPUT, context, newBundle);

			if (ret == BundleFilter::ACCEPT)
			{
				/* inject accepted bundle into bundle core */
				dtn::core::BundleCore::getInstance().inject(newBundle.source, newBundle, false);
			}
		}

		void USBConvergenceLayer::eventBeaconReceived(dtn::net::DiscoveryBeacon &b)
		{
			dtn::core::BundleCore::getInstance().getDiscoveryAgent().onBeaconReceived(b);
		}
	}
}
