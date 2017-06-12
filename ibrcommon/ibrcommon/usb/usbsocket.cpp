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
	usbsocket::usbinput::usbinput(size_t len, int fd)
	{
		_fd = fd;
		_len = len;
		_buf = new char[len];
		_actual_len = 0;
	}

	usbsocket::usbinput::~usbinput()
	{
		delete _buf;
	}

	usbsocket::usbsocket(const usbinterface &iface, const char &endpoint_in, const char &endpoint_out, size_t buflen)
			: ep_in(endpoint_in), ep_out(endpoint_out), interface(iface), _input(buflen, 0)
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
		int fds[2];
		err = ::pipe(fds);
		if (err)
		{
			throw socket_exception("Failed to create pipe.");
		}
		_input._fd = fds[0];
		basesocket::_fd = fds[1];
		basesocket::_state = SOCKET_UP;

		/* prep the buffer with one pending transfer */
		if (!prepare_transfer((unsigned char *) _input._buf, _input._len, interface.device(), ep_in, 0, &_input))
		{
			throw socket_exception("Failed to prepare new transfer.");
		}
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
		::close(_input._fd);
		::close (_fd);
	}

	bool usbsocket::prepare_transfer(unsigned char *buf, int buflen, libusb_device_handle *handle, unsigned char endpoint, uint32_t stream_id, usbinput *input)
	{
		struct libusb_transfer *transfer = libusb_alloc_transfer(0);
		if (!transfer)
		{
			return false;
		}
		// TODO use sensible timeout depending on bundle lifetime
		libusb_fill_bulk_stream_transfer(transfer, handle, endpoint, stream_id, buf, buflen, transfer_completed_cb, (void *) &input, 1000);
		int err = libusb_submit_transfer(transfer);
		if (err)
		{
			throw USBError(static_cast<libusb_error>(err));
		}
		return true;
	}

	void usbsocket::transfer_completed_cb(struct libusb_transfer *transfer)
	{
		// TODO handle re-submission etc
		// TODO set socket down on error
		/* if input transfer */
		usbinput *input = static_cast<usbinput *>(transfer->user_data);
		if (input)
		{
			input->_actual_len = transfer->actual_length;
			/* signal that we have new data, and write new data */
			//::write(datagramsocket::_fd, transfer->buffer, transfer->actual_length);
			int err = ::write(input->_fd, "1", 1);
			if (!err)
			{
				IBRCOMMON_LOGGER_TAG("usbinput", warning) << "Failed to signal new data" << IBRCOMMON_LOGGER_ENDL;
			}

			/* prepare the buffer with one pending transfer */
			if (!prepare_transfer(transfer->buffer, transfer->length, transfer->dev_handle, transfer->endpoint, 0, input))
			{
				IBRCOMMON_LOGGER_TAG("libsub", critical) << "Failed to prepare next transfer." << IBRCOMMON_LOGGER_ENDL;
			}
		}
		else
		{
			/* out transfer completed */
			delete transfer->buffer;
		}
		libusb_free_transfer (transfer);
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

	ssize_t usbsocket::recvfrom(char *buf, size_t buflen, int flags, ibrcommon::vaddress &addr) throw (socket_exception)
	{
		/* block until data is ready */
		char pseudo[1];
		size_t err = read(datagramsocket::fd(), pseudo, 1);
		if (!err)
		{
			throw socket_exception("read failed");
		}
		::memcpy(buf, _input._buf, _input._actual_len);
		return _input._actual_len;
	}

	void usbsocket::sendto(const char *buf, size_t buflen, int flags, const ibrcommon::vaddress &addr) throw (socket_exception)
	{
		char *out_buf = new char[buflen];
		::memcpy(out_buf, buf, buflen);
		if (!prepare_transfer((unsigned char *) out_buf, buflen, interface.device(), ep_out, 0, NULL))
		{
			throw socket_exception("Failed to prepare new transfer.");
		}
	}


	bool usbsocket::operator==(const usbsocket &socket) const
	{
		return socket.interface == this->interface;
	}
}
