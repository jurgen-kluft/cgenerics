#ifndef __XBASE_MAP_T_H__
#define __XBASE_MAP_T_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

//==============================================================================
// INCLUDES
//==============================================================================
#include "xbase/x_debug.h"
#include "xbase/x_range.h"
#include "xbase/x_indexed_t.h"

//==============================================================================
// xCore namespace
//==============================================================================
namespace xcore
{
	// ----------------------------------------------------------------------------------------
	//   MAP
	// ----------------------------------------------------------------------------------------
	template<typename K, typename V>
	class map_t;


	template<typename K, typename V>
	class map_as_array_t
	{
		u32			m_capacity;
		u32			m_count;
		K*			m_keys;
		V*			m_vals;
		u32*		m_data;

	public:

	};


	template<typename K, typename V>
	struct map_value_t
	{
						map_value_t(map_t<K, V>& map, K const& key);

		operator		bool ()				{ return m_item != NULL; }
		operator		bool () const		{ return m_item != NULL; }

		operator		V const &()			{ return m_item->m_value; }
		operator		V const &() const	{ return m_item->m_value; }

		map_t<K, V>&	operator = (const V& value);

		map_t<K, V>&	m_map;
		K const&		m_key;
		map_item_t<K, V>* m_item;
	};

	template<typename K, typename V>
	struct map_cvalue_t
	{
		inline					map_cvalue_t(map_t<K, V> const& map, K const& key)
			: m_map(map)
			, m_key(key)
			, m_item(NULL) {}

		const K&				key() const		{ return m_item->m_key; }
		const V&				value() const	{ return m_item->m_value; }

		operator				bool() const	{ return m_item != NULL; }
		operator				V() const		{ return m_item->m_value; }

		map_t<K, V> const&		m_map;
		K const&				m_key;
		map_item_t<K, V> const*	m_item;
	};

	template<typename K, typename V>
	class map_t
	{
	public:
		K				m_empty_key;
		V				m_empty_value;

		alloc_t*			m_mem;

		map_items_t<K, V> m_items;
		map_nodes_t		m_nodes;
		map_table_t		m_table;

						map_t();
						~map_t();

		s32				len() const;
		s32				cap() const;

		map_value_t<K, V> operator [] (const K& key);
		map_cvalue_t<K, V> operator [] (const K& key) const;

		bool			remove(K const& key);

		map_iter_t<K, V> begin();

	protected:
		friend struct map_value_t<K, V>;
		u32				get(K const& key) const;
		void			add(K const& key, V const& value);
	};

	template<typename K, typename V>
	map_t<K, V>::map_t()
	{

	}

	template<typename K, typename V>
	map_t<K, V>::~map_t()
	{
		m_mem->deallocate(m_table.m_table);
	}

	template<typename K, typename V>
	s32				map_t<K, V>::len() const
	{
		return m_items.size();
	}

	template<typename K, typename V>
	s32				map_t<K, V>::cap() const
	{
		return 0;
	}

	template<typename K, typename V>
	map_value_t<K, V>	map_t<K, V>::operator [] (const K& key)
	{
		u32 const iitem = get(key);
		map_value_t<K, V> value(*this, key);
		if (map_index::is_item(iitem))
		{	// Are the keys indentical, if so then we have found the item.
			map_item_t<K,V>* pitem = m_items.get(iitem);
			if (pitem->m_key == key)
				value.m_item = pitem;
		}
		return value;
	}

	template<typename K, typename V>
	map_cvalue_t<K, V>		map_t<K, V>::operator [] (const K& key) const
	{
		u32 const iitem = get(key);
		map_cvalue_t<K, V> cvalue(*this, key);
		if (map_index::is_item(iitem))
			cvalue.m_item = m_items.get(iitem);
		return cvalue;
	}

	template<typename K, typename V>
	map_iter_t<K, V> map_t<K, V>::begin()
	{
		map_iter_t<K, V> iter;
		iter.m_idx = 0;
		iter.m_max = 0;
		iter.m_item = NULL;
		iter.m_nodes = &m_nodes;
		iter.m_items = &m_items;
		iter.m_stack_idx = 0;
		iter.m_table_index = 0;
		iter.m_table = &m_table;
	}

	template<typename K, typename V>
	u32		map_t<K,V>::get(K const& key) const
	{
		map_node_indexer indxr;
		indxr.initialize(map_key_hash<K>(key));

		map_node_t const* pnode = NULL;

		u32 const ientry = m_table.get(indxr.root());
		if (map_index::is_item(ientry))
		{
			// We need to put a node here
			// We also need the hash of the key of this item
			// map_item_t<K, V> const* pitem = m_items.get(iitem);
			return ientry;
		}
		else if (map_index::is_node(ientry)) {
			// The entry in the table is a node, ok
			pnode = m_nodes.get(ientry);
		}
		else if (map_index::is_null(ientry)) {
			return map_index::nill;
		}

		u32 depth = 1;
		while (depth < indxr.max_depth())
		{
			u32 const ientry = pnode->get(indxr.get(depth));
			if (map_index::is_item(ientry))
			{
				// We need to put a node here
				// We also need the hash of the key of this item
				//map_item_t<K, V>* pitem = m_items.get(iitem);
				return ientry;
			}
			else if (map_index::is_node(ientry)) {
				// The entry is a node, ok
				pnode = m_nodes.get(ientry);
			}
			else if (map_index::is_null(ientry)) {
				return map_index::nill;
			}
			++depth;
		}
		return map_index::nill;
	}

	template<typename K, typename V>
	void			map_t<K,V>::add(K const& key, V const& value)
	{
		map_node_indexer indxr;
		indxr.initialize(map_key_hash<K>(key));

		u32 depth = 0;
		map_node_t* pnode = NULL;
		while (depth < 17)
		{
			if (depth == 0)
			{
				u32 const ientry = m_table.get(indxr.root());
				if (map_index::is_item(ientry))
				{
					// We need to put a node here
					// We also need the hash of the key of this item
					map_item_t<K, V>* pitem = m_items.get(ientry);
					u32 const inode_next = m_nodes.alloc();
					map_node_t* pnode_next = m_nodes.get(inode_next);
					map_node_indexer indxr2;
					indxr2.initialize(map_key_hash<K>(pitem->m_key));
					pnode_next->set(indxr2.get(depth + 1), ientry);
					m_table.set(indxr.root(), inode_next);
					pnode = pnode_next;
				}
				else if (map_index::is_node(ientry)) {
					// The entry in the table is a node, ok
					pnode = m_nodes.get(ientry);
				}
				else if (map_index::is_null(ientry)) {
					// We can put the item here and we are done
					u32 const iitem = m_items.alloc();
					map_item_t<K, V>* pitem = m_items.get(iitem);
					pitem->m_key = key;
					pitem->m_value = value;
					m_table.set(indxr.root(), iitem);
					break;
				}
			}
			else {
				// We are at depth > 0 and we have a node
				u32 const ientry = pnode->get(indxr.get(depth));
				if (map_index::is_item(ientry))
				{
					// We need to put a node here
					// We also need the hash of the key of this item
					map_item_t<K, V>* pitem = m_items.get(ientry);
					u32 const inode_next = m_nodes.alloc();
					map_node_t* pnode_next = m_nodes.get(inode_next);
					map_node_indexer indxr2;
					indxr2.initialize(map_key_hash<K>(pitem->m_key));
					pnode_next->set(indxr2.get(depth + 1), ientry);
					pnode->set(indxr.get(depth), inode_next);
					pnode = pnode_next;
				}
				else if (map_index::is_node(ientry)) {
					// The entry is a node, ok
					pnode = m_nodes.get(ientry);
				}
				else if (map_index::is_null(ientry)) {
					// We can put the item here and we are done
					u32 const iitem = m_items.alloc();
					map_item_t<K, V>* pitem = m_items.get(iitem);
					pitem->m_key = key;
					pitem->m_value = value;
					pnode->set(indxr.get(depth), iitem);
					break;
				}
			}

			++depth;
		}
	}

	template<typename K, typename V>
	bool			map_t<K, V>::remove(K const& key)
	{
		map_node_indexer indxr;
		indxr.initialize(map_key_hash<K>(key));

		u32	path_nodes[32];
		map_node_t* pnode = NULL;

		u32 depth = 0;
		while (depth < indxr.max_depth())
		{
			u32 ientry;
			if (depth == 0)
			{
				ientry = m_table.get(indxr.get(depth));
			}
			else
			{
				ientry = pnode->get(indxr.get(depth));
			}

			if (map_index::is_item(ientry))
			{
				map_item_t<K, V>* pitem = m_items.get(ientry);
				if (pitem->m_key == key) {
					m_items.dealloc(ientry);
					if (depth == 0) {
						m_table.set(indxr.get(depth), map_index::nill);
						return true;
					}
					break;
				}
				else {
					return false;
				}
			}
			else if (map_index::is_node(ientry)) {
				// The entry is a node, ok
				path_nodes[depth] = ientry;
				pnode = m_nodes.get(ientry);
			}
			else if (map_index::is_null(ientry)) {
				return map_index::nill;
			}
			++depth;
		}

		// Go back up the path and see if we can dealloc empty map nodes
		if (depth < indxr.max_depth()) {
			while (depth > 0) {
				depth -= 1;
				u32 const inode = path_nodes[depth];
				pnode = m_nodes.get(inode);
				pnode->set(indxr.get(depth+1), map_index::nill);
				if (!pnode->is_empty())
					break;
				m_nodes.dealloc(inode);
				if (depth == 0) {
					m_table.set(indxr.get(depth), map_index::nill);
					break;
				}
			}
			return true;
		}

		return false;
	}

	template<typename K, typename V>
	void				make(alloc_t* mem, map_t<K, V>& proto, s32 cap);

	template<typename K, typename V>
	bool				iterate(map_iter_t<K, V>& iter);


	template<typename K, typename V>
	void				make(alloc_t* mem, map_t<K, V>& proto, s32 cap)
	{
		proto.m_mem = mem;
		make(mem, proto.m_items.m_items, cap);
		make(mem, proto.m_nodes.m_nodes, cap);
		proto.m_table.m_table_size = 1 << 10;
		proto.m_table.m_table = (u32*)mem.allocate_and_clear(sizeof(u32) * proto.m_table.m_table_size, sizeof(void*), 0xFFFFFFFF);
	}

	template<typename K, typename V>
	bool				iterate(map_iter_t<K, V>& iter)
	{
		return iter.next();
	}


	// ----------------------------------------------------------------------------------------
	// ----------------------------------------------------------------------------------------
	template<typename K, typename V>
	map_value_t<K, V>::map_value_t(map_t<K, V>& map, K const& key)
		: m_map(map)
		, m_key(key)
		, m_item(NULL)
	{
	}

}

#endif
