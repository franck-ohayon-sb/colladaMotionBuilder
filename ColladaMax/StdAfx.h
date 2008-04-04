/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/
/*
	Based on the 3dsMax COLLADA Tools:
	Copyright (C) 2005-2006 Autodesk Media Entertainment
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _STD_AFX_H_
#define _STD_AFX_H_

#pragma warning(disable:4100)
#pragma warning(disable:4201)
#pragma warning(disable:4238)
#pragma warning(disable:4239)
#pragma warning(disable:4244)
#pragma warning(disable:4245)
#pragma warning(disable:4512)
#pragma warning(disable:4702)

#define _CRT_SECURE_NO_DEPRECATE 1 // MSVS 2005 support.

#include <Max.h>
#include "resource.h"
#include <istdplug.h>
#include <iparamb2.h>
#include <iparamm2.h>
#include <bitmap.h>
#include <CustAttrib.h>
#include <imtl.h>
#include <iskin.h>
#include <dummy.h>
#include <splshape.h>
#include <shaders.h>
#include <stdmat.h>
#include <IDxMaterial.h>
#include <icustattribcontainer.h>
#include <object.h>
#include <MeshNormalSpec.h>
#include <modstack.h>
#include <simpobj.h>
#if MAX_VERSION_MAJOR >= 9
#include <ILayerControl.h>
#endif // MAX 9.0+

// Added for material plugin
#include <ID3D9GraphicsWindow.h>
#include <IDx9VertexShader.h>
#include <IStdDualVS.h>
#include <IViewportManager.h>
#include <Notify.h> 
#include <Ref.h>
#include <RTMax.h>
#include <CustCont.h>

// these macros are redefined in FUtils
#undef SAFE_DELETE
#undef SAFE_DELETE_ARRAY
#undef SAFE_RELEASE

// Biped
#include <CS/bipexp.h>
#ifndef SKELOBJ_CLASS_ID
#define SKELOBJ_CLASS_ID Class_ID(0x9125, 0) // this is the class for biped geometric objects
#endif // SKELOBJ_CLASS_ID
#ifndef BIPED_CLASS_ID
#define BIPED_CLASS_ID Class_ID(0x9155, 0)
#endif // BIPED_CLASS_ID

#undef RadToDeg
#undef DegToRad
#undef GetObject

#pragma warning(default:4100)
#pragma warning(default:4201)
//#pragma warning(default:4238) // rvalue used as lvalue
//#pragma warning(default:4239) // reference conversion
//#pragma warning(default:4244) // conversion implies possible loss of data
#pragma warning(default:4245)
#pragma warning(default:4512)
#pragma warning(default:4702)

#include "IInstanceMgr.h"

// Important Win32 Handles
extern TCHAR *GetString(int id);
extern HINSTANCE hInstance;

#include "FCollada.h"
#include "MaxConvert.h"
#include "ColladaMaxTypes.h"
#include "FUtils/FUDaeSyntax.h"
#include "resource.h"

// General purpose type definitions
typedef fm::pvector<ReferenceTarget> PointerList;

class ColladaEffectParameter;
typedef fm::pvector<ColladaEffectParameter> ColladaEffectParameterList;

// Added for ColladaEffect
#include <d3dx9.h>
#include <d3dx9shader.h>
#include <cg/cg.h>
#include <cg/cgd3d9.h>

#define TIME_INITIAL_POSE		0
#define TIME_EXPORT_START		OPTS->AnimStart()

#define DXMATERIAL_CLASS_ID	Class_ID(0xed995e4, 0x6133daf2)

#endif // _STD_AFX_H_
