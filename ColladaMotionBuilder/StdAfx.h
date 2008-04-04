/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COLLADA_MB_PCH_
#define _COLLADA_MB_PCH_

// Win32
#ifdef WIN32
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE
#include <malloc.h> // re-include for a strange reason.
#undef _malloca
#endif // WIN32

// FCollada
#define NO_LIBXML
#include "FCollada.h"
#undef _WIN32_WINNT

// Motion Builder SDK
#include <fbsdk/fbsdk.h>
#if defined(K_KERNEL_VERSION) && !defined(K_FILMBOX_MAIN_VERSION_AS_STRING)
#define K_FILMBOX_POINT_RELEASE_VERSION K_KERNEL_VERSION
#define K_FILMBOX_MAIN_VERSION_AS_STRING ((float) K_KERNEL_VERSION / 1000.0f)
#endif // K_KERNEL_VERSION

// Global export/import options.
#include "Options.h"
extern ColladaOptions* GetOptions();

// Global helpers
inline FMVector2 ToFMVector2(const FBVector2d& v) { return FMVector2((float) v[0], (float) v[1]); }
inline FMVector3 ToFMVector3(const FBPropertyAnimatableColor& c) { FBColor c2 = c; return FMVector3(c2[0], c2[1], c2[2]); }
inline FMVector3 ToFMVector3(const FBVector3d& v) { return FMVector3((float) v[0], (float) v[1], (float) v[2]); }
inline FMVector4 ToFMVector4(const FBPropertyAnimatableColor& c) { FBColor c2 = c; return FMVector4(c2[0], c2[1], c2[2], 1.0f); } // NOTE: force alpha to 1.0f!
inline FMVector4 ToFMVector4(const FBVector4d& v) { return FMVector4((float) v[0], (float) v[1], (float) v[2], (float) v[3]); }
inline FMMatrix44 ToFMMatrix44(const FBMatrix& mx) { FMMatrix44 m2 = (double*) mx; return m2.Transposed(); }
inline float ToSeconds(FBTime& time) { return (float) (time.GetSecondDouble()); }
inline float ToSeconds(const FBPropertyTime& time) { FBTime t = (FBTime) time; return ToSeconds(t); }

#endif // _COLLADA_MB_PCH_

