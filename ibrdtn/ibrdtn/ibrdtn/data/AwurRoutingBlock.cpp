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

		AwurHop::AwurHop() : _eid(), _platform(HPP), _timeout(0)
		{
		}

		AwurHop::AwurHop(const EID &eid, Platform platform, const size_t &timeout = 0) : _eid(eid), _platform(platform), _timeout(timeout)
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
			Dictionary dict;

			/* destination */
			dict.add(destination.getEID());
			const Dictionary::Reference &ref = dict.getRef(destination.getEID());
			/* 1 byte for flags */
			len += sizeof(char) + ref.first.getLength() + ref.second.getLength();

			/* source */
			dict.add(source.getEID());
			const Dictionary::Reference &srcref = dict.getRef(source.getEID());
			/* 1 byte for flags */
			len += sizeof(char) + srcref.first.getLength() + srcref.second.getLength();

			/* number of hops */
			len += 1;

			for (const auto &hop : chain.getHops())
			{
				dict.add(hop.getEID());
				const Dictionary::Reference &ref = dict.getRef(hop.getEID());

				/* 1 byte for flags */
				len += sizeof(char) + SDNV<Timeout>(hop.getTimeout()).getLength() + ref.first.getLength() + ref.second.getLength();
			}

			len += dict.getSize();

			return len;
		}

		std::ostream &AwurRoutingBlock::serialize(std::ostream &stream, Length &length) const
		{
			/* create a dictionary in which to store the EIDs */
			Dictionary dict;

			/* destination */
			char flags;
			if (destination.getPlatform() == AwurHop::HPP)
			{
				flags = 0;
			}
			else
			{
				flags = 1;
			}
			dict.add(destination.getEID());
			const Dictionary::Reference &destref = dict.getRef(destination.getEID());
			stream.write(&flags, 1);
			stream << destref.first << destref.second;

			/* source */
			if (source.getPlatform() == AwurHop::HPP)
			{
				flags = 0;
			}
			else
			{
				flags = 1;
			}
			dict.add(source.getEID());
			const Dictionary::Reference &srcref = dict.getRef(source.getEID());
			stream.write(&flags, 1);
			stream << srcref.first << srcref.second;

			const auto &hops = chain.getHops();
			SDNV<Length> num_hops(hops.size());
			stream << num_hops;

			for (const auto &hop : hops)
			{
				dict.add(hop.getEID());
			}

			/* for each hop include the platform, the offset of the scheme part and
			 * the offset of the SSP */
			for (const auto &hop : hops)
			{
				const Dictionary::Reference &ref = dict.getRef(hop.getEID());

				char flags;
				if (hop.getPlatform() == AwurHop::HPP)
				{
					flags = 0;
				}
				else
				{
					flags = 1;
				}

				stream << flags << SDNV<Timeout>(hop.getTimeout()) << ref.first << ref.second;
			}

			/* in the end, there will be the dict */
			stream << dict;

			length = getLength();

			return stream;
		}

		std::istream &AwurRoutingBlock::deserialize(std::istream &stream, const Length &length)
		{
			/* destination */
			char destflags = 0;
			stream >> destflags;
			Dictionary::Reference destref;
			stream >> destref.first >> destref.second;
			AwurHop::Platform destpf;
			if (destflags == 0)
			{
				destpf = AwurHop::HPP;
			} else {
				destpf = AwurHop::LPP;
			}

			/* source */
			char srcflags = 0;
			stream >> srcflags;
			Dictionary::Reference srcref;
			stream >> srcref.first >> srcref.second;
			AwurHop::Platform srcpf;
			if (srcflags == 0)
			{
				srcpf = AwurHop::HPP;
			} else {
				srcpf = AwurHop::LPP;
			}

			SDNV<Length> num_hops;
			stream >> num_hops;

			/* extract flags, platforms and timeouts in order */
			std::vector<char> flagsv;
			std::vector<Timeout> timeouts;
			std::vector<Dictionary::Reference> refs;
			for (Length i = 0; i < num_hops.get<Length>(); i++)
			{
				char flags = 0;
				stream >> flags;
				flagsv.push_back(flags);

				SDNV<Timeout> timeout;
				stream >> timeout;
				timeouts.push_back(timeout.get<Timeout>());

				Dictionary::Reference ref;
				stream >> ref.first >> ref.second;
				refs.push_back(ref);
			}

			/* get the dict */
			Dictionary dict;
			stream >> dict;

			destination = AwurHop(dict.get(destref.first, destref.second), destpf, 0);
			source = AwurHop(dict.get(srcref.first, srcref.second), srcpf, 0);

			/* build the hops */
			for (Length i = 0; i < flagsv.size(); i++)
			{
				AwurHop::Platform pf;
				if (destflags == 0)
				{
					pf = AwurHop::HPP;
				} else {
					pf = AwurHop::LPP;
				}

				AwurHop hop(dict.get(refs[i].first, refs[i].second), pf, timeouts[i]);
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
