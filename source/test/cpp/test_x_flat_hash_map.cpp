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
                    ctrl.clr_hash(p);
                }
            }
        }
        UNITTEST_TEST(group_empty)
        {
            flat_hashmap_n::group_t group;
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
            flat_hashmap_n::group_t group;
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
            flat_hashmap_n::group_t group;
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
            CHECK_EQUAL(true, map.insert(0, 0));
            CHECK_EQUAL(false, map.insert(0, 0));
        }
    }
}
UNITTEST_SUITE_END
