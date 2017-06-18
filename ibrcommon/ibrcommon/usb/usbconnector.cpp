/*
 * usbconnector.cpp
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

#include "usbconnector.h"

#ifndef usb_error_string
#define usb_error_string(e) libusb_strerror((enum libusb_error) e)
#endif

namespace ibrcommon
{
	usbconnector::usb_device_cb_registration::usb_device_cb_registration(const uint16_t &_vendor, const uint16_t &_product, const uint8_t &_interface_num,
	                                                                     const libusb_hotplug_callback_handle *_handle)
	 : vendor_id(_vendor), product_id(_product), interface(_interface_num), handle(_handle)
	{
	}

	int usbconnector::libusb_hotplug_cb(struct libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *cb)
	{
		usb_device_cb *call_this = static_cast<usb_device_cb *>(cb);
		libusb_device_handle *handle;
		int err = libusb_open(device, &handle);
		if (err)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("usbconnector::libusb_hotplug_cb", 90) << "failed to open device" << IBRCOMMON_LOGGER_ENDL;
			return -1;
		}

		uint8_t busnum = libusb_get_bus_number(device);
		uint8_t address = libusb_get_device_address(device);
		std::stringstream s;
		s << "usb" << ':' << busnum << ':' << address;
		std::string iface_name = s.str();
		libusb_device_descriptor desc = {0};
		err = libusb_get_device_descriptor(device, &desc);
		if (err)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("usbconnector::libusb_hotplug_cb", 90) << libusb_error(err) << IBRCOMMON_LOGGER_ENDL;
			return -1;
		}

		unsigned char serial[200];
		err = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, serial, 200);
		if (err)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("usbconnector::libusb_hotplug_cb", 90) << libusb_error(err) << IBRCOMMON_LOGGER_ENDL;
			return -1;
		}
		std::stringstream ss;
		ss << serial;
		usbinterface iface(iface_name, handle, DEFAULT_INTERFACE, desc.idVendor, desc.idProduct, ss.str());

		switch (event)
		{
			case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
				call_this->interface_discovered(iface);
				break;
			case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
				call_this->interface_lost(iface);
				break;
		}
		return 0;
	}

	usbconnector::usb_device_cb_registration *usbconnector::register_device_cb(usb_device_cb *cb, uint16_t vendor, uint16_t product, uint8_t interface)
	{
		if (vendor == 0)
		{
			vendor = LIBUSB_HOTPLUG_MATCH_ANY;
		}
		if (product == 0)
		{
			product = LIBUSB_HOTPLUG_MATCH_ANY;
		}
		if (_cap_hotplug)
		{
			/* register callback for new device; do not save handle do not need it */
			libusb_hotplug_callback_handle *cb_handle = new libusb_hotplug_callback_handle();
			int err = libusb_hotplug_register_callback(
			  _usb_context, static_cast<libusb_hotplug_event>(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT), LIBUSB_HOTPLUG_NO_FLAGS,
			  vendor, product, LIBUSB_HOTPLUG_MATCH_ANY, libusb_hotplug_cb, static_cast<void *>(cb), cb_handle);
			if (err < LIBUSB_SUCCESS)
			{
				throw USBError(static_cast<libusb_error>(err));
			}
			else
			{
				usb_device_cb_registration *registration = new usb_device_cb_registration(vendor, product, interface, cb_handle);
				ibrcommon::MutexLock l(_hotplug_handles_lock);
				_hotplug_handles[cb].push_back(registration);
				return registration;
			}
		}
		return NULL;
	}

	void usbconnector::unregister_device_cb(usb_device_cb *cb, usb_device_cb_registration *regi)
	{
		if (_cap_hotplug)
		{
			ibrcommon::MutexLock l(_hotplug_handles_lock);
			std::vector<usb_device_cb_registration *> registrations = _hotplug_handles[cb];

			/* find all registrations that matches the vendor + product pair */
			for (auto &reg : registrations)
			{
				if (reg == regi)
				{
					libusb_hotplug_deregister_callback(_usb_context, *(reg->handle));
					// TODO clean up empty registrations
					if (reg != NULL)
					{
						delete reg;
					}
					reg = NULL;
				}
			}
		}
		else
		{
			// TODO
		}
	}

	void usbconnector::usb_loop() throw()
	{
		_run = true;

		IBRCOMMON_LOGGER_DEBUG_TAG("usbconnector::usb_loop", 80) << "started" << IBRCOMMON_LOGGER_ENDL;

		/* Add callbacks */
		libusb_set_pollfd_notifiers(_usb_context, fd_added_callback, usb_fd_removed_callback, this);
		const struct libusb_pollfd **usb_fds = libusb_get_pollfds(_usb_context);
		const struct libusb_pollfd **usb_iter = usb_fds;
		if (!usb_iter)
		{
			// TODO
			std::cerr << "No USB file descriptors." << std::endl;
		}
		else
		{
			while (*usb_iter != NULL)
			{
				add_fd((*usb_iter)->fd, (*usb_iter)->events);
				usb_iter++;
			}
		}

		struct timeval timeout;
		struct timeval zero_timeout = {0, 0};
		static int lock = 1;
		fd_set inset;
		fd_set outset;

		while (_run)
		{
			FD_ZERO(&inset);
			FD_ZERO(&outset);

			/* copy the sets because select will modify them */
			inset = _usb_in;
			outset = _usb_out;

			ibrcommon::MutexLock l(_pollfd_lock);
			libusb_get_next_timeout(_usb_context, &timeout);
			int num_fds = ::select(_high_fd + 1, &inset, &outset, NULL, &timeout);

			if (num_fds < 0)
			{
				throw socket_exception(strerror(errno));
			}
			else if (num_fds == 0) // timeout
			{
				/* let libusb handle its internal events */
				libusb_handle_events_completed(_usb_context, &lock);
				// IBRCOMMON_LOGGER_DEBUG_TAG("usbconnector::run", 80) << "timeout" << IBRCOMMON_LOGGER_ENDL;
			}
			else
			{
				/* let libusb handle its internal events */
				libusb_handle_events_timeout_completed(_usb_context, &zero_timeout, &lock);
				// IBRCOMMON_LOGGER_DEBUG_TAG("usbconnector::run", 80) << "activity" << IBRCOMMON_LOGGER_ENDL;
			}
		}
		IBRCOMMON_LOGGER_DEBUG_TAG("usbconnector::usb_loop", 80) << "exiting" << IBRCOMMON_LOGGER_ENDL;
		libusb_free_pollfds(usb_fds);
	}

	void usbconnector::usb_fd_removed_callback(int fd, void *user_data)
	{
		(static_cast<usbconnector *>(user_data))->remove_fd(fd);
	}

	void usbconnector::fd_added_callback(int fd, short events, void *user_data)
	{
		(static_cast<usbconnector *>(user_data))->add_fd(fd, events);
	}

	void usbconnector::remove_fd(int fd)
	{
		ibrcommon::MutexLock l(_pollfd_lock);
		FD_CLR(fd, &_usb_in);
		FD_CLR(fd, &_usb_out);
		// TODO high_fd_
	}

	void usbconnector::add_fd(int fd, short events)
	{
		ibrcommon::MutexLock l(_pollfd_lock);

		/* libusb only knows POLLIN and POLLOUT */
		if (events & POLLIN)
		{
			FD_SET(fd, &_usb_in);
		}
		if (events & POLLOUT)
		{
			FD_SET(fd, &_usb_out);
		}
		if (fd > _high_fd)
		{
			_high_fd = fd;
		}
	}

	usbconnector& usbconnector::get_instance()
	{
		static usbconnector _instance;
		return _instance;
	}

	usbconnector::usbconnector() : _cap_hotplug(true), _high_fd(0), _run(false)
	{
		int res = libusb_init(&_usb_context);
		if (res)
		{
			throw USBError(static_cast<libusb_error>(res));
		}

		/* detect capabilities */
		if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG))
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("usbconnector", 90) << "no usb hotplug support detected" << IBRCOMMON_LOGGER_ENDL;
			_cap_hotplug = false;
		}
	}

	usbconnector::~usbconnector()
	{
		_run = false;
		libusb_exit(_usb_context);
	}

	libusb_device_handle *usbconnector::usb_discover(const uint16_t &vendor, const uint16_t &product)
	{
		/* TODO maybe use libusb_hotplug_flag::LIBUSB_HOTPLUG_ENUMERATE */

		/* get the device list */
		libusb_device **dev_list;
		int cnt_devices = libusb_get_device_list(_usb_context, &dev_list);
		if (cnt_devices < 0)
		{
			throw USBError(static_cast<libusb_error>(cnt_devices));
		}

		libusb_device *found = NULL;
		libusb_device_handle *found_handle = NULL;
		for (ssize_t i = 0; i < cnt_devices; i++)
		{
			libusb_device *device = dev_list[i];
			libusb_device_descriptor desc = {0};
			libusb_get_device_descriptor(device, &desc);
			if (desc.idVendor == vendor && desc.idVendor == product)
			{
				int err = libusb_open(device, &found_handle);
				if (err)
				{
					throw USBError(static_cast<libusb_error>(err));
				}
				break;
			}
		}
		libusb_free_device_list(dev_list, 1);
		return found_handle;
	}

	bool usbconnector::hotplug()
	{
		return _cap_hotplug;
	}

	void matchDevice(uint16_t vendor, uint16_t product, uint8_t interface)
	{
	}

	void usbconnector::stop_run()
	{
		_run = false;
	}
}
