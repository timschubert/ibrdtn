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
	namespace usb
	{
		usbsocket::usbsocket(const usbinterface &iface, const char &endpoint_in, const char &endpoint_out)
		 : ep_in(endpoint_in), ep_out(endpoint_out), interface(iface)
		{
			_state = SOCKET_DOWN;
		}

		usbsocket::~usbsocket()
		{
		}

		void usbsocket::up() throw(socket_exception)
		{
			/* allocate a stream for each endpoint */
			uint8_t eps[] = {ep_in, ep_out};
			int err = libusb_alloc_streams(interface.device(), 1, eps, 2);
			if (err)
			{
				throw usb::USBError(static_cast<libusb_error>(err));
			}
			_state = SOCKET_UP;
		}

		void usbsocket::down() throw(socket_exception)
		{
			uint8_t eps[] = {ep_in, ep_out};
			int err = libusb_free_streams(interface.device(), eps, 2);
			if (err)
			{
				throw usb::USBError(static_cast<libusb_error>(err));
			}
			_state = SOCKET_DOWN;
		}

		void usbsocket::prepare_transfer(unsigned char *buf, int buflen, int flags, const ibrcommon::vaddress &addr, libusb_device_handle *handle,
		                                 unsigned char endpoint, uint32_t stream_id)
		{
			struct libusb_transfer *transfer = libusb_alloc_transfer(0);
			if (!transfer)
			{
				fprintf(stderr, "USB: failed to send\n");
			}
			libusb_fill_bulk_stream_transfer(transfer, handle, endpoint, stream_id, buf, buflen, transfer_completed_cb, NULL, 1000
			                                 // TODO use sensible timeout depending on bundle lifetime
			                                 );
			int err = libusb_submit_transfer(transfer);
			if (err)
			{
				throw USBError(static_cast<libusb_error>(err));
			}
		}

		ssize_t usbsocket::recvfrom(char *buf, size_t buflen, int flags, ibrcommon::vaddress &addr) throw(socket_exception)
		{
			if (_state != SOCKET_UP)
			{
				throw socket_error(ERROR_CLOSED, "socket not up");
			}

			// TODO check if buflen > MAXINT
			prepare_transfer((unsigned char *) buf, buflen, flags, addr, interface.device(), ep_in, 0);
			return buflen;
		}

		void usbsocket::sendto(const char *buf, size_t buflen, int flags, const ibrcommon::vaddress &addr) throw(socket_exception)
		{
			if (_state != SOCKET_UP)
			{
				throw socket_error(ERROR_CLOSED, "socket not up");
			}

			/* ATTENTION: usbsocket will attempt to free this buffer after the transfer completes */
			prepare_transfer((unsigned char *) buf, buflen, flags, addr, interface.device(), ep_out, 1);
		}

		void usbsocket::transfer_completed_cb(struct libusb_transfer *transfer)
		{
			// TODO handle re-submission etc
			// TODO error handling with exceptions
			if (transfer->user_data)
			{
				// TODO send event / handle custody
				/* so ugly ;_; */
				//(static_cast<usbsocket_cb *>(transfer->user_data))->event_data_in((char *) (transfer->buffer), transfer->actual_length); // use length?
			}
			else
			{ // probably outgoing transfer, anyway that buffer is not needed anymore
				free(transfer->buffer);
			}
			libusb_free_transfer(transfer);
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
}
