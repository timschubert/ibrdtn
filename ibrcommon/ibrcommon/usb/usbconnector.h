/*
 * usbconnector.h
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

#ifndef USBCONNECTOR_H_
#define USBCONNECTOR_H_

#include "ibrcommon/Logger.h"
#include "ibrcommon/net/vaddress.h"
#include "ibrcommon/net/vinterface.h"
#include "ibrcommon/net/vsocket.h"
#include "ibrcommon/thread/Thread.h"
#include "usbinterface.h"
#include <libusb-1.0/libusb.h>
#include <poll.h>
#include <sys/select.h>


namespace ibrcommon
{
	class usbconnector
	{
	public:
		class usb_device_cb
		{
		public:
			virtual void interface_discovered(usbinterface &iface) = 0;
			virtual void interface_lost(usbinterface &iface) = 0;
		};

		class usb_device_cb_registration
		{
		public:
			usb_device_cb_registration(const int &_vendor, const int &_product, const int &_interface_num,
			                           const libusb_hotplug_callback_handle *_handle);
			const int vendor_id;
			const int product_id;
			const int interface;
			const libusb_hotplug_callback_handle *handle;
		};

		virtual ~usbconnector();
		static usbconnector &get_instance();
		static int libusb_hotplug_cb(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *cb);

		static void fd_added_callback(int fd, short events, void *user_data);
		static void usb_fd_removed_callback(int fd, void *con);


		bool hotplug();
		usbinterface open(const int &vendor, const int &product);

		usbconnector(usbconnector const &) = delete;
		void operator=(usbconnector const &) = delete;

		usb_device_cb_registration *register_device_cb(usb_device_cb *cb, int vendor, int product, int interface);
		void unregister_device_cb(usb_device_cb *cb, usb_device_cb_registration *reg);

		virtual void usb_loop(void) throw();

	private:
		usbconnector();

		libusb_context *_usb_context;
		bool _cap_hotplug;

		ibrcommon::Mutex _hotplug_handles_lock;
		std::map<usb_device_cb *, std::vector<usb_device_cb_registration *> > _hotplug_handles;

		/* libusb does only use POLLIN and POLLOUT events */
		ibrcommon::Mutex _pollfd_lock;
		fd_set _usb_in;
		fd_set _usb_out;
		int _high_fd;

		void add_fd(int fd, short events);
		void remove_fd(int fd);
	};

	static const int DEFAULT_INTERFACE = 0;
}

#endif // USBCONNECTOR_H_
