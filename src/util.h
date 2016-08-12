#ifndef util_hpp
#define util_hpp

#include <boost/lexical_cast.hpp>
#include <memory>
#include <stdexcept>
#include <stdio.h>
#include <sstream>
#include <vector>

#include "types.h"

// Static / global variable exists in a per - thread context(thread local storage).
#if defined (__GNUC__)
//#define THREAD_LOCAL __thread
#define THREAD_LOCAL thread_local
#elif defined (_MSC_VER)
#define THREAD_LOCAL __declspec(thread)
#else // !__GNUC__ && !_MSC_VER
#error "Define a thread local storage qualifier for your compiler/platform!"
#endif

struct number_format_error: public std::runtime_error
{
    using runtime_error::runtime_error;
};

inline bool tt_is_space(char c)
{
    return c == ' ';
}

inline bool tt_is_digit(char c)
{
    return (c >= '0') && (c <= '9');
}

template<typename T>
struct NumberParsingContainer { typedef T type; };
template<>
struct NumberParsingContainer<uint8_t> { typedef uint64_t type; };
template<>
struct NumberParsingContainer<uint16_t> { typedef uint64_t type; };
template<>
struct NumberParsingContainer<uint32_t> { typedef uint64_t type; };
template<>
struct NumberParsingContainer<int8_t> { typedef int64_t type; };
template<>
struct NumberParsingContainer<int16_t> { typedef int64_t type; };
template<>
struct NumberParsingContainer<int32_t> { typedef int64_t type; };

template <typename T>
inline T parse_string(const char * str)
{
    typedef typename NumberParsingContainer<T>::type Container;
    Container result = 0;
    while (tt_is_space(*str)) {
        str++;
    }
    bool negative = false;
    if (std::is_signed<T>()) {
        if (*str == '-') {
            negative = true;
            str++;
        }
    }
    const char * ptr1 = str;
    while (tt_is_digit(*str)) {
        char digit = *(str++) - '0';
        result = 10 * result + digit;
    }
    if (str == ptr1) {
// No digits found
        throw number_format_error("parse_string");
    }
    while (tt_is_space(*str)) {
        str++;
    }
    if (*str != 0) {
        // Couldn't reach the end of the string.
        // There must be some wrong characters in the string.
        throw number_format_error("parse_string");
    }
        if (std::is_signed<T>() && negative) {
        result = 0 - result;
    }
    T tresult = (T)result;
    if (tresult != result) {
// Indicates an overflow.
        // It is possible that there will be an overflow when all the higher bits are still set to zero, and we won't catch it,
        // however the likelihood of this is negligibly low.
        throw number_format_error("parse_string overflow");
    }
    return tresult;
}

template <>
inline float_t parse_string(const char * str_value)
{
    char * ptr;
    errno = 0;
    float_t result = strtof(str_value, &ptr);
    if (str_value == ptr) {
    throw number_format_error("strtof");
}
    // The rest of the string better be whitespaces, or else.
    while (isspace(*ptr)) {
        ptr++;
    }
    if (*ptr != 0) {
        throw number_format_error("strtof");
    }
    if (errno == ERANGE) {
        throw number_format_error("strtof overflow");
    }
    return result;
}



template <typename T>
inline std::string to_string(T value)
{
    return std::to_string(value);
}

template <>
inline std::string to_string(float_t value)
{
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

inline std::string format_float(float_t value, uint32_t precision, bool fixed = true)
{
    std::ostringstream oss;
    oss.precision(precision);
    if (fixed) {
        oss << std::fixed;
    }
    else {
//oss << std::scientific;
    }
    oss << value;
    return oss.str();
}
    
inline std::string std_strerror(int error_code)
{
    const size_t BUF_SIZE = 1024;
    char * buf = reinterpret_cast<char *>(malloc(BUF_SIZE * sizeof(buf[0])));
#ifdef _WIN32
    if (0 != strerror_s(buf, BUF_SIZE, error_code)) {
        throw std::runtime_error("strerr_s failed");
    }
#else
    if (0 != strerror_r(error_code, buf, BUF_SIZE)) {
        throw std::runtime_error("strerror_r failed");
    }
#endif
    std::string result(buf);
    free(buf);
    return result;
}

#define TIMER_START(t) \
std::chrono::high_resolution_clock::time_point (t)= std::chrono::high_resolution_clock::now();

#define TIMER_FINISH(t) ( \
std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::high_resolution_clock::now() - (t)).count() \
)

inline float_t sigmoid(float_t x) noexcept
{
    float_t exp_x = exp(x);
    return exp_x / (1 + exp_x);
}

inline float_t sigmoid_prime(float_t x) noexcept
{
    float_t s = sigmoid(x);
    return s * (1 - s);
}

inline float_t sigmoid_prime_prime(float_t x) noexcept
{
    float_t s = sigmoid(x);
    float_t ss = sigmoid_prime(x);
    return ss * (1 - s) - ss*s;
}

// Don't worry, I don't understand this either.
// For more details see http://stackoverflow.com/questions/20843271/passing-a-non-copyable-closure-object-to-stdfunction-parameter
template< typename signature >
struct make_copyable_function_helper;
template< typename R, typename... Args >
struct make_copyable_function_helper<R(Args...)> {
    template<typename input>
    std::function<R(Args...)> operator()(input&& i) const {
        auto ptr = std::make_shared< typename std::decay<input>::type >(std::forward<input>(i));
        return [ptr](Args... args)->R {
            return (*ptr)(std::forward<Args>(args)...);
        };
    }
};

template< typename signature, typename input >
std::function<signature> make_copyable_function(input && i) {
    return make_copyable_function_helper<signature>()(std::forward<input>(i));
}

extern std::vector<float_t> DCG_cache;

inline float_t get_dcg_coefficient_explicit(DOC_ID pos)
{
    return (float_t)1.0 / log2((float_t)2.0 + pos);
}

inline void init_DCG_cache(DOC_ID size)
{
    assert(DCG_cache.size() == 0);
    DCG_cache.resize(size);
    for (DOC_ID i = 0; i < size; i++) {
        DCG_cache[i] = get_dcg_coefficient_explicit(i);
    }
}

inline float_t get_dcg_coefficient(DOC_ID pos)
{
    assert(DCG_cache.size() > 0);
    if (pos < DCG_cache.size()) {
        return DCG_cache[pos];
    }
    return get_dcg_coefficient_explicit(pos);
}

template<class T>
inline std::string join(const std::vector<T> & vector, const std::string & delimiter)
{
    std::ostringstream oss;
    for (size_t i = 0; i < vector.size(); i++) {
        if (i > 0) {
            oss << delimiter;
        }
        oss << std::to_string(vector[i]);
    }
    return oss.str();
}

#endif /* util_hpp */
