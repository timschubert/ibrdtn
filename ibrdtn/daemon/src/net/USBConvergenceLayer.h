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
#include "USBTransferService.h"
#include "core/BundleCore.h"
#include "core/BundleEvent.h"
#include "ibrcommon/Exceptions.h"
#include "ibrcommon/Logger.h"
#include "ibrcommon/usb/usbconnector.h"
#include "ibrcommon/usb/usbsocket.h"
#include "ibrcommon/usb/usbstream.h"
#include "ibrcommon/net/vaddress.h"
#include "ibrcommon/net/vinterface.h"
#include "ibrdtn/data/Serializer.h"
#include "net/ConvergenceLayer.h"

using namespace ibrcommon;

namespace dtn
{
	class USBTransferService;

	namespace net
	{
		enum USBConvergenceLayerMask
		{
			COMPAT = 0xC0,
			TYPE = 0x30,
			SEQNO = 0x0C,
			FLAGS = 0x03
		};

		enum USBConvergenceLayerType
		{
			DATA = 0x10,
			DISCOVERY = 0x20,
			ACK = 0x30,
			NACK = 0x40
		};

		class USBConnection
		{
		public:
			USBConnection(ibrcommon::usbsocket *sock, const dtn::core::Node &node);
			virtual ~USBConnection();

			bool match(const dtn::core::Node &node) const;
			bool match(const dtn::data::EID &destination) const;
			bool match(const dtn::core::NodeEvent &evt) const;

			usbstream& getStream();

		private:
			usbsocket *_socket;
			usbstream _stream;
			const dtn::core::Node &_node;
		};

		class USBConvergenceLayer: public ConvergenceLayer,
				public dtn::daemon::IntegratedComponent,
				public DiscoveryBeaconHandler,
				public usbconnector::usb_device_cb,
				public dtn::core::EventReceiver<DiscoveryBeaconEvent>,
				public TimerCallback,
				public dtn::core::EventReceiver<dtn::core::NodeEvent>
		{
		public:
			USBConvergenceLayer(uint16_t vendor, uint16_t product, uint8_t inerfaceNum, uint8_t endpointIn, uint8_t endpointOut);

			/** @see ConvergenceLayer */
			virtual ~USBConvergenceLayer();
			virtual dtn::core::Node::Protocol getDiscoveryProtocol() const;
			virtual void queue(const dtn::core::Node &n, const dtn::net::BundleTransfer &job);
			virtual void resetStats();
			virtual void getStats(ConvergenceLayer::stats_data &data) const;

			/** @see DiscoveryBeaconHandler */
			virtual void onUpdateBeacon(const ibrcommon::vinterface &iface, DiscoveryBeacon &beacon) throw (NoServiceHereException);
			virtual void onAdvertiseBeacon(const ibrcommon::vinterface &iface, DiscoveryBeacon &beacon) throw (NoServiceHereException);

			/** @see usbconnector::usb_device_cb */
			virtual void interface_discovered(usbinterface &iface);
			virtual void interface_lost(usbinterface &iface);

			void raiseEvent(const DiscoveryBeaconEvent &event) throw ();
			void raiseEvent(const TimeEvent &event) throw ();
			void raiseEvent(const NodeEvent &event) throw ();

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

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
			std::list<USBConnection*> getConnections(const Node &node);


			/**
			 * Handle an incoming discovery beacon
			 */
			void handle_discovery(DiscoveryBeacon &beacon, usbsocket *sock);

		protected:
			void __cancellation() throw ();

			void componentUp() throw ();
			void componentDown() throw ();
			void componentRun() throw ();

		private:
			usbconnector &_con;
			USBTransferService *_usb;
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
			std::vector<USBConnection*> _connections;

			/**
			 * Maps discovere EIDs to usbsockets
			 * TODO
			 */
			std::map<dtn::data::EID, usbsocket *> _discoveredSockets;
			Mutex _discoveryLock;

			bool _run;

			/**
			 * connect to these endpoints if a new matching USB interface is found
			 */
			uint8_t _endpointIn;
			uint8_t _endpointOut;

			/**
			 * callback registration for hotplug
			 */
			usbconnector::usb_device_cb_registration *_cb_registration;
		};
	}
}

#endif // USBCONVERGENCELAYER_H_
