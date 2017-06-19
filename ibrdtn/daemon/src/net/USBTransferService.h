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

#ifndef IBRDTN_DAEMON_SRC_NET_USBTRANSFERSERVICE_H_
#define IBRDTN_DAEMON_SRC_NET_USBTRANSFERSERVICE_H_

#include "BundleTransfer.h"
#include "Configuration.h"
#include "DiscoveryService.h"
#include "core/BundleCore.h"
#include "core/Node.h"
#include "core/NodeEvent.h"
#include "ibrcommon/Logger.h"
#include "ibrcommon/usb/usbstream.h"
#include "storage/BundleStorage.h"
#include <map>
#include <sstream>

namespace dtn
{
	namespace net
	{
		enum USBConvergenceLayerMask
		{
			COMPAT = 0xC0,
			TYPE = 0x30,
			SEQNO = 0x0C,
			FLAGS = 0x03
		};

		enum USBConvergenceLayerType
		{
			DATA = 0x10,
			DISCOVERY = 0x20,
			ACK = 0x30,
			NACK = 0x40
		};

		class USBConnection
		{
		public:
			USBConnection(ibrcommon::usbsocket &socket, dtn::core::Node &node);
			USBConnection(ibrcommon::usbsocket &socket, const dtn::data::EID &id);
			virtual ~USBConnection();

			bool match(const dtn::core::Node &node) const;
			bool match(const dtn::data::EID &destination) const;
			bool match(const dtn::core::NodeEvent &evt) const;

			ibrcommon::usbstream &getStream();
			const dtn::core::Node &getNode() const;
			void addNode(const dtn::core::Node &node);

		private:
			ibrcommon::usbsocket &_socket;
			dtn::core::Node _node;
		};

		class USBTransferService : public ibrcommon::JoinableThread
		{
		public:
			class Task
			{
			public:
				Task(USBConnection &con, const dtn::net::BundleTransfer &transfer, const uint8_t flags);

				USBConnection &connection();
				const dtn::net::BundleTransfer transfer() const;
				const uint8_t flags() const;
				const uint8_t sequence_number() const;

			private:
				const dtn::core::Node _recipient;
				USBConnection &_con;
				const dtn::net::BundleTransfer _transfer;
				const uint8_t _flags;
				uint8_t _this_sequence_number;
			};

			void queue(Task *t);
			void submit(Task *t);

			USBTransferService();

			virtual ~USBTransferService();

			void run() throw();
			void __cancellation() throw();
			void raiseEvent(const dtn::core::NodeEvent &event) throw();

		private:
			/**
			 * configuration for USB convergence layer
			 */
			const daemon::Configuration::USB &_config;

			/**
			 * Tasks to be processed
			 */
			ibrcommon::Queue<Task *> _tasks;

			/**
			 * Stores the bundle storage of the daemon
			 */
			dtn::storage::BundleStorage &_storage;

			/**
			 * false if service is to be shutdown
			 */
			bool _run;
		};
	}
}

#endif /* IBRDTN_DAEMON_SRC_NET_USBTRANSFERSERVICE_H_ */
