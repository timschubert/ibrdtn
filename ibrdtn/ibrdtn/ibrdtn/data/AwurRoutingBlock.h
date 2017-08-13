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

#define AWUR_HPP (1 << 1)
#define AWUR_LPP (1 << 2)

namespace dtn
{
	namespace data
	{
		class AwurHop
		{
		public:
			static const AwurHop NOHOP;

			AwurHop();
			AwurHop(const EID &eid, char flags, const Timeout timeout);

			const EID &getEID() const;
			char getFlags() const;
			Timeout getTimeout() const;

			bool operator<(const AwurHop &other) const;
			bool operator>(const AwurHop &other) const;
			bool operator==(const AwurHop &other) const;
			bool operator!=(const AwurHop &other) const;

		private:
			EID _eid;
			char _flags;
			Timeout _timeout;
		};

		class AwurRoutingBlock: public ExtensionBlock
		{
		public:
			class Factory: public dtn::data::ExtensionBlock::Factory
			{
			public:
				Factory()
						: dtn::data::ExtensionBlock::Factory(AwurRoutingBlock::BLOCK_TYPE)
				{
				}
				;
				virtual ~Factory()
				{
				}
				;
				virtual dtn::data::Block *create();
			};

			static const block_t BLOCK_TYPE;

			Length getLength() const;
			std::ostream &serialize(std::ostream &stream, Length &length) const;
			std::istream &deserialize(std::istream &stream, const Length &length);

			Number sequence_number;
			EID destination;
			EID source;
			deque<AwurHop> chain;

		protected:
			AwurRoutingBlock();
		};

		static AwurRoutingBlock::Factory __AwurRoutingBlockFactory__;
	}
}

#endif
