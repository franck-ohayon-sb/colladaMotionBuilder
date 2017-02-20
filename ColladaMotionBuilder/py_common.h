#ifndef _PY_COMMON_H_
#define _PY_COMMON_H_

#include "StdAfx.h"

#pragma warning(disable:4996) // 'function': was declared deprecated.
#pragma warning(disable:4265) // 



//--- SDK include
#include <pyfbsdk/pyfbsdk.h>

#ifdef KARCH_ENV_WIN
#include <windows.h>
#endif

#include "py_function.h"

#endif //_PY_COMMON_H_