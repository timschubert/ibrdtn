/*
 * usbinterface.cpp
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

#include "usbinterface.h"

namespace ibrcommon
{
	usbdevice::usbdevice(libusb_context *context, uint16_t vendor, uint16_t product)
	{
		_handle = libusb_open_device_with_vid_pid(context, vendor, product);
		if (_handle == NULL)
		{
			throw usb_device_error("Failed to open device.");
		}

		_device = libusb_get_device(_handle);
	}

	usbdevice::usbdevice(libusb_context *context, libusb_device *device)
		: _device(device)
	{
		int err = libusb_open(device, &_handle);
		if (err)
		{
			throw usb_device_error("Failed to open device.");
		}
	}

	uint8_t usbdevice::get_bus() const
	{
		return libusb_get_bus_number(_device);
	}

	uint8_t usbdevice::get_address() const
	{
		return libusb_get_device_address(_device);
	}

	std::ostream& operator<<(std::ostream &out, const usbdevice &dev)
	{
		out << (int) libusb_get_device_address(dev._device);
		out << "@";
		out << libusb_get_bus_number(dev._device);
		return out;
	}

	libusb_device_handle *usbdevice::get_handle() const
	{
		return _handle;
	}

	usbinterface::usbinterface(const std::string &name, usbdevice &device, const uint8_t &iface)
			: vinterface(name), _device(device), interface_num(iface)
	{
	}

	usbinterface::~usbinterface()
	{
	}

	void usbinterface::set_up()
	{
		int err = libusb_claim_interface(_device.get_handle(), interface_num);
		if (err)
		{
			throw USBError(static_cast<libusb_error>(err));
		}
	}

	void usbinterface::set_down()
	{
		/* Note: streams are automagically freed when releasing an interface */
		int err = libusb_release_interface(_device.get_handle(), interface_num);
		if (err)
		{
			throw USBError(static_cast<libusb_error>(err));
		}
	}

	libusb_device_handle* usbinterface::device() const
	{
		return _device.get_handle();
	}

	std::string usbinterface::get_name() const
	{
		return _name;
	}

	std::ostream& operator<<(std::ostream &out, const usbinterface &iface)
	{
		out << iface.interface_num << iface._device;
		return out;
	}
}
