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
#include <iostream>
#include <sstream>

namespace ibrcommon
{
	static const char *usb_error_string(int e)
	{
		return libusb_strerror((libusb_error) e);
	}

	class USBError: public ibrcommon::Exception
	{
	public:
		USBError(int e)
		 : Exception(usb_error_string(e))
		{
		}

		USBError(const char *msg)
				: Exception(msg)
		{
		}
	};

	class usb_device_error : public USBError
	{
	public:
		usb_device_error(int e)
		 : USBError(e) {}

		usb_device_error(const char *msg)
		 : USBError(msg) {}
	};

	class usbdevice
	{
	public:
		usbdevice();
		usbdevice(libusb_context *context, uint16_t vendor, uint16_t product);
		usbdevice(libusb_device *device);
		virtual ~usbdevice();

		void close();

		libusb_device_handle *get_handle() const;

		std::string getName() const;
		bool isNone() const;

		bool operator==(const usbdevice &rhs) const;
		bool operator!=(const usbdevice &rhs) const;

	private:
		libusb_device_descriptor _desc;
		libusb_device_handle *_handle;
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

		usbinterface();
		usbinterface(usbdevice &device, int interface);
		virtual ~usbinterface();

		const usbdevice& get_device() const;
		libusb_device_handle *get_handle() const;

		void set_up() const;
		void set_down() const;

	private:
		usbdevice _device;
		int interface_num;
	};
}

#endif // USBINTERFACE_H_
