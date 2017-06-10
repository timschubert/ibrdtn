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
			const uint8_t vendor_id;
			const uint8_t product_id;
			const libusb_hotplug_callback_handle *handle;
		};

		class usb_thread : public ibrcommon::DetachedThread
		{
		public:
			usb_thread(libusb_context *con);
			virtual ~usb_thread();

			static void fd_added_callback(int fd, short events, void *user_data);
			static void usb_fd_removed_callback(int fd, void *con);

		protected:
			virtual void setup() throw();
			virtual void run(void) throw();
			virtual void finally(void) throw();
			virtual void __cancellation() throw();

		private:
			/* libusb does only use POLLIN and POLLOUT events */
			vsocket _libusb_polling;
			ibrcommon::Mutex _pollfd_lock;
			socketset _pollin;
			socketset _pollout;
			libusb_context *_context;

			void usb_loop();

			void add_fd(int fd, short events);
			void remove_fd(int fd);
		};

		virtual ~usbconnector();
		static usbconnector &get_instance();
		static int libusb_hotplug_cb(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *cb);

		libusb_device_handle *usb_discover(const uint16_t &vendor, const uint16_t &product);
		bool hotplug();

		usbconnector(usbconnector const &) = delete;
		void operator=(usbconnector const &) = delete;

		void register_device_cb(usb_device_cb *cb, int vendor, int product);
		void deregister_device_cb(usb_device_cb *cb, int vendor, int product);

	private:
		usbconnector();

		libusb_context *_usb_context;
		bool _cap_hotplug;

		static ibrcommon::Mutex _hotplug_handles_lock;
		static std::map<usb_device_cb *, std::vector<usb_device_cb_registration> > _hotplug_handles;
	};

	static bool usb_match_device(libusb_device *device, const uint16_t &vendor, const uint16_t &product);

	static const int DEFAULT_INTERFACE = 0;
}

#endif // USBCONNECTOR_H_
