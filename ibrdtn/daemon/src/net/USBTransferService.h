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

#include "Logger.h"
#include "ibrcommon/usb/usbstream.h">
#include <sstream>
#include <map>

namespace dtn
{
	namespace net
	{
		class USBTransferService : public ibrcommon::JoinableThread
		{
		public:
			class Task
			{
			public:
				Task(const dtn::core::Node &recipient, const dtn::net::BundleTransfer &transfer);

				const dtn::core::Node node() const;
				const dtn::net::BundleTransfer transfer() const;

			private:
				const dtn::core::Node _recipient;
				const dtn::net::BundleTransfer _transfer;
			};

			void queue(Task *t);
			void submit(Task *t);

			static USBTransferService &getInstance() const;
			USBTransferService(USBTransferService const &) = delete;
			void operator=(USBTransferService const &) = delete;

			virtual ~USBTransferService();

			void run() throw();
			void __cancellation() throw();
			void raiseEvent(const dtn::core::NodeEvent &event) throw();

		private:
			/**
			 * configuration for USB convergence layer
			 */
			daemon::Configuration::USB &_config;

			/**
			 * USB convergence layer
			 */
			dtn::net::USBConvergenceLayer &_usb;
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

			/**
			 * Constructor
			 */
			USBTransferService(USBConvergenceLayer &usbLayer);
		};
	}
}

#endif /* IBRDTN_DAEMON_SRC_NET_USBTRANSFERSERVICE_H_ */
