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

		USBTransferService::Task::Task(const dtn::core::Node &recipient, const dtn::net::BundleTransfer &transfer, const uint8_t flags)
		: _recipient(recipient), _transfer(transfer), _flags(flags)
		{
			static uint8_t sequence_number;
			sequence_number = (sequence_number + 1) % 4;
			_this_sequence_number = sequence_number;
		}

		const dtn::core::Node USBTransferService::Task::node() const
		{
			return _recipient;
		}

		const dtn::net::BundleTransfer USBTransferService::Task::transfer() const
		{
			return _transfer;
		}

		const uint8_t USBTransferService::Task::flags() const
		{
			return _flags;
		}

		const uint8_t USBTransferService::Task::sequence_number() const
		{
			return _this_sequence_number;
		}

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

		//USBTransferService &USBTransferService::getInstance(USBConvergenceLayer &clayer)
		//{
		//	static USBTransferService instance(clayer);
		//	return instance;
		//}

		void USBTransferService::queue(USBTransferService::Task *t)
		{
			_tasks.push(t);
		}

		void USBTransferService::submit(USBTransferService::Task *t)
		{
			try
			{
				std::list<USBConnection*> connections = _usb.getConnections(t->node());
				if (connections.empty())
				{
					dtn::net::TransferAbortedEvent::raise(t->node().getEID(), t->transfer().getBundle(),
							dtn::net::TransferAbortedEvent::REASON_CONNECTION_DOWN);
				}
				else
				{
					ibrcommon::usbstream& usb = connections.front()->getStream();

					/* prepend header */
					uint8_t header = USBConvergenceLayerType::DATA & USBConvergenceLayerMask::TYPE;

					/* Put the sequence number for this bundle into the outgoing header */
					header |= (t->sequence_number() << 2) & USBConvergenceLayerMask::SEQNO;
					header |= t->flags() & USBConvergenceLayerMask::FLAGS;
					usb << header;

					/* transmit Bundle */
					if (!_storage.contains(t->transfer().getBundle()))
					{
						IBRCOMMON_LOGGER_DEBUG(80) << "Bundle " << t->transfer().getBundle() << " not found" << IBRCOMMON_LOGGER_ENDL;
					}
					else
					{
						dtn::data::DefaultSerializer(usb) << _storage.get(t->transfer().getBundle());
					}
				}
			} catch (ibrcommon::socket_exception &e)
			{
				IBRCOMMON_LOGGER_DEBUG(80)
					<< e.what() << IBRCOMMON_LOGGER_ENDL;
				dtn::net::TransferAbortedEvent::raise(t->node().getEID(), t->transfer().getBundle(), dtn::net::TransferAbortedEvent::REASON_UNDEFINED);
			}
		}

		void USBTransferService::__cancellation() throw()
		{
		}
	}
}
