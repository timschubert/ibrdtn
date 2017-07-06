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
			enum Platform
			{
				LPP = 'l',
				HPP = 'h',
			};

			AwurHop();
			AwurHop(const EID &eid, Platform platform, const size_t &timeout);

			const EID &getEID() const;
			Platform getPlatform() const;
			Timeout getSlot() const;

			bool operator==(const AwurHop &other) const;
			bool operator!=(const AwurHop &other) const;

		private:
			EID _eid;
			Platform _platform;

			/* tells the router for which slot to wait before transmitting the bundle */
			size_t _slot;
		};

		class AwurPath
		{
		public:
			AwurPath();
			AwurPath(const deque<AwurHop> &hops);
			virtual ~AwurPath();

			bool getExpired() const;
			void expire();
			void addHop(const AwurHop &hop);
			void popNextHop();
			const AwurHop &getDestination() const;
			const AwurHop &getNextHop() const;
			const deque<AwurHop> &getHops() const;
			bool empty() const;

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

			void setPath(const AwurPath &path);
			const AwurPath& getPath() const;

			bool getPathRequested() const;

		protected:
			AwurRoutingBlock();

		private:
			AwurPath _chain;
		};

		static AwurRoutingBlock::Factory __AwurRoutingBlockFactory__;
	}
}

#endif
