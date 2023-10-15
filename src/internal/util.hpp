/*
 * bit7z - A C++ static library to interface with the 7-zip shared libraries.
 * Copyright (c) 2014-2022 Riccardo Ostani - All Rights Reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef UTIL_HPP
#define UTIL_HPP

#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

#include "internal/com.hpp"
#include "internal/fs.hpp"

#if defined( _WIN32 ) && defined( BIT7Z_AUTO_PREFIX_LONG_PATHS )
#include "internal/fsutil.hpp"
#endif

namespace bit7z {

#if defined( BIT7Z_USE_NATIVE_STRING ) && defined( _WIN32 )
// On Windows, with native strings enabled, strings are already wide!
#   define WIDEN( tstr ) tstr
#else
#   define WIDEN( tstr ) bit7z::widen(tstr)

auto narrow( const wchar_t* wideString, size_t size ) -> std::string;

auto widen( const std::string& narrowString ) -> std::wstring;
#endif

inline auto path_to_tstring( const fs::path& path ) -> tstring {
    /* In an ideal world, we should only use fs::path's string< tchar >() function for converting a path to a tstring.
     * However, MSVC converts paths to std::string using the system codepage instead of UTF-8,
     * which is the default encoding of bit7z. */
#if defined( _WIN32 ) && defined( BIT7Z_USE_NATIVE_STRING )
    return path.wstring();
#elif defined( _WIN32 ) && defined( BIT7Z_USE_SYSTEM_CODEPAGE )
    /* If we encounter a path with Unicode characters, MSVC will throw an exception
     * while converting from a fs::path to std::string if any character is invalid in the system codepage.
     * Hence, here we use bit7z's own string conversion function, which substitutes invalid Unicode characters
     * with '?' characters. */
    const auto& native_path = path.native();
    return narrow( native_path.c_str(), native_path.size() );
#else
    return path.u8string();
#endif
}

#if defined( _MSC_VER ) && !defined( BIT7Z_USE_NATIVE_STRING ) && !defined( BIT7Z_USE_SYSTEM_CODEPAGE )
#define PATH_FROM_TSTRING( str ) fs::u8path( str )
#else
#define PATH_FROM_TSTRING( str ) fs::path{ str }
#endif

inline auto tstring_to_path( const tstring& str ) -> fs::path {
#if defined( _WIN32 ) && defined( BIT7Z_AUTO_PREFIX_LONG_PATHS )
    auto result = PATH_FROM_TSTRING( str );
    if ( filesystem::fsutil::should_format_long_path( result ) ) {
        result = filesystem::fsutil::format_long_path( result );
    }
    return result;
#else
    // By default, MSVC treats strings as encoded using the system codepage, but bit7z uses UTF-8.
    return PATH_FROM_TSTRING( str );
#endif
}

inline auto path_to_wide_string( const fs::path& path ) -> std::wstring {
#if defined( _MSC_VER ) || !defined( BIT7Z_USE_STANDARD_FILESYSTEM )
    return path.wstring();
#else
    /* On some compilers and platforms (e.g., GCC before v12.3),
     * the direct conversion of the fs::path to wstring might throw an exception due to unicode characters.
     * So we simply convert to tstring, and then widen it if necessary. */
    return WIDEN( path.string< tchar >() );
#endif
}

constexpr inline auto check_overflow( int64_t position, int64_t offset ) noexcept -> bool {
    return ( ( offset > 0 ) && ( position > ( ( std::numeric_limits< int64_t >::max )() - offset ) ) ) ||
           ( ( offset < 0 ) && ( position < ( ( std::numeric_limits< int64_t >::min )() - offset ) ) );
}

/* Safe integer comparison like in C++20 */
#ifdef __cpp_if_constexpr

template< class T, class U >
constexpr auto cmp_less( T first, U second ) noexcept -> bool {
    using UT = std::make_unsigned_t< T >;
    using UU = std::make_unsigned_t< U >;
    if constexpr ( std::is_signed< T >::value == std::is_signed< U >::value ) {
        return first < second;
    } else if constexpr ( std::is_signed< T >::value ) {
        return ( first < 0 ) || ( UT( first ) < second );
    } else {
        return ( second >= 0 ) && ( first < UU( second ) );
    }
}

#else // SFINAE implementation for C++14

template< class T, class U >
constexpr auto
cmp_less( T t, U u ) noexcept -> std::enable_if_t< std::is_signed< T >::value == std::is_signed< U >::value, bool > {
    return t < u;
}

template< class T, class U >
constexpr auto
cmp_less( T t, U u ) noexcept -> std::enable_if_t< std::is_signed< T >::value && !std::is_signed< U >::value, bool > {
    return ( t < 0 ) || ( std::make_unsigned_t< T >( t ) < u );
}

template< class T, class U >
constexpr auto
cmp_less( T t, U u ) noexcept -> std::enable_if_t< !std::is_signed< T >::value && std::is_signed< U >::value, bool > {
    return ( u >= 0 ) && ( t < std::make_unsigned_t< U >( u ) );
}

#endif

template< class T, class U >
constexpr auto cmp_greater( T first, U second ) noexcept -> bool {
    return cmp_less( second, first ); // NOLINT(*-suspicious-call-argument)
}

template< class T, class U >
constexpr auto cmp_greater_equal( T first, U second ) noexcept -> bool {
    return !cmp_less( first, second );
}

template< bool B >
using bool_constant = std::integral_constant< bool, B >; // like C++17's std::bool_constant

template< typename T, typename I = T >
using is_com_type = bool_constant< std::is_base_of< CMyUnknownImp, T >::value && std::is_base_of< I, T >::value >;

template< typename T, typename I = T, class... Args >
inline auto make_com( Args&& ... args ) -> CMyComPtr< typename std::enable_if< is_com_type< T, I >::value, I >::type > {
    return CMyComPtr< I >( new T( std::forward< Args >( args )... ) ); //-V2511
}

}  // namespace bit7z

#endif //UTIL_HPP
