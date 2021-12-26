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
            array_t<s32>* keys = array_t<s32>::create(0, 1024, 65536);
            array_t<s32>* values = array_t<s32>::create(0, 1024, 65536);
            array_t<s8>* entry_h = array_t<s8>::create(0, 1024, 65536);
            array_t<u16>* entry_l = array_t<u16>::create(0, 1024, 65536);
            hashmap_t<s32, s32> hm(keys,values,entry_h,entry_l);

            keys->add_item(100);
            values->add_item(-100);
            
            hm.insert(0);
            hm.rehash(1);
            hm[0] = 100;

        }

    }
}
UNITTEST_SUITE_END
