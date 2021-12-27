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
#define XCORE_PREDICT_TRUE(statement) (statement)

    namespace flash_hashmap_n
    {
        class group_t
        {
        public:
            u32 m_empty;
            u32 m_deleted;
            // Note; Maybe we should changed m_deleted into m_full?

            inline u32  GetFull() const { return ~(m_empty | m_deleted); }
            inline bool IsFull() const { return (m_empty | m_deleted) == 0; }
            inline bool HasEmpty() const { return m_empty == 0; }
            inline bool IsEmpty(s8 slot) const { return (m_empty & (1 << slot)) != 0; }
            inline bool IsDeleted(s8 slot) const { return (m_deleted & (1 << slot)) != 0; }
            inline bool IsFull(s8 slot) const { return ((m_empty | m_deleted) & (1 << slot)) == 0; }
            inline bool IsEmptyOrDeleted(s8 slot) const { return ((m_empty | m_deleted) & (1 << slot)) != 0; }
            inline void SetEmpty(s8 slot) { m_empty |= (1 << slot); }
            inline void SetNotEmpty(s8 slot) { m_empty &= ~(1 << slot); }
            inline void SetDeleted(s8 slot) { m_deleted |= (1 << slot); }

            inline s8   IndexOfEmptyOrDeleted() const { return (s8)xfindFirstBit(m_empty | m_deleted); }
            inline void DeletedToEmptyAndFullToDeleted()
            {
                u32 const full = ~(m_empty | m_deleted);
                m_empty        = m_deleted;
                m_deleted      = full;
            }
        };

        class ctrl_t
        {
        public:
            union
            {
                u64 m_qw[4];
                u32 m_dw[8];
                u8  m_b8[32];
            };

            // Writing the 8 bit hash bit by bit over 8 different sequential u32 registers then it would be
            // easy to generate the mask, like:
            void set_hash(h2_t hash, s8 const i) 
            { 
                u32 const bits[] = { 0, (1<<i) };
                u32 const mask = ~bits[1];
                for (s8 j=0; j<8; j++)
                {
                    m_dw[j] = (m_dw[j] & mask) | bits[(hash&1)];
                    hash = (hash >> 1);
                }
            }

            u32 match(h2_t hash, u32 mask) const
            {
                // If we need to match with '1's, we need to AND with 0xFFFFFFFF
                // If we need to match with '0's, we need to INVERT and AND with 0xFFFFFFFF
                // Note: We could do 4 u64 instead of 8 u32 but we need to expand the incoming
                //       mask to u64 mask64 = ((u64)mask | ((u64)mask<<32));
                //       Do not know if that is faster though, need to check with compiler explorer (https://godbolt.org/))
                static u32 const masks[] = { 0xFFFFFFFF, 0x00000000 };
                mask = mask & (masks[(hash & 1)] ^ m_dw[0]); 
                for (s8 i = 1; i < 8 && mask != 0; i++)
                {
                    hash = hash >> 1; 
                    mask = mask & (masks[(hash & 1)] ^ m_qw[1]);
                }

                // Mask will give us a '1' bit at each position where the hash was matching
                // Main advantage is that we can mask this with the group information and come
                // up with entries that need to be checked.
                // So z_mask(h2, group->GetFull()) will give you a u32 that contains bits set
                // for slots that are 'set' and have the same hash.
                return mask;
            }
        };

        template <typename T> class data_t
        {
        public:
            T m_refs[32];
        };

        // Returns a hash seed.
        //
        // The seed consists of the ctrl_ pointer, which adds enough entropy to ensure
        // non-determinism of iteration order in most cases.
        inline u64 HashSeed(const ctrl_t* ctrl)
        {
            // The low bits of the pointer have little or no entropy because of
            // alignment. We shift the pointer to try to use higher entropy bits. A
            // good number seems to be 12 bits, because that aligns with page size.
            return reinterpret_cast<uintptr_t>(ctrl) >> 12;
        }

        typedef u8 h2_t;

        inline u64  H1(u64 hash, const ctrl_t* ctrl) { return (hash >> 8) ^ HashSeed(ctrl); }
        inline h2_t H2(u64 hash) { return hash & 0xFF; }

        class probe_seq
        {
        public:
            probe_seq(u64 hash, u64 mask)
            {
                ASSERT(((mask + 1) & mask) == 0 && "not a mask");
                mask_   = mask;
                offset_ = hash & mask_;
                index_  = 0;
            }
            u64 offset() const { return offset_; }
            u64 offset(u64 i) const { return (offset_ + i) & mask_; }

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

        template <typename Key> class Fnv1aHash
        {
        public:
            inline u64 operator()(const Key& key) { return 0; }
        };

        template <typename Key, typename Value, typename Hasher = Fnv1aHash<Key>> class hashmap_t
        {
            inline probe_seq probe(u64 hash, u64 capacity) const { return probe_seq(H1(hash, ctrl_->items()), capacity); }

            array_t<group_t>*     group_;
            array_t<ctrl_t>*      ctrl_;
            array_t<data_t<u32>>* refs_;
            array_t<Key>*         keys_;
            array_t<Value>*       values_;
            u64                   capacity_;

        public:
            Value* find(const Key& key)
            {
                Hasher     hasher;
                u64 const  hash = hasher(key);
                h2_t const h    = H2(hash);
                auto       seq  = probe(hash, capacity_);
                while (true)
                {
                    group_t* g = group_->get_item(seq.offset());
                    ctrl_t*  c = ctrl_->get_item(seq.offset());
                    u32 const mask = g->GetFull();
                    for (s8 i = c->Begin(h, mask); !c->End(i); c->Next(h, i, mask))
                    {
                        if (g->IsFull(i))
                        {
                            const u32  other_ref = refs_->get_item((u32)seq.offset())->m_refs[i];
                            const Key& other_key = keys_->get_item(other_ref);
                            if (key == other_key)
                                return values_->get_item(other_ref);
                        }
                    }
                    if (g->HasEmpty())
                        return nullptr;

                    seq.next();
                    ASSERTS(seq.index() <= capacity_, "full table!");
                }
            }

            bool Insert(Key const& key, Value const& value)
            {
                Hasher     hasher;
                u64 const  hash = hasher(key);
                h2_t const h    = H2(hash);
                auto       seq  = probe(hash, capacity_);
                while (true)
                {
                    group_t* g = group_->get_item(seq.offset());
                    ctrl_t*  c = ctrl_->get_item(seq.offset());
                    u32 const mask = g->GetFull();
                    for (s8 i = c->Begin(h, mask); !c->End(i); c->Next(h, i, mask))
                    {
                        if (g->IsFull(i))
                        {
                            const u32  other_ref = refs_->get_item(seq.offset())->m_refs[i];
                            const Key& other_key = keys_->get_item(other_ref);
                            if (XCORE_PREDICT_TRUE(key == other_key))
                                return false;
                        }
                    }
                    if (XCORE_PREDICT_TRUE(g.MatchEmpty()))
                        break;
                    seq.next();
                    ASSERTS(seq.index() <= capacity_, "full table!");
                }

                auto target = find_first_non_full(ctrl_, hash, capacity_);
                if ((growth_left() == 0 && !IsDeleted(ctrl_[target.offset])))
                {
                    rehash_and_grow_if_necessary();
                    target = find_first_non_full(ctrl_, hash, capacity_);
                }
                ++size_;
                growth_left() -= IsEmpty(ctrl_[target.offset]);
                SetCtrl(target.offset, H2(hash), capacity_, ctrl_, slots_, sizeof(slot_type));
            }
        };

    } // namespace flash_hashmap_n

} // namespace xcore

#endif // __X_GENERICS_CONTAINERS_FLAT_HASH_MAP_H__