
#ifndef CSTREAM_DEFS_H
#define CSTREAM_DEFS_H

#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>

// Checks whether the compiler supports a Clang Feature Checking Macro, and if
// so, checks whether it supports the provided builtin function "x" where x
// is one of the functions noted in
// https://clang.llvm.org/docs/LanguageExtensions.html
//
// Note: Use this macro to avoid an extra level of #ifdef __has_builtin check.
// http://releases.llvm.org/3.3/tools/clang/docs/LanguageExtensions.html
#ifdef __has_builtin
#define HAS_BUILTIN(x) __has_builtin(x)
#else
#define HAS_BUILTIN(x) 0
#endif

// Ensure that PRIu32 and friends get defined for both C99
// and C++ consumers
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#define INT2VOIDPTR(i) (void *)(intptr_t)(i)

#define FAST_MAX(x, y) ((x) ^ (((x) ^ (y)) & -((x) < (y))))
#define FAST_MIN(x, y) ((y) ^ (((x) ^ (y)) & -((x) < (y))))

#define UNUSED_PARAMETER(x) ((void)x)

#if HAS_BUILTIN(__builtin_expect) || (defined(__GNUC__) && !defined(__clang__))
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

#if HAS_BUILTIN(__builtin_offsetof) || \
    (defined(__GNUC__) && !defined(__clang__))
#define OFFSET_OF(type, field) __builtin_offsetof(type, field)
#else
#define OFFSET_OF(type, field) ((size_t)(&((type *)0)->field))
#endif

#define CONTAINER_OF(ptr_, type_, member_) \
  ((type_ *)(void *)((char *)ptr_ + OFFSET_OF(type_, member_)))

// IS_LITTLE_ENDIAN
// IS_BIG_ENDIAN
//
// Checks the endianness of the platform.
//
// Notes: uses the built in endian macros provided by GCC (since 4.6) and
// Clang (since 3.2); see
// https://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html.
// Otherwise, if _WIN32, assume little endian. Otherwise, bail with an error.
#if defined(IS_BIG_ENDIAN)
#error "IS_BIG_ENDIAN cannot be directly set."
#endif
#if defined(IS_LITTLE_ENDIAN)
#error "IS_LITTLE_ENDIAN cannot be directly set."
#endif

#if (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
     __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define IS_LITTLE_ENDIAN 1
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define IS_BIG_ENDIAN 1
#elif defined(_WIN32)
#define IS_LITTLE_ENDIAN 1
#else
#error "endian detection needs to be set up for your compiler"
#endif

#endif