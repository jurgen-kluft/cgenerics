#ifndef __X_GENERICS_CONTAINERS_FLAT_HASH_MAP_H__
#define __X_GENERICS_CONTAINERS_FLAT_HASH_MAP_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

#include "xbase/x_allocator.h"
#include "xbase/x_debug.h"
#include "xbase/x_darray.h"
#include "xbase/x_hash.h"
#include "xbase/x_integer.h"
#include "xbase/x_math.h"
#include "xbase/x_memory.h"

namespace xcore
{
    namespace flat_hashmap_n
    {
        class bitmask_t
        {
        public:
            inline bitmask_t(u32 _mask)
                : m_mask(_mask)
            {
            }

            bitmask_t& operator++()
            {
                m_mask &= (m_mask - 1);
                return *this;
            }
            explicit operator bool() const { return m_mask != 0; }
            u32      operator*() const { return lowest_bit_set(); }
            u32      lowest_bit_set() const { return xfindFirstBit(m_mask); }
            u32      highest_bit_set() const { return xfindLastBit(m_mask); }

            bitmask_t begin() const { return *this; }
            bitmask_t end() const { return bitmask_t(0); }

            u32 trailing_zeros() const { return xcountTrailingZeros(m_mask); }
            u32 leading_zeros() const { return xcountLeadingZeros(m_mask); }

        private:
            friend bool operator==(const bitmask_t& a, const bitmask_t& b) { return a.m_mask == b.m_mask; }
            friend bool operator!=(const bitmask_t& a, const bitmask_t& b) { return a.m_mask != b.m_mask; }

            u32 m_mask;
        };

        // Returns a hash seed.
        //
        // The seed consists of a unique stable pointer which adds enough entropy to ensure
        // non-determinism of iteration order in most cases.
        inline u64 hash_seed(const void* unique_stable_ptr)
        {
            // The low bits of the pointer have little or no entropy because of
            // alignment. We shift the pointer to try to use higher entropy bits. A
            // good number seems to be 12 bits, because that aligns with page size.
            return reinterpret_cast<uptr>(unique_stable_ptr) >> 12;
        }

        typedef u8 h2_t;

        inline u64  H1(u64 hash, const void* unique_stable_ptr) { return (hash >> 8) ^ hash_seed(unique_stable_ptr); }
        inline h2_t H2(u64 hash) { return hash & 0xFF; }

        class ctrl_t
        {
        public:
            union
            {
                u64 m_hash_QW[4];
                u8  m_hash_B8[32];
            };

            u32 m_refs[32];

            u32 m_empty;
            u32 m_deleted;

            void clear()
            {
                m_hash_QW[0] = 0;
                m_hash_QW[1] = 0;
                m_hash_QW[2] = 0;
                m_hash_QW[3] = 0;
                m_empty   = 0xffffffff;
                m_deleted = 0;
            }

            // Writing the 8 bit hash bit by bit over 8 different sequential u32 registers then it would be
            // easy to generate the mask, like:
            void set_hash(h2_t hash, s8 const i) { m_hash_B8[i] = hash; }
            void clr_hash(s8 const i) { m_hash_B8[i] = 0; }

            u32 match(h2_t hash, u32 mask) const
            {
                bitmask_t bitmask(mask);
                for (s32 i : bitmask)
                {
                    if (m_hash_B8[i] != hash)
                        mask &= ~(1 << i);
                }
                return mask;
            }

            // Note; Maybe we should changed m_deleted into m_used?

            inline bool is_full() const { return (m_empty | m_deleted) == 0; }
            inline u32  get_used() const { return ~(m_empty | m_deleted); }
            inline bool has_empty() const { return m_empty != 0; }
            inline bool has_deleted() const { return m_deleted != 0; }
            inline bool is_empty(s8 slot) const { return (m_empty & (1 << slot)) != 0; }
            inline bool is_deleted(s8 slot) const { return (m_deleted & (1 << slot)) != 0; }
            inline bool is_used(s8 slot) const { return ((m_empty | m_deleted) & (1 << slot)) == 0; }
            inline bool is_empty_or_delete(s8 slot) const { return ((m_empty | m_deleted) & (1 << slot)) != 0; }
            inline s8   index_of_deleted() const { return (s8)xfindFirstBit(m_deleted); }
            inline s8   index_of_empty() const { return (s8)xfindFirstBit(m_empty); }
            inline s8   index_of_empty_or_deleted() const { return (s8)xfindFirstBit(m_empty | m_deleted); }

            inline void set_empty(s8 slot) { m_empty |= (1 << slot); m_deleted &= ~(1 << slot); }
            inline void set_deleted(s8 slot) { m_deleted |= (1 << slot); m_empty &= ~(1 << slot); }
            inline void set_used(s8 slot)
            {
                m_empty &= ~(1 << slot);
                m_deleted &= ~(1 << slot);
            }
            inline void deleted_to_empty_and_full_to_deleted()
            {
                u32 const full = ~(m_empty | m_deleted);
                m_empty        = m_deleted;
                m_deleted      = full;
            }

            inline void set_item(u32 item_index, s8 i) { m_refs[i] = item_index; }
            inline u32  replace_item(u32 item_index, s8 i) { u32 const old_item = m_refs[i];  m_refs[i] = item_index; return old_item;  }
        };

        class prober_t
        {
        public:
            prober_t(u64 hash, u64 mask)
            {
                ASSERTS(((mask + 1) & mask) == 0, "not a mask");
                m_mask  = mask;
                m_offset = hash & m_mask;
                m_index  = 0;
            }
            u32 offset() const { return (u32)m_offset; }
            u32 offset(u32 i) const { return (u32)((m_offset + i) & m_mask); }

            void next()
            {
                m_index += 1;
                m_offset += m_index;
                m_offset &= m_mask;
            }

            // 0-based probe index. The i-th probe in the probe sequence.
            u64 index() const { return m_index; }

        private:
            u64 m_mask;
            u64 m_offset;
            u64 m_index;
        };

        struct findinfo_t
        {
            u32 offset;
            s8 index;
            u64 probe_length;
        };

        // 64 bit Fowler/Noll/Vo FNV-1a hash
        inline u64 FNV1A64(const xbyte* bp, s32 numbytes, u32 seed)
        {
            u64          hash64 = (u64)seed ^ (14695981039346656037ull);
            xbyte const* be     = bp + numbytes;
            while (bp < be)
            {
                hash64 ^= (u64)*bp++;
                hash64 *= ((u64)0x100000001b3ULL);
            }
            return hash64;
        }

        template <typename Key> class Fnv1aHash
        {
        public:
            inline u64 operator()(const Key& key) const { return FNV1A64((xbyte const*)&key, sizeof(Key), 981039); }
        };

        template <typename Key, typename Value, typename Hasher = Fnv1aHash<Key>> class hashmap_t
        {
            inline prober_t probe(u64 hash, u64 capacity) const { return prober_t(H1(hash, m_ctrls), capacity); }

            array_t<ctrl_t>* m_ctrls;
            array_t<Key>*    m_keys;
            array_t<Value>*  m_values;
            u64              m_size;
            u64              m_capacity; // number of elements == (m_capacity + 1) * 32
            u64              m_growth_left;

        public:
            // User expects capacity to be in the number of elements
            hashmap_t(u32 capacity = 64)
            {
                u32 const n = (capacity >> 5);
                m_ctrls      = array_t<ctrl_t>::create(n, n);
                m_keys       = array_t<Key>::create(0, n * 32);
                m_values     = array_t<Value>::create(0, n * 32);
                m_size       = 0;
                m_capacity   = n - 1;
                reset_growth_left();
                clear_ctrls(0, n);
            }

            bool empty() const { return !size(); }
            u64  size() const { return m_size; }
            u64  capacity() const { return m_capacity; }
            u64  max_size() const { return (limits_t<u64>::maximum()); }
            void reset_growth_left() { growth_left() = (size_to_grow((capacity() + 1) * 32) - m_size); }
            u64& growth_left() { return m_growth_left; }

            Value* find(const Key& key)
            {
                Hasher     hasher;
                u64 const  hash = hasher(key);
                h2_t const h    = H2(hash);
                auto       seq  = probe(hash, m_capacity);
                while (true)
                {
                    ctrl_t*   ctrl    = m_ctrls->get_item(seq.offset());
                    bitmask_t bitmask = ctrl->match(h, ctrl->get_used());
                    for (u32 i : bitmask)
                    {
                        const u32  other_ref = m_ctrls->get_item((u32)seq.offset())->m_refs[i];
                        const Key* other_key = m_keys->get_item(other_ref);
                        if (key == *other_key)
                            return m_values->get_item(other_ref);
                    }
                    if (ctrl->has_empty())
                        break;

                    seq.next();
                    ASSERTS(seq.index() <= m_capacity, "full table!");
                }
                return nullptr;
            }

            bool insert(Key const& key, Value const& value)
            {
                Hasher     hasher;
                u64 const  hash = hasher(key);
                h2_t const h    = H2(hash);
                prober_t   seq  = probe(hash, m_capacity);
                while (true)
                {
                    ctrl_t*   ctrl    = m_ctrls->get_item(seq.offset());
                    bitmask_t bitmask = ctrl->match(h, ctrl->get_used());
                    for (u32 i : bitmask)
                    {
                        const u32  other_ref = m_ctrls->get_item(seq.offset())->m_refs[i];
                        const Key* other_key = m_keys->get_item(other_ref);
                        if (key == *other_key)
                            return false;
                    }
                    if (ctrl->has_empty())
                        break;
                    seq.next();
                    ASSERTS(seq.index() <= m_capacity, "full table!");
                }

                findinfo_t target = find_first_non_full(hash, m_capacity);
                if (growth_left() == 0 && !is_deleted(target.offset, target.index))
                {
                    rehash_and_grow_if_necessary();
                    target = find_first_non_full(hash, m_capacity);
                }
                ++m_size;
                growth_left() -= is_empty(target.offset, target.index);

                u32 const item_index = m_keys->size();
                m_keys->add_item(key);
                m_values->add_item(value);
                set_ctrl(target, H2(hash), item_index);
                return true;
            }

            bool erase(Key const& key)
            {
                // find the group and slot for this key
                // get the last entry of the m_keys/m_values, find it
                // swap the key/value (m_keys, m_values) with the last entry
                // change the item index of the last entry

                return false;
            }

            class iterator
            {
            public:
                iterator()
                    : m_keys(nullptr)
                    , m_values(nullptr)
                    , m_index(0)
                {
                }
                iterator(iterator const& i)
                    : m_keys(i.m_keys)
                    , m_values(i.m_values)
                    , m_index(i.m_index)
                {
                }

                const Key& operator*() const { return *m_keys->get_item(m_index); }
                const Key& operator->() const { return *m_keys->get_item(m_index); }

                const Key&   first() const { return *m_keys->get_item(m_index); }
                const Value& second() const { return *m_values->get_item(m_index); }

                iterator& operator++()
                {
                    ++m_index;
                    return *this;
                }
                iterator operator++(int)
                {
                    iterator i = *this;
                    m_index++;
                    return i;
                }

                bool operator==(const iterator& other) const { return m_keys == other.m_keys && m_values == other.m_values && m_index == other.m_index; }
                bool operator!=(const iterator& other) const { return m_keys != other.m_keys || m_values != other.m_values || m_index != other.m_index; }

            private:
                iterator(array_t<Key>* keys, array_t<Value>* values, u32 index = 0)
                    : m_keys(keys)
                    , m_values(values)
                    , m_index(0)
                {
                }

                array_t<Key>*   m_keys;
                array_t<Value>* m_values;
                u32             m_index;
            };

            class const_iterator
            {
            public:
                const_iterator()
                    : m_keys(nullptr)
                    , m_values(nullptr)
                    , m_index(0)
                {
                }
                const_iterator(const const_iterator& i)
                    : m_keys(i.m_keys)
                    , m_values(i.m_values)
                    , m_index(i.m_index)
                {
                }
                const_iterator(iterator i)
                    : m_keys(i.m_keys)
                    , m_values(i.m_values)
                    , m_index(i.m_index)
                {
                }

                Key const& operator*() const { return *m_keys->get_item(m_index); }
                Key const& operator->() const { return *m_keys->get_item(m_index); }

                Key const&   first() const { return *m_keys->get_item(m_index); }
                Value const& second() const { return *m_values->get_item(m_index); }

                const_iterator& operator++()
                {
                    ++m_index;
                    return *this;
                }
                const_iterator operator++(int)
                {
                    const_iterator i = *this;
                    m_index++;
                    return i;
                }

                bool operator==(const const_iterator& other) const { return m_keys == other.m_keys && m_values == other.m_values && m_index == other.m_index; }
                bool operator!=(const const_iterator& other) const { return m_keys != other.m_keys || m_values != other.m_values || m_index != other.m_index; }

            private:
                const_iterator(const array_t<Key>* keys, const array_t<Value>* values)
                    : m_keys(keys)
                    , m_values(values)
                    , m_index(0)
                {
                }

                array_t<Key> const*   m_keys;
                array_t<Value> const* m_values;
                u32                   m_index;
            };

            iterator begin() { return iterator(m_keys, m_values); }
            iterator end() { return iterator(m_keys, m_values, m_keys->size()); }

            const_iterator begin() const { return const_iterator(m_keys, m_values); }
            const_iterator end() const { return const_iterator(m_keys, m_values, m_keys->size()); }

        private:
            // General notes on capacity/growth methods below:
            // - We use 7/8th as maximum load factor. For 16-wide groups, that gives an
            //   average of two empty slots per group.
            // - For (capacity+1) >= 32, growth is 7/8*capacity.
            // - For (capacity+1) < 32, growth == capacity. In this case, we never need
            //   to probe (the whole table fits in one group) so we don't need a load
            //   factor less than 1.

            // Given `capacity` of the table, returns the size (i.e. number of full slots)
            // at which we should grow the capacity.
            inline bool is_valid_capacity(u64 n) const { return ((n + 1) & n) == 0 && n > 0; }
            // Rounds up the capacity to the next power of 2 minus 1, with a minimum of 1.
            inline u64 normalize_capacity(u64 n) const { return n ? (0xffffffffffffffffull >> xcountLeadingZeros(n)) : 1; }
            inline u64 size_to_grow(u64 max_size) const
            {
                return (max_size * 8 - max_size) / 8; // `n*7/8`
            }

            inline bool is_used(u32 offset, u32 index) const
            {
                ctrl_t* ctrl = m_ctrls->get_item(offset);
                return ctrl->is_used(index);
            }

            inline bool is_empty(u32 offset, u32 index) const
            {
                ctrl_t* ctrl = m_ctrls->get_item(offset);
                return ctrl->is_empty(index);
            }

            inline bool is_deleted(u32 offset, u32 index) const
            {
                ctrl_t* ctrl = m_ctrls->get_item(offset);
                return ctrl->is_deleted(index);
            }

            inline findinfo_t find_first_non_full(u64 hash, u64 capacity) const
            {
                auto seq = probe(hash, capacity);
                while (true)
                {
                    ctrl_t* ctrl = m_ctrls->get_item(seq.offset());

                    // Prioritize deleted before empty
                    if (ctrl->has_deleted())
                    {
                        return {seq.offset(), ctrl->index_of_deleted(), seq.index()};
                    }
                    else if (ctrl->has_empty())
                    {
                        return {seq.offset(), ctrl->index_of_empty(), seq.index()};
                    }
                    seq.next();
                    ASSERTS(seq.index() <= capacity, "full table!");
                }
            }

            void rehash_and_grow_if_necessary()
            {
                if (m_capacity == 0)
                {
                    resize(32);
                }
                else if (m_capacity > 1 && ((size() * u64{32}) <= ((m_capacity * 32) * u64{25})))
                {
                    // Squash DELETED without growing if there is enough capacity.
                    //
                    // drop_deletes_without_resize();
                }
                else
                {
                    // Otherwise grow the container.
                    resize(m_capacity * 2 + 1);
                }
            }

            // Reset all ctrl bytes back to Empty, except the sentinel.
            inline void reset_ctrls(u32 from, u32 to)
            {
                // NOTE: This can be optimized a lot, by not iterating over groups, but just pure memory
                for (u32 i = from; i < to; i++)
                {
                    ctrl_t* ctrl = m_ctrls->get_item(i);
                    ctrl->deleted_to_empty_and_full_to_deleted();
                }
            }

            inline void clear_ctrls(u32 from, u32 to)
            {
                // NOTE: This can be optimized a lot, by not iterating over groups, but just pure memory
                for (u32 i = from; i < to; i++)
                {
                    ctrl_t* ctrl = m_ctrls->get_item(i);
                    ctrl->clear();
                }
            }

            void initialize_slots()
            {
                ASSERT(m_capacity > 0);

                u32 const oldsize = m_ctrls->size();
                u32 const n       = (u32)(m_capacity + 1);
                m_ctrls->set_capacity(n);
                m_ctrls->set_size(n);

                clear_ctrls(oldsize, m_ctrls->size());
                reset_ctrls(0, oldsize);
                reset_growth_left();
            }

            inline void set_ctrl(findinfo_t const& fi, h2_t hash, u32 item_index)
            {
                ctrl_t* ctrl = m_ctrls->get_item(fi.offset);
                ctrl->set_hash(hash, fi.index);
                ctrl->set_used(fi.index);
                ctrl->m_refs[fi.index] = item_index;
            }

            void resize(u64 new_capacity)
            {
                ASSERT(is_valid_capacity(new_capacity));

                const u64 old_capacity = m_capacity;
                m_capacity              = new_capacity;
                initialize_slots();

                // 
                // The logic below supports hashing in-place.
                // 
                // When we rehash a 'deleted' element and find the group and slot we
                // should want to insert it at, if that slot is 'deleted' as well we
                // need to pop the one that is at that slot. If we encounter a slot that
                // is marked as 'empty' we can simply set and continue at the top level
                // finding a 'deleted' slot.
                // 

                Hasher hasher;
                for (u32 g = 0; g <= old_capacity; ++g)
                {
                    ctrl_t* ctrl = m_ctrls->get_item(g);
                    for (s8 i = 0; i < 32; ++i)
                    {
                        if (ctrl->is_deleted(i))
                        {
                            ctrl->set_empty(i);

                            u32     current_item = ctrl->m_refs[i];
                            while (true)
                            {
                                u64 const hash = hasher(*m_keys->get_item(current_item));
                                findinfo_t target = find_first_non_full(hash, m_capacity);
                                ctrl_t* target_ctrl = m_ctrls->get_item(target.offset);

                                target_ctrl->set_hash(H2(hash), target.index);
                                if (target_ctrl->is_empty(target.index))
                                {
                                    target_ctrl->set_used(target.index);
                                    target_ctrl->set_item(current_item, target.index);
                                    break;
                                }
                                
                                target_ctrl->set_used(target.index);

                                // replace the item with a new one and get the previous item
                                current_item = target_ctrl->replace_item(current_item, target.index);
                            }
                        }
                    }
                }
            }
        };

    } // namespace flat_hashmap_n

} // namespace xcore

#endif // __X_GENERICS_CONTAINERS_FLAT_HASH_MAP_H__
