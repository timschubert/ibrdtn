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
		USBConvergenceLayer::USBConvergenceLayer(usbconnector &con)
				: _con(con), _usb(USBTransferService::getInstance()), dtn::daemon::IndependentComponent(), _run(false), _fakedServicesTimer(this, 1000)
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
			/* prepare a new transfer task */
			USBTransferService::Task *task = new USBTransferService::Task(n, job);

			_usb.queue(task);
		}

		void USBConvergenceLayer::onAdvertiseBeacon(const ibrcommon::vinterface &iface, DiscoveryBeacon &beacon) throw (NoServiceHereException)
		{
			/* broadcast beacon over USB */
			std::stringstream ss;
			/* "header", ASCII "1" for beacon, "0" for Bundle */
			ss << "1" << beacon;
			std::string datastr = ss.str();
			char *data = datastr.c_str();
			vaddress empty;
			for (auto &s : _vsocket.getAll())
			{
				usbsocket *sock = dynamic_cast<usbsocket *>(s);
				if (iface == sock->interface)
				{
					sock->sendto(data, datastr.size(), 0, empty);
				}
			}

			/* cross-layer discovery via usb */
			if (dtn::daemon::Configuration::getInstance().getDiscovery().enableCrosslayer())
			{
				for (auto &s : _vsocket.getAll())
				{
					usbsocket *sock = dynamic_cast<usbsocket *>(s);
					sock->sendto(data, datastr.size(), 0, empty);
				}
			}
		}

		void USBConvergenceLayer::onUpdateBeacon(const ibrcommon::vinterface &iface, DiscoveryBeacon &beacon) throw (NoServiceHereException)
		{
			if (!_vsocket.get(iface).empty())
			{
				beacon.addService(DiscoveryService(getDiscoveryProtocol(), "usb=" + _config.getOwnAddress()));

				for (auto &fake : _fakedServices)
				{
					beacon.addService(fake);
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

		void USBConvergenceLayer::__cancellation() throw ()
		{
		}

		void USBConvergenceLayer::componentUp() throw ()
		{
			// register as discovery beacon handler
			dtn::core::BundleCore::getInstance().getDiscoveryAgent().registerService(this);
		}

		void USBConvergenceLayer::componentDown() throw ()
		{
			// un-register as discovery beacon handler
			dtn::core::BundleCore::getInstance().getDiscoveryAgent().unregisterService(this);
		}

		void USBConvergenceLayer::componentRun() throw ()
		{
			_run = true;

			dtn::net::DiscoveryAgent &agent = dtn::core::BundleCore::getInstance().getDiscoveryAgent();

			struct timeval tv;
			tv.tv_sec = 1;
			tv.tv_usec = 0;

			while (_run)
			{
				socketset fds;
				try
				{
					_vsocket.select(&fds, NULL, NULL, &tv);
					for (auto &fd : fds)
					{
						usbsocket *sock = dynamic_cast<usbsocket>(fd);
						char data[1500];
						vaddress sender = sock->interface.getAddresses();
						DiscoveryBeacon beacon = agent.obtainBeacon();

						ssize_t len = 0;
						try
						{
							len = sock->recvfrom(data, 1500, 0, sender);
							if (len < 0) continue;
						} catch (socket_exception &e)
						{
							continue;
						}

						stringstream ss;
						ss.write(data, len);

						try
						{
							ss >> beacon;

							if (beacon.isShort())
							{
								/* generate EID */
								beacon.setEID(dtn::data::EID("usb://[" + sender.address()));

								/* add this CL if list is empty */
								beacon.addService(dtn::net::DiscoveryService(dtn::core::Node::CONN_USB, "usb=" + sender.address()));
							}

							DiscoveryBeacon::service_list &services = beacon.getServices();
							for (auto &service : services)
							{
								if (service.getParameters().find("usb") != std::string::npos)
								{
									/* make shure the address is correct */
									service.update("ip=" + sender.address() + ";" + service.getParameters());
								}
							}

							/* announce the beacon */
							agent.onBeaconReceived(beacon);

							if (_config.getGateway())
							{
								MutexLock l(_fakedServicesLock);
								_fakedServicesTimer.reset();
								/* fake all services from neighbor as our own */
								_fakedServices = services; //.insert(_fakedServices.end(), services.begin(), services.end());
							}

							if (_config.getProxy())
							{
								DiscoveryBeaconEvent::raise(beacon, EventDiscoveryBeaconAction::DISCOVERY_PROXY, *sock);
							}
						} catch (const dtn::InvalidDataException&)
						{
						} catch (const ibrcommon::IOException&)
						{
						}
					}
				} catch (const vsocket_timeout &)
				{
					tv.tv_sec = 1;
					tv.tv_usec = 0;
				}
			}
		}

		void USBConvergenceLayer::raiseEvent(const TimeEvent &event) throw ()
		{
			switch (event.getAction())
			{
			case dtn::core::TIME_SECOND_TICK:
				break;
			default:
				break;
			}
		}

		size_t USBConvergenceLayer::timeout(Timer *t)
		{
			MutexLock l(_fakedServicesLock);
			_fakedServices = DiscoveryBeacon::service_list();
			// TODO -> config
			return 10000;
		}

		void USBConvergenceLayer::raiseEvent(const DiscoveryBeaconEvent &event) throw ()
		{
			switch (event.getAction())
			{
			case DISCOVERY_PROXY:
				if (_config.getProxy())
				{
					std::stringstream ss;
					ss << event.getBeacon();
					std::string data = ss.str();
					for (auto &s : _vsocket.getAll())
					{
						usbsocket *sock = dynamic_cast<usbsocket *>(s);
						if (*sock != event.getSender())
						{
							vaddress empty;
							sock->sendto(data.c_str(), data.size(), 0, empty);
						}
					}
				}
				break;
			default:
				break;
			}
		}

		void USBConvergenceLayer::interface_discovered(ibrcommon::usbinterface &iface)
		{
			dtn::net::DiscoveryAgent &agent = dtn::core::BundleCore::getInstance().getDiscoveryAgent();
			agent.registerService(iface, this);

			usbsocket *sock = new usbsocket(iface);
			try
			{
				sock->up();
				_vsocket.add(sock, iface);
			} catch (const socket_exception&)
			{
				delete sock;
			}
		}

		void USBConvergenceLayer::interface_lost(ibrcommon::usbinterface &iface)
		{
			dtn::net::DiscoveryAgent &agent = dtn::core::BundleCore::getInstance().getDiscoveryAgent();
			agent.unregisterService(iface, this);

			for (auto &s : _vsocket.getAll())
			{
				usbsocket *sock = dynamic_cast<usbsocket *>(s);
				if (sock->interface == iface)
				{
					_vsocket.remove(s);
					sock->down();
					delete sock;
					break;
				}
			}
		}
	}
}
