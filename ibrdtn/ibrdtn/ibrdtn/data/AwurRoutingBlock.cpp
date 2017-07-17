/*
 * AwurRoutingBlock.h
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

#include "AwurRoutingBlock.h"

using namespace ibrcommon;

namespace dtn
{
	namespace data
	{
		const AwurHop AwurHop::NOHOP;

		Block *AwurRoutingBlock::Factory::create()
		{
			return new AwurRoutingBlock();
		}

		AwurHop::AwurHop() : _eid(), _platform(0), _timeout(0)
		{
		}

		AwurHop::AwurHop(const EID &eid, char flags, const size_t &timeout = 0) : _eid(eid), _platform(flags), _timeout(timeout)
		{
		}

		const EID &AwurHop::getEID() const
		{
			return _eid;
		}

		char AwurHop::getPlatform() const
		{
			return _platform;
		}

		Timeout AwurHop::getTimeout() const
		{
			return _timeout;
		}

		bool AwurHop::operator<(const AwurHop &other) const
		{
			if (this->_eid == other._eid) {
				return this->_platform < other._platform;
			} else {
				return this->_eid < other._eid;
			}
		}

		bool AwurHop::operator>(const AwurHop &other) const
		{
			if (this->_eid == other._eid) {
				return this->_platform > other._platform;
			} else {
				return this->_eid > other._eid;
			}
		}

		bool AwurHop::operator==(const AwurHop &other) const
		{
			return _eid == other._eid && _platform == other._platform;
		}

		bool AwurHop::operator!=(const AwurHop &other) const
		{
			return !(*this == other);
		}

		const block_t AwurRoutingBlock::BLOCK_TYPE = 241;

		AwurRoutingBlock::AwurRoutingBlock() : dtn::data::Block(BLOCK_TYPE)
		{
		}

		bool AwurPath::empty() const
		{
			return _path.empty();
		}

		Length AwurRoutingBlock::getLength() const
		{
			Length len(0);

			/* destination */
			auto compr = destination.getEID().getCompressed();
			len += compr.first.getLength() + compr.second.getLength();

			/* source */
			compr = source.getEID().getCompressed();
			len += compr.first.getLength() + compr.second.getLength();

			/* number of hops */
			Number num_hops;

			for (const auto &hop : chain.getHops())
			{
				num_hops += 1;

				compr = destination.getEID().getCompressed();
				len += sizeof(char) + SDNV<Timeout>(hop.getTimeout()).getLength() + compr.first.getLength() + compr.second.getLength();
			}

			len += num_hops.getLength();

			return len;
		}

		std::ostream &AwurRoutingBlock::serialize(std::ostream &stream, Length &length) const
		{
			char flags;

			/* destination */
			auto compr = destination.getEID().getCompressed();
			stream << compr.first << compr.second;

			/* source */
			compr = source.getEID().getCompressed();
			stream << compr.first << compr.second;

			auto hops = chain.getHops();
			SDNV<Length> num_hops(hops.size());
			stream << num_hops;

			for (const auto &hop : hops)
			{
				flags = hop.getPlatform();
				compr = source.getEID().getCompressed();
				stream.write(&flags, 1);
				stream << SDNV<Timeout>(hop.getTimeout()) << compr.first << compr.second;
			}

			length = getLength();

			return stream;
		}

		std::istream &AwurRoutingBlock::deserialize(std::istream &stream, const Length &length)
		{
			char pf;

			/* destination */
			Number ipn_node;
			Number ipn_app;

			/* destination */
			stream >> ipn_node >> ipn_app;
			destination = AwurHop(EID(ipn_node, ipn_app), 0, 0);

			/* source */
			stream >> ipn_node >> ipn_app;
			source = AwurHop(EID(ipn_node, ipn_app), 0, 0);

			SDNV<Length> num_hops;
			stream >> num_hops;

			/* extract hops in order */
			SDNV<Timeout> timeout;
			for (Length i = 0; i < num_hops.get<Length>(); i++)
			{
				stream.read(&pf, 1);
				stream >> timeout;
				stream >> ipn_node >> ipn_app;

				AwurHop hop(EID(ipn_node, ipn_app), pf, timeout.get<Timeout>());
				chain.addHop(hop);
			}

			return stream;
		}

		AwurPath::~AwurPath()
		{
			_stale = true;
		}

		AwurPath::AwurPath() : _stale(true)
		{
		}

		bool AwurPath::getExpired() const
		{
			return _path.empty() || _stale;
		}

		void AwurPath::expire()
		{
			_stale = true;
		}

		const AwurHop &AwurPath::getDestination() const
		{
			return _path.back();
		}

		void AwurPath::popNextHop()
		{
			if (!_path.empty())
				_path.pop_front();
		}

		const AwurHop &AwurPath::getNextHop() const
		{
			if (empty()) {
				return AwurHop::NOHOP;
			} else {
				return _path.front();
			}
		}

		bool AwurPath::operator==(const AwurHop &other) const
		{
			return this->getDestination() == other;
		}

		bool AwurPath::operator!=(const AwurHop &other) const
		{
			return this->getDestination() != other;
		}

		bool AwurPath::operator==(const AwurPath &other) const
		{
			return this->getDestination() == other.getDestination();
		}

		bool AwurPath::operator!=(const AwurPath &other) const
		{
			return !(*this == other);
		}

		void AwurPath::addHop(const AwurHop &hop)
		{
			_path.push_back(hop);
		}

		const deque<AwurHop> &AwurPath::getHops() const
		{
			return _path;
		}
	}
}
