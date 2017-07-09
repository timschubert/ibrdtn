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
#include "ConnectionEvent.h"
#include "DiscoveryService.h"
#include "core/BundleCore.h"
#include "core/Node.h"
#include "core/NodeEvent.h"
#include "ibrcommon/Logger.h"
#include "ibrcommon/net/socketstream.h"
#include "ibrcommon/thread/RWLock.h"
#include "ibrcommon/thread/RWMutex.h"
#include "ibrcommon/usb/usbsocket.h"
#include "storage/BundleStorage.h"

using namespace ibrcommon;

namespace dtn
{
	namespace net
	{
		class USBConnection : public dtn::core::EventReceiver<dtn::core::NodeEvent>
		{
		public:
			class USBConnectionCallback
			{
			public:
				virtual void eventBundleReceived(Bundle &b) = 0;
				virtual void eventBeaconReceived(DiscoveryBeacon &b) = 0;
			};

			USBConnection(usbsocket *sock, dtn::core::Node &node, USBConnectionCallback &cb);
			virtual ~USBConnection();

			void raiseEvent(const NodeEvent &event) throw();

			bool match(const dtn::core::Node &node) const;
			bool match(const dtn::data::EID &destination) const;
			bool match(const dtn::core::NodeEvent &evt) const;

			const dtn::core::Node &getNode() const;

			void queue(const dtn::net::BundleTransfer &transfer);
			void processJobs();
			void processInput();

			void close();

			friend USBConnection &operator<<(USBConnection &out, const dtn::data::Bundle &bundle);

			friend USBConnection &operator<<(USBConnection &out, const DiscoveryBeacon &beacon);

		private:
			RWMutex _rw;
			socketstream *_stream;
			usbsocket *_socket;

			Queue<dtn::net::BundleTransfer> _work;

			dtn::core::Node &_node;

			/**
			 * usb covergence layer config
			 */
			const daemon::Configuration::USB &_config;

			USBConnectionCallback &_cb;

			/**
			 * Process a bundle transfer
			 */
			void __processJob(BundleTransfer &job);

			friend USBConnection &operator>>(USBConnection &in, dtn::data::Bundle &bundle);

			friend USBConnection &operator>>(USBConnection &in, DiscoveryBeacon &beacon);
		};
	}
}

#endif /* IBRDTN_DAEMON_SRC_NET_USBCONNECTION_H_ */
