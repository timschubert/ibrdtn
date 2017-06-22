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
#include <libusb-1.0/libusb.h>
#include <algorithm>
#include <unistd.h>

namespace ibrcommon
{
	class usbsocket: public datagramsocket
	{
	public:
		const uint8_t ep_in;
		const uint8_t ep_out;
		const usbinterface interface;

		usbsocket(const usbinterface &iface, const uint8_t &endpoint_in, const uint8_t &endpoint_out, size_t queue_size);
		virtual ~usbsocket();

		void up() throw (socket_exception);
		void down() throw (socket_exception);

		bool prepare_transfer(uint8_t *buf, int buflen, libusb_device_handle *handle, const uint8_t endpoint, int flags, libusb_transfer_cb_fn cb);

		virtual ssize_t recvfrom(char *buf, size_t buflen, int flags, ibrcommon::vaddress &addr) throw (socket_exception);
		virtual void sendto(const char *buf, size_t buflen, int flags, const ibrcommon::vaddress &addr) throw (socket_exception);

		bool operator==(const usbsocket& rhs) const;
		bool operator!=(const usbsocket& rhs) const;

	private:
		static const std::string TAG;

		Queue<libusb_transfer *> _input;
		size_t _max_queue_size;
		int _pipe_trick;

		static void usb_send(struct libusb_device_handle *handle, uint8_t *message, int length, libusb_transfer_cb_fn cb);
		static void transfer_in_cb(struct libusb_transfer *transfer);
		static void transfer_out_cb(struct libusb_transfer *transfer);
	};
}

#endif // USBSOCKET_H_
