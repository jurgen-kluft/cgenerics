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
    //    1/1, 2 bytes, max 512 elements      -->   struct hashmap_entry_t<s8, s8>
    //    1/2, 3 bytes, max 128 K elements    -->   struct hashmap_entry_t<s8, u16>
    //    2/2, 4 bytes, max  32 M elements    -->   struct hashmap_entry_t<s16, u16>
    //    1/4, 5 bytes, max   8 G elements    -->   struct hashmap_entry_t<s8, u32>
    //
    // +-------|-------|-------|-------|-------|-------|-------|-------|
    // | empty |  age  |  pos4 |  pos3 |  pos2 |  pos1 |  pos0 | value |
    // +-------|-------|-------|-------|-------|-------|-------|-------|
    //
    // pos = desired position bits
    // age = the age of this entry, to see if it is old
    // empty = this bit markes if the entry is empty
    // value = high bit of value

    template <> struct hashmap_entry_t<s8, u8>
    {
        hashmap_entry_t()
            : m_d(nullptr)
            , m_v(nullptr)
        {
        }
        hashmap_entry_t(s8* d, u8* v)
            : m_d(d)
            , m_v(v)
        {
        }

        void next()
        {
            m_d++;
            m_v++;
        }

        inline bool is_valid() const { return m_d != nullptr; }
        inline bool is_empty() const { return *m_d < 0; }
        void        set_empty()
        {
            *m_d = -1;
            *m_v = -1;
        }
        inline bool is_old() const { return (*m_d & 0x40) == 0x4; }
        inline void set_old() { *m_d = (*m_d & 0xBF) | 0x4; }
        inline bool has_value() const { return *m_d >= 0; }
        inline u32  value() const { return (u32)((*m_d & 1) << 8) | (u32)*m_v; }
        u32         value(u32 v)
        {
            u32 const old_v = value();
            *m_v            = (u8)(v);
            *m_d            = ((v >> 8) & 1) | (*m_d & 0xFE);
            return old_v;
        }

        inline bool is_at_desired_position() const { return *m_d <= 0; }
        inline u8   distance_from_desired() const { return (*m_d & 0x3e) >> 1; }
        u8          distance_from_desired(u8 d)
        {
            u8 const old_d = distance_from_desired();
            *m_d           = ((d << 1) & 0x3E) | (*m_d & 0xC1);
            return old_d;
        }

        void emplace(s8 d, u32 v)
        {
            *m_v = v & 0xff;
            *m_d = (d << 1);
            *m_d = ((v >> 8) & 0x1) | (*m_d & 0xFE);
        }

        u32 displace(s8 d, u32 v)
        {
            u32 const old_v = value();
            *m_v            = (u8)v;
            *m_d            = ((v >> 8) & 0x1) | ((d << 1) & 0x3E);
            return old_v;
        }

        void destroy_value()
        {
            *m_d = -1;
            *m_v = -1;
        }

        inline bool operator < (const hashmap_entry_t& rhs) const { return (m_v < rhs.m_v); }
        inline bool operator <= (const hashmap_entry_t& rhs) const { return (m_v <= rhs.m_v); }

    private:
        s8* m_d;
        u8* m_v;
    };

    template <> struct hashmap_entry_t<s8, u16>
    {
        hashmap_entry_t()
            : m_d(nullptr)
            , m_v(nullptr)
        {
        }
        hashmap_entry_t(s8* d, u16* v)
            : m_d(d)
            , m_v(v)
        {
        }

        void next()
        {
            m_d++;
            m_v++;
        }

        inline bool is_valid() const { return m_d != nullptr; }
        inline bool is_empty() const { return *m_d < 0; }
        void        set_empty()
        {
            *m_d = -1;
            *m_v = -1;
        }
        inline bool is_old() const { return (*m_d & 0x40) == 0x4; }
        inline void set_old() { *m_d = (*m_d & 0xBF) | 0x4; }
        inline bool has_value() const { return *m_d >= 0; }
        inline u32  value() const { return (u32)((*m_d & 1) << 16) | (u32)*m_v; }
        u32         value(u32 v)
        {
            u32 const old_v = value();
            *m_v            = (u16)(v);
            *m_d            = ((v >> 16) & 1) | (*m_d & 0xFE);
            return old_v;
        }

        inline bool is_at_desired_position() const { return *m_d <= 0; }
        inline u8   distance_from_desired() const { return (*m_d & 0x3e) >> 1; }
        u8          distance_from_desired(u8 d)
        {
            u8 const old_d = distance_from_desired();
            *m_d           = ((d << 1) & 0x3E) | (*m_d & 0xC1);
            return old_d;
        }

        void emplace(s8 d, u32 v)
        {
            *m_v = (u16)v;
            *m_d = (d << 1);
            *m_d = ((v >> 16) & 0x1) | (*m_d & 0xFE);
        }

        u32 displace(s8 d, u32 v)
        {
            u32 const old_v = value();
            *m_v            = (u8)v;
            *m_d            = ((v >> 16) & 0x1) | ((d << 1) & 0x3E);
            return old_v;
        }

        void destroy_value()
        {
            *m_d = -1;
            *m_v = -1;
        }

        inline bool operator < (const hashmap_entry_t& rhs) const { return (m_v < rhs.m_v); }
        inline bool operator <= (const hashmap_entry_t& rhs) const { return (m_v <= rhs.m_v); }

    private:
        s8*  m_d;
        u16* m_v;
    };

    template <> struct hashmap_entry_t<s16, u16>
    {
        hashmap_entry_t()
            : m_d(nullptr)
            , m_v(nullptr)
        {
        }
        hashmap_entry_t(s16* d, u16* v)
            : m_d(d)
            , m_v(v)
        {
        }

        void next()
        {
            m_d++;
            m_v++;
        }

        inline bool is_valid() const { return m_d != nullptr; }
        inline bool is_empty() const { return *m_d < 0; }
        void        set_empty()
        {
            *m_d = -1;
            *m_v = -1;
        }
        inline bool is_old() const { return (*m_d & 0x4000) == 0x400; }
        inline void set_old() { *m_d = (*m_d & 0xBFFF) | 0x4000; }
        inline bool has_value() const { return *m_d >= 0; }
        inline u32  value() const { return (u32)((*m_d & 0x1FF) << 16) | (u32)*m_v; }
        u32         value(u32 v)
        {
            u32 const old_v = value();
            *m_v            = (u16)(v);
            *m_d            = ((v >> 16) & 0x1FF) | (*m_d & 0x3E00);
            return old_v;
        }

        inline bool is_at_desired_position() const { return *m_d <= 0; }
        inline u8   distance_from_desired() const { return (*m_d & 0x3E00) >> 9; }
        u8          distance_from_desired(u8 d)
        {
            u8 const old_d = distance_from_desired();
            *m_d           = (((u16)d << 9) & 0x3E00) | (*m_d & 0xC1FF);
            return old_d;
        }

        void emplace(s8 d, u32 v)
        {
            *m_v = (u16)v;
            *m_d = ((v >> 16) & 0x1FF) | (((u16)d << 9) & 0x3E00);
        }

        u32 displace(s8 d, u32 v)
        {
            u32 const old_v = value();
            *m_v            = (u8)v;
            *m_d            = ((v >> 16) & 0x1FF) | (((u16)d << 9) & 0x3E00);
            return old_v;
        }

        void destroy_value()
        {
            *m_d = -1;
            *m_v = -1;
        }

        inline bool operator < (const hashmap_entry_t& rhs) const { return (m_v < rhs.m_v); }
        inline bool operator <= (const hashmap_entry_t& rhs) const { return (m_v <= rhs.m_v); }

    private:
        s16* m_d;
        u16* m_v;
    };

    template <> struct hashmap_entry_t<s8, u32>
    {
        typedef u64 vxx;

        hashmap_entry_t()
            : m_d(nullptr)
            , m_v(nullptr)
        {
        }
        hashmap_entry_t(s8* d, u32* v)
            : m_d(d)
            , m_v(v)
        {
        }

        void next()
        {
            m_d++;
            m_v++;
        }

        inline bool is_valid() const { return m_d != nullptr; }
        inline bool is_empty() const { return *m_d < 0; }
        void        set_empty()
        {
            *m_d = -1;
            *m_v = -1;
        }
        inline bool is_old() const { return (*m_d & 0x40) == 0x4; }
        inline void set_old() { *m_d = (*m_d & 0xBF) | 0x4; }
        inline bool has_value() const { return *m_d >= 0; }
        inline vxx  value() const { return (vxx)((vxx)(*m_d & 1) << 32) | (vxx)*m_v; }
        vxx         value(vxx v)
        {
            vxx const old_v = value();
            *m_v            = (u16)(v);
            *m_d            = ((v >> 32) & 1) | (*m_d & 0xFE);
            return old_v;
        }

        inline bool is_at_desired_position() const { return *m_d <= 0; }
        inline u8   distance_from_desired() const { return (*m_d & 0x3E) >> 1; }
        u8          distance_from_desired(u8 d)
        {
            u8 const old_d = distance_from_desired();
            *m_d           = ((d << 1) & 0x3E) | (*m_d & 0xC1);
            return old_d;
        }

        void emplace(s8 d, vxx v)
        {
            *m_v = (u16)v;
            *m_d = (d << 1);
            *m_d = ((v >> 32) & 0x1) | (*m_d & 0xFE);
        }

        vxx displace(s8 d, vxx v)
        {
            vxx const old_v = value();
            *m_v            = (u32)v;
            *m_d            = (u8)((v >> 32) & 0x1) | ((d << 1) & 0x3E);
            return old_v;
        }

        void destroy_value()
        {
            *m_d = -1;
            *m_v = -1;
        }

        inline bool operator < (const hashmap_entry_t& rhs) const { return (m_v < rhs.m_v); }
        inline bool operator <= (const hashmap_entry_t& rhs) const { return (m_v <= rhs.m_v); }

    private:
        s8*  m_d;
        u32* m_v;
    };

} // namespace xcore

#endif // __X_GENERICS_CONTAINERS_HASHMAP_ENTRY_H__