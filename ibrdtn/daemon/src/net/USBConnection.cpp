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
			: ibrcommon::usbstream(sock, buflen),
			  _in_sequence_number(0),
			  _out_sequence_number(0),
			  _node(_node)
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

		USBMessageType USBConnection::getNextType()
		{
			uint8_t header;
			if (!((*this) >> header))
			{
				throw dtn::InvalidDataException("failed to parse usb header");
			}

			/* ignored */
			const uint8_t flags = (header & CONVERGENCE_LAYER_MASK_FLAGS);

			const uint8_t seqnr = ((header & CONVERGENCE_LAYER_MASK_SEQNO) >> 2);
			// TODO check seqnr, is usb so probably no problem
			_in_sequence_number = seqnr;

			const int type = (header & CONVERGENCE_LAYER_MASK_TYPE);

			switch (type)
			{
			case CONVERGENCE_LAYER_TYPE_DATA:
				return DATA;
				break;

			case CONVERGENCE_LAYER_TYPE_DISCOVERY:
				return DISCOVERY;
				break;

			case CONVERGENCE_LAYER_TYPE_ACK:
				return ACK;
				break;

			case CONVERGENCE_LAYER_TYPE_NACK:
				return NACK;
				break;

			case CONVERGENCE_LAYER_TYPE_COMMAND:
				return COMMAND;
				break;

			default:
				clear();
				ignore(numeric_limits<streamsize>::max());
				throw dtn::InvalidDataException("failed to parse usb header type");
				break;
			}
		}

		USBConnection& operator<<(USBConnection &out, const dtn::data::Bundle &bundle)
		{
			uint8_t header = 0;
			header |= CONVERGENCE_LAYER_TYPE_DATA & CONVERGENCE_LAYER_MASK_TYPE;
			header |= (out._out_sequence_number << 2) & CONVERGENCE_LAYER_MASK_SEQNO;
			static_cast<ibrcommon::usbstream &>(out) << header;
			DefaultSerializer(out) << bundle;
			out._out_sequence_number = (out._out_sequence_number + 1) % 4;

			out.flush();

			return out;
		}

		USBConnection& operator<<(USBConnection &out, const DiscoveryBeacon &beacon)
		{
			uint8_t header = 0;
			header |= (CONVERGENCE_LAYER_TYPE_DISCOVERY & CONVERGENCE_LAYER_MASK_TYPE);
			header |= ((out._out_sequence_number << 2) & CONVERGENCE_LAYER_MASK_SEQNO);


			static_cast<ibrcommon::usbstream&>(out) << (char) header << beacon;
			out._out_sequence_number = (out._out_sequence_number + 1) % 4;

			out.flush();

			return out;
		}

		USBConnection& operator>>(USBConnection &in, dtn::data::Bundle &bundle)
		{
			DefaultDeserializer(in) >> bundle;
			if (in.fail())
			{
				in.clear();
				in.ignore(numeric_limits<streamsize>::max());
			}

			return in;
		}

		USBConnection& operator>>(USBConnection &in, DiscoveryBeacon &beacon)
		{
			if (!(in >> beacon))
			{
				in.clear();
				in.ignore(numeric_limits<streamsize>::max());
			}

			return in;
		}

		void USBConnection::reconnect(ibrcommon::usbinterface &iface, const uint8_t &endpointIn, const uint8_t &endpointOut)
		{
			_sock.destroy();
			_sock.add(new ibrcommon::usbsocket(iface, endpointIn, endpointOut));
			_sock.up();
		}

		void USBConnection::addServices(DiscoveryBeacon &beacon)
		{
			ibrcommon::MutexLock l(_fakedServicesLock);

			for (auto &service : _fakedServices)
			{
				beacon.addService(service);
			}
		}

		void USBConnection::setServices(DiscoveryBeacon::service_list &services)
		{
			_fakedServices = services;
		}
	}
}
