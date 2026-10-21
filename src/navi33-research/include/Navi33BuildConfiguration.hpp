#ifndef COBALT64_NAVI33_BUILD_CONFIGURATION_HPP
#define COBALT64_NAVI33_BUILD_CONFIGURATION_HPP

#if defined(_CPPUNWIND) || defined(__cpp_exceptions) || defined(__EXCEPTIONS)
#error "Navi33 research targets must compile with exceptions disabled"
#endif

#if defined(_CPPRTTI) || defined(__GXX_RTTI)
#error "Navi33 research targets must compile with RTTI disabled"
#endif

#endif
