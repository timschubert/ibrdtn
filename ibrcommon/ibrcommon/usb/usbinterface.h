/*
 * usbinterface.h
 *
 * Copyright (C) 2011 IBR, TU Braunschweig
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
#ifndef USBINTERFACE_H_
#define USBINTERFACE_H_

#include "ibrcommon/net/vinterface.h"
#include "ibrcommon/net/vaddress.h"
#include <libusb-1.0/libusb.h>

#ifndef usb_error_string
#define usb_error_string(e) libusb_strerror((enum libusb_error) e)
#endif

namespace ibrcommon
{
	class USBError: public ibrcommon::Exception
	{
	public:
		USBError(libusb_error e)
		{
			Exception(usb_error_string(e));
		}

		USBError(const char *msg)
				: Exception(msg)
		{
		}
	};

	class usbinterface: public vinterface
	{
	public:
		class usb_link_cb
		{
		public:
			virtual ~usb_link_cb()
			{
			}

			virtual void event_link_up(usbinterface &iface) = 0;
			virtual void event_link_down(usbinterface &iface) = 0;
		};

		usbinterface(const std::string &name, libusb_device_handle *device, const uint8_t &bus, const uint8_t &address, const uint8_t &interface);
		virtual ~usbinterface();

		void set_up();
		void set_down();

		libusb_device_handle *device() const;
		const uint8_t& get_bus() const ;
		const uint8_t& get_address() const;
		const uint8_t& get_interface_num() const;

		bool operator==(const usbinterface& rhs) const;
		bool operator!=(const usbinterface& rhs) const;

	private:
		libusb_device_handle *_device;
		uint8_t bus;
		uint8_t address;
		uint8_t interface_num;
	};
}

#endif // USBINTERFACE_H_
