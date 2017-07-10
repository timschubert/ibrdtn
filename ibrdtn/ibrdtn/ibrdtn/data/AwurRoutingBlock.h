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

#ifndef AWUR_ROUTING_BLOCK_H_
#define AWUR_ROUTING_BLOCK_H_

#include "ibrcommon/net/socket.h"
#include <functional>
#include <ibrdtn/api/PlainSerializer.h>
#include <ibrdtn/data/ExtensionBlock.h>
#include <deque>

using namespace ibrcommon;

namespace dtn
{
	namespace data
	{
		class AwurChainEmptyException : Exception
		{
		public:
			AwurChainEmptyException() : Exception()
			{
			}

			virtual ~AwurChainEmptyException() throw()
			{
			}
		};

		class AwurHop
		{
		public:
			static const AwurHop NOHOP;

			enum Platform
			{
				LPP = 'l',
				HPP = 'h',
			};

			AwurHop();
			AwurHop(const EID &eid, Platform platform, const size_t &timeout);

			const EID &getEID() const;
			Platform getPlatform() const;
			Timeout getTimeout() const;

			bool operator<(const AwurHop &other) const;
			bool operator>(const AwurHop &other) const;
			bool operator==(const AwurHop &other) const;
			bool operator!=(const AwurHop &other) const;

		private:
			EID _eid;
			Platform _platform;

			/* tells the router when to forward the bundle */
			size_t _timeout;
		};

		class AwurPath
		{
		public:
			AwurPath();
			virtual ~AwurPath();

			bool getExpired() const;
			void expire();
			void addHop(const AwurHop &hop);
			void popNextHop();
			const AwurHop &getDestination() const;
			const AwurHop &getNextHop() const;
			const deque<AwurHop> &getHops() const;
			bool empty() const;

			bool operator==(const AwurPath &other) const;
			bool operator!=(const AwurPath &other) const;
			bool operator==(const AwurHop &other) const;
			bool operator!=(const AwurHop &other) const;

		private:
			deque<AwurHop> _path;
			bool _stale;
		};

		class AwurRoutingBlock : public Block
		{
		public:
			class Factory : public dtn::data::ExtensionBlock::Factory
			{
			public:
				Factory() : dtn::data::ExtensionBlock::Factory(AwurRoutingBlock::BLOCK_TYPE){};
				virtual ~Factory(){};
				virtual dtn::data::Block *create();
			};

			static const block_t BLOCK_TYPE;

			Length getLength() const;
			std::ostream &serialize(std::ostream &stream, Length &length) const;
			std::istream &deserialize(std::istream &stream, const Length &length);

			AwurHop destination;
			AwurHop source;
			AwurPath chain;

		protected:
			AwurRoutingBlock();
		};

		static AwurRoutingBlock::Factory __AwurRoutingBlockFactory__;
	}
}

namespace std
{
	template <> struct hash<dtn::data::AwurHop>
	{
		typedef dtn::data::AwurHop argument_type;
		typedef size_t result_type;
		size_t operator()(const argument_type &k) const
			{
				size_t const h1(hash<string>{}(k.getEID().getString()));
				size_t const h2(hash<char>{}(static_cast<char>(k.getPlatform())));
				return h1 ^ (h2 << 1);
			}
	};
	template <> struct hash<dtn::data::AwurPath>
	{
		typedef dtn::data::AwurPath argument_type;
		typedef size_t result_type;
		size_t operator()(const argument_type &k) const
			{
				size_t const h1(hash<dtn::data::AwurHop>{}(k.getDestination()));
				return h1;
			}
	};
}

#endif
