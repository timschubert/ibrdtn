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
	const std::string usbsocket::TAG = "usbsocket";

	usbsocket::usbsocket(const usbinterface &iface, const uint8_t &endpoint_in, const uint8_t &endpoint_out, size_t buflen)
		: clientsocket(), ep_in(endpoint_in), ep_out(endpoint_out), interface(iface), _buffer_length(buflen), _internal_fd(-1)
	{
	}

	usbsocket::~usbsocket()
	{
	}

	const usbinterface &usbsocket::getInterface()
	{
		return interface;
	}

	void usbsocket::up() throw(socket_exception)
	{
		/* create unnamed unix domain socket pair */
		int pair[2];
		int err = ::socketpair(AF_UNIX, SOCK_DGRAM, 0, pair);
		if (err)
		{
			throw usb_socket_error(::strerror(errno));
		}

		_fd = pair[0];
		set(BLOCKING, false);
		_internal_fd = pair[1];

		/* prepare the buffer with one pending transfer */
		uint8_t *transfer_buffer = new uint8_t[_buffer_length];

		IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 80) << "Preparing initial in-bound transfer" << IBRCOMMON_LOGGER_ENDL;
		int *fd = new int;
		*fd = _internal_fd;
		submit_new_transfer(this->interface.get_handle(), this->ep_in, transfer_buffer, _buffer_length, transfer_in_cb, (void *) fd, 0, 0);

		clientsocket::up();
	}

	void usbsocket::down() throw(socket_exception)
	{
		clientsocket::down();

		::close(_fd);
		::close(_internal_fd);
		_fd = -1;
		_internal_fd = -1;
	}

	void usbsocket::transfer_in_cb(struct libusb_transfer *transfer)
	{
		// TODO handle failure
		switch (transfer->status)
		{
			case LIBUSB_TRANSFER_ERROR:
				IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 80) << "In-bound transfer encountered error" << IBRCOMMON_LOGGER_ENDL;
				break;

			case LIBUSB_TRANSFER_TIMED_OUT:
				IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 90) << "In-bound transfer timed out" << IBRCOMMON_LOGGER_ENDL;
				break;

			case LIBUSB_TRANSFER_CANCELLED:
				IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 80) << "In-bound transfer was cancelled" << IBRCOMMON_LOGGER_ENDL;
				break;

			case LIBUSB_TRANSFER_STALL:
				IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 80) << "In-bound transfer completed with stalled endpoint" << IBRCOMMON_LOGGER_ENDL;
				break;

			case LIBUSB_TRANSFER_NO_DEVICE:
				IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 70) << "In-bound transfer completed with device lost" << IBRCOMMON_LOGGER_ENDL;
				break;

			case LIBUSB_TRANSFER_OVERFLOW:
				IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 80) << "In-bound transfer completed with overflow" << IBRCOMMON_LOGGER_ENDL;
				break;

			case LIBUSB_TRANSFER_COMPLETED:
				IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 90) << "In-bound transfer completed successfully" << IBRCOMMON_LOGGER_ENDL;
				break;

			default:
				break;
		}

		int *fd = static_cast<int *>(transfer->user_data);
		if (transfer->status == LIBUSB_TRANSFER_COMPLETED)
		{
			ssize_t err = ::send(*fd, transfer->buffer, transfer->actual_length, 0);
			if (err < 0)
			{
				IBRCOMMON_LOGGER_TAG(TAG, warning) << "Failed to transfer new data" << IBRCOMMON_LOGGER_ENDL;
				return;
			}
		}

		/* this is very ugly but at least we don't leak memory */
		int *new_fd = new int;
		*new_fd = *fd;

		try
		{
			submit_new_transfer(transfer->dev_handle, transfer->endpoint, transfer->buffer, transfer->length, transfer_in_cb, new_fd, 0, transfer->timeout);
		}
		catch (ibrcommon::Exception &e)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 80) << "IN: " << e.what() << IBRCOMMON_LOGGER_ENDL;
		}

		delete fd;
	}

	void usbsocket::transfer_out_cb(struct libusb_transfer *transfer)
	{
		switch (transfer->status)
		{
			case LIBUSB_TRANSFER_ERROR:
			case LIBUSB_TRANSFER_TIMED_OUT:
			case LIBUSB_TRANSFER_CANCELLED:
			case LIBUSB_TRANSFER_STALL:
			case LIBUSB_TRANSFER_NO_DEVICE:
			case LIBUSB_TRANSFER_OVERFLOW:
				IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 80) << "Out-bound transfer completed with error" << IBRCOMMON_LOGGER_ENDL;
				break;

			case LIBUSB_TRANSFER_COMPLETED:
				IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 80) << "Out-bound transfer completed successfully" << IBRCOMMON_LOGGER_ENDL;
				break;

			default:
				break;
		}
	}

	bool usbsocket::submit_new_transfer(libusb_device_handle *dev_handle, int endpoint, uint8_t *buffer, int length, libusb_transfer_cb_fn cb, void *user_data,
	                                    int extra_flags, size_t timeout)
	{
		libusb_transfer *next_transfer = libusb_alloc_transfer(0);
		if (next_transfer == NULL)
		{
			throw usb_socket_error("Failed to allocate transfer");
		}

		libusb_fill_bulk_transfer(next_transfer, dev_handle, endpoint, buffer, length, cb, user_data, timeout);
		// next_transfer->flags |= LIBUSB_TRANSFER_FREE_BUFFER;
		next_transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;

		int err = libusb_submit_transfer(next_transfer);
		if (err < 0)
		{
			throw usb_socket_no_device(usb_error_string(err));
		}

		return true;
	}

	bool match_device(libusb_device *device, const uint16_t &vendor, const uint16_t &product)
	{
		struct libusb_device_descriptor *descriptor = (struct libusb_device_descriptor *) malloc(sizeof(struct libusb_device_descriptor));
		int err = libusb_get_device_descriptor(device, descriptor);
		if (err)
		{
			throw usb_device_error(usb_error_string(err));
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

	ssize_t usbsocket::recv(char *buf, size_t buflen, int flags) throw(socket_exception)
	{
		if (this->_state != SOCKET_UP)
		{
			throw usb_socket_error("socket not ready");
		}
		/* data is ready */
		ssize_t length = ::recv(this->fd(), buf, buflen, flags);

		if (length < 0)
		{
			throw usb_socket_error(::strerror(errno));
		}

		return length;
	}

	ssize_t usbsocket::send(const char *buf, size_t buflen, int flags) throw(socket_exception)
	{
		/* check if the socket can handle request */
		if (this->_state != SOCKET_UP)
		{
			throw usb_socket_error("socket not ready");
		}

		if (buflen > _buffer_length)
		{
			buflen = _buffer_length;
		}

		/* libusb will free this buffer, see LIBUSB_TRANSFER_FREE_BUFFER */
		uint8_t *output = new uint8_t[_buffer_length];
		::memcpy(output, buf, buflen);

		IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 90) << "Submitting new out-bound transfer." << IBRCOMMON_LOGGER_ENDL;

		/* throw all errors from here */
		bool res = submit_new_transfer(this->interface.get_handle(), this->ep_out, output, buflen, transfer_out_cb, (void *) &(this->_internal_fd),
		                    (LIBUSB_TRANSFER_ADD_ZERO_PACKET | LIBUSB_TRANSFER_SHORT_NOT_OK | LIBUSB_TRANSFER_FREE_BUFFER), 1000);
		if (!res)
		{
			throw usb_socket_error("failed to submit transfer");
		}

		return buflen;
	}

	bool usbsocket::operator==(const usbsocket &rhs) const
	{
		return this->interface == rhs.interface;
	}

	bool usbsocket::operator!=(const usbsocket &rhs) const
	{
		return !(*this == rhs);
	}

	size_t usbsocket::getMTU() const
	{
		return _buffer_length;
	}
}
