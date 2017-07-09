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
	usbstream::usbstream(usbsocket *sock) : std::iostream(this), _buflen(sock->getMTU()), _in_buf(_buflen), out_buf_(_buflen)
	{
		/* set put area to free */
		setp(&out_buf_[0], &out_buf_[0] + _buflen - 1);

		/* set get area to empty */
		setg(0, 0, 0);

		_sock.add(sock);
		_sock.up();
	}

	usbstream::~usbstream()
	{
		_sock.destroy();
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
		setp(&out_buf_[0], &out_buf_[0] + _buflen - 1);

		if (!std::char_traits<char>::eq_int_type(c, std::char_traits<char>::eof()))
		{
			*iend++ = std::char_traits<char>::to_char_type(c);
		}

		/* if there is nothing to send, just return */
		if ((iend - ibegin) == 0)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("usbstream", 80) << "overflow() nothing to sent" << IBRCOMMON_LOGGER_ENDL;
			return std::char_traits<char>::not_eof(c);
		}

		try
		{
			socketset writeset;
			_sock.select(NULL, &writeset, NULL, NULL);

			if (writeset.size() == 0)
			{
				throw socket_exception("no select results returned");
			}

			ssize_t bytes = (iend - ibegin);

			usbsocket *sock = static_cast<usbsocket *>(*(writeset.begin()));
			ssize_t send = sock->send(&out_buf_[0], bytes, 0);

			if (send < bytes)
			{
				// we did not sent all bytes
				char *resched_begin = ibegin + send;
				char *resched_end = iend;

				// bytes left to send
				size_t bytes_left = resched_end - resched_begin;

				// move the data to the begin of the buffer
				::memcpy(ibegin, resched_begin, bytes_left);

				// new free buffer
				char *buffer_begin = ibegin + bytes_left;

				// mark the buffer as free
				setp(buffer_begin, &out_buf_[0] + _buflen - 1);
			}
		}
		catch (vsocket_timeout &e)
		{
			/* interrupted by timeout is temporary failure */
			IBRCOMMON_LOGGER_DEBUG_TAG("usbstream", 80) << e.what() << IBRCOMMON_LOGGER_ENDL;
			this->setstate(failbit);
			return overflow(c);
		}
		catch (vsocket_interrupt &e)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("usbstream", 85) << "select interrupted: " << e.what() << IBRCOMMON_LOGGER_ENDL;
			this->setstate(badbit);
			close();
			throw;
		}
		catch (usb_socket_no_device &e)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("usbstream", 80) << e.what() << IBRCOMMON_LOGGER_ENDL;
			this->setstate(badbit);
			close();
			throw;
		}
		catch (usb_device_error &e)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("usbstream", 80) << e.what() << IBRCOMMON_LOGGER_ENDL;
			this->setstate(badbit);
			close();
			throw;
		}
		catch (socket_error &e)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("usbstream", 80) << e.what() << IBRCOMMON_LOGGER_ENDL;

			/* temporary failure */
			if (e.code() == ERROR_AGAIN)
			{
				this->setstate(failbit);
				return overflow(c);
			}

			/* permanent failure */
			this->setstate(badbit);
			close();
		}
		catch (socket_exception &e)
		{
			/* other error */
			this->setstate(badbit);
			close();
			throw;
		}

		return std::char_traits<char>::not_eof(c);
	}

	void usbstream::close()
	{
		_sock.destroy();
	}

	std::char_traits<char>::int_type usbstream::underflow()
	{
		try
		{
			socketset readset;
			_sock.select(&readset, NULL, NULL, NULL);

			if (readset.size() == 0)
			{
				throw socket_exception("no select result returned");
			}

			usbsocket *sock = static_cast<usbsocket *>(*(readset.begin()));
			ssize_t bytes = sock->recv(&_in_buf[0], _buflen, 0);

			if (bytes == 0)
			{
				IBRCOMMON_LOGGER_DEBUG_TAG("usbstream", 85) << "receive returned zero: " << errno << IBRCOMMON_LOGGER_ENDL;
				this->setstate(badbit);
				close();
				return std::char_traits<char>::eof();
			}

			setg(&_in_buf[0], &_in_buf[0], &_in_buf[0] + bytes);

			return std::char_traits<char>::not_eof(_in_buf[0]);
		}
		catch (vsocket_timeout &e)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("usbstream", 80) << e.what() << IBRCOMMON_LOGGER_ENDL;
			this->setstate(failbit);
		}
		catch (vsocket_interrupt &e)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("usbstream", 85) << "select interrupted: " << e.what() << IBRCOMMON_LOGGER_ENDL;
			this->setstate(badbit);
			close();
		}
		catch (usb_socket_no_device &e)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("usbstream", 80) << e.what() << IBRCOMMON_LOGGER_ENDL;
			this->setstate(badbit);
			close();
		}
		catch (usb_device_error &e)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("usbstream", 80) << e.what() << IBRCOMMON_LOGGER_ENDL;
			this->setstate(badbit);
			close();
		}
		catch (socket_error &e)
		{
			/* set error code from socket */
			IBRCOMMON_LOGGER_DEBUG_TAG("usbstream", 80) << e.what() << IBRCOMMON_LOGGER_ENDL;
			if (e.code() == ERROR_AGAIN)
			{
				this->setstate(failbit);
				return std::char_traits<char>::not_eof(_in_buf[0]);
			}

			this->setstate(badbit);
			close();
		}
		catch (socket_exception &e)
		{
			this->setstate(badbit);
			IBRCOMMON_LOGGER_DEBUG_TAG("usbstream", 80) << e.what() << IBRCOMMON_LOGGER_ENDL;
			close();
		}

		return std::char_traits<char>::eof();
	}
}
