#ifndef __X_GENERICS_CONTAINERS_MAP_H__
#define __X_GENERICS_CONTAINERS_MAP_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

#include "xbase/x_allocator.h"
#include "xbase/x_binary_search.h"
#include "xbase/x_darray.h"
#include "xbase/x_hash.h"
#include "xbase/x_math.h"
#include "xbase/x_memory.h"

#include "xgenerics/x_hash_map_entry.h"

namespace xcore
{
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

    inline u32 FNV1A_32(u64 hash64)
    {
        u32 hash32 = (u32)(hash64 ^ (hash64 >> 32));
        return hash32 ^ (hash32 >> 16);
    }

    struct fibonacci_hash_policy
    {
        static constexpr u8 cnumbits = 64;

        fibonacci_hash_policy()
            : m_shift(cnumbits - 1)
        {
        }

        u32  index_for_hash(u64 hash, u32 /*m_num_slots_minus_one*/) const { return (u32)((11400714819323198485ull * hash) >> m_shift); }
        u32  keep_in_range(u32 index, u32 m_num_slots_minus_one) const { return index & m_num_slots_minus_one; }
        void commit(u32 shift) { m_shift = shift; }
        void reset() { m_shift = cnumbits - 1; }

        u32 next_size(u32& current_size) const
        {
            current_size = math::maximum(u32(1), math::next_power_of_two(current_size));
            current_size = current_size << 1;
            return (u32)cnumbits - math::log2(current_size);
        }

    private:
        u32 m_shift;
    };

    struct prime_number_hash_policy
    {
        prime_number_hash_policy();

        static s32 compare_size_fn(const void* inItem, const void* inData, s32 inIndex)
        {
            u64  value  = *(u64*)inItem;
            u64* values = (u64*)inData;
            if (value < values[inIndex])
                return -1;
            if (value > values[inIndex])
                return -1;
            return 0;
        }

        u32 next_size(u32& current_size) const
        {
            u32 index    = x_LowerBound(&current_size, s_prime_array, s_prime_array_len * sizeof(u64), compare_size_fn);
            current_size = (u32)s_prime_array[index + 1];
            return current_size;
        }
        void commit(u32 new_size) { m_current_prime_size = new_size; }
        void reset() { m_current_prime_size = 0; }

        u32 index_for_hash(u64 hash, u32 /*num_slots_minus_one*/) const { return (hash % m_current_prime_size); }
        u32 keep_in_range(u32 index, u32 num_slots_minus_one) const { return index > num_slots_minus_one ? (index % m_current_prime_size) : index; }

    private:
        static u32 const s_prime_array[];
        static s32 const s_prime_array_len;
        u32              m_current_prime_size;
    };

    template <typename Key, typename Value, typename HashPolicy = fibonacci_hash_policy, typename EntryHT = s8, typename EntryLT = u16> class hashmap_t
    {
        using hash_policy = HashPolicy;
        using entry_type  = hashmap_entry_t<EntryHT, EntryLT>;

        static constexpr s8 cmin_lookups = 4;
        static constexpr s8 cspecial_end_value = 0;
        static s8           compute_max_lookups(u64 new_num_elements)
        {
            s8 desired = math::log2(new_num_elements);
            return math::maximum(cmin_lookups, desired);
        }

        inline entry_type get_entry(u64 index) { return entry_type(m_entry_ht->get_item(index), m_entry_lt->get_item(index)); }


        void reset_to_empty_state()
        {

        }

        array_t<Key>*     m_keys;
        array_t<Value>*   m_values;
        array_t<EntryHT>* m_entry_ht;
        array_t<EntryLT>* m_entry_lt;
        u32               m_num_elements;
        u32               m_num_slots_minus_one;
        s8                m_max_lookups = cmin_lookups - 1;
        u32               m_max_load_factor; // ~980 == 98%
        hash_policy       m_hash_policy;

    public:
        hashmap_t(array_t<Key>* keys, array_t<Value>* values, array_t<EntryHT>* entry_ht, array_t<EntryLT>* entry_lt)
        {
            m_keys                = keys;
            m_values              = values;
            m_entry_ht            = entry_ht;
            m_entry_lt            = entry_lt;
            m_num_elements        = 0;
            m_num_slots_minus_one = 0;
            m_max_load_factor     = 750;
        }

        inline Value& operator[](const Key& key)
        {
            u32 const   slot = compute_slot_for_key(key);
            bool        newlyadded;
            entry_type* entry = emplace(-1, &key, slot, newlyadded);
            return m_values + entry->value();
        }

        inline Value* operator[](const Key& key) const
        {
            Value* found = this->find(key);
            return found;
        }

        const Value& at(const Key& key) const
        {
            Value* found = this->find(key);
            return found;
        }

        inline bool compares_equal(const Key& a, const Key& b) const { return a == b; }

        bool insert(u32 item_index)
        {
            Key*      key         = m_keys->get_item(item_index);
            u32 const slot        = m_hash_policy.index_for_hash(hash_object(*key), m_num_slots_minus_one);
            bool      newly_added = false;
            entry_type entry = emplace(item_index, key, slot, newly_added);
            return entry.is_valid();
        }

        Value* find(const Key& key)
        {
            u32 const   slot = m_hash_policy.index_for_hash(hash_object(key), m_num_slots_minus_one);
            entry_type it   = get_entry(slot);
            for (s8 distance = 0; it.distance_from_desired() >= distance; ++distance, it.next())
            {
                if (compares_equal(key, *m_keys->get_item(it.value())))
                    return m_values->get_item(it.value());
            }
            return nullptr;
        }

        u32 size() const { return m_num_elements; }
        u32 max_size() const { return m_entry_ht->get_cap_max(); }
        u32 max_slots() const { return m_num_slots_minus_one ? m_num_slots_minus_one + 1 : 0; }
        u32 compute_slot_for_key(const Key& key) const { return m_hash_policy.index_for_hash(hash_object(key), m_num_slots_minus_one); }
        s32 load_factor() const
        {
            u32 buckets = max_slots();
            if (buckets)
                return (m_num_elements << 10) / max_slots() / 10;
            else
                return 0;
        }
        void max_load_factor(s8 percentage) { m_max_load_factor = percentage * 10; }
        s32  max_load_factor() const { return m_max_load_factor / 10; }

        template <typename U> u64 hash_object(const U& key) const { return FNV1A((const xbyte*)&key, (s32)sizeof(U), 998104665); }

        entry_type emplace(s32 item_index, Key* item_key, u32 slot, bool& out_newlyadded)
        {
            entry_type current_entry         = get_entry(slot);
            s8         distance_from_desired = 0;
            for (; current_entry.distance_from_desired() >= distance_from_desired; current_entry.next(), ++distance_from_desired)
            {
                if (compares_equal(*item_key, *m_keys->get_item(current_entry.value())))
                {
                    out_newlyadded = false;
                    return current_entry;
                }
            }

            // if we need to add a key to the keys darray we also need to add
            // a value to the value darray.

            out_newlyadded = true;
            return emplace_new_key(distance_from_desired, current_entry, item_index, item_key, slot, out_newlyadded);
        }

        entry_type emplace_new_key(s8 distance_from_desired, entry_type current_entry, u32 item_index, Key* item_key, u32 slot, bool& out_newlyadded)
        {
            if (m_num_slots_minus_one == 0 || distance_from_desired == m_max_lookups || ((m_num_elements + 1) > (((m_num_slots_minus_one + 1) << 10) / m_max_load_factor)))
            {
                grow();
                return emplace(item_index, item_key, slot, out_newlyadded);
            }
            else if (current_entry.is_empty())
            {
                current_entry.emplace(distance_from_desired, slot);
                ++m_num_elements;
                out_newlyadded = true;
                return current_entry;
            }

            u32 to_insert = slot;

            distance_from_desired = current_entry.distance_from_desired(distance_from_desired);
            to_insert             = current_entry.value(to_insert);

            entry_type result = current_entry;
            for (++distance_from_desired, current_entry.next();; current_entry.next())
            {
                if (current_entry.is_empty())
                {
                    current_entry.emplace(distance_from_desired, to_insert);
                    ++m_num_elements;
                    out_newlyadded = true;
                    return result;
                }
                else if (current_entry.distance_from_desired() < distance_from_desired)
                {
                    distance_from_desired = current_entry.distance_from_desired(distance_from_desired);
                    to_insert             = current_entry.value(to_insert);
                    ++distance_from_desired;
                }
                else
                {
                    ++distance_from_desired;
                    if (distance_from_desired == m_max_lookups)
                    {
                        to_insert = result.value(to_insert);
                        grow();
                        return emplace(item_index, item_key, to_insert, out_newlyadded);
                    }
                }
            }
        }

        void grow()
        {
            u32 new_max_slots;
            do
            {
                new_max_slots  = max_slots();
                auto to_commit = m_hash_policy.next_size(new_max_slots);
                if (new_max_slots == max_slots())
                    return;
                m_hash_policy.commit(to_commit);

                s8 new_max_lookups = compute_max_lookups(new_max_slots);
                m_entry_lt->set_capacity(new_max_slots + new_max_lookups);
                m_entry_ht->set_capacity(new_max_slots + new_max_lookups);
            } while (!rehash(new_max_slots));
        }

        void shrink()
        {
            do
            {
                u32  new_max_slots   = max_slots();
                auto new_prime_index = m_hash_policy.prev_size(new_max_slots);
                if (new_max_slots == 0)
                {
                    m_hash_policy.commit(new_prime_index);
                    reset_to_empty_state();
                    return;
                }

                if (rehash(new_max_slots))
                {
                    s8 new_max_lookups = compute_max_lookups(new_max_slots);
                    m_entry_ht->set_capacity(new_max_slots + new_max_lookups);
                    m_entry_lt->set_capacity(new_max_slots + new_max_lookups);
                    break;
                }
                else
                {
                    // Seems we cannot shrink, stay at the current size.
                    // However we do need to rehash to rectify our failed rehash
                    rehash(max_slots());
                    break;
                }
            } while (true);
        }

        bool rehash(u32 new_num_slots)
        {
            s8 new_max_lookups = compute_max_lookups(new_num_slots);

            // in-place rehashing
            entry_type it     = get_entry(0);
            entry_type it_end = get_entry(max_slots());
            while (it < it_end)
            {
                if (!it.is_empty())
                    it.set_old();
                it.next();
            }
            entry_type special_end_item = get_entry(new_num_slots + new_max_lookups - 1);
            while (it <= special_end_item)
            {
                it.set_empty();
                it.next();
            }
            special_end_item.distance_from_desired(cspecial_end_value);

            u32 const old_num_slots_minus_one = m_num_slots_minus_one;
            m_num_slots_minus_one             = new_num_slots - 1;

            s8 const old_max_lookups   = m_max_lookups;
            m_max_lookups              = new_max_lookups;
            u32 const old_num_elements = m_num_elements;
            m_num_elements             = 0;
            u32 end                    = (old_num_slots_minus_one + old_max_lookups);
            for (u32 i = 0; i < end; i++)
            {
                entry_type it = get_entry(i);
                if (it.is_old() && it.has_value())
                {
                    // to be re-hashed and re-inserted
                    do
                    {
                        Key*      key         = m_keys->get_item(it.value());
                        u32 const slot        = compute_slot_for_key(*key);
                        bool      should_grow = false;
                        entry_type  prev        = displace(it.value(), slot, should_grow);
                        if (should_grow)
                        {
                            // ouch, we reached a situation where we did not resize enough.
                            // We still have a key slot that reaches 'm_max_lookups', better
                            // resize again to get out of this situation.
                            m_num_elements        = old_num_elements;
                            m_num_slots_minus_one = old_num_slots_minus_one;
                            m_max_lookups         = old_max_lookups;
                            return false;
                        }
                        it = prev;
                    } while (it.is_valid());
                }
            }
            ASSERT(m_num_elements == old_num_elements);
            return true;
        }

        entry_type displace(s32 item_index, u32 slot, bool& grow)
        {
            s8 distance_from_desired = 0;
            grow                     = false;

            entry_type current_entry = get_entry(slot);
            if (current_entry.is_old())
            {
                current_entry.emplace(distance_from_desired, item_index);
                ++m_num_elements;
                return current_entry;
            }

            for (; current_entry.distance_from_desired() >= distance_from_desired; current_entry.next(), ++distance_from_desired)
            {
                if (current_entry.is_old())
                {
                    return current_entry;
                }
            }

            return displace_new_key(distance_from_desired, current_entry, item_index, grow);
        }

        entry_type displace_new_key(s8 distance_from_desired, entry_type current_entry, u32 item_index, bool& grow)
        {
            if (m_num_slots_minus_one == 0 || distance_from_desired == m_max_lookups || ((m_num_elements + 1) > (((m_num_slots_minus_one + 1) << 10) / m_max_load_factor)))
            {
                // we need to indicate that we haven't resized enough, we still detected an entry with maximum lookups
                grow = true;
                return entry_type();
            }
            else if (current_entry.is_empty())
            {
                current_entry.emplace(distance_from_desired, item_index);
                ++m_num_elements;
                return current_entry;
            }
            u32 to_insert = item_index;

            distance_from_desired = current_entry.distance_from_desired(distance_from_desired);
            to_insert             = current_entry.value(to_insert);

            for (++distance_from_desired, current_entry.next();; current_entry.next())
            {
                if (current_entry.is_empty())
                {
                    current_entry.displace(distance_from_desired, to_insert);
                    ++m_num_elements;
                    return current_entry;
                }
                else if (current_entry.is_old())
                {
                    current_entry.displace(distance_from_desired, to_insert);
                    ++m_num_elements;
                    return current_entry;
                }
                else if (current_entry.distance_from_desired() < distance_from_desired)
                {
                    distance_from_desired = current_entry.distance_from_desired(distance_from_desired);
                    to_insert             = current_entry.value(to_insert);
                    ++distance_from_desired;
                }
                else
                {
                    ++distance_from_desired;
                    if (distance_from_desired == m_max_lookups)
                    {
                        // we need to indicate that we haven't resized enough, we still detected an entry with maximum lookups
                        grow = true;
                        return entry_type();
                    }
                }
            }

            return entry_type();
        }
    };
} // namespace xcore

#endif // __X_GENERICS_CONTAINERS_MAP_H__