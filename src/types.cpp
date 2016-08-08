#include "types.h"

template <>
RawFeatureType get_feature_type_from_template<uint8_t>() { return RawFeatureType::UINT8; }

template <>
RawFeatureType get_feature_type_from_template<int8_t>() { return RawFeatureType::INT8; }

template <>
RawFeatureType get_feature_type_from_template<uint16_t>() { return RawFeatureType::UINT16; }

template <>
RawFeatureType get_feature_type_from_template<int16_t>() { return RawFeatureType::INT16; }

template <>
RawFeatureType get_feature_type_from_template<uint32_t>() { return RawFeatureType::UINT32; }

template <>
RawFeatureType get_feature_type_from_template<int32_t>() { return RawFeatureType::INT32; }

template <>
RawFeatureType get_feature_type_from_template<float_t>() { return RawFeatureType::FLOAT; }

static const char * feature_type_strings[] = {
    "",
    "uint8",
    "int8",
    "uint16",
    "int16",
    "uint32",
    "int32",
    "float"
};

const char * feature_type_to_string(RawFeatureType t)
{
    return feature_type_strings[static_cast<uint32_t>(t)];
}

