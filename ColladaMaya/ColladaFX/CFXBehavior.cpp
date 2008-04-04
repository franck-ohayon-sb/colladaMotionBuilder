/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "ColladaFX/CFXBehavior.h"
#include <maya/MFnNumericAttribute.h>

CFXBehavior::CFXBehavior(void)
{
}

CFXBehavior::~CFXBehavior(void)
{
}

//
//	Description:
//		Returns a new instance of this class
//
void *CFXBehavior::creator()
{
	return new CFXBehavior;
}

bool CFXBehavior::shouldBeUsedFor(	MObject &sourceNode, MObject &destinationNode,
									MPlug &sourcePlug, 
									MPlug &destinationPlug)
{
	bool result = false;

	if (MFnDependencyNode(sourceNode).typeName() == "colladafxShader")
	{
        if (MFnDependencyNode(destinationNode).typeName() == "colladafxPasses")
		{
			result = true;
		}
	}
	else if (sourceNode.hasFn(MFn::kTransform))
	{
		if (MFnDependencyNode(destinationNode).typeName() == "colladafxShader")
		{
			result = true;
		}
	}
		
	return result;
}


MStatus CFXBehavior::connectNodeToAttr(MObject &sourceNode, MPlug &destinationPlug, bool force)
{
	MStatus result = MS::kFailure;
	MFnDependencyNode src(sourceNode);
	
	MObject attribute = destinationPlug.attribute();
	
	MFnNumericAttribute fattr(attribute);

	if (src.typeName() == "colladafxShader") 
	{
		MString cmd = "connectAttr ";
		if (force) cmd += "-f ";
		cmd += src.findPlug("outColor").name() + " " + destinationPlug.name();
			
		result = MGlobal::executeCommand(cmd);
	}
	else if (src.typeName() == "transform" && fattr.unitType() == MFnNumericData::k3Float) 
	{
		MString cmd = "connectAttr ";
		if (force) cmd += "-f ";
		cmd += src.findPlug("translate").name() + " " + destinationPlug.name();
			
		result = MGlobal::executeCommand(cmd);
	}
	return result;
}
