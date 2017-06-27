/*
 * usbstream.h
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

#ifndef USBSTREAM_H_
#define USBSTREAM_H_

#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include "usbsocket.h"
#include "ibrcommon/Logger.h"

namespace ibrcommon
{
	class usbstream : public std::basic_streambuf<char, std::char_traits<char> >, public std::iostream
	{
	public:
		usbstream(usbsocket *sock, const size_t buflen);
		virtual ~usbstream();

	protected:
		vsocket _sock;
		socket_error_code error;
		size_t _buflen;

		virtual void close();

		virtual int sync();
		virtual std::char_traits<char>::int_type overflow(std::char_traits<char>::int_type = std::char_traits<char>::eof());
		virtual std::char_traits<char>::int_type underflow();

	private:

		std::vector<char> _in_buf;
		std::vector<char> out_buf_;
	};
}

#endif // USBSTREAM_H_
