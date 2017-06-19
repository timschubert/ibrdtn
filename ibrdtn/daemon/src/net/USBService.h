/*
 * USBService.h
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

#ifndef IBRDTN_DAEMON_SRC_NET_USBSERVICE_H_
#define IBRDTN_DAEMON_SRC_NET_USBSERVICE_H_

#include "ibrcommon/thread/Thread.h"
#include "ibrcommon/usb/usbconnector.h"

namespace dtn
{
	namespace net
	{
		class USBService : public ibrcommon::JoinableThread
		{
		public:
			USBService(ibrcommon::usbconnector &con);
			~USBService();

			/**
			 * starts the run loop of usbconnector
			 */
			virtual void run(void) throw();

			/**
			 * cancels the run loop of usbconnector
			 */
			virtual void __cancellation() throw();

		private:
			ibrcommon::usbconnector &_con;
			bool _run;
		};
	}
}

#endif // IBRDTN_DAEMON_SRC_NET_USBSERVICE_H_
