/*
 * USBTransferService.cpp
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

#include "USBConnection.h"

namespace dtn
{
	namespace net
	{
		USBConnection::USBConnection(ibrcommon::usbsocket *sock, size_t buflen, dtn::core::Node &_node)
			: _sock(sock),
			  _in_sequence_number(0),
			  _out_sequence_number(0),
			  _node(_node),
			  _config(daemon::Configuration::getInstance().getUSB())
		{
		}

		void USBConnection::raiseEvent(const NodeEvent &event) throw()
		{
			const Node &node = event.getNode();
			switch (event.getAction())
			{
				case NODE_DATA_ADDED:
					if (node.has(Node::CONN_DGRAM_USB))
					{
						_node = node;
						ConnectionEvent::raise(ConnectionEvent::CONNECTION_UP, _node);
					}
					break;

				case NODE_DATA_REMOVED:
					if (node.has(Node::CONN_DGRAM_USB) && node == _node)
					{
						ConnectionEvent::raise(ConnectionEvent::CONNECTION_DOWN, _node);

						/* reset the faked services */
						_fakedServices.clear();
					}
					break;

				default:
					break;
			}
		}

		USBConnection::~USBConnection()
		{
			while (!_work.empty())
			{
				BundleTransfer job = _work.poll(10);
				job.abort(TransferAbortedEvent::REASON_CONNECTION_DOWN);
			}
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

		const dtn::core::Node& USBConnection::getNode() const
		{
			return _node;
		}

		void USBConnection::addServices(DiscoveryBeacon &beacon)
		{
			ibrcommon::MutexLock l(_fakedServicesLock);

			for (auto &service : _fakedServices)
			{
				beacon.addService(service);
			}
		}

		void USBConnection::setServices(DiscoveryBeacon::service_list services)
		{
			_fakedServices = services;
		}

		ibrcommon::usbsocket *USBConnection::getSocket() const
		{
			return _sock;
		}


		bool USBConnection::match(const ibrcommon::usbsocket *sock) const
		{
			return this->_sock == sock;
		}

		void USBConnection::queue(const dtn::net::BundleTransfer &job)
		{
			// TODO work-around for contents of writeset beeing ignored by vsocket.select()
			//_work.push(bundle);
			BundleTransfer jobo = job;
			__processJob(jobo);
		}

		void USBConnection::processJobs()
		{
			while (!_work.empty())
			{
				BundleTransfer job = _work.poll(10);
				__processJob(job);
			}
		}

		void USBConnection::__processJob(BundleTransfer &job)
		{
			dtn::storage::BundleStorage &storage = dtn::core::BundleCore::getInstance().getStorage();
			std::ostringstream ss;

			try
			{
				const dtn::data::MetaBundle &bundle = job.getBundle();

				/* transmit Bundle */
				if (!storage.contains(bundle))
				{
					IBRCOMMON_LOGGER_DEBUG(80) << "Bundle " << bundle << " not found" << IBRCOMMON_LOGGER_ENDL;
					job.abort(TransferAbortedEvent::REASON_BUNDLE_DELETED);
				}
				else
				{
					dtn::data::Bundle data = storage.get(job.getBundle());
					dtn::core::FilterContext context;
					context.setPeer(getNode().getEID());
					context.setProtocol(dtn::core::Node::CONN_DGRAM_USB);

					/* push bundle through the filter routines */
					context.setBundle(data);
					BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().filter(dtn::core::BundleFilter::OUTPUT, context, data);

					ss.flush();
					// TODO prepend header
					DefaultSerializer(ss) << data;
					std::string buf = ss.str();
					ibrcommon::vaddress empty = ibrcommon::vaddress();
					_sock->sendto(buf.c_str(), buf.size(), 0, empty);
					job.complete();
				}
			} catch (ibrcommon::socket_error &e)
			{
				/* retry because EAGAIN */
				//_work.push(job);
				job.abort(TransferAbortedEvent::REASON_UNDEFINED);
			} catch (ibrcommon::Exception &e)
			{
				job.abort(TransferAbortedEvent::REASON_UNDEFINED);
			}
		}

		void USBConnection::processBeacon(const DiscoveryBeacon &beacon)
		{
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
					setServices(beacon.getServices());
				}
			}
		}

		void USBConnection::processInput()
		{
			char buf[1000];

			ibrcommon::vaddress empty = ibrcommon::vaddress();
			size_t bytes = _sock->recvfrom(buf, 1000, 0, empty);

			if (bytes == 0)
			{
				IBRCOMMON_LOGGER_DEBUG_TAG("USBConnection", 70) << "Empty message received" << IBRCOMMON_LOGGER_ENDL;
			}

			std::stringstream ss;

			ss.write(buf, bytes);

			dtn::data::Bundle bundle;
			dtn::net::DiscoveryBeacon beacon;

			uint8_t header ;
			if (ss >> header)
			{
				// TODO check & update sequence number
				switch (header)
				{
				case CONVERGENCE_LAYER_TYPE_DISCOVERY:
					if (!(ss >> beacon))
					{
						throw InvalidDataException("failed to parse discovery beacon");
					}

					if (beacon.isShort())
					{
						/* add this CL if list is empty */
						ss.flush();
						ss << "usb=" << "host-mode";
						beacon.addService(dtn::net::DiscoveryService(dtn::core::Node::CONN_DGRAM_USB, ss.str()));
					}

					processBeacon(beacon);

					break;

				case CONVERGENCE_LAYER_TYPE_DATA:
					DefaultDeserializer(ss) >> bundle;
					if (ss.fail())
					{
						throw InvalidDataException("failed to parse bundle");
					}
					__processBundle(bundle);
					break;

				case CONVERGENCE_LAYER_TYPE_ACK:
					break;

				case CONVERGENCE_LAYER_TYPE_NACK:
					break;

				case CONVERGENCE_LAYER_TYPE_TEMP_NACK:
					break;
				}
			}
		}

		void USBConnection::__processBundle(Bundle &newBundle)
		{
			/* validate the bundle */
			try {
				dtn::core::BundleCore::getInstance().validate(newBundle);
			}
			catch (dtn::data::Validator::RejectedException&) {
				throw InvalidDataException("Bundle was rejected by validator");
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
	}
}
