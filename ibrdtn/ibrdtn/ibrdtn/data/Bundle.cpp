/*
 * Bundle.cpp
 *
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
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

#include "ibrdtn/data/Bundle.h"
#include "ibrdtn/data/Serializer.h"
#include "ibrdtn/data/AgeBlock.h"
#include "ibrdtn/data/MetaBundle.h"
#include "ibrdtn/data/BundleID.h"

namespace dtn
{
	namespace data
	{
		Bundle::Bundle()
		{
			// if the timestamp is not set, add a ageblock
			if (_timestamp == 0)
			{
				// add a new ageblock
				push_front<dtn::data::AgeBlock>();
			}
		}

		Bundle::~Bundle()
		{
			clearBlocks();
		}

		Bundle::BlockList::BlockList()
		{
		}

		Bundle::BlockList::~BlockList()
		{
		}

		Bundle::BlockList& Bundle::BlockList::operator=(const Bundle::BlockList &ref)
		{
			_blocks.clear();
			for (std::list<refcnt_ptr<Block> >::const_iterator iter = ref._blocks.begin(); iter != ref._blocks.end(); iter++)
			{
				const refcnt_ptr<Block> &obj = (*iter);
				_blocks.push_back(obj);
			}
			return *this;
		}

		void Bundle::BlockList::push_front(Block *block)
		{
			if (_blocks.empty())
			{
				// set the last block flag
				block->set(dtn::data::Block::LAST_BLOCK, true);
			}

			_blocks.push_front(refcnt_ptr<Block>(block));
		}

		void Bundle::BlockList::push_back(Block *block)
		{
			// set the last block flag
			block->set(dtn::data::Block::LAST_BLOCK, true);

			if (!_blocks.empty())
			{
				// remove the last block flag of the previous block
				dtn::data::Block *lastblock = (_blocks.back().getPointer());
				lastblock->set(dtn::data::Block::LAST_BLOCK, false);
			}

			_blocks.push_back(refcnt_ptr<Block>(block));
		}

		void Bundle::BlockList::insert(Block *block, const Block *before)
		{
			for (std::list<refcnt_ptr<Block> >::iterator iter = _blocks.begin(); iter != _blocks.end(); iter++)
			{
				const dtn::data::Block *lb = (*iter).getPointer();

				if (lb == before)
				{
					_blocks.insert(iter, refcnt_ptr<Block>(block) );
					return;
				}
			}
		}

		void Bundle::BlockList::remove(const Block *block)
		{
			// delete all blocks
			for (std::list<refcnt_ptr<Block> >::iterator iter = _blocks.begin(); iter != _blocks.end(); iter++)
			{
				const dtn::data::Block &lb = (*(*iter));
				if ( &lb == block )
				{
					_blocks.erase(iter++);

					// set the last block bit
					if (!_blocks.empty())
						(*_blocks.back()).set(dtn::data::Block::LAST_BLOCK, true);

					return;
				}
			}
		}

		void Bundle::BlockList::clear()
		{
			// clear the list of objects
			_blocks.clear();
		}

		const Bundle::block_list& Bundle::BlockList::getAll() const
		{
			return _blocks;
		}

		const std::set<dtn::data::EID> Bundle::BlockList::getEIDs() const
		{
			std::set<dtn::data::EID> ret;

			for (std::list<refcnt_ptr<Block> >::const_iterator iter = _blocks.begin(); iter != _blocks.end(); iter++)
			{
				std::list<EID> elist = (*iter)->getEIDList();

				for (std::list<dtn::data::EID>::const_iterator iter = elist.begin(); iter != elist.end(); iter++)
				{
					ret.insert(*iter);
				}
			}

			return ret;
		}

		bool Bundle::operator==(const BundleID& other) const
		{
			return other == (*this);
		}

		bool Bundle::operator==(const MetaBundle& other) const
		{
			return other == (*this);
		}

		bool Bundle::operator!=(const Bundle& other) const
		{
			return PrimaryBlock(*this) != other;
		}

		bool Bundle::operator==(const Bundle& other) const
		{
			return PrimaryBlock(*this) == other;
		}

		bool Bundle::operator<(const Bundle& other) const
		{
			return PrimaryBlock(*this) < other;
		}

		bool Bundle::operator>(const Bundle& other) const
		{
			return PrimaryBlock(*this) > other;
		}

		const Bundle::block_list& Bundle::getBlocks() const
		{
			return _blocks.getAll();
		}

		dtn::data::Block& Bundle::getBlock(int index)
		{
			return _blocks.get(index);
		}
		
		const dtn::data::Block& Bundle::getBlock(int index) const
		{
			return _blocks.get(index);
		}

		void Bundle::remove(const dtn::data::Block &block)
		{
			_blocks.remove(&block);
		}

		void Bundle::clearBlocks()
		{
			_blocks.clear();
		}

		dtn::data::PayloadBlock& Bundle::insert(const dtn::data::Block &before, ibrcommon::BLOB::Reference &ref)
		{
			dtn::data::PayloadBlock *tmpblock = new dtn::data::PayloadBlock(ref);
			dtn::data::Block *block = dynamic_cast<dtn::data::Block*>(tmpblock);

#ifdef __DEVELOPMENT_ASSERTIONS__
			assert(block != NULL);
#endif

			_blocks.insert(block, &before);
			return (*tmpblock);
		}

		dtn::data::PayloadBlock& Bundle::push_front(ibrcommon::BLOB::Reference &ref)
		{
			dtn::data::PayloadBlock *tmpblock = new dtn::data::PayloadBlock(ref);
			dtn::data::Block *block = dynamic_cast<dtn::data::Block*>(tmpblock);

#ifdef __DEVELOPMENT_ASSERTIONS__
			assert(block != NULL);
#endif

			_blocks.push_front(block);
			return (*tmpblock);
		}

		dtn::data::PayloadBlock& Bundle::push_back(ibrcommon::BLOB::Reference &ref)
		{
			dtn::data::PayloadBlock *tmpblock = new dtn::data::PayloadBlock(ref);
			dtn::data::Block *block = dynamic_cast<dtn::data::Block*>(tmpblock);

#ifdef __DEVELOPMENT_ASSERTIONS__
			assert(block != NULL);
#endif

			_blocks.push_back(block);
			return (*tmpblock);
		}

		dtn::data::Block& Bundle::push_front(dtn::data::ExtensionBlock::Factory &factory)
		{
			dtn::data::Block *block = factory.create();

#ifdef __DEVELOPMENT_ASSERTIONS__
			assert(block != NULL);
#endif

			_blocks.push_front(block);
			return (*block);
		}

		dtn::data::Block& Bundle::push_back(dtn::data::ExtensionBlock::Factory &factory)
		{
			dtn::data::Block *block = factory.create();

#ifdef __DEVELOPMENT_ASSERTIONS__
			assert(block != NULL);
#endif

			_blocks.push_back(block);
			return (*block);
		}
		
		Block& Bundle::insert(dtn::data::ExtensionBlock::Factory& factory, const dtn::data::Block& before)
		{
			dtn::data::Block *block = factory.create();
			
#ifdef __DEVELOPMENT_ASSERTIONS__
			assert(block != NULL);
#endif

			_blocks.insert(block, &before);
			return (*block);
		}

		string Bundle::toString() const
		{
			return PrimaryBlock::toString();
		}

		size_t Bundle::blockCount() const
		{
			return _blocks.size();
		}

		bool Bundle::allEIDsInCBHE() const
		{
			if(    _destination.isCompressable()
			    && _source.isCompressable()
			    && _reportto.isCompressable()
			    && _custodian.isCompressable()
			  )
			{
				const block_list &blocks = _blocks.getAll();
				for( block_list::const_iterator it = blocks.begin(); it != blocks.end(); it++ ) {
					if( (*it)->get( Block::BLOCK_CONTAINS_EIDS ) )
					{
						std::list< EID > blockEIDs = (*it)->getEIDList();
						for( std::list< EID >::const_iterator itBlockEID = blockEIDs.begin(); itBlockEID != blockEIDs.end(); itBlockEID++ )
						{
							if( ! itBlockEID->isCompressable() )
							{
								return false;
							}
						}
					}
				}
				return true;
			}
			else
			{
				return false;
			}
		}

		size_t Bundle::BlockList::size() const
		{
			return _blocks.size();
		}

		const Block& Bundle::BlockList::get(int index) const
		{
			if(index < 0 || index >= _blocks.size()){
				throw NoSuchBlockFoundException();
			}

			std::list<refcnt_ptr<Block> >::const_iterator iter = _blocks.begin();
			std::advance(iter, index);
			return *((*iter).getPointer());
		}

		Block& Bundle::BlockList::get(int index)
		{
			if(index < 0 || index >= _blocks.size()){
				throw NoSuchBlockFoundException();
			}

			std::list<refcnt_ptr<Block> >::iterator iter = _blocks.begin();
			std::advance(iter, index);
			return *((*iter).getPointer());
		}
	}
}

