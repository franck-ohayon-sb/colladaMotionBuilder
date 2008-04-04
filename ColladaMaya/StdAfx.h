/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _STD_AFX_H_
#define _STD_AFX_H_

//
// Precompiled Header
//

#define _CRT_SECURE_NO_DEPRECATE 1
#define _MGLFunctionTable_h_ // We don't want this one included by Maya 8.5
class MGLFunctionTable;

#ifdef MAC_TIGER
#include <maya/OpenMayaMac.h>
#endif

// CRT
#include <math.h>
#include <string.h>

// Maya SDK
#include <maya/MBoundingBox.h>
#include <maya/MColor.h>
#include <maya/MColorArray.h>
#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MDistance.h>
#include <maya/MEulerRotation.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MObjectArray.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MPxData.h>
#include <maya/MQuaternion.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MTime.h>
#include <maya/MTimeArray.h>
#include <maya/MVector.h>
#include <maya/MVectorArray.h>
#include <maya/MFnStringData.h>

#ifdef WIN32
#include "GLExt/gl.h"
#include "GLExt/glext.h"
#include "GLExt/glprocs.h"
#elif defined(MAC_PLUGIN)
#include <OpenGL/OpenGL.h>
#elif defined(LINUX)
#include <GL/gl.h>
#else
#error "Unsupported platform"
#endif //all platforms..

#include <Cg/cg.h>
#include <Cg/cgGL.h>

// FCollada is Feeling Software's COLLADA object model.
#include "FCollada.h"
#include "FCDocument/FCDocument.h"
#include "TranslatorHelpers/MayaConvert.h"

#ifndef UINT_MAX
#define UINT_MAX 0xFFFFFFFF
#endif

#ifndef INT_MAX
#define INT_MAX 0x7FFFFFFF
#endif

#include "DaeEntity.h"
#include "FUtils/FUDaeSyntax.h"

#include "DaeDoc.h"
#include "DaeAgeiaPhysicsScene.h"
#include "DaeAnimationLibrary.h"
#include "DaeCameraLibrary.h"
#include "DaeBaseLibrary.h"
#include "DaeControllerLibrary.h"
#include "DaeEmitterLibrary.h"
#include "DaeEntityManager.h"
#include "DaeForceFieldLibrary.h"
#include "DaeGeometryLibrary.h"
#include "DaeLightLibrary.h"
#include "DaeMaterialLibrary.h"
#include "DaeNativePhysicsScene.h"
#include "DaePhysicsMaterialLibrary.h"
#include "DaePhysicsModelLibrary.h"
#include "DaePhysicsSceneLibrary.h"
#include "DaePhysicsScene.h"
#include "DaeSceneGraph.h"
#include "DaeTextureLibrary.h"

#endif // _STD_AFX_H_
