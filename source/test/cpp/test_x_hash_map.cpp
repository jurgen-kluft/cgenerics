#include "xbase/x_allocator.h"
#include "xbase/x_darray.h"

#include "xgenerics/x_hash_map.h"
#include "xgenerics/x_vector.h"

#include "xunittest/xunittest.h"

using namespace xcore;

extern xcore::alloc_t* gTestAllocator;

UNITTEST_SUITE_BEGIN(hashmap)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(construct)
        {
            s32* keys = create_darray<s32>(0, 1024, 65536);
            s32* values = create_darray<s32>(0, 1024, 65536);
            u32* meta = create_darray<u32>(0, 1024, 65536);
            hashmap_t<s32, s32> hm(keys,values,meta);

        }

    }
}
UNITTEST_SUITE_END
