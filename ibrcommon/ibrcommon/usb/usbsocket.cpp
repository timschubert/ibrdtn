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
	usbsocket::usbsocket(const usbinterface &iface, const uint8_t &endpoint_in, const uint8_t &endpoint_out, size_t queue_size)
			: ep_in(endpoint_in), ep_out(endpoint_out), interface(iface), _max_queue_size(queue_size)
	{
		basesocket::_state = SOCKET_DOWN;
		int fds[2];
		int err = ::pipe(fds);
		if (err)
		{
			throw socket_exception("Failed to create pipe.");
		}

		basesocket::_fd = fds[0];
		_pipe_trick = fds[1];
	}

	usbsocket::~usbsocket()
	{
		::close(_pipe_trick);
	}

	void usbsocket::up() throw (socket_exception)
	{
		if (basesocket::_state != SOCKET_UP)
		{
			basesocket::_state = SOCKET_UP;

			/* prepare the buffer with one pending transfer */
			uint8_t *transfer_buffer = new uint8_t[1000];
			if (!prepare_transfer(transfer_buffer, 1000, interface.device(), ep_in,
					(LIBUSB_TRANSFER_SHORT_NOT_OK),
					transfer_in_cb))
			{
				throw socket_exception("Failed to prepare initial transfer.");
			}
		}
	}

	void usbsocket::down() throw (socket_exception)
	{
		if (basesocket::_state != SOCKET_DOWN)
		{
			basesocket::_state = SOCKET_DOWN;
		}
	}

	bool usbsocket::prepare_transfer(uint8_t *buf, int buflen, libusb_device_handle *handle, const uint8_t endpoint, int flags, libusb_transfer_cb_fn cb)
	{
		struct libusb_transfer *transfer = libusb_alloc_transfer(0);
		if (!transfer)
		{
			return false;
		}
		// TODO use sensible timeout depending on bundle lifetime
		libusb_fill_bulk_transfer(transfer, handle, endpoint, buf, buflen, cb, (void *) this, 1000);
		transfer->flags |= flags;
		int err = libusb_submit_transfer(transfer);
		if (err < 0)
		{
			std::stringstream ss;
			ss << usb_error_string(err);
			return false;
		}
		return true;
	}

	void usbsocket::transfer_in_cb(struct libusb_transfer *transfer)
	{
		// TODO handle re-submission etc
		// TODO set socket down on error

		/* signal that we have new data, and write new data */
		usbsocket *sock = static_cast<usbsocket *>(transfer->user_data);
		sock->_input.push(transfer);

		int err = ::write(sock->datagramsocket::_fd, "1", 1);
		if (err < 1)
		{
			IBRCOMMON_LOGGER_TAG("usbtransfer", warning) << "Failed to signal new data" << IBRCOMMON_LOGGER_ENDL;
		}

	}

	void usbsocket::transfer_out_cb(struct libusb_transfer *transfer)
	{
		// TODO handle re-submission etc
		// TODO set socket down on error

		if (1)
		{
			libusb_free_transfer(transfer);
		}
	}

	bool match_device(libusb_device *device, const uint16_t &vendor, const uint16_t &product)
	{
		struct libusb_device_descriptor *descriptor = (struct libusb_device_descriptor *) malloc(sizeof(struct libusb_device_descriptor));
		int err = libusb_get_device_descriptor(device, descriptor);
		if (err)
		{
			std::stringstream ss;
			ss << usb_error_string(err);
			throw socket_error(ERROR_READ, ss.str());
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
		if (err < 1)
		{
			throw socket_exception("read failed");
		}

		_input.poll();
		libusb_transfer *next_transfer = _input.take();
		if (buflen < next_transfer->actual_length)
		{
			throw socket_exception("Buffer was too small");
		}

		::memcpy((void *) buf, (void *) next_transfer->buffer, next_transfer->actual_length);

		/* does not free the buffer, so we can reuse it */
		libusb_free_transfer(next_transfer);

		if (!prepare_transfer(next_transfer->buffer,
				next_transfer->length,
				interface.device(),
				ep_in,
				LIBUSB_TRANSFER_SHORT_NOT_OK,
				(libusb_transfer_cb_fn) transfer_out_cb))
		{
			throw socket_exception("Failed to prepare new transfer while receiving.");
		}

		return next_transfer->actual_length;
	}

	void usbsocket::sendto(const char *buf, size_t buflen, int flags, const ibrcommon::vaddress &addr) throw (socket_exception)
	{
		uint8_t *transfer_buffer = new uint8_t[buflen];
		::memcpy(transfer_buffer, buf, buflen);

		if (!prepare_transfer(transfer_buffer,
				buflen,
				interface.device(),
				ep_out,
				LIBUSB_TRANSFER_SHORT_NOT_OK | LIBUSB_TRANSFER_FREE_BUFFER,
				(libusb_transfer_cb_fn) transfer_out_cb))
		{
			throw socket_exception("Failed to prepare new transfer while sending.");
		}
	}

	bool usbsocket::operator==(const usbsocket& rhs) const
	{
		return this->interface == rhs.interface;
	}

	bool usbsocket::operator!=(const usbsocket& rhs) const
	{
		return !(*this== rhs);
	}
}
