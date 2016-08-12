#ifndef __tealtree__enum_factory__
#define __tealtree__enum_factory__

// For more details, see:
// http://stackoverflow.com/questions/147267/easy-way-to-use-variables-of-enum-types-as-string-in-c/29561365#29561365

#include <algorithm>
#include <string>
#include <vector>

inline char tolower(char c)
{
    if ((c >= 'A') && (c <= 'Z')) {
        return c - 'A' + 'a';
    }
    return c;
}

inline std::string tolower(const std::string & str)
{
    std::string data(str);
    for (size_t i = 0; i < str.size(); i++) {
        data[i] = tolower(str[i]);
    }
    //std::transform(data.begin(), data.end(), data.begin(), ::tolower);
    return data;
}

// expansion macro for enum value definition
#define ENUM_VALUE(EnumType, name,assign) name assign,

// expansion macro for enum to string conversion
#define ENUM_CASE(EnumType, name,assign) case EnumType::name: return tolower(#name);

// expansion macro for string to enum conversion
#define ENUM_STRCMP(EnumType, name,assign) if (str == tolower(#name)) return EnumType::name;

/// declare the access function and define enum values
#define DECLARE_ENUM(EnumType,ENUM_DEF) \
  enum class EnumType { \
    ENUM_DEF(EnumType, ENUM_VALUE) \
  }; \
  std::string to_string(EnumType dummy); \
  EnumType parse_##EnumType##_value  (const std::string & str); \

/// define the access function names
#define DEFINE_ENUM(EnumType,ENUM_DEF) \
  std::string to_string(EnumType value) \
  { \
    switch(value) \
    { \
      ENUM_DEF(EnumType, ENUM_CASE) \
      default: return ""; /* handle input error */ \
    } \
  } \
  EnumType parse_##EnumType##_value (const std::string& str) \
  { \
    ENUM_DEF(EnumType, ENUM_STRCMP) \
    return (EnumType)0; /* handle input error */ \
  } \

#endif /* defined(__tealtree__enum_factory__) */
