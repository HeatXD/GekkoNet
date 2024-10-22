#pragma once

#ifdef _WIN32
#  ifdef GEKKONET_DLL_EXPORT  // Defined when building the DLL
#    define GEKKONET_API __declspec(dllexport)
#  else                      // Defined when using the DLL
#    define GEKKONET_API __declspec(dllimport)
#  endif
#else
#  define GEKKONET_API  // Empty for non-Windows platforms
#endif
