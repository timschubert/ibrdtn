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
		 , _interfaceNum(interfaceNum)
		 , _vendor_id(vendor)
		 , _product_id(product)
		 , _service(NULL)
		 , _interface(usbconnector::get_instance().open(vendor, product, interfaceNum))
		{
			_service = new USBService(usbconnector::get_instance());
			_interface.set_up();
		}

		USBConvergenceLayer::~USBConvergenceLayer()
		{
			_interface.set_down();
			delete _service;
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
					*con << storage.get(bundle);
				}
			} catch (ibrcommon::socket_exception &e)
			{
				IBRCOMMON_LOGGER_DEBUG(80) << e.what() << IBRCOMMON_LOGGER_ENDL;
				dtn::net::TransferAbortedEvent::raise(job.getNeighbor().getNode(), bundle, dtn::net::TransferAbortedEvent::REASON_UNDEFINED);
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

			{
				MutexLock l(_connectionsLock);
				for (auto *con : _connections)
				{
					if (con->match(n))
					{
						submit(con, job);
						return;
					}
				}
			}

			dtn::net::TransferAbortedEvent::raise(n.getEID(), job.getBundle(), dtn::net::TransferAbortedEvent::REASON_UNDEFINED);
		}

		void USBConvergenceLayer::onAdvertiseBeacon(const vinterface &iface, const DiscoveryBeacon &beacon) throw ()
		{
			/* broadcast beacon over USB */
			IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "Sending beacon over USB." << IBRCOMMON_LOGGER_ENDL;

			MutexLock l(_connectionsLock);

			for (auto *con : _connections)
			{
				try
				{
					/* send beacon */
					(*con) << beacon;
				} catch (ibrcommon::Exception &e)
				{
					IBRCOMMON_LOGGER_DEBUG_TAG("USBConvergenecLayer", 80) << "Failed to transmit beacon: " << e.what() << IBRCOMMON_LOGGER_ENDL;
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

			if (_config.getGateway())
			{
				MutexLock l(_connectionsLock);
				for (auto *con : _connections)
				{
					con->addServices(beacon);
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
				dtn::core::BundleCore::getInstance().getDiscoveryAgent().registerService(_interface, this);
			} catch (ibrcommon::Exception &e)
			{
				IBRCOMMON_LOGGER_TAG(TAG, warning) << "Failed to set USB CL up: " << e.what() << IBRCOMMON_LOGGER_ENDL;
			}

			usbconnector::get_instance().register_device_cb(this, _vendor_id, _product_id);
		}

		void USBConvergenceLayer::componentDown() throw()
		{
			try
			{
				usbconnector::get_instance().unregister_device_cb(this, _vendor_id, _product_id);

				dtn::core::BundleCore::getInstance().getDiscoveryAgent().unregisterService(_interface, this);

				try
				{
					this->stop();
					this->join();
				} catch (const ThreadException &e)
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 90) << e.what() << IBRCOMMON_LOGGER_ENDL;
				}

				{
					MutexLock l(_connectionsLock);
					for (auto &con : _connections)
					{
						delete con;
						_connections.erase(con);
					}
				}
			} catch (ibrcommon::Exception &e)
			{
				IBRCOMMON_LOGGER_TAG(TAG, warning) << "Failed to set USB CL down: " << e.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		//void USBConvergenceLayer::recover()
		//{
		//	try
		//	{
		//		_interface = usbconnector::get_instance().open(_vendor_id, _product_id, _interfaceNum);
		//		_interface.set_up();
		//		_connection->reconnect(_interface, _endpointIn, _endpointOut);
		//		IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "Recovered interface" << IBRCOMMON_LOGGER_ENDL;
		//	}
		//	catch (ibrcommon::Exception &e)
		//	{
		//		IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "recover: " << e.what() << IBRCOMMON_LOGGER_ENDL;
		//		Thread::sleep(1000);
		//	}
		//}

		void USBConvergenceLayer::processInput(USBConnection *con)
		{
			dtn::data::Bundle bundle;
			DiscoveryBeacon beacon = dtn::core::BundleCore::getInstance().getDiscoveryAgent().obtainBeacon();

			USBMessageType type = con->getNextType();

			switch (type) {

				case DATA:
				if (!(*con >> bundle))
				{
					throw dtn::InvalidDataException("failed to parse bundle");
				}
				processIncomingBundle(bundle);
				break;

				case DISCOVERY:
				if (!(*con >> beacon))
				{
					throw dtn::InvalidDataException("failed to parse discovery message");
				}
				processBeacon(con, beacon);
				break;

				case ACK:
				// TODO

				case NACK:
				// TODO

				case COMMAND:
				// TODO

				default:
					throw dtn::InvalidDataException("failed to parse USB message");
				break;
			}
		}

		void USBConvergenceLayer::componentRun() throw()
		{
			_run = true;

			try {
				while (_run)
				{
					if (_connections.empty())
					{
						IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "Disconnected" << IBRCOMMON_LOGGER_ENDL;
						Thread::sleep(1000);
					}

					// TODO support multiple connections / sockets
					for (auto *con : _connections)
					{
						try
						{
							processInput(con);
							break;
						} catch (const dtn::InvalidDataException &e)
						{
							IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << e.what() << IBRCOMMON_LOGGER_ENDL;
						}
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
			_run = false;
		}

		void USBConvergenceLayer::processBeacon(USBConnection *con, DiscoveryBeacon &beacon)
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

			/* announce the beacon, ignores own beacons, that is from the same EID */
			dtn::core::BundleCore::getInstance().getDiscoveryAgent().onBeaconReceived(beacon);

			/*
			 * fake the services of a node that is attached via USB and has the same EID
			 * they are send to other attached nodes
			 */
			if (beacon.getEID() == dtn::core::BundleCore::local)
			{
				if (_config.getProxy())
				{
					con->setServices(beacon.getServices());
				}
			}
		}

		void USBConvergenceLayer::onReceiveBeacon(const ibrcommon::vinterface &iface, const DiscoveryBeacon &beacon) throw ()
		{
			if (_config.getProxy())
			{
				for (auto *con : _connections)
				{
					/* prevent loop-back */
					if (!con->match(beacon.getEID()))
					{
						(*con) << beacon;
					}
				}
			}
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
				dtn::core::BundleCore::getInstance().getDiscoveryAgent().unregisterService(_interface, this);

				usbinterface iface("amphis", dev, _interfaceNum);
				_interface.set_down();
				iface.set_up();
				_interface = iface;

				dtn::core::BundleCore::getInstance().getDiscoveryAgent().registerService(_interface, this);

				usbsocket *sock = new usbsocket(_interface, _endpointIn, _endpointOut);
				MutexLock l(_connectionsLock);

				dtn::core::Node nonode = dtn::core::Node();
				_connections.insert(new USBConnection(sock, 1000, nonode));
			} catch (ibrcommon::Exception &e)
			{
				IBRCOMMON_LOGGER_TAG("USBConvergenecLayer", warning) << "failed to open new device: " << e.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void USBConvergenceLayer::device_lost(const usbdevice &dev)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("USBConvergenecLayer", 80) << "device lost" << IBRCOMMON_LOGGER_ENDL;

			try
			{
				dtn::core::BundleCore::getInstance().getDiscoveryAgent().unregisterService(_interface, this);

				/* clear connections */
				MutexLock l(_connectionsLock);
				for (auto *con : _connections)
				{
					delete con;
				}

				_connections.clear();

				_interface.set_down();
			} catch (ibrcommon::Exception &e)
			{
				IBRCOMMON_LOGGER_TAG("USBConvergenecLayer", warning) << "failed to set interface down after device was lost: " << e.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}
	}
}
