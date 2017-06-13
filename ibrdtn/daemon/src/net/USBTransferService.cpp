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

#include "USBTransferService.h"

namespace dtn
{
	namespace net
	{
		USBTransferService::USBTransferService(USBConvergenceLayer &usbLayer)
				: _config(daemon::Configuration::getInstance().getUSB()), _usb(usbLayer), _storage(dtn::core::BundleCore::getInstance().getStorage())
		{
			_run = true;
			this->start();
		}

		USBTransferService::~USBTransferService()
		{
			this->stop();
			this->join();

			_tasks.reset();
			while (!_tasks.empty())
			{
				Task *t = _tasks.front();
				_tasks.pop();
				delete t;
				t = NULL;
			}
			_tasks.abort();
		}

		void USBTransferService::run() throw ()
		{
			while (_run)
			{
				try
				{
					USBTransferService::Task *t = _tasks.poll();
					/* check storage for this bundle */
					if (!_storage.contains(t->transfer().getBundle()))
					{
						/* cancel task */
						delete t;
						t = NULL;
						continue;
					}

					submit(t);
					delete t;
					t = NULL;
				} catch (ibrcommon::QueueUnblockedException&)
				{
					IBRCOMMON_LOGGER_DEBUG(42)
						<< "Queue was unblocked." << IBRCOMMON_LOGGER_ENDL;
				}
			}
		}

		USBTransferService &USBTransferService::getInstance() const
		{
			static USBTransferService instance;
			return instance;
		}

		USBTransferService &USBTransferService::getInstance() const
		{
			static USBTransferService instance;
			return instance;
		}

		void USBTransferService::queue(USBTransferService::Task *t)
		{
			_tasks.push(t);
		}

		void USBTransferService::submit(USBTransferService::Task *t)
		{
			try
			{
				std::list<USBConnection> connections = _usb.getConnection(t->node());
				if (connections.empty())
				{
					dtn::net::TransferAbortedEvent::raise(t->node().getEID(), t->transfer().getBundle(),
							dtn::net::TransferAbortedEvent::REASON_CONNECTION_DOWN);
				}
				else
				{
					ibrcommon::usbstream& usb = connections.begin()->getStream();

					/* prepend "header" */
					usb << "0";

					/* transmit Bundle */
					dtn::data::DefaultSerializer(usb) << t->transfer().getBundle();
				}
			} catch (ibrcommon::socket_exception &e)
			{
				IBRCOMMON_LOGGER_DEBUG(80)
					<< e.what() << IBRCOMMON_LOGGER_ENDL;
				dtn::net::TransferAbortedEvent::raise(t->node().getEID(), t->transfer().getBundle(), dtn::net::TransferAbortedEvent::REASON_UNDEFINED);
			}
		}
	}
}
