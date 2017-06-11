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
	void usbconnector::usb_thread::__cancellation() throw ()
	{
		_libusb_polling.destroy();
	}

	void usbconnector::usb_thread::setup() throw ()
	{
		/* Add callbacks */
		libusb_set_pollfd_notifiers(_context, fd_added_callback, usb_fd_removed_callback, this);
		const struct libusb_pollfd **usb_fds = libusb_get_pollfds(_context);
		const struct libusb_pollfd **usb_iter = usb_fds;
		if (!usb_iter)
		{
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

	int usbconnector::libusb_hotplug_cb(struct libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *cb)
	{
		usb_device_cb *call_this = static_cast<usb_device_cb *>(cb);
		libusb_device_handle *handle;
		int err = libusb_open(device, &handle);
		if (err)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("usbconnector::libusb_hotplug_cb", 90)
				<< "failed to open device" << IBRCOMMON_LOGGER_ENDL;
			return -1;
		}

		uint8_t busnum = libusb_get_bus_number(device);
		uint8_t address = libusb_get_device_address(device);
		std::stringstream s;
		s << "usb" << ':' << busnum << ':' << address;
		std::string iface_name = s.str();
		usbinterface iface(iface_name, handle, DEFAULT_INTERFACE);

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

	void usbconnector::register_device_cb(usb_device_cb *cb, int vendor, int product)
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
			ibrcommon::MutexLock l(_hotplug_handles_lock);
			/* register callback for new device; do not save handle do not need it */
			libusb_hotplug_callback_handle *cb_handle = new libusb_hotplug_callback_handle();
			int err = libusb_hotplug_register_callback(_usb_context,
					static_cast<libusb_hotplug_event>(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT), LIBUSB_HOTPLUG_NO_FLAGS, vendor,
					product, LIBUSB_HOTPLUG_MATCH_ANY, libusb_hotplug_cb, static_cast<void *>(cb), cb_handle);
			if (err)
			{
				throw USBError(static_cast<libusb_error>(err));
			}
			else
			{
				usb_device_cb_registration registration = {vendor, product, cb_handle};
				_hotplug_handles[cb].push_back(registration);
			}
		}
		else
		{
			// TODO
		}
	}

	void usbconnector::deregister_device_cb(usb_device_cb *cb, int vendor, int product)
	{
		if (_cap_hotplug)
		{
			ibrcommon::MutexLock l(_hotplug_handles_lock);
			std::vector<usb_device_cb_registration> registrations = _hotplug_handles[cb];

			/* find all registrations that matches the vendor + product pair */
			for (auto reg : registrations)
			{
				if (reg.vendor_id == vendor && reg.product_id == product)
				{
					libusb_hotplug_deregister_callback(_usb_context, *(reg.handle));
					delete reg.handle;
				}
			}
		}
		else
		{
			// TODO
		}
	}

	void usbconnector::usb_thread::finally() throw ()
	{
		_libusb_polling.destroy();
	}

	usbconnector::usb_thread::usb_thread(libusb_context *con)
			: _context(con)
	{
	}

	usbconnector::usb_thread::~usb_thread()
	{
	}

	void usbconnector::usb_thread::run() throw ()
	{
		for (;;)
		{
			/* copy the sets because select will modify them */
			socketset out_pollset = _pollout;
			socketset in_pollset = _pollout;

			struct timeval timeout;
			libusb_get_next_timeout(_context, &timeout);
			try
			{
				ibrcommon::MutexLock l(_pollfd_lock);
				_libusb_polling.select(&in_pollset, &out_pollset, NULL, &timeout);
			} catch (socket_exception &e)
			{
				IBRCOMMON_LOGGER_DEBUG_TAG("usb_thread::run", 90)
					<< "error in select on libsub fds" << IBRCOMMON_LOGGER_ENDL;
			}
			/* if the system requires non-zero timeouts, this will lead to bugs */
			struct timeval zero_timeout = {0, 0};
			if (!out_pollset.empty() || !in_pollset.empty())
			{
				/* let libusb handle its internal events */
				libusb_handle_events_timeout(_context, &zero_timeout);
			}
		}
	}

	void usbconnector::usb_thread::usb_fd_removed_callback(int fd, void *user_data)
	{
		(static_cast<usb_thread *>(user_data))->remove_fd(fd);
	}

	void usbconnector::usb_thread::fd_added_callback(int fd, short events, void *user_data)
	{
		(static_cast<usb_thread *>(user_data))->add_fd(fd, events);
	}

	void usbconnector::usb_thread::remove_fd(int fd)
	{
		ibrcommon::MutexLock l(_pollfd_lock);
		for (auto &s : _libusb_polling.getAll())
		{
			if (s->fd() == fd)
			{
				_libusb_polling.remove(s);
			}
		}
		for (auto &s : _pollin)
		{
			if (s->fd() == fd)
			{
				_pollin.erase(s);
			}
		}
		for (auto &s : _pollout)
		{
			if (s->fd() == fd)
			{
				_pollout.erase(s);
			}
		}
	}

	void usbconnector::usb_thread::add_fd(int fd, short events)
	{
		ibrcommon::MutexLock l(_pollfd_lock);
		filesocket *b = new filesocket(fd);
		_libusb_polling.add(b);

		/* libusb does only know POLLIN and POLLOUT */
		if (events & POLLIN)
		{
			_pollin.insert(b);
		}
		if (events & POLLOUT)
		{
			_pollout.insert(b);
		}
	}

	usbconnector &usbconnector::get_instance()
	{
		static usbconnector _instance;
		return _instance;
	}

	usbconnector::usbconnector()
			: _cap_hotplug(true)
	{
		int res = libusb_init(&_usb_context);
		if (res)
		{
			throw USBError(static_cast<libusb_error>(res));
		}

		/* detect capabilities */
		if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG))
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("usb_thread::setup", 90)
				<< "no usb hotplug support detected" << IBRCOMMON_LOGGER_ENDL;
			_cap_hotplug = false;
		}

		usb_thread *_thread = new usb_thread(_usb_context);
		_thread->start();
	}

	usbconnector::~usbconnector()
	{
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
}
