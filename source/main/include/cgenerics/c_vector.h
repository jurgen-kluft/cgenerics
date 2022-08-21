#ifndef __C_GENERICS_VECTOR_H__
#define __C_GENERICS_VECTOR_H__
#include "cbase/c_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

#include "cbase/c_math.h"

namespace ncore
{
    template <typename T> inline void value_copy(T* dst, T const* src, s32 item_count)
    {
        while (item_count > 0)
        {
            *dst++ = *src++;
            --item_count;
        }
    }

    template <typename T> inline s32 value_compare(T const& lhs, T const& rhs)
    {
        if (lhs < rhs)
            return -1;
        else if (lhs > rhs)
            return 1;
        return 0;
    }

    class vector_base_t
    {
    public:
        inline bool empty() const { return !m_size; }
        inline u32  size() const { return m_size; }
        inline u32  size_in_bytes() const { return m_size * m_sizeof; }
        inline u32  capacity() const { return m_capacity; }
        void        clear();
        void        reserve(u32 new_capacity);
        void        resize(u32 new_size);

    protected:
        vector_base_t(u32 sizeofitem)
            : m_p(nullptr)
            , m_size(0)
            , m_sizeof(sizeofitem)
            , m_capacity(0)
        {
        }

        bool __set_capacity(u32 min_new_capacity);
        void __copy_range(void* dst, void* src, u32 n);
        void __insert(u32 index, const void* p, u32 n);
        void __erase(u32 start, u32 n);
        void __reverse();
        s32  __compare(vector_base_t const* rhs) const;
        bool __is_equal(vector_base_t const* rhs) const;

        void* __assume_ownership();
        bool  __grant_ownership(void* p, u32 sizeofitem, u32 size, u32 capacity);

        void* m_p;
        u32   m_size;
        u32   m_sizeof;
        u32   m_capacity;
    };

    template <typename T> class vector_t : protected vector_base_t
    {
    public:
        vector_t()
            : vector_base_t(sizeof(T))
        {
        }
        vector_t(u32 n)
            : vector_base_t(sizeof(T))
        {
            __set_capacity(n);
        }

        vector_t(const vector_t& other)
            : vector_base_t(other.m_sizeof)
        {
            __set_capacity(other.m_size);
            m_size = other.m_size;
            __copy_range(m_p, other.m_p, m_size);
        }

        ~vector_t()
        {
            if (m_p)
            {
                __set_capacity(0);
            }
        }

        void        insert(u32 index, const T* p, u32 n);
        void        erase(u32 start, u32 n) { __erase(start, n); }
        inline void erase(u32 index) { __erase(index, 1); }
        void        reverse();

        T*   assume_ownership() { return (T*)__assume_ownership(); }
        bool grant_ownership(T* p, u32 size, u32 capacity) { __grant_ownership(p, size, capacity); }

        inline const T* begin() const { return (T*)m_p; }
        T*              begin() { return (T*)m_p; }

        inline const T* end() const { return (T*)m_p + m_size; }
        T*              end() { return (T*)m_p + m_size; }

        inline const T* ptr_at(u32 i) const
        {
            ASSERT(i < m_capacity);
            return ((T const*)m_p) + i;
        }
        inline T* ptr_at(u32 i)
        {
            ASSERT(i < m_capacity);
            return ((T*)m_p) + i;
        }

        inline const T& at(u32 i) const { return (i >= m_size) ? *ptr_at(0) : *ptr_at(i); }
        inline T&       at(u32 i) { return (i >= m_size) ? *ptr_at(0) : *ptr_at(i); }
        inline const T& front() const { return *ptr_at(0); }
        inline T&       front() { return *ptr_at(0); }
        inline const T& back() const { return *ptr_at(m_size - 1); }
        inline T&       back() { return *ptr_at(m_size - 1); }

        inline T* enlarge(u32 size)
        {
            u32 cur_size = m_size;
            resize(cur_size + size);
            return ptr_at(cur_size);
        }

        inline void push_front(const T& obj) { __insert(0, &obj, 1); }
        inline void push_back(const T& obj)
        {
            ASSERT(!m_p || (&obj < m_p) || (&obj >= ptr_at(m_size)));
            if (m_size >= m_capacity)
                __set_capacity(m_size + 1);
            value_copy(ptr_at(m_size), &obj, 1);
            m_size++;
        }

        inline void push_back_value(T obj)
        {
            if (m_size >= m_capacity)
                __set_capacity(m_size + 1);
            value_copy(ptr_at(m_size), &obj, 1);
            m_size++;
        }

        inline void pop_back()
        {
            ASSERT(m_size);
            if (m_size)
                m_size--;
        }

        vector_t<T>& append(const vector_t<T>& other)
        {
            if (other.m_size)
                insert(m_size, other.m_p, other.m_size);
            return *this;
        }

        vector_t& append(const T* p, u32 n)
        {
            if (n)
                insert(m_size, p, n);
            return *this;
        }

        inline void erase(T* p)
        {
            ASSERT(((T*)p >= (T*)m_p) && (p < ((T*)m_p + m_size)));
            erase(static_cast<u32>((T*)p - (T*)m_p));
        }

        void erase_unordered(u32 index)
        {
            ASSERT(index < m_size);
            if ((index + 1) < m_size)
            {
                T* item = ptr_at(index);
                T* back = ptr_at(m_size - 1);
                value_copy<T>(item, back, 1);
            }
            pop_back();
        }

        inline bool operator==(const vector_t& rhs) const;
        inline bool operator<(const vector_t& rhs) const;

        inline s32 find(const T& item) const
        {
            const T* p     = begin();
            const T* p_end = end();

            u32 index = 0;
            while (p != p_end)
            {
                if (value_compare<T>(&item, p) == 0)
                    return index;
                p++;
                index++;
            }

            return -1;
        }

        inline s32 find_sorted(const T& key) const
        {
            if (m_size)
            {
                // Uniform binary search - Knuth Algorithm 6.2.1 U, unrolled twice.
                s32 i = ((m_size + 1) >> 1) - 1;
                s32 m = m_size;

                for (;;)
                {
                    ASSERT_OPEN_RANGE(i, 0, (s32)m_size);
                    const T* pKey_i = (T*)ptr_at(i);
                    s32      cmp    = value_compare(key, *pKey_i);
                    if (cmp == 0)
                        return i;
                    m >>= 1;
                    if (!m)
                        break;
                    //  0 -> 0
                    // -1 -> 1
                    //  1 -> 0
                    // -1 -> 1
                    cmp = (1 - cmp) >> 1;
                    i += (((m + 1) >> 1) ^ cmp) - cmp;

                    ASSERT_OPEN_RANGE(i, 0, (s32)m_size);
                    pKey_i = (T*)ptr_at(i);
                    cmp    = value_compare(key, *pKey_i);
                    if (cmp == 0)
                        return i;
                    m >>= 1;
                    if (!m)
                        break;
                    cmp = (1 - cmp) >> 1;
                    i += (((m + 1) >> 1) ^ cmp) - cmp;
                }
            }

            return -1;
        }

        inline u32 count_occurences(const T& key) const
        {
            const T* p     = (T*)m_p;
            const T* p_end = (T*)m_p + m_size;

            u32 c = 0;
            while (p != p_end)
            {
                if (key == *p)
                    c++;
                p++;
            }
            return c;
        }

        inline s32 find(const void* item) const
        {
            const u8* p     = (u8*)m_p;
            const u8* p_end = (u8*)m_p + m_size * m_sizeof;

            u32 index = 0;
            while (p != p_end)
            {
                if (value_compare<T>(item, p, m_sizeof) == 0)
                    return index;
                p += m_sizeof;
                index++;
            }

            return -1;
        }

        inline void set_all(const T& key)
        {
            T* pDst     = (T*)m_p;
            T* pDst_end = (T*)pDst + m_size;
            while (pDst != pDst_end)
            {
                value_copy<T>(pDst, &key, 1);
                pDst++;
            }
        }
    };

} // namespace ncore

#endif // __C_GENERICS_VECTOR_H__