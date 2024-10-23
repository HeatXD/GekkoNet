#pragma once

#ifdef _WIN32
#ifdef GEKKONET_STATIC
// Static library - no import/export needed
#define GEKKONET_API
#else
#ifdef GEKKONET_DLL_EXPORT
// Building the DLL
#define GEKKONET_API __declspec(dllexport)
#else
// Using the DLL
#define GEKKONET_API __declspec(dllimport)
#endif
#endif
#else
// Non-Windows platforms don't need special export macros
#define GEKKONET_API
#endif

#include <stdint.h>

namespace Gekko {
	typedef uint8_t u8;
	typedef uint16_t u16;
	typedef uint32_t u32;
	typedef uint64_t u64;

	typedef int8_t i8;
	typedef int16_t i16;
	typedef int32_t i32;
	typedef int64_t i64;

	typedef float f32;
	typedef double f64;

	typedef int32_t Frame;
	typedef int32_t Handle;
}
