/*
 * usbsocket.cpp
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

#include "usbsocket.h"

namespace ibrcommon
{
	usbsocket::transfer::transfer(const char *buf, size_t buflength, ibrcommon::vaddress &addr, const transfer_cb callback, usbsocket &sock, bool input_mode)
	: _addr(addr), _transmitted(0), _aborted(false), _buflen(buflength), _cb(callback), _sock(sock), in(input_mode)
	{
		if (in)
		{
			_buf = new uint8_t[usbsocket::transfer::INBUFLEN];
		}
		else
		{
			_buf = new uint8_t[_buflen];
			::memcpy(_buf, buf, _buflen);
		}
	}

	usbsocket::transfer::~transfer()
	{
		delete _buf;
	}

	void usbsocket::transfer::abort()
	{
		_aborted = true;
	}

	bool usbsocket::transfer::aborted()
	{
		return _aborted;
	}

	uint8_t* usbsocket::transfer::buf()
	{
		return _buf;
	}

	size_t usbsocket::transfer::buflen()
	{
		return _buflen;
	}

	usbsocket::usbsocket(const usbinterface &iface, const char &endpoint_in, const char &endpoint_out)
			: ep_in(endpoint_in), ep_out(endpoint_out), interface(iface)
	{
		basesocket::_state = SOCKET_DOWN;
	}

	usbsocket::~usbsocket()
	{
	}

	void usbsocket::up() throw (socket_exception)
	{
		/* allocate a stream for each endpoint */
		uint8_t eps[] = {ep_in, ep_out};
		int err = libusb_alloc_streams(interface.device(), 1, eps, 2);
		if (err)
		{
			throw USBError(static_cast<libusb_error>(err));
		}
		basesocket::_state = SOCKET_UP;
	}

	void usbsocket::down() throw (socket_exception)
	{
		uint8_t eps[] = {ep_in, ep_out};
		int err = libusb_free_streams(interface.device(), eps, 2);
		if (err)
		{
			throw USBError(static_cast<libusb_error>(err));
		}
		basesocket::_state = SOCKET_DOWN;
	}

	void usbsocket::prepare_transfer(unsigned char *buf, int buflen, transfer *trans, libusb_device_handle *handle, unsigned char endpoint, uint32_t stream_id,
			bool in)
	{
		struct libusb_transfer *transfer = libusb_alloc_transfer(0);
		if (!transfer)
		{
			fprintf(stderr, "USB: failed to send\n");
		}
		libusb_fill_bulk_stream_transfer(transfer, handle, endpoint, stream_id, buf, buflen, transfer_completed_cb, (void *) trans, 1000
		// TODO use sensible timeout depending on bundle lifetime
				);
		int err = libusb_submit_transfer(transfer);
		if (err)
		{
			throw USBError(static_cast<libusb_error>(err));
		}
	}

	void usbsocket::recvfrom(transfer *trans) throw (socket_exception)
	{
		if (basesocket::_state != SOCKET_UP)
		{
			throw socket_error(ERROR_CLOSED, "socket not up");
		}

		// TODO check if buflen > MAXINT
		prepare_transfer((unsigned char *) trans->buf(), trans->buflen(), trans, interface.device(), ep_in, 0, true);
	}

	void usbsocket::sendto(transfer *trans) throw (socket_exception)
	{
		if (basesocket::_state != SOCKET_UP)
		{
			throw socket_error(ERROR_CLOSED, "socket not up");
		}

		prepare_transfer((unsigned char *) trans->buf(), trans->buflen(), trans, interface.device(), ep_out, 1, false);
	}

	usbsocket& usbsocket::transfer::sock()
	{
		return _sock;
	}

	void usbsocket::transfer_completed_cb(struct libusb_transfer *usb_transfer)
	{
		// TODO handle re-submission etc
		// TODO set socket down on error
		if (usb_transfer->user_data)
		{
			ibrcommon::usbsocket::transfer *trans = static_cast<transfer*>(usb_transfer->user_data);
			trans->_transmitted = usb_transfer->actual_length;
			trans->done.signal();
			if (!trans->aborted())
			{
				trans->cb().transfer_completed(trans);
			}
		}
		libusb_free_transfer(usb_transfer);
	}

	bool match_device(libusb_device *device, const uint16_t &vendor, const uint16_t &product)
	{
		struct libusb_device_descriptor *descriptor = (struct libusb_device_descriptor *) malloc(sizeof(struct libusb_device_descriptor));
		int err = libusb_get_device_descriptor(device, descriptor);
		if (err)
		{
			throw USBError(static_cast<libusb_error>(err));
		}
		else
		{
			if (descriptor->idVendor == vendor && descriptor->idProduct == product)
			{
				return true;
			}
		}
		return false;
	}
}
