#ifndef __XBASE_QUEUE_T_H__
#define __XBASE_QUEUE_T_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

//==============================================================================
// INCLUDES
//==============================================================================
#include "xbase/x_debug.h"
#include "xbase/x_allocator.h"
#include "xbase/x_range.h"

// ----------------------------------------------------------------------------------------
//   QUEUE
// ----------------------------------------------------------------------------------------

namespace xcore
{
	template<typename T>
	class queue_is_nill_t
	{
	public:
					queue_is_nill_t() : m_allocator(nullptr), m_max(0), m_size(0) {}
					queue_is_nill_t(alloc_t* allocator, s32 max) {}

		s32			max() const				{ return 0; }
		s32			size() const			{ return 0; }
		bool		is_empty() const		{ return true; }
		bool		is_full() const			{ return true; }
		bool		enqueue(T const& item)	{ return false; }
		bool		dequeue(T& item)		{ return false; }
	};

	template<typename T>
	class queue_as_array_t
	{
		alloc_t*		m_allocator;
		T*			m_array;
		s32			m_max;
		s32			m_head;
		s32			m_tail;

	public:
					queue_as_array_t() : m_allocator(nullptr), m_array(nullptr), m_max(0), m_head(0), m_tail(0) 
					{
					}
					queue_as_array_t(alloc_t* allocator, s32 max) : m_allocator(allocator), m_max(max) 
					{
						m_array = (T*)m_allocator->allocator(sizeof(T) * m_max, X_ALIGNMENT_DEFAULT);
					}
					~queue_as_array_t()
					{
						//TODO: called a destructor on every still present item
						m_allocator->deallocate(m_array);
						m_array = NULL;
					}

		s32			max() const				{ return m_max; }
		s32			size() const			{ return m_head - m_tail; }
		bool		is_empty() const		{ return m_tail == m_head; }
		bool		is_full() const			{ return (m_head > m_tail) && (m_head % max()) == m_tail; }

		bool		enqueue(T const& item)
		{
			if (is_full())
				return false;
			s32 const index = m_head % max();
			m_data[index] = item;
			m_head++;
			return true;
		}
		bool		dequeue(T& item)
		{
			if (is_empty())
				return false;
			s32 const index = m_tail % max();
			item = m_data[index];
			m_tail += 1;
			if (m_tail >= max())
			{
				m_tail = m_tail % max();
				m_head = m_head % max();
			}
			return true;
		}
	};

	template<typename T>
	class queue_as_list_t
	{
		struct node_t
		{
			inline		node_t() : m_next(NULL), m_prev(NULL) {}
			inline		node_t(const T& item) : m_next(NULL), m_prev(NULL), m_item(item) {}
			node_t*		m_next;
			node_t*		m_prev;
			T			m_item;
		};
		alloc_t*		m_allocator;
		node_t		m_sentry;
		s32			m_max;
		s32			m_size;

	public:
					queue_as_list_t() : m_allocator(nullptr), m_max(0), m_size(0) 
					{
					}

					queue_as_list_t(alloc_t* allocator, s32 max) : m_allocator(allocator), m_max(max), m_size(0) 
					{
						m_sentry.m_next = &m_sentry;
						m_sentry.m_prev = &m_sentry;
					}
					~queue_as_list() 
					{
						clear();
					}

		void			make(alloc_t* allocator, s32 max) 
		{
			m_allocator = allocator;
			m_max = max;
			m_size = 0;
			m_sentry.m_next = &m_sentry;
			m_sentry.m_prev = &m_sentry;
		}

		void		clear()
		{
			xheap node_heap(m_allocator);
			node_t* node = dequeue_node();
			while (node != nullptr)
			{
				node_heap.destruct<node_t>(node);
				node = dequeue_node();
			}
			m_size = 0;
			m_sentry.m_next = &m_sentry;
			m_sentry.m_prev = &m_sentry;
		}

		s32			max() const					{ return limits_t<s32>::max(); }
		s32			size() const				{ return m_size; }
		bool		is_empty() const			{ return m_size == 0; }
		bool		is_full() const				{ return false; }
		
		bool		enqueue(T const& item)
		{
			xheap node_heap(m_allocator);
			node_t* node = node_heap.construct<node_t>(item);
			node->m_next = &m_sentry;
			node->m_prev = m_sentry.m_prev;
			node->m_prev->m_next = node;
			m_size++;
			return true;
		}
		bool		dequeue(T& item)
		{
			node_t* node = dequeue_node();
			if (node == nullptr)
				return false;
			xheap heap(m_allocator);
			item = node->m_item;
			heap.destruct(node);
			return true;
		}
	private:
		node_t*		dequeue_node()
		{
			if (m_size > 0)
			{
				node_t* node = m_sentry.m_next;
				m_sentry.m_next = node->m_next;
				m_sentry.m_next->m_prev = &m_sentry;
				m_size--;
				return node;
			}
			return nullptr;
		}
	};

	template<typename T, typename S = queue_as_array_t>
	class queue_t
	{
	public:
		inline			queue_t() : m_strategy() { }
		inline			queue_t(alloc_t* allocator, s32 max) : m_strategy(allocator, max) { }

		void			make(alloc_t* allocator, s32 max) 
		{
			m_strategy.make(allocator, max)
		}

		s32				size() const			{ return m_strategy.size(); }
		s32				max() const				{ return m_strategy.max(); }

		bool			is_empty() const		{ return m_strategy.is_empty(); }
		bool			is_full() const			{ return m_strategy.is_full(); }

		bool			enqueue(T const& item)	{ return m_strategy.enqueue(item); }
		bool			dequeue(T& item)		{ return m_strategy.dequeue(item); }

		S				m_strategy;
	};


	template<typename T>
	void				make(alloc_t* mem, queue_t<T>& proto, s32 cap);
	template<typename T>
	bool				append(queue_t<T>& array, T const& element);


	template<typename T>
	inline void			make(alloc_t* mem, queue_t<T>& proto, s32 cap)
	{
		make(mem, proto.m_data, cap);
		proto.m_head = 0;
		proto.m_tail = 0;
	}

	template<typename T>
	inline bool			enqueue(queue_t<T>& q, T const& element)
	{
		return q.enqueue(element);
	}

	template<typename T>
	inline bool			dequeue(queue_t<T>& q, T & element)
	{
		return q.dequeue(element);
	}

}

#endif
