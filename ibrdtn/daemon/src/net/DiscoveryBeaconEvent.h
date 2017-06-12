/*
 * DiscoveryBeaconEvent.h
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

#ifndef IBRDTN_DAEMON_SRC_NET_DISCOVERYBEACONEVENT_H_
#define IBRDTN_DAEMON_SRC_NET_DISCOVERYBEACONEVENT_H_

#include "core/Event.h"
#include "core/EventDispatcher.h"
#include "net/DiscoveryBeacon.h"
#include "ibrcommon/net/socket.h"

namespace dtn
{
	namespace net
	{
		enum EventDiscoveryBeaconAction
		{
			DISCOVERY_PROXY = 0
		};

		class DiscoveryBeaconEvent : public dtn::core::Event
		{
		public:
			virtual ~DiscoveryBeaconEvent();

			EventDiscoveryBeaconAction getAction() const;
			const DiscoveryBeacon& getBeacon() const;
			const std::string getName() const;
			std::string getMessage() const;
			const ibrcommon::basesocket& getSender() const;

			static void raise(DiscoveryBeacon &beacon, EventDiscoveryBeaconAction action, ibrcommon::basesocket &sock);

		private:
			DiscoveryBeaconEvent(DiscoveryBeacon &beacon, EventDiscoveryBeaconAction action, ibrcommon::basesocket &sock);

			const DiscoveryBeacon m_beacon;
			const EventDiscoveryBeaconAction m_action;
			const ibrcommon::basesocket &m_sock;
		};
	}
}



#endif /* IBRDTN_DAEMON_SRC_NET_DISCOVERYBEACONEVENT_H_ */
