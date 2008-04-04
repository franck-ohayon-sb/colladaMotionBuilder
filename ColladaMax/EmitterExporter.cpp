/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "AnimationExporter.h"
#include "EmitterExporter.h"
#include "EntityExporter.h"
#include "ForceExporter.h"
#include "GeometryExporter.h"
#include "NodeExporter.h"
#include "MaterialExporter.h"
#include "DocumentExporter.h"
#include "ControllerExporter.h"
#include "Common/MaxParticleDefinitions.h"
#include "Common/ConversionFunctions.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDExtra.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDAnimation.h"
#include "FCDocument/FCDAnimationChannel.h"
#include "FCDocument/FCDAnimationKey.h"
#include "FCDocument/FCDAnimationCurve.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDController.h"
#include "FCDocument/FCDControllerInstance.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDGeometryInstance.h"
#include "FCDocument/FCDEmitter.h"
#include "FCDocument/FCDEmitterInstance.h"
#include "FCDocument/FCDEmitterParticle.h"
#include "FCDocument/FCDEmitterObject.h"

// taken from <simpmod.h>
#define SIMPWSMMOD_OBREF		0
#define SIMPWSMMOD_NODEREF		1
#define SIMPWSMMOD_PBLOCKREF	2

