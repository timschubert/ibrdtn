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
		if (_handle == nullptr)
		{
			throw usb_device_error("Failed to open device.");
		}
		libusb_device *device = libusb_get_device(_handle);

		int err = libusb_get_device_descriptor(device, &_desc);
		if (err < 0)
		{
			throw usb_device_error("failed to obtain device descriptor");
		}
	}

	usbdevice::~usbdevice()
	{
	}

	usbdevice::usbdevice(libusb_device *device)
		: _handle(nullptr)
	{
		int err = libusb_get_device_descriptor(device, &_desc);
		if (err < 0)
		{
			throw usb_device_error("failed to obtain device descriptor");
		}

		err = libusb_open(device, &_handle);
		//if (err)
		//{
		//	throw usb_device_error("failed to open device handle");
		//}
	}

	void usbdevice::close()
	{
		libusb_close(_handle);
	}

	libusb_device_handle *usbdevice::get_handle() const
	{
		if (_handle == nullptr)
		{
			throw usb_device_error("failed to open device handle");
		}
		return _handle;
	}

	usbinterface::usbinterface(usbdevice &device, int iface)
			: _device(device), interface_num(iface)
	{
		set_up();
	}

	usbinterface::~usbinterface()
	{
		set_down();
	}

	libusb_device_handle *usbinterface::get_handle() const
	{
		return _device.get_handle();
	}

	void usbinterface::set_up() const
	{
		int err = libusb_claim_interface(get_handle(), interface_num);
		if (err < 0)
		{
			throw USBError(static_cast<libusb_error>(err));
		}
	}

	void usbinterface::set_down() const
	{
		/* Note: streams are automagically freed when releasing an interface */
		int err = libusb_release_interface(get_handle(), interface_num);
		if (err)
		{
			throw USBError(static_cast<libusb_error>(err));
		}
	}

	const usbdevice& usbinterface::get_device() const
	{
		return _device;
	}

	bool usbdevice::operator==(const usbdevice &rhs) const
	{
		return this->_desc.idVendor == rhs._desc.idVendor && this->_desc.idProduct == rhs._desc.idProduct;
	}

	bool usbdevice::operator!=(const usbdevice &rhs) const
	{
		return !((*this) == rhs);
	}
}
