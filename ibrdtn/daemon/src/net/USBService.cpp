/*
 * USBService.cpp
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

#include "USBService.h"

namespace dtn
{
	namespace net
	{
		USBService::USBService(ibrcommon::usbconnector &con)
			: _con(con), _run(true)
		{
			this->start();
		}

		USBService::~USBService()
		{
			this->stop();
			this->join();
		}

		void USBService::run(void) throw ()
		{
			while (_run)
			{
				_con.usb_loop();
				yield();
			}
		}

		void USBService::__cancellation() throw ()
		{
			_run = false;
		}
	}
}
