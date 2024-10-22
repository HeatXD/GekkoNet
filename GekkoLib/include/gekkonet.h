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
