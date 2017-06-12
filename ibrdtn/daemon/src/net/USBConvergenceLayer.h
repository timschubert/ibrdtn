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
#include "core/BundleCore.h"
#include "core/BundleEvent.h"
#include "net/ConvergenceLayer.h"
#include "ibrcommon/Exceptions.h"
#include "ibrcommon/Logger.h"
#include "ibrcommon/net/vaddress.h"
#include "ibrcommon/net/vinterface.h"
#include "ibrcommon/net/usb/usbconnector.h"
#include "ibrdtn/data/Serializer.h"

using namespace ibrcommon;

namespace dtn
{
	namespace net
	{
		class USBConvergenceLayer: public ConvergenceLayer,
				public dtn::daemon::IntegratedComponent,
				public DiscoveryBeaconHandler,
				public usbconnector::usb_device_cb,
				public dtn::core::EventReceiver<DiscoveryBeaconEvent>,
				public TimerCallback
		{
		public:
			USBConvergenceLayer(usbconnector &connector);

			/** @see ConvergenceLayer */
			virtual ~USBConvergenceLayer();
			virtual dtn::core::Node::Protocol getDiscoveryProtocol() const;
			virtual void queue(const dtn::core::Node &n, const dtn::net::BundleTransfer &job);
			virtual void open(const dtn::core::Node &);
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

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

			/**
			 * @see ibcommon::usbconnector::usb_device_cb
			 */
			virtual void interface_discovered(ibrcommon::usbinterface &iface);
			virtual void interface_lost(ibrcommon::usbinterface &iface);


			size_t timeout(Timer *t);

		protected:
			void __cancellation() throw ();

			void componentUp() throw ();
			void componentDown() throw ();
			void componentRun() throw ();

		private:
			usbconnector _con;
			USBTransferService _usb;
			dtn::daemon::Configuration::USB _config;
			Timer _fakedServicesTimer;
			Mutex _fakedServicesLock;
			DiscoveryBeacon::service_list _fakedServices;
			vsocket _vsocket;
			bool _run;
		};
	}
}
