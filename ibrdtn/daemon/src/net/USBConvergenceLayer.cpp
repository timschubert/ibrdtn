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
		{
		}

		USBConvergenceLayer::~USBConvergenceLayer()
		{
		}

		dtn::core::Node::Protocol USBConvergenceLayer::getDiscoveryProtocol() const
		{
			return dtn::core::Node::CONN_DGRAM_USB;
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
						con->queue(job);
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

			std::stringstream ss;
			ss << beacon;
			std::string buf = ss.str();
			vaddress addr = vaddress();

			for (auto *sock : _socket.get(iface))
			{
				usbsocket *usbsock = static_cast<usbsocket *>(sock);

				/* send now or fail */
				try
				{
					usbsock->sendto(buf.c_str(), buf.size(), 0, addr);
				} catch (ibrcommon::Exception &e)
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 90) << "failed to sen beacon: " << e.what() << IBRCOMMON_LOGGER_ENDL;
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
					/* add services to beacon */
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
				usbconnector::get_instance().register_device_cb(this, _vendor_id, _product_id);

			} catch (ibrcommon::Exception &e)
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
					_socket.destroy();
				}

				MutexLock i(_interfacesLock);
				for (auto &iface : _interfaces)
				{
					dtn::core::BundleCore::getInstance().getDiscoveryAgent().unregisterService(iface, this);
					_interfaces.erase(iface);
				}

			} catch (ibrcommon::Exception &e)
			{
				IBRCOMMON_LOGGER_TAG(TAG, warning) << "Failed to set USB CL down: " << e.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void USBConvergenceLayer::componentRun() throw()
		{
			_run = true;
			socketset readset;
			socketset writeset;
			socketset errorset;

			try {
				while (_run)
				{
					readset.clear();
					writeset.clear();
					errorset.clear();

					if (_connections.empty())
					{
						IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "no connections" << IBRCOMMON_LOGGER_ENDL;
						Thread::sleep(1000);
					}

					try
					{
						_socket.select(&readset, &writeset, &errorset, NULL);
					} catch (vsocket_interrupt &)
					{
					}

					MutexLock l(_connectionsLock);
					for (auto *con : _connections)
					{
						usbsocket *sock = con->getSocket();

						if (readset.find(sock) != readset.end())
						{
							con->processInput();
						}

						if (writeset.find(sock) != writeset.end())
						{
							con->processJobs();
						}

						if (errorset.find(sock) != errorset.end())
						{
							__processError(con);
						}
					}
				}
			} catch (std::exception &e)
			{
				/* ignore all other errors */
			}
		}

		void USBConvergenceLayer::__cancellation() throw()
		{
			_run = false;
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
						con->processBeacon(beacon);
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
				MutexLock l(_interfacesLock);
				for (auto &iface : _interfaces)
				{
					/* do not allow two interfaces on same device */
					if (dev == iface.get_device())
					{
						return;
					}
				}

				//usbinterface iface(usbconnector::get_instance().open_interface(_vendor_id, _product_id, _interfaceNum));
				usbinterface iface(dev, _interfaceNum);
				_interfaces.insert(iface);
				dtn::core::BundleCore::getInstance().getDiscoveryAgent().registerService(iface, this);

				usbsocket *sock = new usbsocket(iface, _endpointIn, _endpointOut);

				MutexLock c(_connectionsLock);
				dtn::core::Node nonode = dtn::core::Node();
				_connections.insert(new USBConnection(sock, 1000, nonode));

				_socket.add(sock, iface);

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
				MutexLock l(_interfacesLock);
				for (auto &iface : _interfaces)
				{
					/* we only allow one iface per device (see above) */
					if (iface.get_device() == dev)
					{
						dtn::core::BundleCore::getInstance().getDiscoveryAgent().unregisterService(iface, this);
						_interfaces.erase(iface);

						/* clear connections */
						MutexLock l(_connectionsLock);
						for (auto *con : _connections)
						{
							/* only one connection per interface */
							if (con->getSocket()->interface.get_device() == dev)
							{
								_socket.remove(con->getSocket());
								delete con;
								_connections.erase(con);
								break;
							}
						}
					}
				}
			} catch (ibrcommon::Exception &e)
			{
				IBRCOMMON_LOGGER_TAG("USBConvergenecLayer", warning) << "failed to set interface down after device was lost: " << e.what() << IBRCOMMON_LOGGER_ENDL;
			}

				/* clear connections */
				MutexLock l(_connectionsLock);
				for (auto *con : _connections)
				{
					_socket.remove(con->getSocket());
					delete con;
				}

				_connections.clear();

		}

		void USBConvergenceLayer::__processError(USBConnection *con)
		{
			/* remove connection on error */
			//_socket.remove(con->getSocket());
			//_connections.erase(con);
			//delete con;
		}
	}
}
