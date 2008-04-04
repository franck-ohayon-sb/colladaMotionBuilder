/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include <maya/MFnAttribute.h>
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDPhysicsScene.h"
#include "TranslatorHelpers/CDagHelper.h"

DaePhysicsSceneLibrary::DaePhysicsSceneLibrary(DaeDoc* pDoc)
:	DaeBaseLibrary(pDoc) 
{
}

void DaePhysicsSceneLibrary::Import()
{
	FCDPhysicsSceneLibrary* library = CDOC->GetPhysicsSceneLibrary();
	for (size_t i = 0; i < library->GetEntityCount(); ++i)
	{
		FUAssert(i == 0, MGlobal::displayWarning(
				MString("Multiple physics scenes not supported.")); break;);

		FCDPhysicsScene* scene = library->GetEntity(i);
		if (scene->GetUserHandle() == NULL) ImportScene(scene);
	}
}

void DaePhysicsSceneLibrary::ImportScene(FCDPhysicsScene* scene)
{
	MStatus status;
	MGlobal::executeCommand("nxLazilyCreateSolverAndDebug");
	MFnDependencyNode solverFn(CDagHelper::GetNode("nxRigidSolver1"), &status);
	if (status)
	{
		const FMVector3& gravity = scene->GetGravity();
		const FMVector3& gDir = gravity.Normalize();
		MVector gravityDirection(gDir.x, gDir.y, gDir.z);

		MStatus plugStatus;
		MPlug plug;
		plug = solverFn.findPlug(MString("gravityDirection"), &plugStatus);
		if (plugStatus)
		{
			CDagHelper::SetPlugValue(plug, MVector(gravityDirection));
		}
		plug = solverFn.findPlug(MString("gravityMagnitude"), &plugStatus);
		if (plugStatus)
		{
			CDagHelper::SetPlugValue(plug, gravity.Length());
		}
		plug = solverFn.findPlug(MString("stepSize"), &plugStatus);
		if (plugStatus)
		{
			CDagHelper::SetPlugValue(plug, scene->GetTimestep());
		}
	}
}
