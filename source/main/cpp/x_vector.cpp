#include "xbase/x_target.h"
#include "xbase/x_allocator.h"
#include "xbase/x_context.h"
#include "xbase/x_memory.h"

#include "xgenerics/x_vector.h"

namespace xcore
{
    bool vector_base_t::__set_capacity(u32 new_capacity)
    {
        if (new_capacity == m_capacity)
            return true;

        if (new_capacity == 0)
        {
            // release data
        }
        else if (new_capacity > m_capacity)
        {
            ASSERT(m_size <= m_capacity);
            ASSERT(new_capacity < (0x7FFF0000U / m_sizeof));

            bool grow_hint = true;
            if ((grow_hint) && (!math::is_power_of_2(new_capacity)))
                new_capacity = math::power_of_2_ceiling(new_capacity);

            ASSERT((new_capacity > 0) && (new_capacity > m_capacity));

            const xsize_t desired_size = m_sizeof * new_capacity;
            {
                void* new_p;
                alloc_t* alloc = context_t::runtime_alloc();
                if (m_p == nullptr)
                {
                    new_p = alloc->allocate(desired_size);
                }
                else
                {
                    new_p = reallocate(alloc, m_p, m_size, desired_size);
                }

                if (!new_p)
                {
                    return false;
                }

                m_p = new_p;
            }

            m_capacity = static_cast<u32>(new_capacity);
        }
        else
        {
            // shrink
            return false;
        }
        return true;
    }

    void vector_base_t::__copy_range(void* dst, void* src, u32 n) {}

    void vector_base_t::clear()
    {
        if (m_p)
        {
            __set_capacity(0);
            m_p        = NULL;
            m_size     = 0;
            m_capacity = 0;
        }
    }

    void vector_base_t::reserve(u32 new_capacity) { __set_capacity(new_capacity); }

    // resize(0) sets the container to empty, but does not free the allocated block.
    void vector_base_t::resize(u32 new_size)
    {
        if (m_size != new_size)
        {
            if (new_size < m_size) {}
            else
            {
                if (new_size > m_capacity)
                    __set_capacity(new_size);
            }

            m_size = new_size;
        }
    }

    void vector_base_t::__insert(u32 index, const void* p, u32 n)
    {
        ASSERT(index <= m_size);
        if (!n)
            return;

        const u32 orig_size = m_size;
        resize(m_size + n);

        const u32 num_to_move = orig_size - index;

        // This overwrites the destination object bits, but bitwise copyable means we don't need to worry about destruction.
        x_memmove((xbyte*)m_p + (index + n) * m_sizeof, (xbyte*)m_p + index * m_sizeof, m_sizeof * num_to_move);

        xbyte* pDst = (xbyte*)m_p + m_sizeof * index;

        // This copies in the new bits, overwriting the existing objects, which is OK for copyable types that don't need destruction.
        x_memcpy(pDst, p, m_sizeof * n);
    }

    void vector_base_t::__erase(u32 start, u32 n)
    {
        ASSERT((start + n) <= m_size);
        if ((start + n) > m_size)
            return;

        if (!n)
            return;

        const u32 num_to_move = m_size - (start + n);

        xbyte*       pDst = (xbyte*)m_p + start * m_sizeof;
        const xbyte* pSrc = (xbyte*)m_p + (start + n) * m_sizeof;

        // Copy "down" the objects to preserve, filling in the empty slots.
        x_memmove(pDst, pSrc, num_to_move * m_sizeof);

        m_size -= n;
    }

    bool vector_base_t::__is_equal(vector_base_t const* rhs) const
    {
        if (m_size != rhs->m_size && m_sizeof != rhs->m_sizeof)
            return false;
        if (m_size)
        {
            return x_memcmp(m_p, rhs->m_p, m_sizeof * m_size) == 0;
        }
        return true;
    }

    s32 vector_base_t::__compare(vector_base_t const* rhs) const
    {
        const u32 min_size = math::minimum(m_size, rhs->m_size);

        const xbyte* pSrc     = (xbyte*)m_p;
        const xbyte* pSrc_end = (xbyte*)m_p + min_size * m_sizeof;
        const xbyte* pDst     = (xbyte*)rhs->m_p;

        while ((pSrc < pSrc_end) && (x_memcmp(pSrc, pDst, m_sizeof) == 0))
        {
            pSrc += m_sizeof;
            pDst += m_sizeof;
        }

        if (pSrc < pSrc_end)
            return x_memcmp(pSrc, pDst, m_sizeof);

        if (m_size < rhs->m_size)
            return -1;
        else if (m_size > rhs->m_size)
            return 1;
        return 0;
    }

    void vector_base_t::__reverse()
    {
        u32    j    = m_size >> 1;
        xbyte* pSrc = (xbyte*)m_p;
        xbyte* pDst = (xbyte*)m_p + (m_size * m_sizeof);
        for (u32 i = 0; i < j; i++)
        {
            pDst -= m_sizeof;
            xmem::memswap(pSrc, pDst, m_sizeof);
            pSrc += m_sizeof;
        }
    }

    void* vector_base_t::__assume_ownership()
    {
        void* p    = m_p;
        m_p        = NULL;
        m_size     = 0;
        m_capacity = 0;
        return p;
    }

    // Caller is granting ownership of the indicated heap block.
    // Block must have size constructed elements, and have enough room for capacity elements.
    bool vector_base_t::__grant_ownership(void* p, u32 sizeofitem, u32 size, u32 capacity)
    {
        // To to prevent the caller from obviously shooting themselves in the foot.
        if ((((xbyte*)p + capacity) > m_p) && (p < ((xbyte*)m_p + m_capacity)))
        {
            // Can grant ownership of a block inside the container itself!
            ASSERT(0);
            return false;
        }

        if (size > capacity)
        {
            ASSERT(0);
            return false;
        }

        if (!p)
        {
            if (capacity)
            {
                ASSERT(0);
                return false;
            }
        }
        else if (!capacity)
        {
            ASSERT(0);
            return false;
        }

        clear();
        m_p        = p;
        m_size     = size;
        m_sizeof   = sizeofitem;
        m_capacity = capacity;
        return true;
    }

} // namespace xcore