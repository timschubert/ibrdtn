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

		AwurHop::AwurHop(const EID &eid, Platform platform, bool pathIsComplete) : _eid(eid), _platform(platform), _pathComplete(pathIsComplete)
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

		bool AwurHop::getPathComplete() const
		{
			return _pathComplete;
		}


		bool AwurHop::operator==(const AwurHop &other) const
		{
			return _eid == other._eid && _platform == other._platform;
		}

		const block_t AwurRoutingBlock::BLOCK_TYPE = 241;

		AwurRoutingBlock::AwurRoutingBlock() : dtn::data::Block(BLOCK_TYPE)
		{
		}

		Length AwurRoutingBlock::getLength() const
		{
			Length len(0);

			/* number of hops */
			len += 1;

			Dictionary dict;

			for (const auto &hop : _chain)
			{
				dict.add(hop.getEID());
				const Dictionary::Reference &ref = dict.getRef(hop.getEID());

				/* 1 byte for flags */
				len += sizeof(char) + ref.first.getLength() + ref.second.getLength();
			}

			len += dict.getSize();

			return len;
		}

		std::ostream &AwurRoutingBlock::serialize(std::ostream &stream, Length &length) const
		{
			/* create a dictionary in which to store the EIDs */
			Dictionary dict;

			for (const auto &hop : _chain)
			{
				dict.add(hop.getEID());
			}

			/* the number of hops in the list */
			dtn::data::Length num_hops(_chain.size());
			stream << num_hops;

			/* for each hop include the platform, the offset of the scheme part and
			 * the offset of the SSP */
			for (const auto &hop : _chain)
			{
				const Dictionary::Reference &ref = dict.getRef(hop.getEID());

				char flags = 0;
				if (hop.getPlatform() == AwurHop::HPP)
				{
					flags |= (1 << 1);
				}
				else
				{
					flags &= ~(1 << 1);
				}

				if (hop.getPathComplete())
				{
					flags |= (1 << 2);
				}
				else
				{
					flags &= ~(1 << 2);
				}

				stream << flags << ref.first << ref.second;
			}

			/* in the end, there will be the dict */
			stream << dict;

			length = getLength();

			return stream;
		}

		bool AwurRoutingBlock::getPathRequested() const
		{
			return _chain.begin()->getPathComplete();
		}

		void AwurHop::setPathComplete(bool val)
		{
			_pathComplete = val;
		}

		std::istream &AwurRoutingBlock::deserialize(std::istream &stream, const Length &length)
		{
			std::vector<char> flagsv;
			std::vector<Dictionary::Reference> refs;

			Length num_hops;
			stream >> num_hops;

			/* extract platforms and offsets in order */
			for (Length i = 0; i < num_hops; i++)
			{
				char flags;
				stream >> flags;
				flagsv.push_back(flags);

				Dictionary::Reference ref;
				stream >> ref.first >> ref.second;
				refs.push_back(ref);
			}

			/* get the dict */
			Dictionary dict;
			stream >> dict;

			/* build the nodes */
			for (Length i = 0; i < num_hops; i++)
			{
				AwurHop::Platform pf;
				bool isComplete = false;

				switch (flagsv[i] & (1 << 1))
				{
					case 1:
						pf = AwurHop::HPP;
						break;
					default:
						pf = AwurHop::LPP;
						break;
				}

				switch (flagsv[i] & (1 << 2))
				{
					case 1:
						isComplete = true;
						break;
					default:
						isComplete = false;
						break;
				}

				AwurHop hop(dict.get(refs[i].first, refs[i].second), pf, isComplete);
				_chain.push_back(hop);
			}

			return stream;
		}

		const AwurHop &AwurRoutingBlock::popNextHop()
		{
			if (!_chain.empty())
			{
				const AwurHop &popped = _chain.back();
				_chain.pop_back();
				return popped;
			} else {
				throw AwurChainEmptyException();
			}
		}

		void AwurRoutingBlock::addNextHop(const AwurHop &hop)
		{
			_chain.push_back(hop);
		}
	}
}
