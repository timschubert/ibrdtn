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
		USBConnection::USBConnection(ibrcommon::usbsocket *sock, const dtn::core::Node &node)
				: _socket(sock), _node(node), _stream(*sock)
		{
		}

		USBConnection::~USBConnection()
		{
		}

		bool USBConnection::match(const dtn::core::Node &node) const
		{
			return node == _node;
		}

		bool USBConnection::match(const dtn::data::EID &destination) const
		{
			return _node.getEID().sameHost(destination);
		}

		bool USBConnection::match(const dtn::core::NodeEvent &evt) const
		{
			return match(evt.getNode());
		}

		usbstream& USBConnection::getStream()
		{
			return _stream;
		}

		USBConvergenceLayer::USBConvergenceLayer(uint16_t vendor, uint16_t product, uint8_t interfaceNum, uint8_t endpointIn, uint8_t endpointOut)
			: dtn::daemon::IntegratedComponent(), _run(false), _con(ibrcommon::usbconnector::get_instance()), _config(daemon::Configuration::getInstance().getUSB()), _endpointIn(endpointIn), _endpointOut(endpointOut)
		{
			_usb = new USBTransferService(*this);
			_cb_registration = _con.register_device_cb(this, vendor, product, interfaceNum);
		}

		USBConvergenceLayer::~USBConvergenceLayer()
		{
			_con.unregister_device_cb(this, _cb_registration);
			delete _usb;
			for (auto e : _connections)
			{
				delete e;
			}
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
			/* prepare a new transfer task */
			USBTransferService::Task *task = new USBTransferService::Task(n, job, 0);

			_usb->queue(task);
		}

		void USBConvergenceLayer::onAdvertiseBeacon(const ibrcommon::vinterface &iface, DiscoveryBeacon &beacon) throw (NoServiceHereException)
		{
			/* broadcast beacon over USB */
			std::stringstream ss;

			/* prepend header */
			uint8_t header = USBConvergenceLayerType::DISCOVERY;

			ss << header << beacon;
			std::string datastr = ss.str();
			const char *data = datastr.c_str();
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
				try
				{
					const usbinterface &usbiface = dynamic_cast<const usbinterface&>(iface);
					std::stringstream ss;
					ss << "usb=" << "host-mode" << ";vendor=" << usbiface.vendor << ";product=" << usbiface.product << ";serial=" << usbiface.serial;
					DiscoveryService srv("usb", ss.str());
					beacon.addService(srv);

					for (auto &fake : _fakedServices)
					{
						for (auto &service : fake.second)
						{
							beacon.addService(service);
						}
					}
				} catch (std::bad_cast&)
				{

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
						usbsocket *sock = dynamic_cast<usbsocket *>(fd);
						char data[1500];
						usbinterface iface = sock->interface;
						stringstream ss;
						ss << iface.vendor << "." << iface.product << "." << iface.serial;
						vaddress sender(ss.str(), "");

						ssize_t len = 0;
						try
						{
							len = sock->recvfrom(data, 1500, 0, sender);
							if (len < 0) continue;
						} catch (socket_exception &e)
						{
							continue;
						}

						stringstream ss1;
						ss1.write(data, len);

						try
						{
							/* parse header */
							uint8_t header = 0;
							ss >> header;

							/* find header and parse */
							const int flags = (header & USBConvergenceLayerMask::FLAGS);
							const int seqnr = (header & USBConvergenceLayerMask::SEQNO) >> 2;

							DiscoveryBeacon beacon = agent.obtainBeacon();

							switch (header & USBConvergenceLayerMask::TYPE) {
								case USBConvergenceLayerType::DATA:
								IBRCOMMON_LOGGER_TAG("USBConvergenceLayer", info) << "Incoming data frame (seqnr = " << seqnr << ") from " << iface.interface_num << sock->ep_in << IBRCOMMON_LOGGER_ENDL;
								// TODO submit bundle event
								break;

								case USBConvergenceLayerType::DISCOVERY:
								IBRCOMMON_LOGGER_TAG("USBConvergenceLayer", info) << "Incoming discovery (seqnr = " << seqnr << ") from " << iface.interface_num << sock->ep_in << IBRCOMMON_LOGGER_ENDL;
								ss >> beacon;
								handle_discovery(beacon, sock);
								break;

								case USBConvergenceLayerType::ACK:
								IBRCOMMON_LOGGER_TAG("USBConvergenceLayer", info) << "Incoming acknowledgment (seqnr = " << seqnr << ") from " << iface.interface_num << sock->ep_in << IBRCOMMON_LOGGER_ENDL;
								// TODO
								break;

								case USBConvergenceLayerType::NACK:
								IBRCOMMON_LOGGER_TAG("USBConvergenceLayer", info) << "Incoming negative acknowledgment (seqnr = " << seqnr << ") from " << iface.interface_num << sock->ep_in << IBRCOMMON_LOGGER_ENDL;
								// TODO
								break;

								default:
								IBRCOMMON_LOGGER_TAG("USBConvergenceLayer", warning) << "Incoming data not recognized (seqnr = " << seqnr << ") from " << iface.interface_num << sock->ep_in << IBRCOMMON_LOGGER_ENDL;
								break;
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

		void USBConvergenceLayer::handle_discovery(DiscoveryBeacon &beacon, usbsocket *sock)
		{
			usbinterface iface = sock->interface;
			if (beacon.isShort()) {
				/* generate EID */
				stringstream ss;
				ss << iface.vendor;
				ss << iface.product;
				ss << iface.interface_num;
				beacon.setEID(dtn::data::EID("usb://[" + ss.str()));

				/* add this CL if list is empty */
				ss.flush();
				ss << "usb=" << "host-mode" << ";vendor=" << iface.vendor << ";product=" << iface.product << ";serial=" << iface.serial;
				beacon.addService(dtn::net::DiscoveryService(dtn::core::Node::CONN_DGRAM_USB, ss.str()));
			}

			DiscoveryBeacon::service_list &services = beacon.getServices();

			{
				MutexLock l(_discoveryLock);
				_discoveredSockets[beacon.getEID()] = sock;
			}
			/* announce the beacon */
			dtn::net::DiscoveryAgent &agent = dtn::core::BundleCore::getInstance().getDiscoveryAgent();
			agent.onBeaconReceived(beacon);

			if (_config.getGateway()) {
				MutexLock l(_fakedServicesLock);
				const auto &timer = _fakedServicesTimers.find(beacon.getEID());
				if (timer != _fakedServicesTimers.end()) {
					timer->second->reset();
				}
				else {
					/* set timeout */
					_fakedServicesTimers[beacon.getEID()] = new Timer(*this, 1000);

					/* copy services */
					_fakedServices[beacon.getEID()] = services;
				}
			}

			if (_config.getProxy()) {
				DiscoveryBeaconEvent::raise(beacon, EventDiscoveryBeaconAction::DISCOVERY_PROXY, *sock);
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
					throw ibrcommon::Timer::StopTimerException();
				}
			}

			/* else error occurred, immediately expire the timer */
			return 0;
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
						try
						{
						usbsocket *sock = dynamic_cast<usbsocket *>(s);
						if (*sock != dynamic_cast<const usbsocket&>(event.getSender()))
						{
							vaddress empty;
							sock->sendto(data.c_str(), data.size(), 0, empty);
						}
						}
						catch (std::bad_cast&)
						{
							break;
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

			try
			{
				usbsocket *sock = new usbsocket(iface, _endpointIn, _endpointOut, 1000);
				sock->up();
				_vsocket.add(sock, iface);
			} catch (const socket_exception&)
			{
				IBRCOMMON_LOGGER_DEBUG(80) << "usbsocket failed to obtain socket" << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void USBConvergenceLayer::raiseEvent(const NodeEvent &event) throw ()
		{
			const Node &node = event.getNode();
			switch (event.getAction())
			{
			case NODE_DATA_ADDED:
				if (node.has(Node::CONN_DGRAM_USB))
				{
					MutexLock l(_discoveryLock);
					const auto &found = _discoveredSockets.find(node.getEID());
					if (found != _discoveredSockets.end() && found->second != NULL)
					{
						MutexLock l(_connectionsLock);

						USBConnection *con = new USBConnection(found->second, node);
						_connections.push_back(con);
					}
				}
				break;
			case NODE_DATA_REMOVED:
			{
				MutexLock l(_discoveryLock);
				_discoveredSockets[node.getEID()] = NULL;
			}
				break;
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

		std::list<USBConnection*> USBConvergenceLayer::getConnections(const Node &node)
		{
			std::list<USBConnection*> cons;
			ibrcommon::MutexLock l(_connectionsLock);
			for (auto con : _connections)
			{
				if (con->match(node))
				{
					cons.push_back(con);
				}
			}
			return cons;
		}


		const std::string USBConvergenceLayer::getName() const
		{
			return "USBConvergenceLayer";
		}
	}
}
