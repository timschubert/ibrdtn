/*
 * usbsocket.h
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

#ifndef USBSOCKET_H_
#define USBSOCKET_H_

#include "usbinterface.h"
#include "ibrcommon/thread/Queue.h"
#include "ibrcommon/net/socket.h"
#include "ibrcommon/net/socketstream.h"
#include "ibrcommon/Logger.h"
#include "sys/types.h"
#include "sys/socket.h"
#include <libusb-1.0/libusb.h>
#include <algorithm>
#include <unistd.h>
#include <sys/select.h>

namespace ibrcommon
{
	class usb_socket_error : public socket_error
	{
	public:
		usb_socket_error(const char *msg)
		 : socket_error(ERROR_AGAIN, msg) {}

		usb_socket_error(socket_error_code error, std::string msg)
		 : socket_error(error, msg) {}
	};

	class usb_socket_no_device : public usb_socket_error
	{
	public:
		usb_socket_no_device(const char *msg)
		 : usb_socket_error(ERROR_CLOSED, msg) { }
	};

	class usb_socket_transfer_error : public usb_socket_error
	{
	public:
		usb_socket_transfer_error(const char *msg)
		 : usb_socket_error(msg) {}
	};

	class usb_socket_timed_out : public usb_socket_error
	{
	public:
		usb_socket_timed_out(const char *msg)
		 : usb_socket_error(msg) {}
	};

	class usb_socket_stall : public usb_socket_error
	{
	public:
		usb_socket_stall(const char *msg)
		 : usb_socket_error(msg) {}
	};

	class usb_socket_overflow : public usb_socket_error
	{
	public:
		usb_socket_overflow(const char *msg)
		 : usb_socket_error(msg) {}
	};

	class usbsocket: public datagramsocket
	{
	public:
		/**
		 * Creates a new usbsocket that is set down.
		 *
		 * @param iface interface to use for transfers
		 * @param enpoint_in endpoint to use for incoming transfers
		 * @param enpoint_out endpoint to use for outgoing transfers
		 * @throws socket_exception
		 * @throws usb_device_error
		 */
		usbsocket(const usbinterface &iface, const uint8_t &endpoint_in, const uint8_t &endpoint_out, size_t buflen);

		/**
		 * Destroys the socket and closes the socket pair.
		 */
		virtual ~usbsocket();

		void up() throw (socket_exception);
		void down() throw (socket_exception);

		virtual ssize_t recvfrom(char *buf, size_t buflen, int flags, ibrcommon::vaddress &addr) throw (socket_exception);
		virtual void sendto(const char *buf, size_t buflen, int flags, const ibrcommon::vaddress &addr) throw (socket_exception);

		const usbinterface &getInterface();

		/**
		 * Compares by interface.
		 */
		bool operator==(const usbsocket& rhs) const;

		/**
		 * Compares by interface.
		 */
		bool operator!=(const usbsocket& rhs) const;

	private:
		/**
		 * USB bulk endpoint for incoming transfers
		 */
		const uint8_t ep_in;

		/**
		 * USB bulk endpoint for outgoing transfers
		 */
		const uint8_t ep_out;

		/**
		 * USB interface to transmit transfers on, stores the device handle
		 */
		const usbinterface &interface;

		/**
		 * Debug tag
		 */
		static const std::string TAG;

		/**
		 * The size of the internal buffers used for transfers
		 */
		size_t _buffer_length;

		/**
		 * Callback for libusb to call when finishing an incomming transfer.
		 *
		 * Writes the data to the internal socket of the socket pair and prepares and submits the next transfer.
		 */
		static void transfer_in_cb(struct libusb_transfer *transfer);

		/**
		 * Callback for libusb to call when finishing an outgoing transfer.
		 *
		 * Reads the data from the internal socket of the socket pair and prepares and submits the next transfer.
		 */
		static void transfer_out_cb(struct libusb_transfer *transfer);

		/**
		 * Prepares and submits a new transfer.
		 */
		static bool submit_new_transfer(libusb_device_handle *dev_handle, int endpoint, uint8_t *buffer, int length, libusb_transfer_cb_fn cb, void *user_data, int extra_flags, size_t timeout);

		/**
		 * Socket that is the internal part of the socket pair. The other end of the "bi-pipe" can be used to select(2) for the socket.
		 * @see datagramsocket::fd()
		 */
		int _internal_fd;
	};
}

#endif // USBSOCKET_H_
