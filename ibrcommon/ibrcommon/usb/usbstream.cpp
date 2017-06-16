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

#include "usbstream.h"

namespace ibrcommon
{
	usbstream::usbstream(usbsocket &sock)
			: std::iostream(this),
			  _sock(sock),
			  _in_buf_free(true),
			  out_buf_free_(true),
			  _in_buf_len(0),
			  out_buf_len_(0)
	{
	}

	usbstream::~usbstream()
	{
	}

	int usbstream::sync()
	{
		int ret = std::char_traits<char>::eq_int_type(this->overflow(std::char_traits<char>::eof()), std::char_traits<char>::eof()) ? -1 : 0;
		return ret;
	}

	std::char_traits<char>::int_type usbstream::overflow(std::char_traits<char>::int_type c)
	{
		char *ibegin = &out_buf_[0];
		char *iend = pptr();

		/* mark the buffer as free */
		setp(&out_buf_[0], &out_buf_[0] + out_buf_len_ - 1);

		if (!std::char_traits<char>::eq_int_type(c, std::char_traits<char>::eof()))
		{
			*iend++ = std::char_traits<char>::to_char_type(c);
		}

		/* if there is nothing to send, just return */
		if ((iend - ibegin) == 0)
		{
			return std::char_traits<char>::not_eof(c);
		}

		try
		{
			vaddress empty;
			_sock.sendto(&(out_buf_)[0], iend - ibegin, 0, empty);

			/* update the position of the free buffer */
			char *buffer_begin = iend;
			setp(buffer_begin, &out_buf_[0] + out_buf_len_ - 1);
		} catch (USBError &e)
		{
			// TODO log error with logger
			std::cerr << e.what() << std::endl;

			// TODO abort if final failure condition
			/* try again */
			overflow(c);
		}

		return std::char_traits<char>::not_eof(c);
	}

	std::char_traits<char>::int_type usbstream::underflow()
	{
		size_t received = 0;
		vaddress empty;
		try
		{
			received = _sock.recvfrom(&_in_buf[0], _in_buf_len, 0, empty);
		} catch (USBError &e)
		{
			// TODO logging
			std::cerr << e.what() << std::endl;
		}
		setg(&_in_buf[0], &_in_buf[0], &_in_buf[received]);
		return std::char_traits<char>::not_eof(_in_buf[0]);
	}
}
