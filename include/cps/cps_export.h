#ifndef CPS_EXPORT_H
#define CPS_EXPORT_H

/* CPS_API export / import macro.
 *
 *   - Static build (CPS_STATIC_DEFINE defined): CPS_API is empty.
 *   - Shared build on MSVC: dllexport while building cps_EXPORTS, dllimport otherwise.
 *   - Shared build on GCC/Clang: default visibility (relies on CMAKE_CXX_VISIBILITY_PRESET=hidden
 *     to hide everything else).
 */

#if defined(CPS_STATIC_DEFINE)
#  define CPS_API
#  define CPS_NO_EXPORT
#else
#  if defined(_MSC_VER)
#    ifdef cps_EXPORTS
#      define CPS_API __declspec(dllexport)
#    else
#      define CPS_API __declspec(dllimport)
#    endif
#    define CPS_NO_EXPORT
#  else
#    define CPS_API __attribute__((visibility("default")))
#    define CPS_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#endif /* CPS_EXPORT_H */
