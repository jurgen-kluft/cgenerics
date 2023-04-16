#include "cbase/c_allocator.h"

#include "cgenerics/c_hash_map.h"
#include "cgenerics/c_vector.h"

#include "cunittest/cunittest.h"

using namespace ncore;



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
