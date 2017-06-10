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

namespace ibrcommon
{
	class usbstream: public std::basic_streambuf<char,std::char_traits<char> >, public std::iostream, public usbsocket::transfer::transfer_cb
	{
	public:
		usbstream(usbsocket &sock);
		virtual ~usbstream();

		void transfer_completed(usbsocket::transfer *trans);

	protected:
		virtual int sync();
		virtual std::char_traits<char>::int_type overflow(std::char_traits<char>::int_type = std::char_traits<char>::eof());
		virtual std::char_traits<char>::int_type underflow();

	private:
		std::vector<char> _in_buf;
		size_t _in_buf_len;
		bool _in_buf_free;

		std::vector<char> out_buf_;
		size_t out_buf_len_;
		bool out_buf_free_;

		std::vector<char> out_;

		usbsocket &_sock;
		ibrcommon::Conditional in_buf_cond;

		bool _waiting_in;
	};
}

#endif // USBSTREAM_H_
