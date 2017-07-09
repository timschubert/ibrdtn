/*
 * USBConvergenceLayer.h
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

#ifndef USBCONVERGENCELAYER_H_
#define USBCONVERGENCELAYER_H_

#include "Component.h"
#include "ConvergenceLayer.h"
#include "DiscoveryBeacon.h"
#include "USBConnection.h"
#include "USBService.h"
#include "core/BundleCore.h"
#include "core/BundleEvent.h"
#include "ibrcommon/Exceptions.h"
#include "ibrcommon/Logger.h"
#include "ibrcommon/net/vaddress.h"
#include "ibrcommon/net/vinterface.h"
#include "ibrcommon/usb/usbconnector.h"
#include "ibrcommon/usb/usbinterface.h"
#include "ibrcommon/usb/usbsocket.h"
#include "ibrcommon/usb/usbstream.h"
#include "ibrdtn/data/Serializer.h"

using namespace ibrcommon;

namespace dtn
{
	namespace net
	{
		class USBBundleError : public ibrcommon::Exception
		{
		public:
			USBBundleError()
			{
			}
			USBBundleError(const char *msg) : ibrcommon::Exception(msg)
			{
			}
		};

		class USBConvergenceLayer : public ConvergenceLayer,
		                            public DiscoveryBeaconHandler,
		                            public usbconnector::usbdevice_cb,
		                            public dtn::daemon::IndependentComponent,
					    public USBConnection::USBConnectionCallback
		{
		public:
			static const std::string TAG;

			USBConvergenceLayer(uint16_t vendor, uint16_t product, int inerfaceNum, uint8_t endpointIn, uint8_t endpointOut);

			void raiseEvent(const NodeEvent &event) throw();

			/** @see ConvergenceLayer */
			virtual ~USBConvergenceLayer();
			virtual dtn::core::Node::Protocol getDiscoveryProtocol() const;
			virtual void queue(const dtn::core::Node &n, const dtn::net::BundleTransfer &job);
			virtual void resetStats();
			virtual void getStats(ConvergenceLayer::stats_data &data) const;

			/** @see DiscoveryBeaconHandler */
			virtual void onUpdateBeacon(const vinterface &iface, DiscoveryBeacon &beacon) throw(NoServiceHereException);
			virtual void onAdvertiseBeacon(const vinterface &iface, const DiscoveryBeacon &beacon) throw();

			/** @see ibrcommon::usbconnector:usbinterface_cb */
			void device_discovered(usbdevice &dev);
			void device_lost(const usbdevice &dev);

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

			void eventBundleReceived(Bundle &b);
			void eventBeaconReceived(DiscoveryBeacon &b);

		protected:
			void __cancellation() throw();

			void componentUp() throw();
			void componentDown() throw();
			void componentRun() throw();

		private:
			/**
			 * Configuration for all USB_CLs
			 */
			const dtn::daemon::Configuration::USB &_config;

			/**
			 * True while the CL is supposed to process input, false otherwise
			 */
			bool _run;

			/**
			 * connect to these endpoints if a new matching USB interface is found
			 */
			uint8_t _endpointIn;
			uint8_t _endpointOut;

			/**
			 * Interface number on the USB device
			 */
			int _interfaceNum;

			/**
			 * The vendor id for the device for that an interface is created
			 */
			int _vendor_id;

			/**
			 * The product id for the device for that an interface is created
			 */
			int _product_id;

			/**
			 * Service that runs the USB connector
			 */
			USBService _service;

			/**
			 * Locks access to interfaces
			 */
			Mutex _interfacesLock;

			/**
			 * The USB interface the CL is connected to
			 * New sockets are opened on this interface
			 *
			 * @see _connections
			 */
			usbinterface _interface;

			/**
			 * Locks access to connections
			 */
			Mutex _connectionsLock;

			/**
			 * Stream to the USB interface that the CL is connected to
			 */
			USBConnection *_connection;

			/**
			 * The default frame length
			 */
			int _frame_length;

			/**
			 * Processes an error condition on a connection
			 *
			 * @param con connection with error condition
			 */
			void __processError();

			/**
			 * Remove a connection
			 */
			void __removeConnection(USBConnection *con);
		};
	}
}

#endif // USBCONVERGENCELAYER_H_
