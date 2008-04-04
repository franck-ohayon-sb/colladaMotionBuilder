/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/*
* COLLADA Emitter Library (Premium only)
*/

#include "StdAfx.h"
//#include <maya/MFnDagNode.h>
#include <maya/MSelectionList.h>
#include "CReferenceManager.h"
#include "DaeControllerLibrary.h"
#include "DaeEmitter.h"
#include "DaeEmitterLibrary.h"
#include "DaeGeometryLibrary.h"
#include "DaeDocNode.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDEmitter.h"
#include "FCDocument/FCDEmitterInstance.h"
#include "FCDocument/FCDEmitterParticle.h"
#include "FCDocument/FCDForceField.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDGeometryInstance.h"
#include "FCDocument/FCDMaterialInstance.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDPlaceHolder.h"
#include "FCDocument/FCDEntityReference.h"
//#include "FCDocument/FCDParticlePCloud.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "FMath/FMFloat.h"



#define MAX_PARTICLE_COUNT 10000
#define MAX_LIFESPAN	   10000.0f

DaeEmitterLibrary::DaeEmitterLibrary(DaeDoc* doc)
:	DaeBaseLibrary(doc)
{
}


DaeEmitterLibrary::~DaeEmitterLibrary()
{
}

