#include "xbase/x_allocator.h"

#include "xgenerics/x_hash_map.h"
#include "xgenerics/x_vector.h"

#include "xunittest/xunittest.h"

using namespace xcore;

extern xcore::alloc_t* gTestAllocator;

UNITTEST_SUITE_BEGIN(vector)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(vector_t)
        {
            //hash_map_t<s32, s32> hm;
            vector_t<s32> v;

            s32 i = 0;
            v.push_back(i);

        }

    }
}
UNITTEST_SUITE_END
