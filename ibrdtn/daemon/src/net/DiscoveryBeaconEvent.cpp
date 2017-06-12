/*
 * DiscoveryBeaconEvent.cpp
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

#include "net/DiscoveryBeaconEvent.h"

namespace dtn
{
	namespace net
	{

		DiscoveryBeaconEvent::DiscoveryBeaconEvent(DiscoveryBeacon &beacon, EventDiscoveryBeaconAction action, ibrcommon::basesocket &sock)
		: m_beacon(beacon), m_action(action), m_sock(sock)
		{
			switch (action)
			{
			case DISCOVERY_PROXY:
				setLoggable(false);
				break;
			default:
				break;
			}
		}

		void DiscoveryBeaconEvent::raise(DiscoveryBeacon &beacon, EventDiscoveryBeaconAction action, ibrcommon::basesocket &sock)
		{
			dtn::core::EventDispatcher<DiscoveryBeaconEvent>::queue(new DiscoveryBeaconEvent(beacon, action, sock));
		}

		DiscoveryBeaconEvent::~DiscoveryBeaconEvent()
		{
		}

		const DiscoveryBeacon& DiscoveryBeaconEvent::getBeacon() const
		{
			return m_beacon;
		}

		EventDiscoveryBeaconAction DiscoveryBeaconEvent::getAction() const
		{
			return m_action;
		}

		const string DiscoveryBeaconEvent::getName() const
		{
			return "DiscoveryBeaconEvent";
		}

		std::string DiscoveryBeaconEvent::getMessage() const
		{
			switch (getAction())
			{
			case DISCOVERY_PROXY:
				return "Beacon " + getBeacon().toString() + " available ";
			default:
				return "unknown";
			}

			return "unknown";
		}

		const ibrcommon::basesocket& DiscoveryBeaconEvent::getSender() const
		{
			return m_sock;
		}
	}
}
