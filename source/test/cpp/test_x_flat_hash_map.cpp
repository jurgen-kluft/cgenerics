#include "xbase/x_allocator.h"
#include "xbase/x_darray.h"

#include "xgenerics/x_flat_hash_map.h"
#include "xgenerics/x_vector.h"

#include "xunittest/xunittest.h"

using namespace xcore;

extern xcore::alloc_t* gTestAllocator;

UNITTEST_SUITE_BEGIN(flat_hashmap)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(fnv1a_1)
        {
            const char* str = "nintendo";
            u64 hash = flat_hashmap_n::FNV1A64((xbyte const*)str, 8, 0);
            CHECK_EQUAL(0x997f51be06a8b6b0, hash);
        }

        UNITTEST_TEST(fnv1a_2)
        {
            const char* str = "HuyyoUllaPonuValiyaVidu12!";
            u64 hash = flat_hashmap_n::FNV1A64((xbyte const*)str, 26, 0);
            CHECK_EQUAL(0x389bd94a9cbf2525, hash);
        }

        UNITTEST_TEST(ctrl_single)
        {
            flat_hashmap_n::ctrl_t ctrl;
            ctrl.clear();

            for (s32 h = 1; h < 255; h++)
            {
                for (s32 p = 0; p < 32; p++)
                {
                    ctrl.set_hash((u8)h, p);
                    u32 match = ctrl.match((u8)h, 0xffffffff);
                    CHECK_EQUAL((u32)(1 << p), match);
                    ctrl.set_hash(0, p);
                }
            }

            for (s32 h = 1; h < 255; h++)
            {
                for (s32 p = 0; p < 32; p++)
                {
                    ctrl.set_hash((u8)h, p);
                    u32 match = ctrl.match((u8)h, 1<<p);
                    CHECK_EQUAL((u32)(1 << p), match);
                    ctrl.set_hash(0, p);
                }
            }
        }
        UNITTEST_TEST(group_empty)
        {
            flat_hashmap_n::ctrl_t group;
            group.clear();

            CHECK_EQUAL(true, group.has_empty());
            CHECK_EQUAL(false, group.has_deleted());
            CHECK_EQUAL(0, group.get_used());

            for (s32 i = 0; i < 32; ++i)
            {
                CHECK_EQUAL(true, group.is_empty(i));
            }
            for (s32 i = 0; i < 32; ++i)
            {
                CHECK_EQUAL(false, group.is_deleted(i));
            }
        }

        UNITTEST_TEST(group_test_empty)
        {
            flat_hashmap_n::ctrl_t group;
            group.clear();

            CHECK_EQUAL(true, group.has_empty());
            CHECK_EQUAL(false, group.has_deleted());

            for (s32 i = 0; i < 32; ++i)
            {
                group.set_empty(i);
                CHECK_EQUAL(true, group.is_empty(i));
            }
        }

        UNITTEST_TEST(group_test_deleted)
        {
            flat_hashmap_n::ctrl_t group;
            group.clear();

            CHECK_EQUAL(true, group.has_empty());
            CHECK_EQUAL(false, group.has_deleted());

            for (s32 i = 0; i < 32; ++i)
            {
                group.set_deleted(i);
                CHECK_EQUAL(true, group.is_deleted(i));
            }
            CHECK_EQUAL(true, group.has_deleted());
        }

        UNITTEST_TEST(insert)
        {
            flat_hashmap_n::hashmap_t<s32, s32> map;
            CHECK_TRUE(map.insert(0, 0));
            CHECK_FALSE(map.insert(0, 0));
        }

        UNITTEST_TEST(insert_to_grow)
        {
            flat_hashmap_n::hashmap_t<s32, s32> map;
            for (s32 i = 0; i < 55; ++i)
            {
                CHECK_EQUAL(true, map.insert(i, i));
            }
            for (s32 i = 0; i < 55; ++i)
            {
                CHECK_NOT_NULL(map.find(i));
            }

            CHECK_EQUAL(true, map.insert(55, 55));
            CHECK_EQUAL(true, map.insert(56, 56));

            for (s32 i = 0; i < 57; ++i)
            {
                CHECK_NOT_NULL(map.find(i));
            }
        }

        UNITTEST_TEST(insert_erase_find_simple)
        {
            flat_hashmap_n::hashmap_t<s32, s32> map;
            for (s32 i = 0; i < 32; ++i)
            {
                CHECK_EQUAL(true, map.insert(i, i));
            }
            for (s32 i = 0; i < 32; ++i)
            {
                CHECK_NOT_NULL(map.find(i));
            }
            for (s32 i = 0; i < 32; i+=2)
            {
                CHECK_EQUAL(true, map.erase(i));
            }
            for (s32 i = 1; i < 32; i+=2)
            {
                CHECK_NOT_NULL(map.find(i));
            }
        }

        UNITTEST_TEST(large_insert)
        {
            const s32 n = 100000;
            flat_hashmap_n::hashmap_t<s32, s32> map;

            for (s32 i = 0; i < n; ++i)
            {
                CHECK_EQUAL(true, map.insert(i, i));
                CHECK_NOT_NULL(map.find(i));
            }

            for (s32 i = 0; i < n; ++i)
            {
                CHECK_NOT_NULL(map.find(i));
            }
            for (s32 i = 0; i < n; i+=2)
            {
                CHECK_EQUAL(true, map.erase(i));
            }
            for (s32 i = 1; i < n; i+=2)
            {
                CHECK_NOT_NULL(map.find(i));
            }
            for (s32 i = 1; i < n; i+=2)
            {
                CHECK_EQUAL(true, map.erase(i));
            }
            CHECK_TRUE(map.empty());
        }

        UNITTEST_TEST(const_iterator)
        {
            const s32 n = 100000;
            flat_hashmap_n::hashmap_t<s32, s32> map(n);
            for (s32 i = 0; i < n; ++i)
            {
                CHECK_EQUAL(true, map.insert(i, i));
            }

            auto iter = map.begin();
            auto end = map.end();
            while (iter < end)
            {   
                ++iter;
            }
        }
    }
}
UNITTEST_SUITE_END
