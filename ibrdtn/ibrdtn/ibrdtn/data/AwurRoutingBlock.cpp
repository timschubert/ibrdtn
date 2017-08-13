/**
 * AwurRoutingBlock.cpp
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
		const block_t AwurRoutingBlock::BLOCK_TYPE = 241;

		Block *AwurRoutingBlock::Factory::create()
		{
			return new AwurRoutingBlock();
		}

		AwurHop::AwurHop()
			: _eid(), _flags(0), _timeout(0)
		{
		}

		AwurHop::AwurHop(const EID &eid, char flags, const Timeout timeout)
			: _eid(eid), _flags(flags), _timeout(timeout)
		{
		}

		const EID &AwurHop::getEID() const
		{
			return _eid;
		}

		char AwurHop::getFlags() const
		{
			return _flags;
		}

		Timeout AwurHop::getTimeout() const
		{
			return _timeout;
		}

		bool AwurHop::operator<(const AwurHop &other) const
		{
			return this->_eid < other._eid;
		}

		bool AwurHop::operator>(const AwurHop &other) const
		{
			return this->_eid > other._eid;
		}

		bool AwurHop::operator==(const AwurHop &other) const
		{
			return _eid == other._eid;
		}

		bool AwurHop::operator!=(const AwurHop &other) const
		{
			return !(*this == other);
		}

		AwurRoutingBlock::AwurRoutingBlock()
		{
			setType(AwurRoutingBlock::BLOCK_TYPE);
			set(REPLICATE_IN_EVERY_FRAGMENT, true);
		}

		Length AwurRoutingBlock::getLength() const
		{
			Length len = this->sequence_number.getLength();

			auto compr = destination.getCompressed();
			len += compr.first.getLength() + compr.second.getLength();
			compr = source.getCompressed();
			len += compr.first.getLength() + compr.second.getLength();

			/* number of hops */
			Number num_hops = chain.size();
			len += num_hops.getLength();

			for (const auto &hop : chain)
			{
				compr = hop.getEID().getCompressed();
				len += sizeof(char) + SDNV<Timeout>(hop.getTimeout()).getLength() + compr.first.getLength() + compr.second.getLength();
			}

			return len;
		}

		std::ostream &AwurRoutingBlock::serialize(std::ostream &stream, Length &length) const
		{
			char flags;

			stream << this->sequence_number;

			auto compr = destination.getCompressed();
			stream << compr.first << compr.second;
			compr = source.getCompressed();
			stream << compr.first << compr.second;

			Number num_hops(chain.size());
			stream << num_hops;

			for (const auto &hop : chain)
			{
				flags = hop.getFlags();
				compr = hop.getEID().getCompressed();
				stream.write(&flags, 1);
				stream << SDNV<Timeout>(hop.getTimeout()) << compr.first << compr.second;
			}

			length = getLength();

			return stream;
		}

		std::istream &AwurRoutingBlock::deserialize(std::istream &stream, const Length &length)
		{
			stream >> this->sequence_number;

			char flags;
			SDNV<Timeout> timeout;
			Number ipn_node;
			Number ipn_app;

			/* destination */
			stream >> ipn_node >> ipn_app;
			destination = EID(ipn_node, ipn_app);

			/* source */
			stream >> ipn_node >> ipn_app;
			source = EID(ipn_node, ipn_app);

			Number num_hops;
			stream >> num_hops;

			/* extract hops in order */
			for (Length i = 0; i < num_hops.get<Length>(); i++)
			{
				stream.read(&flags, 1);
				stream >> timeout;
				stream >> ipn_node >> ipn_app;

				AwurHop hop(EID(ipn_node, ipn_app), flags, timeout.get<Timeout>());
				chain.push_back(hop);
			}

			return stream;
		}
	}
}
