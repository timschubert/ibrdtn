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

		USBConvergenceLayer::USBConvergenceLayer(uint16_t vendor, uint16_t product, uint8_t interfaceNum, uint8_t endpointIn, uint8_t endpointOut)
		 : dtn::daemon::IndependentComponent()
		 , _config(daemon::Configuration::getInstance().getUSB())
		 , _run(false)
		 , _endpointIn(endpointIn)
		 , _endpointOut(endpointOut)
		 , _interface(usbconnector::get_instance().open(vendor, product))
		 , _recovering(false)
		 , _vendor_id(vendor)
		 , _product_id(product)
		{
			usbsocket *sock = new usbsocket(_interface, _endpointIn, _endpointOut);
			_connection = new USBConnection(sock, 1000);
			_service = new USBService(usbconnector::get_instance());
		}

		USBConvergenceLayer::~USBConvergenceLayer()
		{
			delete _service;

			{
				MutexLock l(_fakedServicesLock);
				for (auto &timer : _fakedServicesTimers)
				{
					delete timer.second;
				}
			}
		}

		dtn::core::Node::Protocol USBConvergenceLayer::getDiscoveryProtocol() const
		{
			return dtn::core::Node::CONN_DGRAM_USB;
		}

		void USBConvergenceLayer::submit(USBConnection *con, const dtn::net::BundleTransfer &job)
		{
			dtn::storage::BundleStorage &storage = dtn::core::BundleCore::getInstance().getStorage();
			const dtn::data::MetaBundle &bundle = job.getBundle();
			try
			{
				/* transmit Bundle */
				if (!storage.contains(bundle))
				{
					IBRCOMMON_LOGGER_DEBUG(80) << "Bundle " << bundle << " not found" << IBRCOMMON_LOGGER_ENDL;
				}
				else
				{
					(*con) << storage.get(bundle);
				}
			} catch (ibrcommon::socket_exception &e)
			{
				IBRCOMMON_LOGGER_DEBUG(80) << e.what() << IBRCOMMON_LOGGER_ENDL;
				dtn::net::TransferAbortedEvent::raise(con->getNode().getEID(), bundle, dtn::net::TransferAbortedEvent::REASON_UNDEFINED);
			}
		}

		void USBConvergenceLayer::queue(const dtn::core::Node &n, const dtn::net::BundleTransfer &job)
		{
			/* check if destination supports USB convergence layer */
			const std::list<dtn::core::Node::URI> uri_list = n.get(dtn::core::Node::CONN_DGRAM_USB);
			if (uri_list.empty())
			{
				dtn::net::TransferAbortedEvent::raise(n.getEID(), job.getBundle(), dtn::net::TransferAbortedEvent::REASON_UNDEFINED);
				return;
			}
			if (_connection->match(n))
			{
				submit(_connection, job);
				return;
			}

			dtn::net::TransferAbortedEvent::raise(n.getEID(), job.getBundle(), dtn::net::TransferAbortedEvent::REASON_UNDEFINED);
		}

		void USBConvergenceLayer::onAdvertiseBeacon(const vinterface &iface, const DiscoveryBeacon &beacon) throw ()
		{
			/* broadcast beacon over USB */
			IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "Sending beacon over USB." << IBRCOMMON_LOGGER_ENDL;

			try
			{
				/* send now */
				(*_connection) << beacon;
				_connection->flush();
			} catch (socket_error &e)
			{
				if (_connection->getError() == ERROR_CLOSED)
				{
					interface_lost(_interface);
				}
			}
		}

		void USBConvergenceLayer::onUpdateBeacon(const vinterface &iface, DiscoveryBeacon &beacon) throw(NoServiceHereException)
		{
			std::stringstream ss;
			ss << "usb=" << "host-mode";
			DiscoveryService srv(dtn::core::Node::Protocol::CONN_DGRAM_USB, ss.str());

			IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 90) << "Adding USB information to discovery beacon." << IBRCOMMON_LOGGER_ENDL;
			beacon.addService(srv);

			for (auto &fake : _fakedServices)
			{
				for (auto &service : fake.second)
				{
					beacon.addService(service);
				}
			}
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
				_interface.set_up();
				usbsocket *sock = new usbsocket(_interface, _endpointIn, _endpointOut);
				_connection = new USBConnection(sock, 1000);
			} catch (ibrcommon::Exception &e)
			{
				interface_lost(_interface);
			}
		}

		void USBConvergenceLayer::componentDown() throw()
		{
			try
			{
				this->stop();
				this->join();
			} catch (const ThreadException &e)
			{
				IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 90) << e.what() << IBRCOMMON_LOGGER_ENDL;
			}

			delete _connection;

			try
			{
				_interface.set_down();
			} catch (USBError &e)
			{
				IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 90) << "Failed to set interface " << _interface.toString() << " down" << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void USBConvergenceLayer::recover()
		{
			while (_recovering)
			{
				try
				{
					/* try to open a new interface to the device and open a socket on the interface */
					_interface = usbconnector::get_instance().open(_vendor_id, _product_id);
					usbsocket *sock = new usbsocket(_interface, _endpointIn, _endpointOut);

					delete _connection;
					_connection = new USBConnection(sock, 1000);

					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "Recovered interface" << IBRCOMMON_LOGGER_ENDL;
					_recovering = false;
				}
				catch (usb_device_error &e)
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "recover: " << e.what() << IBRCOMMON_LOGGER_ENDL;
				}
				catch (ibrcommon::Exception &e)
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "recover: " << e.what() << IBRCOMMON_LOGGER_ENDL;
				}

				/* sleep for 1000 ms */
				Thread::sleep(1000);
			}
		}

		void USBConvergenceLayer::processInput(USBConnection *con)
		{

			dtn::data::Bundle bundle;
			DiscoveryBeacon beacon = dtn::core::BundleCore::getInstance().getDiscoveryAgent().obtainBeacon();

			USBMessageType type = _connection->getNextType();

			switch (type) {

				case DATA:
				if (!((*con) >> bundle))
				{
					throw dtn::InvalidDataException("failed to parse bundle");
				}
				processIncomingBundle(bundle);
				break;

				case DISCOVERY:
				if (!((*con) >> beacon))
				{
					throw dtn::InvalidDataException("failed to parse discovery message");
				}
				processBeacon(beacon);
				break;

				case ACK:
				// TODO

				case NACK:
				// TODO

				default:
					throw dtn::InvalidDataException("failed to parse USB message");
				break;
			}

			/* ignore the rest */
			con->ignore(1000);
		}

		void USBConvergenceLayer::componentRun() throw()
		{
			_run = true;

			try {
				while (_run)
				{
					if (_recovering)
					{
						// TODO replace with reconnect via hotplug if device / interface supports  hotplug
						recover();
						continue;
					}
					try
					{
						processInput(_connection);
					} catch (const dtn::InvalidDataException &e)
					{
						IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << e.what() << IBRCOMMON_LOGGER_ENDL;
					} catch (const vsocket_timeout &e)
					{
						IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << e.what() << IBRCOMMON_LOGGER_ENDL;
					} catch (const vsocket_interrupt &e)
					{
						IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << e.what() << IBRCOMMON_LOGGER_ENDL;
						_run = false;
					}
				}
			} catch (std::exception &e)
			{
				/* ignore all other errors */
			}
		}

		void USBConvergenceLayer::processIncomingBundle(Bundle &newBundle)
		{
			/* validate the bundle */
			try {
				dtn::core::BundleCore::getInstance().validate(newBundle);
			}
			catch (dtn::data::Validator::RejectedException&) {
				throw USBBundleError("Bundle was rejected by validator");
			}

			/* create a filter context */
			dtn::core::FilterContext context;
			context.setProtocol(dtn::core::Node::CONN_DGRAM_USB);

			/* push bundle through the filter routines */
			context.setBundle(newBundle);
			BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().filter(dtn::core::BundleFilter::INPUT, context, newBundle);

			if (ret == BundleFilter::ACCEPT)
			{
				/* inject accepted bundle into bundle core */
				dtn::core::BundleCore::getInstance().inject(newBundle.source, newBundle, false);
			}
		}

		void USBConvergenceLayer::__cancellation() throw()
		{
			_recovering = false;
			_run = false;
		}

		void USBConvergenceLayer::processBeacon(DiscoveryBeacon &beacon)
		{
			if (beacon.isShort())
			{
				std::stringstream ss;
				ss << "usb://[" << _interface.toString() << "]";
				beacon.setEID(dtn::data::EID(ss.str()));

				/* add this CL if list is empty */
				ss.flush();
				ss << "usb=" << "host-mode";
				beacon.addService(dtn::net::DiscoveryService(dtn::core::Node::CONN_DGRAM_USB, ss.str()));
			}

			DiscoveryBeacon::service_list &services = beacon.getServices();

			/* find an existing connection for the node, otherwise add the node to a connection for this socket */
			{
				/* we already have discovered this node */
				if (_connection->match(beacon.getEID()))
				{
					// TODO?
				} else {
					_connection->setNode(dtn::core::Node(beacon.getEID()));
				}
			}

			/* announce the beacon */
			dtn::core::BundleCore::getInstance().getDiscoveryAgent().onBeaconReceived(beacon);

			if (_config.getGateway())
			{
				MutexLock l(_fakedServicesLock);
				const auto &timer = _fakedServicesTimers.find(beacon.getEID());
				if (timer != _fakedServicesTimers.end())
				{
					timer->second->reset();
				}
				else
				{
					/* set timeout */
					_fakedServicesTimers[beacon.getEID()] = new Timer(*this, 1000);

					/* copy services */
					_fakedServices[beacon.getEID()] = services;
				}
			}

			if (_config.getProxy())
			{
				DiscoveryBeaconEvent::raise(beacon, EventDiscoveryBeaconAction::DISCOVERY_PROXY);
			}
		}

		size_t USBConvergenceLayer::timeout(Timer *t)
		{
			MutexLock l(_fakedServicesLock);

			/* find timer */
			for (auto &p : _fakedServicesTimers)
			{
				if (p.second == t)
				{
					/* delete expired timer */
					delete p.second;

					/* remove expired EID from faked services */
					_fakedServices.erase(p.first);
					_fakedServicesTimers.erase(p.first);

					/* stop timer */
					throw Timer::StopTimerException();
				}
			}

			/* else error occurred, immediately expire the timer */
			return 0;
		}

		void USBConvergenceLayer::raiseEvent(const DiscoveryBeaconEvent &event) throw()
		{
			switch (event.getAction())
			{
				case DISCOVERY_PROXY:
					if (_config.getProxy())
					{
						/* prevent direct loop-back */
						if (!_connection->match(event.getBeacon().getEID()))
						{
							(*_connection) << event.getBeacon();
						}
					}
					break;
				default:
					break;
			}
		}

		void USBConvergenceLayer::interface_discovered(usbinterface &iface)
		{
			try
			{
				iface.set_up();
				usbsocket *sock = new usbsocket(_interface, _endpointIn, _endpointOut);
				delete _connection;
				_connection = new USBConnection(sock, 1000);
			}
			catch (ibrcommon::Exception &e)
			{
				IBRCOMMON_LOGGER_TAG(TAG, warning) << "Failed to setup socket on interface " << e.what() << IBRCOMMON_LOGGER_ENDL;
				_recovering = true;
			}
		}

		void USBConvergenceLayer::raiseEvent(const NodeEvent &event) throw()
		{
			const Node &node = event.getNode();
			switch (event.getAction())
			{
				case NODE_DATA_ADDED:
					if (node.has(Node::CONN_DGRAM_USB))
					{
						_connection->setNode(node);
					}
					break;
				case NODE_DATA_REMOVED:
				{
					_connection->unsetNode();
					break;
				}
				default:
					break;
			}
		}

		void USBConvergenceLayer::interface_lost(const usbinterface &iface)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "lost interface " << iface.toString() << IBRCOMMON_LOGGER_ENDL;

			if (iface == _interface)
			{
				try
				{
					_interface.set_down();
				}
				catch (ibrcommon::Exception &e)
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "failed to set interface " << iface.toString() << " down" << IBRCOMMON_LOGGER_ENDL;
				}
			}

			IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "starting recovery of " << iface.toString() << IBRCOMMON_LOGGER_ENDL;
			_recovering = true;
		}

		const std::string USBConvergenceLayer::getName() const
		{
			return TAG;
		}
	}
}
