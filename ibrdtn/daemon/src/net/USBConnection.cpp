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

using namespace ibrcommon;

namespace dtn
{
	namespace net
	{
		USBConnection::USBConnection(usbstream &stream, dtn::core::Node &_node, USBConnectionCallback &cb)
		 : _stream(stream), _in_sequence_number(0), _out_sequence_number(0), _node(_node), _config(daemon::Configuration::getInstance().getUSB()), _cb(cb)
		{
		}

		void USBConnection::raiseEvent(const NodeEvent &event) throw()
		{
			const Node &node = event.getNode();
			switch (event.getAction())
			{
				case NODE_DATA_ADDED:
					if (node.has(Node::CONN_USB))
					{
						_node = node;
						ConnectionEvent::raise(ConnectionEvent::CONNECTION_UP, _node);
					}
					break;

				case NODE_DATA_REMOVED:
					if (node.has(Node::CONN_USB) && node == _node)
					{
						ConnectionEvent::raise(ConnectionEvent::CONNECTION_DOWN, _node);
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

		const dtn::core::Node &USBConnection::getNode() const
		{
			return _node;
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
					context.setProtocol(dtn::core::Node::CONN_USB);

					/* push bundle through the filter routines */
					context.setBundle(data);
					BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().filter(dtn::core::BundleFilter::OUTPUT, context, data);

					_stream << data;

					if (_stream.good())
					{
						job.complete();
					}
					else
					{
						job.abort(TransferAbortedEvent::REASON_BUNDLE_DELETED);
					}
				}
			}
			catch (socket_error &e)
			{
				/* retry because EAGAIN */
				//_work.push(job);
				job.abort(TransferAbortedEvent::REASON_UNDEFINED);
			}
			catch (Exception &e)
			{
				job.abort(TransferAbortedEvent::REASON_UNDEFINED);
			}
		}

		USBConnection &operator<<(USBConnection &out, const dtn::data::Bundle &bundle)
		{
			MutexLock l(out._safeLock);
			out._stream << 0x0;
			DefaultSerializer(out._stream) << bundle;
			return out;
		}

		USBConnection &operator>>(USBConnection &in, dtn::data::Bundle &bundle)
		{
			MutexLock l(in._safeLock);
			DefaultDeserializer(in._stream) >> bundle;
			return in;
		}

		USBConnection &operator<<(USBConnection &out, const DiscoveryBeacon &beacon)
		{
			MutexLock l(out._safeLock);
			out._stream << 0xff;
			out._stream << beacon;
			return out;
		}

		USBConnection &operator>>(USBConnection &in, DiscoveryBeacon &beacon)
		{
			MutexLock l(in._safeLock);
			in._stream >> beacon;
			return in;
		}

		void USBConnection::processInput()
		{
			char header;
			{
				MutexLock l(_safeLock);
				_stream >> header;
			}

			if (header == 0)
			{
				Bundle b;
				(*this) >> b;
				_cb.eventBundleReceived(b);
			}
			else
			{
				DiscoveryBeacon b = dtn::core::BundleCore::getInstance().getDiscoveryAgent().obtainBeacon();
				(*this) >> b;
				_cb.eventBeaconReceived(b);
			}
		}

		bool USBConnection::isDown() const
		{
			return _stream.bad();
		}
	}
}
