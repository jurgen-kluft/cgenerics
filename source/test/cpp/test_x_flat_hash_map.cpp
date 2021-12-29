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
                    CHECK_EQUAL((u32)(1<<p), match);
                    ctrl.clr_hash(p);
                }
            }
        }
    }
}
UNITTEST_SUITE_END
