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
	class usbsocket: public basesocket
	{
	public:

		class transfer
		{
		public:
			// TODO to config
			static const size_t INBUFLEN = 10000;

			class transfer_cb
			{
			public:
				void transfer_completed(transfer *trans);
			};

			transfer(const char *buf, size_t _buflen, ibrcommon::vaddress &addr, const transfer_cb callback, usbsocket &sock, bool in);
			~transfer();
			ibrcommon::Conditional done;
			const ibrcommon::vaddress &_addr;
			const bool in;
			int _transmitted;

			void abort();
			bool aborted();
			uint8_t* buf();
			size_t buflen();
			transfer_cb& cb();
			usbsocket& sock();

		private:
			bool _aborted;
			transfer_cb _cb;
			uint8_t *_buf;
			size_t _buflen;
			usbsocket &_sock;
		};

		const uint8_t ep_in;
		const uint8_t ep_out;
		const usbinterface interface;

		usbsocket(const usbinterface &iface, const char &endpoint_in, const char &endpoint_out);
		virtual ~usbsocket();

		void up() throw (socket_exception);
		void down() throw (socket_exception);

		static void prepare_transfer(unsigned char *buf, int buflen, transfer *initiator, libusb_device_handle *handle, unsigned char endpoint,
				uint32_t stream_id, bool in);

		virtual void recvfrom(transfer *trans) throw (socket_exception);
		virtual void sendto(transfer *trans) throw (socket_exception);

	private:
		static void usb_send(struct libusb_device_handle *handle, uint8_t *message, int length, libusb_transfer_cb_fn cb);
		static void transfer_completed_cb(struct libusb_transfer *transfer);
	};
}

#endif // USBSOCKET_H_
