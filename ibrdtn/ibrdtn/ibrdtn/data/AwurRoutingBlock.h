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
#include <ibrdtn/api/PlainSerializer.h>
#include <ibrdtn/data/ExtensionBlock.h>
#include <functional>

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
			enum Platform
			{
				LPP = 'l',
				HPP = 'h',
			};

			AwurHop(const EID &eid, Platform platform, bool pathIsComplete);

			const EID &getEID() const;
			Platform getPlatform() const;
			bool getPathComplete() const;
			void setPathComplete(bool val);

			bool operator==(const AwurHop &other) const;

		private:
			EID _eid;
			Platform _platform;
			bool _pathComplete;
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

			const AwurHop &popNextHop();
			void addNextHop(const AwurHop &hop);

			bool getPathRequested() const;

		protected:
			AwurRoutingBlock();

		private:
			std::vector<AwurHop> _chain;
		};

		static AwurRoutingBlock::Factory __AwurRoutingBlockFactory__;
	}
}

namespace std
{
	template <> struct hash<dtn::data::AwurHop>
	{
		size_t operator()(const dtn::data::AwurHop &k) const
		{
			size_t const h1(hash<string>{}(k.getEID().getString()));
			size_t const h2(hash<char>{}(static_cast<char>(k.getPlatform())));
			return h1 ^ (h2 << 1);
		}
	};
}


#endif
