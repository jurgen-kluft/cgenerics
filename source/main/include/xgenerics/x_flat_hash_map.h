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
        /*
        Erase: When removing an item from this hash map we can do a swap
               remove on the keys_ and values_ array's. When we do this
               we need to find the last key/value in the hashmap and swap
               the refs_ indices.
               If we do this then the result is a compact keys_ and values_
               array which can easily be used for iteration over all keys
               and values that are actually in the hashmap!

        */
        class group_t
        {
        public:
            u32 m_empty;
            u32 m_deleted;
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

            inline void clear()
            {
                m_empty   = 0xffffffff;
                m_deleted = 0;
            }
            inline void set_empty(s8 slot) { m_empty |= (1 << slot); }
            inline void set_not_empty(s8 slot) { m_empty &= ~(1 << slot); }
            inline void set_deleted(s8 slot) { m_deleted |= (1 << slot); }
            inline void set_not_deleted(s8 slot) { m_deleted &= ~(1 << slot); }
            inline void set_used(s8 slot) { m_empty &= ~(1 << slot); m_deleted &= ~(1 << slot); }
            inline void deleted_to_empty_and_full_to_deleted()
            {
                u32 const full = ~(m_empty | m_deleted);
                m_empty        = m_deleted;
                m_deleted      = full;
            }
        };

        class bitmask_t
        {
        public:
            inline bitmask_t(u32 _mask)
                : mask_(_mask)
            {
            }

            bitmask_t& operator++()
            {
                mask_ &= (mask_ - 1);
                return *this;
            }
            explicit operator bool() const { return mask_ != 0; }
            u32      operator*() const { return LowestBitSet(); }
            u32      LowestBitSet() const { return xfindFirstBit(mask_); }
            u32      HighestBitSet() const { return xfindLastBit(mask_); }

            bitmask_t begin() const { return *this; }
            bitmask_t end() const { return bitmask_t(0); }

            u32 TrailingZeros() const { return xcountTrailingZeros(mask_); }
            u32 LeadingZeros() const { return xcountLeadingZeros(mask_); }

        private:
            friend bool operator==(const bitmask_t& a, const bitmask_t& b) { return a.mask_ == b.mask_; }
            friend bool operator!=(const bitmask_t& a, const bitmask_t& b) { return a.mask_ != b.mask_; }

            u32 mask_;
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
                u64 m_qw[4];
                u32 m_dw[8];
                u8  m_b8[32];
            };

            void clear()
            {
                m_qw[0]=0;
                m_qw[1]=0;
                m_qw[2]=0;
                m_qw[3]=0;
            }

            // Writing the 8 bit hash bit by bit over 8 different sequential u32 registers then it would be
            // easy to generate the mask, like:
            void set_hash(h2_t hash, s8 const i)
            {
                // Assuming that hash will be set to 0 when deleted
                m_dw[0] = m_dw[0] | ((hash & 1) << i);
                for (s8 j = 1; j < 8; j++)
                {
                    hash    = (hash >> 1);
                    m_dw[j] = m_dw[j] | ((hash & 1) << i);
                }
            }

            void clr_hash(s8 const i)
            {
                u64 const mask = ~(((u64)0x100000001ull << i));
                m_qw[0]        = m_qw[0] & mask;
                for (s8 j = 1; j < 4; j++)
                {
                    m_qw[j] = m_qw[j] & mask;
                }
            }

            u32 match(h2_t hash, u32 mask) const
            {
                // If we need to match with '1's, we need to AND with 0xFFFFFFFF
                // If we need to match with '0's, we need to INVERT and AND with 0xFFFFFFFF
                // Note: We could do 4 u64 instead of 8 u32 but we need to expand the incoming
                //       mask to u64 mask64 = ((u64)mask | ((u64)mask<<32));
                //       Do not know if that is faster though, need to check with compiler explorer (https://godbolt.org/))
                static u32 const masks[] = {0xFFFFFFFF, 0x00000000};
                mask                     = mask & (masks[(hash & 1)] ^ m_dw[0]);
                for (s8 i = 1; i < 8 && mask != 0; i++)
                {
                    hash = hash >> 1;
                    mask = mask & (masks[(hash & 1)] ^ m_dw[i]);
                }

                // Mask will give us a '1' bit at each position where the hash was matching
                // Main advantage is that we can mask this with the group information and come
                // up with entries that need to be checked.
                // So z_mask(h2, group->GetFull()) will give you a u32 that contains bits set
                // for slots that are 'set' and have the same hash.
                return mask;
            }
        };

        class refs_t
        {
        public:
            u32 m_refs[32];
        };

        class prober_t
        {
        public:
            prober_t(u64 hash, u64 mask)
            {
                ASSERTS(((mask + 1) & mask) == 0, "not a mask");
                mask_   = mask;
                offset_ = hash & mask_;
                index_  = 0;
            }
            u32 offset() const { return (u32)offset_; }
            u32 offset(u32 i) const { return (u32)((offset_ + i) & mask_); }

            void next()
            {
                index_ += 1;
                offset_ += index_;
                offset_ &= mask_;
            }

            // 0-based probe index. The i-th probe in the probe sequence.
            u64 index() const { return index_; }

        private:
            u64 mask_;
            u64 offset_;
            u64 index_;
        };

        struct findinfo_t
        {
            u32 offset;
            u32 index;
            u64 probe_length;
        };

        inline u64 FNV1A(const xbyte* key, s32 wrdlen, u32 seed)
        { // Pippip_Yurii version
            const char* str    = (char*)key;
            const u32   PRIME  = 591798841;
            u64         hash64 = (u64)seed ^ (14695981039346656037ull);
            u32         Cycles, NDhead;
            if (wrdlen > 8)
            {
                Cycles = ((wrdlen - 1) >> 4) + 1;
                NDhead = wrdlen - (Cycles << 3);
                for (; Cycles--; str += 8)
                {
                    hash64 = (hash64 ^ (*(u64*)(str))) * PRIME;
                    hash64 = (hash64 ^ (*(u64*)(str + NDhead))) * PRIME;
                }
            }
            else
            {
#define _PADr_KAZE(x, n) (((x) << (n)) >> (n))
                hash64 = (hash64 ^ _PADr_KAZE(*(u64*)(str + 0), (8 - wrdlen) << 3)) * PRIME;
#undef _PADr_KAZE
            }
            return hash64;
        }

        template <typename Key> class Fnv1aHash
        {
        public:
            inline u64 operator()(const Key& key) const { return FNV1A((xbyte const*)&key, sizeof(Key), 981039); }
        };

        template <typename Key, typename Value, typename Hasher = Fnv1aHash<Key>> class hashmap_t
        {
            inline prober_t probe(u64 hash, u64 capacity) const { return prober_t(H1(hash, ctrls_), capacity); }

            array_t<group_t>*     groups_;
            array_t<ctrl_t>*      ctrls_;
            array_t<refs_t>*      refs_;
            array_t<Key>*         keys_;
            array_t<Value>*       values_;
            u64                   size_;
            u64                   capacity_;    // number of elements == (capacity_ + 1) * 32
            u64                   growth_left_;

        public:
            // User expects capacity to be in the number of elements
            hashmap_t(u32 capacity = 64) {
                u32 const n = (capacity>>5);
                groups_ = array_t<group_t>::create(n, n);
                ctrls_ = array_t<ctrl_t>::create(n, n);
                refs_ = array_t<refs_t>::create(n, n);
                keys_ = array_t<Key>::create(0, n*32);
                values_ = array_t<Value>::create(0, n*32);
                size_ = 0;
                capacity_ = n-1;
                reset_growth_left();
                clear_ctrls(0, n);
                clear_groups(0, n);
            }

            bool empty() const { return !size(); }
            u64  size() const { return size_; }
            u64  capacity() const { return capacity_; }
            u64  max_size() const { return (limits_t<u64>::maximum()); }
            void reset_growth_left() { growth_left() = (size_to_grow(capacity()*32) - size_); }
            u64& growth_left() { return growth_left_; }

            Value* find(const Key& key)
            {
                Hasher     hasher;
                u64 const  hash = hasher(key);
                h2_t const h    = H2(hash);
                auto       seq  = probe(hash, capacity_);
                while (true)
                {
                    group_t*  g       = groups_->get_item(seq.offset());
                    ctrl_t*   c       = ctrlsm(seq.offset());
                    bitmask_t bitmask = c->match(h, g->get_used());
                    for (u32 i : bitmask)
                    {
                        const u32  other_ref = refs_->get_item((u32)seq.offset())->m_refs[i];
                        const Key& other_key = keys_->get_item(other_ref);
                        if (key == other_key)
                            return values_->get_item(other_ref);
                    }
                    if (g->has_empty())
                        break;

                    seq.next();
                    ASSERTS(seq.index() <= capacity_*32, "full table!");
                }
                return nullptr;
            }

            bool insert(Key const& key, Value const& value)
            {
                Hasher     hasher;
                u64 const  hash = hasher(key);
                h2_t const h    = H2(hash);
                prober_t   seq  = probe(hash, capacity_);
                while (true)
                {
                    group_t*  g       = groups_->get_item(seq.offset());
                    ctrl_t*   c       = ctrls_->get_item(seq.offset());
                    bitmask_t bitmask = c->match(h, g->get_used());
                    for (u32 i : bitmask)
                    {
                        const u32  other_ref = refs_->get_item(seq.offset())->m_refs[i];
                        const Key* other_key = keys_->get_item(other_ref);
                        if (key == *other_key)
                            return false;
                    }
                    if (g->has_empty())
                        break;
                    seq.next();
                    ASSERTS(seq.index() <= capacity_, "full table!");
                }

                findinfo_t target = find_first_non_full(hash, capacity_);
                if (growth_left() == 0 && !is_deleted(target.offset, target.index))
                {
                    rehash_and_grow_if_necessary();
                    target = find_first_non_full(hash, capacity_);
                }
                ++size_;
                growth_left() -= is_empty(target.offset, target.index);

                u32 const item_index = keys_->size();
                keys_->add_item(key);
                values_->add_item(value);
                set_ctrl(target, H2(hash), item_index);
                return true;
            }

            void set_ctrl(findinfo_t const& fi, h2_t hash, u32 item_index)
            {
                ctrl_t* ctrl = ctrls_->get_item(fi.offset);
                ctrl->set_hash(hash, fi.index);
                group_t* group = groups_->get_item(fi.offset);
                group->set_used(fi.index);
                refs_t* refs = refs_->get_item(fi.offset);
                refs->m_refs[fi.index] = item_index;
            }

            bool erase(Key const& key)
            {
                // find the group and slot for this key
                // get the last entry of the keys_/values_, find it
                // swap the key/value (keys_, values_) with the last entry
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
                iterator(array_t<Key>* keys, array_t<Value>* values, u32 index=0)
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

            iterator begin() { return iterator(keys_, values_); }
            iterator end() { return iterator(keys_, values_, keys_->size()); }

            const_iterator begin() const { return const_iterator(keys_, values_); }
            const_iterator end() const { return const_iterator(keys_, values_, keys_->size()); }

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
                group_t* g = groups_->get_item(offset);
                return g->is_used(index);
            }

            inline bool is_empty(u32 offset, u32 index) const
            {
                group_t* g = groups_->get_item(offset);
                return g->is_empty(index);
            }

            inline bool is_deleted(u32 offset, u32 index) const
            {
                group_t* g = groups_->get_item(offset);
                return g->is_deleted(index);
            }

            inline findinfo_t find_first_non_full(u64 hash, u64 capacity) const
            {
                auto seq = probe(hash, capacity);
                while (true)
                {
                    group_t* g = groups_->get_item(seq.offset());

                    // Prioritize deleted before empty
                    if (g->has_deleted())
                    {
                        return {seq.offset(), (u32)g->index_of_deleted(), seq.index()};
                    }
                    else if (g->has_empty())
                    {
                        return {seq.offset(), (u32)g->index_of_empty(), seq.index()};
                    }
                    seq.next();
                    ASSERTS(seq.index() <= capacity, "full table!");
                }
            }

            void rehash_and_grow_if_necessary()
            {
                if (capacity_ == 0)
                {
                    resize(32);
                }
                else if (capacity_ > 1 && ((size() * u64{32}) <= ((capacity_*32) * u64{25})))
                {
                    // Squash DELETED without growing if there is enough capacity.
                    //
                    //drop_deletes_without_resize();
                }
                else
                {
                    // Otherwise grow the container.
                    resize(capacity_ * 2 + 1);
                }
            }

            // Reset all ctrl bytes back to Empty, except the sentinel.
            inline void reset_groups(u32 from, u32 to)
            {
                // NOTE: This can be optimized a lot, by not iterating over groups, but just pure memory
                for (u32 i = from; i < to; i++)
                {
                    group_t* g = groups_->get_item(i);
                    g->deleted_to_empty_and_full_to_deleted();
                }
            }

            inline void clear_ctrls(u32 from, u32 to)
            {
                // NOTE: This can be optimized a lot, by not iterating over groups, but just pure memory
                for (u32 i = from; i < to; i++)
                {
                    ctrl_t* c = ctrls_->get_item(i);
                    c->clear();
                }
            }

            inline void clear_groups(u32 from, u32 to)
            {
                // NOTE: This can be optimized a lot, by not iterating over groups, but just pure memory
                for (u32 i = from; i < to; i++)
                {
                    group_t* g = groups_->get_item(i);
                    g->clear();
                }
            }

            void initialize_slots()
            {
                ASSERT(capacity_>0);

                u32 const oldsize = groups_->size();
                u32 const n = (u32)(capacity_ + 1);
                groups_->set_capacity(n);
                groups_->set_size(n);
                ctrls_->set_capacity(n);
                ctrls_->set_size(n);
                refs_->set_capacity(n);
                refs_->set_size(n);

                clear_groups(oldsize, groups_->size());
                reset_groups(0, oldsize);
                reset_growth_left();
            }

            void resize(u64 new_capacity)
            {
                ASSERT(is_valid_capacity(new_capacity));

                const u64 old_capacity = capacity_;
                capacity_              = new_capacity;
                initialize_slots();

                // When we rehash a 'deleted' element and find the group and slot we
                // should want to insert it at we need to pop the one that is at
                // that slot if it is marked as deleted. If we encounter a slot that is
                // marked as 'empty' we can simply insert and continue at the top level
                // finding a 'deleted' slot.

                u64 total_probe_length = 0;
                Hasher     hasher;
                for (u64 i = 0; i != (old_capacity*32); ++i)
                {
                    if (is_deleted((u32)(i >> 5), (u32)(i & 0x1F)))
                    {
                        refs_t* refs = refs_->get_item((u32)(i >> 5));
                        u32 current_item = refs->m_refs[(u32)(i & 0x1F)];
                        while (true)
                        {
                            u64 hash = hasher(*keys_->get_item(current_item));

                            findinfo_t target = find_first_non_full(hash, capacity_);
                            total_probe_length += target.probe_length;
                            if (is_empty(target.offset, target.index))
                            {
                                set_ctrl(target, H2(hash), current_item);
                                break;
                            }
                            refs_t* target_refs = refs_->get_item(target.offset);
                            u32 previous_item = target_refs->m_refs[target.index];
                            set_ctrl(target, H2(hash), current_item);

                            // take the previous item that was at this slot and insert it
                            current_item = previous_item;
                        }
                    }
                }
            }
        };

    } // namespace flash_hashmap_n

} // namespace xcore

#endif // __X_GENERICS_CONTAINERS_FLAT_HASH_MAP_H__