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
	usbinterface::usbinterface(const std::string &name, libusb_device_handle *device, const uint8_t &bus_num, const uint8_t &addr, const uint8_t &iface)
			: vinterface(name), _device(device), bus(bus_num), address(addr), interface_num(iface)
	{
	}

	usbinterface::~usbinterface()
	{
	}

	void usbinterface::set_up()
	{
		int err = libusb_claim_interface(_device, interface_num);
		if (err)
		{
			throw USBError(static_cast<libusb_error>(err));
		}
	}

	void usbinterface::set_down()
	{
		/* Note: streams are automagically freed when releasing an interface */
		// TODO close sockets (send event)
		int err = libusb_release_interface(_device, interface_num);
		if (err)
		{
			throw USBError(static_cast<libusb_error>(err));
		}
	}

	libusb_device_handle *usbinterface::device() const
	{
		return _device;
	}

	bool usbinterface::operator==(const usbinterface& rhs) const
	{
		return this->interface_num == rhs.interface_num
				&& this->bus == rhs.bus
				&& this->address == rhs.address;
	}

	bool usbinterface::operator!=(const usbinterface& rhs) const
	{
		return !(*this == rhs);
	}
}
