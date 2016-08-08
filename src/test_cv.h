#ifndef __tealtree__TEST_CV__
#define __tealtree__TEST_CV__

#include <stdio.h>

#include "compact_vector.h"
#include "types.h"
#include "util.h"

class TestCV
{
public:
    template<uint8_t BITS1, uint8_t BITS2>
    static CompactVector<BITS2> convert(CompactVector<BITS1> & cv)
    {
        CompactVector<BITS2> result;
        for (DOC_ID i = 0; i < cv.size(); i++) {
            result.push_back(cv[i]);
        }
        result.push_back_flush();
        return result;
    }

    template<uint8_t BITS>
    static CompactVector<BITS> copy(CompactVector<BITS>  src, CompactVector<BITS>  dest, DOC_ID from, DOC_ID to, DOC_ID size)
    {
        CompactVector<BITS> result = dest;
        result.copy(src, from, to, size);
        return result;
}

    template<uint8_t BITS1, uint8_t BITS2>
    static bool equals(CompactVector<BITS1> & cv1, CompactVector<BITS2> & cv2)
    {
        if (cv1.size() != cv2.size()) {
            return false;
        }
        for (DOC_ID i = 0; i < cv1.size(); i++) {
            if (cv1[i] != cv2[i]) {
                return false;
            }
        }
        return true;
    }

    template<uint8_t BITS1, uint8_t BITS2>
    static void test_copy(CompactVector<BITS1> src, CompactVector<BITS1> dest, DOC_ID from, DOC_ID to, DOC_ID size)
    {
        if ((to == 17) && (size == 15)) {
            dest.size();
        }
        CompactVector<BITS1> r1 = TestCV::copy<BITS1>(src, dest, from, to, size);
        CompactVector<BITS2> r2 = TestCV::copy<BITS2>(convert<BITS1, BITS2>(src), convert<BITS1, BITS2>(dest), from, to, size);
        assert(equals(r1, r2));
    }

    static void test_all()
    {
        CompactVector<4> cv1;
        for (DOC_ID i = 0; i < 70; i++) {
            cv1.push_back(i % 15);
        }
        cv1.push_back_flush();

        CompactVector<4> dest;
        for (DOC_ID i = 0; i < 1000; i++) {
            dest.push_back(0);
        }
        dest.push_back_flush();

        for (DOC_ID from = 0; from <= 32; from++) {
            for (DOC_ID to = 0; to <= 32; to++) {
                for (DOC_ID size = 0; size < 32; size++) {
                    test_copy<4, 8>(cv1, dest, from, to, size);
                }
            }
        }

    }
};


#endif /* defined(__tealtree__TEST_CV__) */
