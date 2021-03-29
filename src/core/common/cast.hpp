#ifndef OT_CORE_COMMON_CODE_UTILS_HPP
#define OT_CORE_COMMON_CODE_UTILS_HPP

#include <type_traits>

template <typename T, typename From> T ot_api_cast(From *aPointer)
{
    using To = typename std::remove_pointer<T>::type;

    static_assert(std::is_standard_layout<To>::value, "Not standard-layout");
    static_assert(std::is_base_of<From, To>::value, "Not convertible");
    // static_assert(std::is_convertible<From, To>::value, "Not convertible");
    return reinterpret_cast<To *>(aPointer);
}

#endif // OT_CORE_COMMON_CODE_UTILS_HPP
