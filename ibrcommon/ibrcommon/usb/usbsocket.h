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
#include "ibrcommon/net/socket.h"
#include "ibrcommon/net/socketstream.h"
#include <libusb-1.0/libusb.h>

namespace ibrcommon
{
	namespace usb
	{

		class usbsocket : public datagramsocket
		{
		public:
			enum socket_status
			{
				socket_up,
				socket_down
			};

			// TODO replace with event
			class usbsocketcb
			{
			public:
				virtual ~usbsocketcb()
				{
				}
				virtual void event_data_out(char *buf, size_t buflen) = 0;
				virtual void event_data_in(char *buf, size_t buflen) = 0;
			};

			const uint8_t ep_in;
			const uint8_t ep_out;
			const usbinterface interface;

			usbsocket(const usbinterface &iface, const char &endpoint_in, const char &endpoint_out);
			virtual ~usbsocket();

			void up() throw(socket_exception);
			void down() throw(socket_exception);

			static void prepare_transfer(unsigned char *buf, int buflen, int flags, const ibrcommon::vaddress &addr, libusb_device_handle *handle,
			                             unsigned char endpoint, uint32_t stream_id);

			virtual ssize_t recvfrom(char *buf, size_t buflen, int flags, ibrcommon::vaddress &addr) throw(socket_exception);
			virtual void sendto(const char *buf, size_t buflen, int flags, const ibrcommon::vaddress &addr) throw(socket_exception);

		private:
			static void usb_send(struct libusb_device_handle *handle, uint8_t *message, int length, libusb_transfer_cb_fn cb);
			static void transfer_completed_cb(struct libusb_transfer *transfer);
		};
	}
}

#endif // USBSOCKET_H_
