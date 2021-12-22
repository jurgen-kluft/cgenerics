#ifndef __X_GENERICS_CONTAINERS_HASHMAP_ENTRY_H__
#define __X_GENERICS_CONTAINERS_HASHMAP_ENTRY_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

namespace xcore
{
    template <typename T, typename S> struct hashmap_entry_t
    {
    };

    // layout using 2 arrays, one for the high part of hashmap_entry_t and one for the low part of hashmap_entry_t.
    //
    // 1/1, 2 bytes, max 512 elements
    // 1/2, 3 bytes, max 128 K elements
    // 2/2, 4 bytes, max  32 M elements
    // 1/4, 5 bytes, max   8 G elements
    //
    // +-------|-------|-------|-------|-------|-------|-------|-------|
    // | empty |  age  |  pos4 |  pos3 |  pos2 |  pos1 |  pos0 | value |
    // +-------|-------|-------|-------|-------|-------|-------|-------|
    //
    // pos = desired position bits
    // age = the age of this entry, to see if it is old
    // empty = this bit markes if the entry is empty
    // value = high bit of value

    // template <> struct hashmap_entry_t<s8, s8>
    // template <> struct hashmap_entry_t<s8, u16>
    // template <> struct hashmap_entry_t<s16, u16>
    // template <> struct hashmap_entry_t<s8, u32>

    // NOTE: We could make the members pointers and then we could increment/iterate the entry easily

    template <> struct hashmap_entry_t<s8, u8>
    {
        hashmap_entry_t(s8* d, u8* v)
            : m_d(d)
            , m_v(v)
        {
        }
        ~hashmap_entry_t() {}

        static hashmap_entry_t* empty_default_table()
        {
            static hashmap_entry_t result[cmin_lookups] = {{}, {}, {}, {cspecial_end_value}};
            return result;
        }

        bool is_empty() const { return m_d < 0; }
        void set_empty()
        {
            m_d = -1;
            m_v = -1;
        }
        bool is_old() const
        {
            u8 age = m_d & 0x40;
            return age == 0x4;
        }
        void set_old() { m_d = (m_d & 0xBF) | 0x4; }

        bool has_value() const { return m_d >= 0; }
        u32  value() const { return (u32)((m_d & 1) << 8) | (u32)m_v; }
        u32  value(u32 value)
        {
            u32 old_v = ((u32)(m_d & 1) << 8) | (u32)m_v;
            m_v       = (value & 0xff);
            m_d       = ((value >> 8) & 1) | (m_d & 0xFE);
            return old_v;
        }

        bool is_at_desired_position() const { return m_d <= 0; }
        u8   distance_from_desired() const { return (m_d & 0x3e) >> 1; }
        u8   distance_from_desired(u8 desired)
        {
            u8 old_d = (m_d >> 1) & 0x1F;
            m_d      = ((desired << 1) & 0x3E) | (m_d & 0xC1);
            return old_d;
        }

        void emplace(s8 distance, u32 value)
        {
            m_v = value & 0xff;
            m_d = (distance << 1);
            m_d = ((value >> 8) & 0x1) | (m_d & 0xFE);
        }

        u32 displace(s8 distance, u32 value)
        {
            u8 const old_v = get_value();
            m_v            = (u8)value;
            m_d            = ((value >> 8) & 0x1) | (m_d & 0xFE);
            return old_v;
        }

        void destroy_value()
        {
            m_d = -1;
            m_v = -1;
        }

        static constexpr s8 cspecial_end_value = 0;

    private:
        s8& m_d;
        u8& m_v;
    };

    template <> struct hashmap_entry_t<s8, u16>
    {
        hashmap_entry_t(s8* d, u16* v)
            : m_d(d)
            , m_v(v)
        {
        }
        ~hashmap_entry_t() {}

        static hashmap_entry_t* empty_default_table()
        {
            static hashmap_entry_t result[cmin_lookups] = {{}, {}, {}, {cspecial_end_value}};
            return result;
        }

        bool is_empty() const { return m_d < 0; }
        void set_empty() { m_v = -1; }
        bool is_old() const
        {
            u8 age = m_d & 0x40;
            return age == 0x4;
        }
        void set_old() { m_d = (m_d & 0xBF) | 0x4; }

        bool has_value() const { return m_d >= 0; }
        u32  value() const { return m_v & 0xffff; }
        u32  value(u32 value)
        {
            u32 old_v = m_v & 0xffff;
            m_v       = (value & 0xffff);
            return old_v;
        }

        bool is_at_desired_position() const { return m_d <= 0; }
        u8   distance_from_desired() const { return m_d & 0x1f; }
        u8   distance_from_desired(u8 desired)
        {
            u8 old_d = m_d;
            m_d      = desired;
            return old_d;
        }

        void emplace(s8 distance, u32 value)
        {
            m_v = value & 0xffff;
            m_d = distance;
        }

        u32 displace(s8 distance, u32 value)
        {
            u8 const old_v = m_v & 0xffff;
            m_v            = value;
            m_d            = distance;
            return old_v;
        }

        void destroy_value()
        {
            m_d = -1;
            m_v = -1;
        }

        static constexpr s8 cspecial_end_value = 0;

    private:
        s8&  m_d;
        u16& m_v;
    };

    template <> struct hashmap_entry_t<s16, u16>
    {
        hashmap_entry_t(s16* d, u16* v)
            : m_d(((s8*)d)[0])
            , m_vh(((s8*)d)[1])
            , m_vl(*v)
        {
        }
        ~hashmap_entry_t() {}

        static hashmap_entry_t* empty_default_table()
        {
            static hashmap_entry_t result[cmin_lookups] = {{}, {}, {}, {cspecial_end_value}};
            return result;
        }

        bool is_empty() const { return m_d < 0; }
        void set_empty()
        {
            m_d  = -1;
            m_vh = -1;
            m_vl = -1;
        }
        bool is_old() const
        {
            u8 age = m_d & 0x40;
            return age == 0x4;
        }
        void set_old() { m_d = (m_d & 0xBF) | 0x4; }

        bool has_value() const { return m_d >= 0; }
        u32  value() const { return ((u32)m_vh << 16) | ((u32)m_vl & 0xffff); }
        u32  value(u32 value)
        {
            u32 old_v = m_v & 0xffffff;
            m_vh      = value >> 16;
            m_vl      = value & 0xffff;
            return old_v;
        }

        bool is_at_desired_position() const { return m_d <= 0; }
        u8   distance_from_desired() const { return m_d & 0x1f; }
        u8   distance_from_desired(u8 desired)
        {
            u8 old_d = m_d;
            m_d      = desired;
            return old_d;
        }

        void emplace(s8 distance, u32 value)
        {
            m_vl = value & 0xffff;
            m_vh = (value >> 16);
            m_d  = distance;
        }

        u32 displace(s8 distance, u32 value)
        {
            u8 const old_v = m_v & 0xffffff;
            m_vh           = value >> 16;
            m_vl           = value & 0xffff;
            m_d            = distance;
            return old_v;
        }

        void destroy_value()
        {
            m_d = -1;
            m_v = -1;
        }

        static constexpr s16 cspecial_end_value = 0;

    private:
        s8&  m_d;
        s8&  m_vh;
        u16& m_vl;
    };

    template <> struct hashmap_entry_t<s8, u32>
    {
        hashmap_entry_t(s8* d, u32* v)
            : m_d(*d)
            , m_v(*v)
        {
        }
        ~hashmap_entry_t() {}

        static hashmap_entry_t* empty_default_table()
        {
            static hashmap_entry_t result[cmin_lookups] = {{}, {}, {}, {cspecial_end_value}};
            return result;
        }

        bool is_empty() const { return m_d < 0; }
        void set_empty()
        {
            m_d = -1;
            m_v = -1;
        }
        bool is_old() const
        {
            u8 age = m_d & 0x40;
            return age == 0x4;
        }
        void set_old() { m_d = (m_d & 0xBF) | 0x4; }

        bool has_value() const { return m_d >= 0; }
        u64  value() const { return (u32)m_v; }
        u64  value(u64 value)
        {
            u64 old_v = m_v;
            m_v       = (u32)value;
            return old_v;
        }

        bool is_at_desired_position() const { return m_d <= 0; }
        u8   distance_from_desired() const { return m_d & 0x1f; }
        u8   distance_from_desired(u8 desired)
        {
            u8 old_d = m_d;
            m_d      = desired;
            return old_d;
        }

        void emplace(s8 distance, u64 value)
        {
            m_v = (u32)(value);
            m_d = distance;
        }

        u64 displace(s8 distance, u64 value)
        {
            u64 const old_v = m_v;
            m_v             = (u32)(value);
            m_d             = distance;
            return old_v;
        }

        void destroy_value()
        {
            m_d = -1;
            m_v = -1;
        }

        static constexpr s16 cspecial_end_value = 0;

    private:
        s8&  m_d;
        u32& m_v;
    };

} // namespace xcore

#endif // __X_GENERICS_CONTAINERS_HASHMAP_ENTRY_H__