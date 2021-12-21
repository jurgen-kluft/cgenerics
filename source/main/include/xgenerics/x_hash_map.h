#ifndef __X_GENERICS_CONTAINERS_MAP_H__
#define __X_GENERICS_CONTAINERS_MAP_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

#include "xbase/x_allocator.h"
#include "xbase/x_darray.h"
#include "xbase/x_hash.h"
#include "xbase/x_math.h"

/*
We want a hash map that doesn't have to care about the types of the key and value. It only should be able to
compute the hash of the key and compare a value with another value.
The hash map should only store information related to its purpose which is to quickly check if a key is contained
in the map and to retrieve the value belonging to a key.
Also a reallocation (like in-place growing or when using virtual memory darray's) should be able to work.

s32* keys = darray_create<s32>(0, 4096, 16*65536);
s32* values = darray_create<s32>(0, 4096, 16*65536);
u32* meta = darray_create<u32>(0, 4096, 16*65536);     // top 5 bits are used for the ll, 27 bits are for the number of items (2^27 = 128 Million items )

map = hashmap_t<s32,s32>(keys, values, meta);
map = hashmap_t<s32,s32>(); // create the keys, values and meta darray's ourselves
map.inserti_range(0, 4096); // // index based range insertion, key and value index are identical

map.insertv(32, 100);  // value based insertion key/value, if key/value is not found it will be added to the backed storage
map.inserti(0); // index based insertion, key/value index, actual key/value are already in the darray storage

map.findv(32);
map.removev(32);
map.removei(0);

// So we should use open addressing with the robin-hood insertion policy as well as the fibonacci hash policy.
// Like: https://github.dev/skarupke/flat_hash_map/blob/master/flat_hash_map.hpp

NOTE: It seems that even if the hashmap is resized and as long as we do not do a lookup we could just continue
      inserting and reaching 'old' items should trigger a rehash only for those items that we encounter when
      re-inserting.
      This could be a nice speedup when inserting. Once we do a lookup and we still have 'old' items we should
      rehash only those.
*/

namespace xcore
{
    struct fibonacci_hash_policy
    {
        static constexpr s8 cnumbits = 64;
        fibonacci_hash_policy()
            : m_shift(cnumbits - 1)
        {
        }

        u64  index_for_hash(u64 hash, u64 /*m_num_slots_minus_one*/) const { return (11400714819323198485ull * hash) >> m_shift; }
        u64  keep_in_range(u64 index, u64 m_num_slots_minus_one) const { return index & m_num_slots_minus_one; }
        void commit(s8 shift) { m_shift = shift; }
        void reset() { m_shift = cnumbits - 1; }

        s8 next_size_over(u64& size) const
        {
            size = math::maximum(u64(2), math::next_power_of_two(size));
            return cnumbits - math::log2(size);
        }

    private:
        s8 m_shift;
    };

    template <typename Key, typename Value, > class hashmap_t
    {
        using hash_policy = fibonacci_hash_policy;

        static constexpr s8 cmin_lookups = 4;
        static s8           compute_max_lookups(u32 new_num_elements)
        {
            s8 desired = math::log2(new_num_elements);
            return math::maximum(cmin_lookups, desired);
        }

        // layout:
        // +-------|-------|-------|-------|-------|-------|-------|-------|
        // | empty | age1  | age0  |  pos4 |  pos3 |  pos2 |  pos1 |  pos0 |
        // +-------|-------|-------|-------|-------|-------|-------|-------|
        // pos = desired position bits
        // age = the age of this entry, to see if it is old
        // empty = this bit markes if the entry is empty
        template <typename T> struct entry_t
        {
            entry_t()
                : m_distance_from_desired(-1)
            {
            }
            entry_t(s8 distance_from_desired)
                : m_distance_from_desired(distance_from_desired)
            {
            }
            ~entry_t() {}

            static entry_t* empty_default_table()
            {
                static entry_t result[cmin_lookups] = {{}, {}, {}, {cspecial_end_value}};
                return result;
            }

            bool is_empty() const { return m_distance_from_desired < 0; }
            bool is_old() const
            {
                u8 age = m_distance_from_desired & 0x70;
                return age == 0x6;
            }
            void set_old() { m_distance_from_desired = (m_distance_from_desired & 0x9F) | 0x6; }

            bool has_value() const { return m_distance_from_desired >= 0; }
            u32  value() const { return m_value & 0x00ffffff; }
            u32  value(u32 value)
            {
                u32 old_value = m_value & 0x00ffffff;
                m_value       = (m_value & 0xff000000) | (value & 0x00ffffff);
                return old_value;
            }

            bool is_at_desired_position() const { return m_distance_from_desired <= 0; }
            u8   desired_position() const { return m_distance_from_desired & 0x1f; }
            u8   desired_position(u8 desired)
            {
                u8 old_distance         = m_distance_from_desired;
                m_distance_from_desired = desired;
                return old_distance;
            }

            void emplace(s8 distance, T value)
            {
                m_value                 = value;
                m_distance_from_desired = distance;
            }

            u32 displace(s8 distance, T value)
            {
                u8 const old_value      = m_value & 0x00ffffff;
                m_value                 = value;
                m_distance_from_desired = distance;
                return old_value;
            }

            void destroy_value() { m_value = -1; }

            static constexpr s8 cspecial_end_value = 0;

        private:
            union
            {
                s8 m_distance_from_desired;
                T  m_value;
            };
        };

        Key*        m_keys;
        Value*      m_values;
        entry_t*    m_meta;
        u64         m_num_elements;
        u64         m_num_slots_minus_one;
        s8          m_max_lookups = cmin_lookups - 1;
        u64         m_max_load_factor; // ~980 == 98%
        hash_policy m_hash_policy;

    public:
        hashmap_t(Key* keys, Value* values) {}

        inline Value& operator[](const Key& key)
        {
            u32 const slot = bucket(key);
            bool      newlyadded;
            entry_t*  entry = emplace(-1, &key, slot, newlyadded);
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

        u32 size() const { return m_num_elements; }
        u32 max_size() const { return get_cap_max(m_value); }
        u32 bucket_count() const { return m_num_slots_minus_one ? m_num_slots_minus_one + 1 : 0; }
        u32 max_bucket_count() const { return (get_cap_max(m_value) - cmin_lookups); }
        u32 bucket(const Key& key) const { return hash_policy.index_for_hash(hash_object(key), m_num_slots_minus_one); }
        s32 load_factor() const
        {
            u32 buckets = bucket_count();
            if (buckets)
                return (m_num_elements << 10) / bucket_count() / 10;
            else
                return 0;
        }
        void max_load_factor(s8 percentage) { m_max_load_factor = percentage * 10; }
        s32  max_load_factor() const { return m_max_load_factor / 10; }

        entry_t* emplace(s32 item_index, Key* item_key, u32 slot, bool& out_newlyadded)
        {
            entry_t* current_entry         = m_meta + slot;
            s8       distance_from_desired = 0;
            for (; current_entry->distance_from_desired >= distance_from_desired; ++current_entry, ++distance_from_desired)
            {
                if (compares_equal(*item_key, m_keys + current_entry->value()))
                {
                    out_newlyadded = false;
                    return current_entry;
                }
            }
            u32 item_index = 0;

            // if we need to add a key to the keys darray we also need to add
            // a value to the value darray.

            out_newlyadded = true;
            return emplace_new_key(distance_from_desired, current_entry, item_index, out_newlyadded);
        }

        entry_t* emplace_new_key(s8 distance_from_desired, entry_t* current_entry, u32 item_index, bool& out_newlyadded)
        {
            if (m_num_slots_minus_one == 0 || distance_from_desired == m_max_lookups || ((m_num_elements + 1) > (((m_num_slots_minus_one + 1) << 10) / m_max_load_factor)))
            {
                grow();
                return emplace(item_index, out_newlyadded);
            }
            else if (current_entry->is_empty())
            {
                current_entry->emplace(distance_from_desired, item_index);
                ++num_elements;
                out_newlyadded = true;
                return current_entry;
            }
            u32 to_insert = item_index;

            distance_from_desired = current_entry->distance_from_desired(distance_from_desired);
            to_insert             = current_entry->value(to_insert);

            entry_t* result = current_entry;
            for (++distance_from_desired, ++current_entry;; ++current_entry)
            {
                if (current_entry->is_empty())
                {
                    current_entry->emplace(distance_from_desired, to_insert);
                    ++num_elements;
                    out_newlyadded = true;
                    return result;
                }
                else if (current_entry->distance_from_desired < distance_from_desired)
                {
                    distance_from_desired = current_entry->distance_from_desired(distance_from_desired);
                    to_insert             = current_entry->value(to_insert);
                    ++distance_from_desired;
                }
                else
                {
                    ++distance_from_desired;
                    if (distance_from_desired == max_lookups)
                    {
                        to_insert = result->value(to_insert);
                        grow();
                        return emplace(to_insert, out_newlyadded);
                    }
                }
            }
        }

        void rehash(u32 new_num_elements)
        {
            new_num_elements = math::maximum(new_num_elements, ((m_num_elements << 10) / m_max_load_factor));
            if (new_num_elements == 0)
            {
                reset_to_empty_state();
                return;
            }

            do
            {
                auto new_prime_index = hash_policy.next_size_over(new_num_elements);
                if (new_num_elements == bucket_count())
                    return;
                s8 new_max_lookups = compute_max_lookups(new_num_elements);

                // re-allocate (might not do anything except commit virtual memory pages)
                set_cap<entry_t>(m_value, new_num_elements + new_max_lookups);

                // in-place rehashing
                entry_t* it     = m_meta;
                entry_t* it_end = m_meta + m_num_elements;
                while (it < it_end)
                {
                    it->set_old();
                    ++it;
                }
                it_end = m_meta + new_num_elements while (it < it_end)
                {
                    it->set_empty();
                    ++it;
                }

                entry_t* special_end_item = m_meta + (num_buckets + new_max_lookups);
                special_end_item->set_distance_from_desired(entry_t::cspecial_end_value);

                x_swap(m_num_slots_minus_one, new_num_elements);
                --m_num_slots_minus_one;

                m_hash_policy.commit(new_prime_index);

                s8 old_max_lookups = max_lookups;
                max_lookups        = new_max_lookups;
                m_num_elements     = 0;
                u64 end            = (new_num_elements + old_max_lookups);
                for (u64 i = 0; i < end; i++)
                {
                    entry_t* it = m_meta + i;
                    if (it->is_old() && it->has_value())
                    {
                        // to be re-hashed and re-inserted
                        do
                        {
                            Key*      prev_key  = m_keys + it->value();
                            u32 const prev_slot = bucket(prev_key);
                            entry_t*  prev      = displace(it->value(), prev_slot, grow);
                            if (grow)
                            {
                                // ouch, we reached a situation where we did not resize enough.
                                // We still have a key slot that reaches m_max_lookups.
                                continue;
                            }
                            it = prev;
                        } while (it != nullptr);
                    }
                }
            } while (true);
        }

        entry_t* displace(s32 item_index, u32 slot, bool& grow)
        {
            s8 distance_from_desired = 0;
            grow = false;

            entry_t* current_entry = m_meta + slot;
            if (current_entry->is_old())
            {
                current_entry->emplace(distance_from_desired, item_index);
                ++num_elements;
                return current_entry;
            }

            for (; current_entry->distance_from_desired >= distance_from_desired; ++current_entry, ++distance_from_desired)
            {
                if (current_entry->is_old())
                {
                    return current_entry;
                }
            }

            return displace_new_key(distance_from_desired, current_entry, item_index, grow);
        }

        entry_t* displace_new_key(s8 distance_from_desired, entry_t* current_entry, u32 item_index, bool& grow)
        {
            if (m_num_slots_minus_one == 0 || distance_from_desired == m_max_lookups || ((m_num_elements + 1) > (((m_num_slots_minus_one + 1) << 10) / m_max_load_factor)))
            {
                // we need to indicate that we haven't resized enough, we still detected an entry with maximum lookups
                grow = true;
                return nullptr;
            }
            else if (current_entry->is_empty())
            {
                current_entry->emplace(distance_from_desired, item_index);
                ++num_elements;
                return current_entry;
            }
            u32 to_insert = item_index;

            distance_from_desired = current_entry->distance_from_desired(distance_from_desired);
            to_insert             = current_entry->value(to_insert);

            for (++distance_from_desired, ++current_entry;; ++current_entry)
            {
                if (current_entry->is_empty())
                {
                    u32 const prev_item_index = current_entry->displace(distance_from_desired, to_insert);
                    ++num_elements;
                    return m_meta + prev_item_index;
                }
                else if (current_entry->is_old())
                {
                    u32 const prev_item_index = current_entry->displace(distance_from_desired, to_insert);
                    ++num_elements;
                    return m_meta + prev_item_index;
                }
                else if (current_entry->distance_from_desired < distance_from_desired)
                {
                    distance_from_desired = current_entry->distance_from_desired(distance_from_desired);
                    to_insert             = current_entry->value(to_insert);
                    ++distance_from_desired;
                }
                else
                {
                    ++distance_from_desired;
                    if (distance_from_desired == max_lookups)
                    {
                        // we need to indicate that we haven't resized enough, we still detected an entry with maximum lookups
                        grow = true;
                        return nullptr;
                    }
                }
            }

            return nullptr;
        }
    };
} // namespace xcore

#endif // __X_GENERICS_CONTAINERS_MAP_H__