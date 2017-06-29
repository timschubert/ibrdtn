/*
 * USBTransferService.h
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

#ifndef IBRDTN_DAEMON_SRC_NET_USBCONNECTION_H_
#define IBRDTN_DAEMON_SRC_NET_USBCONNECTION_H_

#include "BundleTransfer.h"
#include "Configuration.h"
#include "DiscoveryService.h"
#include "core/BundleCore.h"
#include "core/Node.h"
#include "core/NodeEvent.h"
#include "ibrcommon/Logger.h"
#include "ibrcommon/net/dgramheader.h"
#include "ibrcommon/usb/usbstream.h"
#include "ibrcommon/thread/Mutex.h"
#include "storage/BundleStorage.h"
#include "ConnectionEvent.h"

namespace dtn
{
	namespace net
	{
		enum USBMessageType
		{
			DATA, DISCOVERY, ACK, NACK, COMMAND
		};

		class USBConnection
				: public ibrcommon::usbstream,
				  public dtn::core::EventReceiver<dtn::core::NodeEvent>
		{
		public:
			USBConnection(ibrcommon::usbsocket *sock, size_t buflen, dtn::core::Node &node);
			virtual ~USBConnection();

			void raiseEvent(const NodeEvent &event) throw();

			bool match(const dtn::core::Node &node) const;
			bool match(const dtn::data::EID &destination) const;
			bool match(const dtn::core::NodeEvent &evt) const;

			const dtn::core::Node& getNode() const;
			USBMessageType getNextType();

			void setServices(DiscoveryBeacon::service_list &services);
			void addServices(DiscoveryBeacon &beacon);

			void reconnect(ibrcommon::usbinterface &iface, const uint8_t &endpointIn, const uint8_t &endpointOut);

			friend USBConnection& operator<<(USBConnection &out, const dtn::data::Bundle &bundle);
			friend USBConnection& operator<<(USBConnection &out, const DiscoveryBeacon &beacon);

			friend USBConnection& operator>>(USBConnection &in, dtn::data::Bundle &bundle);
			friend USBConnection& operator>>(USBConnection &in, DiscoveryBeacon &beacon);

		private:
			dtn::core::Node &_node;
			uint8_t _in_sequence_number;
			uint8_t _out_sequence_number;

			/**
			 * for locking faked services
			 */
			ibrcommon::Mutex _fakedServicesLock;

			/**
			 * faked services
			 */
			DiscoveryBeacon::service_list _fakedServices;
		};
	}

}

#endif /* IBRDTN_DAEMON_SRC_NET_USBCONNECTION_H_ */
