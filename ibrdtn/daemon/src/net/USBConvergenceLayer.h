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
#include "DiscoveryBeacon.h"
#include "DiscoveryBeaconEvent.h"
#include "USBService.h"
#include "USBTransferService.h"
#include "core/BundleCore.h"
#include "core/BundleEvent.h"
#include "ibrcommon/Exceptions.h"
#include "ibrcommon/Logger.h"
#include "ibrcommon/net/vaddress.h"
#include "ibrcommon/net/vinterface.h"
#include "ibrcommon/net/dgramheader.h"
#include "ibrcommon/usb/usbconnector.h"
#include "ibrcommon/usb/usbsocket.h"
#include "ibrcommon/usb/usbstream.h"
#include "ibrcommon/usb/usbinterface.h"
#include "ibrdtn/data/Serializer.h"
#include "ConvergenceLayer.h"

using namespace ibrcommon;

namespace dtn
{
	namespace net
	{
		class USBConvergenceLayer : public ConvergenceLayer,
		                            public DiscoveryBeaconHandler,
		                            public usbconnector::usb_device_cb,
		                            public dtn::core::EventReceiver<DiscoveryBeaconEvent>,
		                            public dtn::core::EventReceiver<dtn::core::NodeEvent>,
		                            public TimerCallback,
		                            public dtn::daemon::IndependentComponent
		{
		public:
			static const std::string TAG;

			USBConvergenceLayer(uint16_t vendor, uint16_t product, uint8_t inerfaceNum, uint8_t endpointIn, uint8_t endpointOut);

			/** @see ConvergenceLayer */
			virtual ~USBConvergenceLayer();
			virtual dtn::core::Node::Protocol getDiscoveryProtocol() const;
			virtual void queue(const dtn::core::Node &n, const dtn::net::BundleTransfer &job);
			virtual void resetStats();
			virtual void getStats(ConvergenceLayer::stats_data &data) const;

			/** @see DiscoveryBeaconHandler */
			virtual void onUpdateBeacon(const vinterface &iface, DiscoveryBeacon &beacon) throw(NoServiceHereException);
			virtual void onAdvertiseBeacon(const vinterface &iface, const DiscoveryBeacon &beacon) throw();

			/** @see usbconnector::usb_device_cb */
			virtual void interface_discovered(usbinterface &iface);
			virtual void interface_lost(const usbinterface &iface);

			void raiseEvent(const DiscoveryBeaconEvent &event) throw();
			void raiseEvent(const NodeEvent &event) throw();

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

			/**
			 * Try to recover the interface if it was lost
			 */
			void recover();

			/**
			 * Reset timer and faked services for timer that reached timeout
			 */
			size_t timeout(Timer *t);

			/**
			 * Fetches a list of prioritized USBConnection to a Node.
			 *
			 * @param node The fetch the connections to a Node
			 *
			 * @return A sorted list of USBConnections to the Node
			 */
			std::list<USBConnection *> getConnections(const Node &node);


			/**
			 * Handle an incoming discovery beacon
			 */
			void handle_discovery(DiscoveryBeacon &beacon, usbsocket &sock);

			/**
			 * Add a socket to this convergence layer for an interface
			 */
			void addSocket(usbinterface &iface);

		protected:
			void __cancellation() throw();

			void componentUp() throw();
			void componentDown() throw();
			void componentRun() throw();

		private:
			usbconnector &_con;
			USBTransferService *_usb;
			USBService *_service;
			const dtn::daemon::Configuration::USB &_config;

			/**
			 * for locking faked services
			 */
			Mutex _fakedServicesLock;
			/**
			 * timers for validity of faked services
			 */
			std::map<dtn::data::EID, Timer *> _fakedServicesTimers;

			/**
			 * faked services
			 */
			std::map<dtn::data::EID, DiscoveryBeacon::service_list> _fakedServices;

			/**
			 * connected to all known sockets
			 * used for select
			 */
			vsocket _vsocket;

			/**
			 * Stores sockets for neighbors
			 */
			Mutex _connectionsLock;
			std::vector<USBConnection *> _connections;

			bool _run;

			/**
			 * connect to these endpoints if a new matching USB interface is found
			 */
			uint8_t _endpointIn;
			uint8_t _endpointOut;

			/**
			 * callback registration for hotplug
			 */
			//usbconnector::usb_device_cb_registration *_cb_registration;
			usbinterface _interface;

			uint8_t _in_sequence_number;
			uint8_t _out_sequence_number;

			/**
			 * set to true if the interface was lost and has to be recovered
			 */
			bool _recovering;

			/**
			 * The vendor id for the device for that an interface is created
			 */
			int _vendor_id;

			/**
			 * The product id for the device for that an interface is created
			 */
			int _product_id;
		};
	}
}

#endif // USBCONVERGENCELAYER_H_
