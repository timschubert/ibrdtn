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
		 , _con(usbconnector::get_instance())
		 , _config(daemon::Configuration::getInstance().getUSB())
		 , _run(false)
		 , _endpointIn(endpointIn)
		 , _endpointOut(endpointOut)
		 , _interface(_con.open(vendor, product))
		 , _in_sequence_number(0)
		 , _out_sequence_number(0)
		 , _recovering(false)
		 , _vendor_id(vendor)
		 , _product_id(product)
		{
			_service = new USBService(_con);
			_usb = new USBTransferService();
		}

		USBConvergenceLayer::~USBConvergenceLayer()
		{

			delete _usb;
			delete _service;

			{
				MutexLock l(_fakedServicesLock);
				for (auto &timer : _fakedServicesTimers)
				{
					delete timer.second;
				}
			}
			{
				MutexLock l(_connectionsLock);
				for (auto *con : _connections)
				{
					delete con;
				}
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
			MutexLock l(_connectionsLock);
			for (auto *con : _connections)
			{
				if (con->match(n))
				{
					USBTransferService::Task *task = new USBTransferService::Task(*con, job, 0);

					_usb->queue(task);
					return;
				}
			}

			dtn::net::TransferAbortedEvent::raise(n.getEID(), job.getBundle(), dtn::net::TransferAbortedEvent::REASON_UNDEFINED);
		}

		void USBConvergenceLayer::onAdvertiseBeacon(const vinterface &iface, const DiscoveryBeacon &beacon) throw ()
		{
			/* broadcast beacon over USB */
			std::stringstream ss;

			//dgram_header header = {
			//		.compat = 0,
			//		.type = (CONVERGENCE_LAYER_TYPE_DISCOVERY & CONVERGENCE_LAYER_MASK_TYPE),
			//		.seqnr = _out_sequence_number,
			//		.flags = 0
			//};

			uint8_t header = 0;
			header |= (CONVERGENCE_LAYER_TYPE_DISCOVERY & CONVERGENCE_LAYER_MASK_TYPE);
			header |= (_out_sequence_number << 2);

			ss << header << beacon;
			std::string datastr = ss.str();
			const char *data = datastr.c_str();
			vaddress empty;
			for (auto &s : _vsocket.get(iface))
			{
				usbsocket *sock = dynamic_cast<usbsocket *>(s);
				try
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "Sending beacon over USB." << IBRCOMMON_LOGGER_ENDL;

					sock->sendto(data, datastr.size(), 0, empty);
					_out_sequence_number = (_out_sequence_number + 1) % 4;
				}
				catch (usb_socket_no_device &e)
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << e.what() << IBRCOMMON_LOGGER_ENDL;
					interface_lost(sock->interface);
				}
				// TODO handle other errors
				catch (usb_socket_error &e)
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << e.what() << IBRCOMMON_LOGGER_ENDL;
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
			_interface.set_up();
			this->addSocket(_interface);

			_vsocket.up();
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

			_vsocket.down();
			this->removeSocket(_interface);

			try
			{
				_interface.set_down();
			} catch (USBError &e)
			{
				IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 90) << "Failed to set interface " << _interface.toString() << " down" << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void USBConvergenceLayer::removeSocket(const usbinterface &iface)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 90) << "removing sockets from interface" << IBRCOMMON_LOGGER_ENDL;

			/* un-register as discovery beacon handler */
			//dtn::core::BundleCore::getInstance().getDiscoveryAgent().unregisterService(_interface, this);

			for (auto &s : _vsocket.get(iface))
			{
				try
				{
					_vsocket.remove(s);
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 80) << "removed a socket" << IBRCOMMON_LOGGER_ENDL;
				}
				catch (socket_error &e)
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 80) << "failed to remove a socket" << e.what() << IBRCOMMON_LOGGER_ENDL;
				}
			}
		}

		void USBConvergenceLayer::recover()
		{
			while (_recovering)
			{
				try
				{
					/* try to open a new interface to the device and open a socket on the interface */
					_interface = _con.open(_vendor_id, _product_id);
					this->addSocket(_interface);
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "Recovered interface" << IBRCOMMON_LOGGER_ENDL;
					_recovering = false;
				}
				catch (usb_device_error &e)
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "recover: " << e.what() << IBRCOMMON_LOGGER_ENDL;
				}
				catch (socket_exception &e)
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "recover: " << e.what() << IBRCOMMON_LOGGER_ENDL;
				}

				/* sleep for 1000 ms */
				Thread::sleep(1000);
			}
		}

		void USBConvergenceLayer::componentRun() throw()
		{
			_run = true;

			try {
				struct timeval tv;
				tv.tv_sec = 1;
				tv.tv_usec = 0;

				while (_run) {
					if (_recovering)
					{
						// TODO replace with reconnect via hotplug if device / interface supports  hotplug
						recover();
						continue;
					}
					socketset fds;
					try {
						_vsocket.select(&fds, NULL, NULL, &tv);
						for (auto &fd : fds) {
							usbsocket *sock = dynamic_cast<usbsocket *>(fd);
							char data[1000];
							usbinterface iface = sock->interface;
							stringstream ss;
							ss << iface.toString();
							vaddress sender(ss.str(), "");

							ssize_t len = 0;
							try {
								len = sock->recvfrom(data, 1000, 0, sender);
								if (len < 0) continue;
							} catch (socket_exception &e) {
								IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70)
									<< e.what() << IBRCOMMON_LOGGER_ENDL;
								continue;
							}

							/* write incoming data to stream */
							ss.flush();
							ss.write(data, len);

							try {
								/* parse header */
								uint8_t header;
								ss >> header;

								const uint8_t received_sequence_number = ((header & CONVERGENCE_LAYER_MASK_SEQNO) >> 2);

								if (_in_sequence_number != received_sequence_number)
								{
									IBRCOMMON_LOGGER_TAG(TAG, info) << "Incoming frame (seqnr = " << received_sequence_number << ") from " << iface.toString() << sock->ep_in << " received out of order " << IBRCOMMON_LOGGER_ENDL;
								}

								DiscoveryBeacon beacon = dtn::core::BundleCore::getInstance().getDiscoveryAgent().obtainBeacon();
								Bundle bundle;

								switch (header & CONVERGENCE_LAYER_MASK_TYPE) {

									case CONVERGENCE_LAYER_TYPE_DATA:
									IBRCOMMON_LOGGER_TAG(TAG, info) << "Incoming data frame (seqnr = " << received_sequence_number << ") from " << iface.toString() << sock->ep_in << IBRCOMMON_LOGGER_ENDL;
									DefaultDeserializer(ss) >> bundle;
									processIncomingBundle(bundle);
									break;

									case CONVERGENCE_LAYER_TYPE_DISCOVERY:
									IBRCOMMON_LOGGER_TAG(TAG, info) << "Incoming discovery (seqnr = " <<  received_sequence_number << ") from " << iface.toString() << sock->ep_in << IBRCOMMON_LOGGER_ENDL;
									ss >> beacon;
									handle_discovery(beacon, *sock);
									break;

									case CONVERGENCE_LAYER_TYPE_ACK:
									IBRCOMMON_LOGGER_TAG(TAG, info) << "Incoming acknowledgment (seqnr = " << received_sequence_number << ") from " << iface.toString() << sock->ep_in << IBRCOMMON_LOGGER_ENDL;
									// TODO
									break;

									case CONVERGENCE_LAYER_TYPE_NACK:
									IBRCOMMON_LOGGER_TAG(TAG, info) << "Incoming negative acknowledgment (seqnr = " << received_sequence_number << ") from " << iface.toString() << sock->ep_in << IBRCOMMON_LOGGER_ENDL; // TODO
									// TODO
									break;

									default:
									IBRCOMMON_LOGGER_TAG(TAG, warning) << "Incoming data not recognized (seqnr = " << received_sequence_number << ") from " << iface.toString() << sock->ep_in << IBRCOMMON_LOGGER_ENDL;
									break;
								}
							} catch (const dtn::InvalidDataException &e) {
								IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70)
									<< e.what() << IBRCOMMON_LOGGER_ENDL;
							} catch (const IOException &e) {
								IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70)
									<< e.what() << IBRCOMMON_LOGGER_ENDL;
							}
						}
					} catch (const vsocket_timeout &e) {
						tv.tv_sec = 1;
						tv.tv_usec = 0;
					} catch (const vsocket_interrupt &e) {
						IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << e.what() << IBRCOMMON_LOGGER_ENDL;
					}
				}
			} catch (std::exception &e) {
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

		void USBConvergenceLayer::handle_discovery(DiscoveryBeacon &beacon, usbsocket &sock)
		{
			usbinterface iface = sock.interface;
			if (beacon.isShort())
			{
				std::stringstream ss;
				ss << "usb://[" << sock.interface.toString() << "]";
				beacon.setEID(dtn::data::EID(ss.str()));

				/* add this CL if list is empty */
				ss.flush();
				ss << "usb="
				   << "host-mode";
				beacon.addService(dtn::net::DiscoveryService(dtn::core::Node::CONN_DGRAM_USB, ss.str()));
			}

			DiscoveryBeacon::service_list &services = beacon.getServices();

			{
				MutexLock l(_connectionsLock);
				bool found = false;
				for (auto *con : _connections)
				{
					if (con->match(beacon.getEID()))
					{
						found = true;
						break;
					}
				}
				if (!found)
				{
					dtn::core::Node best_guess(beacon.getEID());
					USBConnection *con = new USBConnection(sock, best_guess);
					_connections.push_back(con);
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
				DiscoveryBeaconEvent::raise(beacon, EventDiscoveryBeaconAction::DISCOVERY_PROXY, sock);
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
						std::stringstream ss;
						ss << event.getBeacon();
						std::string data = ss.str();
						for (auto &s : _vsocket.getAll())
						{
							try
							{
								usbsocket *sock = dynamic_cast<usbsocket *>(s);
								if (*sock != dynamic_cast<const usbsocket &>(event.getSender()))
								{
									vaddress empty;
									try
									{
										sock->sendto(data.c_str(), data.size(), 0, empty);
									} catch (socket_error &e)
									{
										IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << e.what() << IBRCOMMON_LOGGER_ENDL;
									}
								}
							}
							catch (std::bad_cast &)
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

		void USBConvergenceLayer::addSocket(usbinterface &iface)
		{
			usbsocket *sock = new usbsocket(iface, _endpointIn, _endpointOut);
			_vsocket.add(sock, iface);
			sock->up();
			IBRCOMMON_LOGGER_TAG(TAG, info) << "obtained a new socket" << IBRCOMMON_LOGGER_ENDL;
			dtn::core::BundleCore::getInstance().getDiscoveryAgent().registerService(iface, this);
		}

		void USBConvergenceLayer::interface_discovered(usbinterface &iface)
		{
			try
			{
				iface.set_up();
				addSocket(iface);
			}
			catch (USBError &e)
			{
				IBRCOMMON_LOGGER_TAG(TAG, warning) << "Failed to setup interface: " << e.what() << IBRCOMMON_LOGGER_ENDL;
			}
			catch (socket_exception &e)
			{
				IBRCOMMON_LOGGER_TAG(TAG, warning) << "Failed to setup socket on interface " << e.what() << IBRCOMMON_LOGGER_ENDL;
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
						MutexLock l(_connectionsLock);
						for (auto *con : _connections)
						{
							if (con->match(node))
							{
								con->addNode(node);
							}
						}
					}
					break;
				case NODE_DATA_REMOVED:
				{
					MutexLock l(_connectionsLock);
					for (std::vector<USBConnection *>::iterator con = _connections.begin(); con != _connections.end(); ++con)
					{
						if ((*con)->match(node))
						{
							delete *con;
							_connections.erase(con);
							break;
						}
					}
					break;
				}
				default:
					break;
			}
		}

		void USBConvergenceLayer::interface_lost(const usbinterface &iface)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "lost interface " << iface.toString() << IBRCOMMON_LOGGER_ENDL;

			removeSocket(iface); // THIS

			if (iface == _interface)
			{
				//_vsocket.down();
				try
				{
					_interface.set_down();
				}
				catch (USBError &e)
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "failed to set interface " << iface.toString() << " down" << IBRCOMMON_LOGGER_ENDL;
				}
			}

			IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "starting recovery of " << iface.toString() << IBRCOMMON_LOGGER_ENDL;
			_recovering = true;
		}

		std::list<USBConnection*> USBConvergenceLayer::getConnections(const Node &node)
		{
			std::list<USBConnection*> cons;
			MutexLock l(_connectionsLock);
			for (auto *con : _connections)
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
			return TAG;
		}
	}
}
