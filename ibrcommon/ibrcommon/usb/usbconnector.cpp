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
	int usbconnector::libusb_hotplug_cb(struct libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *cb)
	{
		try
		{
			usbdevice_cb *call_this = static_cast<usbdevice_cb *>(cb);

			if (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == event)
			{
				usbdevice dev(device);
				call_this->device_discovered(dev);
			}
			else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT)
			{
				usbdevice dev(device);
				call_this->device_lost(dev);
			}
		}
		catch (std::exception &e)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("usbconnector", 80) << e.what() << IBRCOMMON_LOGGER_ENDL;
		}

		return 0;
	}

	void usbconnector::register_device_cb(usbdevice_cb *cb, uint16_t vendor, uint16_t product)
	{
		if (!_cap_hotplug)
		{
			throw USBError("operation not supported");
		}

		if (vendor == 0)
		{
			vendor = LIBUSB_HOTPLUG_MATCH_ANY;
		}
		if (product == 0)
		{
			product = LIBUSB_HOTPLUG_MATCH_ANY;
		}

		/* register callback for new device; do not save handle do not need it */
		libusb_hotplug_callback_handle cb_handle;

		int err = libusb_hotplug_register_callback(
		  _usb_context, static_cast<libusb_hotplug_event>(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT), LIBUSB_HOTPLUG_NO_FLAGS,
		  vendor, product, LIBUSB_HOTPLUG_MATCH_ANY, libusb_hotplug_cb, static_cast<void *>(cb), &cb_handle);

		if (err < LIBUSB_SUCCESS)
		{
			throw USBError(static_cast<libusb_error>(err));
		}

		ibrcommon::MutexLock l(_handles_lock);
		_handles[cb] = cb_handle;
	}

	void usbconnector::unregister_device_cb(usbdevice_cb *cb, uint16_t vendor, uint16_t product)
	{
		ibrcommon::MutexLock l(_handles_lock);
		libusb_hotplug_callback_handle handle = _handles[cb];
		if (handle != NULL)
		{
			libusb_hotplug_deregister_callback(_usb_context, handle);
			_handles[cb] = NULL;
		}
	}

	void usbconnector::usb_loop() throw()
	{
		struct timeval timeout;
		struct timeval zero_timeout = {0, 0};
		static int lock = 0;
		fd_set inset;
		fd_set outset;
		int high;
		FD_ZERO(&inset);
		FD_ZERO(&outset);

		{
			ibrcommon::MutexLock l(_pollfd_lock);

			/* copy the sets because select will modify them */
			inset = _usb_in;
			outset = _usb_out;
			high = _high_fd;
		}

		int err = libusb_get_next_timeout(_usb_context, &timeout);
		struct timeval *select_timeout;
		if (err < 0)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("usbconnector", 80) << strerror(err) << IBRCOMMON_LOGGER_ENDL;
			// throw USBError(static_cast<libusb_error>(err));
		}
		else if (err == 0) // Linux and Darwin Kernels
		{
			timeout = {1, 0};
			select_timeout = &timeout;
			// select_timeout = NULL;
		}
		else
		{
			select_timeout = &timeout;
		}
		int num_fds = ::select(high + 1, &inset, &outset, NULL, select_timeout);

		if (num_fds < 0)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("usbconnector", 80) << strerror(err) << IBRCOMMON_LOGGER_ENDL;
		}
		else if (num_fds == 0) // timeout
		{
			/* let libusb handle its internal events */
			libusb_handle_events_timeout_completed(_usb_context, &zero_timeout, &lock);
		}
		else
		{
			/* let libusb handle its internal events */
			libusb_handle_events_timeout_completed(_usb_context, &zero_timeout, &lock);
		}
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

	usbconnector &usbconnector::get_instance()
	{
		static usbconnector _instance;
		return _instance;
	}

	usbconnector::usbconnector() : _cap_hotplug(true), _high_fd(0)
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
		libusb_free_pollfds(usb_fds);
	}

	usbconnector::~usbconnector()
	{
		libusb_exit(_usb_context);
	}

	bool usbconnector::hotplug()
	{
		return _cap_hotplug;
	}

	usbdevice usbconnector::open(const uint16_t &vendor, const uint16_t &product)
	{
		usbdevice dev(_usb_context, vendor, product);
		return dev;
	}

	usbinterface usbconnector::open(const uint16_t &vendor, const uint16_t &product, int interfaceNum)
	{
		usbdevice dev(_usb_context, vendor, product);
		usbinterface iface(dev, interfaceNum);
		iface.set_up();

		return iface;
	}

	fd_set usbconnector::get_in()
	{
		return this->_usb_in;
	}

	fd_set usbconnector::get_out()
	{
		return this->_usb_out;
	}

	int usbconnector::get_timeout(struct timeval *timeout)
	{
		return libusb_get_next_timeout(_usb_context, timeout);
	}

	void usbconnector::handle_completed()
	{
		static int lock = 0;
		struct timeval zero_timeout = {0, 0};
		libusb_handle_events_timeout_completed(_usb_context, &zero_timeout, &lock);
	}

	int usbconnector::get_high()
	{
		return this->_high_fd;
	}
}
