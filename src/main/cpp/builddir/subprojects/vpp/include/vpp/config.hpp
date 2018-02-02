// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

// For more information on vpp configuration, see doc/config.md
// This file (config.hpp) is automatically generated using cmake for the configuration
// vpp was built with. Do not change it manually.

#pragma once

#define VPP_VMAJOR 0
#define VPP_VMINOR 2
#define VPP_VPATCH 0
#define VPP_VERSION VPP_VMAJOR * 10000u + VPP_VMINOR * 100u + VPP_VPATCH

// If this macro is enabled vpp will only allow one Device instance but nearly all objects
// will consume one word less memory. For more information see resource.hpp
#undef VPP_ONE_DEVICE_OPTIMIZATION

// If this macro is defined, additional checks will be performed (maybe worse performance)
#if !defined(VPP_DEBUG) && !defined(VPP_NDEBUG)
 #define VPP_DEBUG 1
#endif

// Opposite of VPP_DEBUG
#if !defined(VPP_DEBUG) && !defined(VPP_NDEBUG)
 #define VPP_NDEBUG
#endif

// These macros control if vulkan api calls should be error-checked and if so what should be done
// when an error occurs.
#if !defined(VPP_CALL_THROW) && !defined(VPP_CALL_NOCHECK)
 #ifdef VPP_DEBUG
  #define VPP_CALL_THROW
 #else
  #define VPP_CALL_NOCHECK
 #endif
#endif
