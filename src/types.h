#ifndef jadetree_types_h
#define jadetree_types_h

#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdint.h>

#include "enum_factory.h"

typedef uint32_t DOC_ID;

typedef uint32_t TREE_NODE_ID;

typedef uint32_t FEATURE_INDEX;

typedef uint16_t UNIVERSAL_BUCKET;

#define RawFeatureTypeDefinition(T, XX) \
XX(T, UINT8 , =1) \
XX(T, INT8, =2) \
XX(T, UINT16, =3) \
XX(T, INT16, =4) \
XX(T, UINT32, =5) \
XX(T, INT32, =6) \
XX(T, FLOAT, =7) \

/*
enum class RawFeatureType {
    UINT8 =  1,
    INT8 =   2,
    UINT16 = 3,
    INT16 =  4,
    UINT32 = 5,
    INT32 =  6,
    FLOAT =  7
};
*/
DECLARE_ENUM(RawFeatureType, RawFeatureTypeDefinition)

const RawFeatureType MAX_RAW_FEATURE_TYPE = RawFeatureType::FLOAT;

union FeatureValue
{
    uint8_t u8v;
    int8_t i8v;
    uint16_t u16v;
    int16_t i16v;
    uint32_t u32t;
    int32_t i32v;
    float_t fv;

    FeatureValue()
    {
        memset(this, 0, sizeof(FeatureValue));
    }
};

template <typename T>
RawFeatureType get_feature_type_from_template();

#define INSTANTITATE_TEMPLATES_FOR_RAW_FEATURE_TYPES(cc) \
template class cc <uint8_t>;\
template class cc <int8_t>;\
template class cc <uint16_t>;\
template class cc <int16_t>;\
template class cc <uint32_t>;\
template class cc <int32_t>;\
template class cc <float_t>;

const float_t EPSILON = sqrt(std::numeric_limits<float>::min());

#endif
