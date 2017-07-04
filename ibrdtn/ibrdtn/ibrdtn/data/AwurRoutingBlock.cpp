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
		Block *AwurRoutingBlock::Factory::create()
		{
			return new AwurRoutingBlock();
		}

		AwurHop::AwurHop(EID eid, Platform platform)
		{
		}

		const EID &AwurHop::getEID() const
		{
			return _eid;
		}

		AwurHop::Platform AwurHop::getPlatform() const
		{
			return _platform;
		}

		const block_t AwurRoutingBlock::BLOCK_TYPE = 241;

		AwurRoutingBlock::AwurRoutingBlock() : dtn::data::Block(BLOCK_TYPE)
		{
		}

		Length AwurRoutingBlock::getLength() const
		{
			/* very slow */
			std::ostringstream out;
			Length len;
			this->serialize(out, len);

			return len;
		}

		std::ostream &AwurRoutingBlock::serialize(std::ostream &stream, Length &length) const
		{
			/* create a dictionary in which to store the EIDs */
			Dictionary dict;

			for (auto &hop : _chain)
			{
				dict.add(hop.getEID());
			}

			stream << _flags;
			/* the number of hops in the list */
			SDNV<Length> num_hops(_chain.size());
			stream << num_hops;
			length += num_hops.getLength();

			/* for each hop include the platform, the offset of the scheme part and
			 * the offset of the SSP */
			for (auto &hop : _chain)
			{
				AwurHop::Platform platform = hop.getPlatform();
				SDNV<char> sdnv_platform(static_cast<char>(platform));
				const Dictionary::Reference &ref = dict.getRef(hop.getEID());

				stream << sdnv_platform << ref.first << ref.second;
				length += sdnv_platform.getLength() + ref.first.getLength() + ref.second.getLength();
			}

			/* in the end, there will be the dict */
			stream << dict;

			length += dict.getSize();

			return stream;
		}

		bool AwurRoutingBlock::pathRequested() const
		{
			return _flags == 1;
		}

		std::istream &AwurRoutingBlock::deserialize(std::istream &stream, const Length &length)
		{
			std::vector<SDNV<char> > pfs;
			std::vector<Dictionary::Reference> refs;

			stream >> _flags;

			SDNV<Length> num_hops;
			stream >> num_hops;

			/* extract platforms and offsets in order */
			for (Length i = 0; i < num_hops.get<dtn::data::Length>(); i++)
			{
				SDNV<char> platform;
				stream >> platform;
				pfs.push_back(platform);

				Dictionary::Reference ref;
				stream >> ref.first >> ref.second;
				refs.push_back(ref);
			}

			/* get the dict */
			Dictionary dict;
			stream >> dict;

			/* build the nodes */
			for (size_t i = 0; i < pfs.size(); i++)
			{
				AwurHop hop(dict.get(refs[i].first, refs[i].second), static_cast<AwurHop::Platform>(pfs[i].get<char>()));
				_chain.push_back(hop);
			}

			return stream;
		}

		const AwurHop &AwurRoutingBlock::popNextHop()
		{
			const AwurHop &popped = _chain.back();
			_chain.pop_back();
			return popped;
		}

		void AwurRoutingBlock::addNextHop(const AwurHop &hop)
		{
			_chain.push_back(hop);
		}
	}
}
