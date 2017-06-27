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
		USBConnection::USBConnection(ibrcommon::usbsocket *socket, const size_t buflen)
			: ibrcommon::usbstream(socket, buflen), _in_sequence_number(0), _out_sequence_number(0)
		{
			_node = dtn::core::Node();
		}

		USBConnection::USBConnection(ibrcommon::usbsocket *socket, const size_t buflen, dtn::core::Node &node)
			: USBConnection(socket, buflen)
		{
			_node = node;
		}

		void USBConnection::setNode(const dtn::core::Node &node)
		{
			_node = node;
		}

		void USBConnection::unsetNode()
		{
			_node = dtn::core::Node();
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
			if (!(*this >> header))
			{
				/* ignore complete buffer up to EOF */
				this->ignore(_buflen);
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
				this->ignore(_buflen);
				throw dtn::InvalidDataException("failed to parse usb header type");
				break;
			}
		}

		const ibrcommon::socket_error_code USBConnection::getError() const
		{
			return error;
		}

		USBConnection& operator<<(USBConnection &out, const dtn::data::Bundle &bundle)
		{
			uint8_t header = 0;
			header |= CONVERGENCE_LAYER_TYPE_DATA & CONVERGENCE_LAYER_MASK_TYPE;
			header |= (out._out_sequence_number << 2) & CONVERGENCE_LAYER_MASK_SEQNO;
			static_cast<ibrcommon::usbstream &>(out) << header;
			DefaultSerializer(out) << bundle;
			out._out_sequence_number = (out._out_sequence_number + 1) % 4;

			return out;
		}

		USBConnection& operator<<(USBConnection &out, const DiscoveryBeacon &beacon)
		{
			uint8_t header = 0;
			header |= CONVERGENCE_LAYER_TYPE_DISCOVERY & CONVERGENCE_LAYER_MASK_TYPE;
			header |= (out._out_sequence_number << 2) & CONVERGENCE_LAYER_MASK_SEQNO;
			ibrcommon::usbstream &stream = static_cast<ibrcommon::usbstream &>(out);
			stream << header << beacon;
			out._out_sequence_number = (out._out_sequence_number + 1) % 4;

			return out;
		}

		USBConnection& operator>>(USBConnection &in, dtn::data::Bundle &bundle)
		{
			DefaultDeserializer(in) >> bundle;
			return in;
		}

		USBConnection& operator>>(USBConnection &in, DiscoveryBeacon &beacon)
		{
			in >> beacon;
			return in;
		}
	}
}
