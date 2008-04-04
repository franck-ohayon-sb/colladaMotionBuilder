/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_PHYSICS_MODEL_LIBRARY_INCLUDED__
#define __DAE_PHYSICS_MODEL_LIBRARY_INCLUDED__

#include "DaeBaseLibrary.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDPhysicsRigidBody.h"
#include "FCDocument/FCDPhysicsRigidConstraint.h"

class DaePhysicsModelLibrary : public DaeBaseLibrary
{

private:
	struct PlugInfo
	{
		unsigned int outLogicalIndex;
		MObject inNode;
		MString inAttributeName;
		unsigned int inLogicalIndex;
	};
	typedef fm::vector<PlugInfo> PlugInfoList;

public:
	DaePhysicsModelLibrary(DaeDoc* pDoc) :
		DaeBaseLibrary(pDoc) {}

	virtual const char* libraryType() { return DAE_LIBRARY_PMODEL_ELEMENT; }
	virtual const char* childType() { return DAE_PHYSICS_MODEL_ELEMENT; }
	virtual const char* librarySuffix() { return "-PhysicsModel"; }
	
	void createRigidBody(FCDSceneNode* colladaNode, 
			FCDPhysicsRigidBodyInstance* instance);
	void createRigidConstraint(FCDPhysicsRigidConstraint* rigidConstraint);

private:
	void setDegreesOfFreedom(FCDPhysicsRigidConstraint* rigidConstraint,
							 const MObject& rigidConstraintObj);
	void setupShape(const FCDPhysicsShape* shape, const MObject& transformObj, 
			MFnDagNode rigidBodyDagFn, const MObject& rigidBodyObj, const fstring& name,
			bool connectTransform, const FMMatrix44& prevTransformMatrix,
			const FMVector3& prevScale);
	MObject setupAnalyticShape(const FCDPhysicsShape* shape, 
			MFnDagNode rigidBodyDagFn, const MObject& rigidBodyObj, const fstring& name);
	void animateConstraint(FCDPhysicsRigidConstraint* rigidConstraint,
						   const MObject& rigidConstraintObj);
};

#endif // __DAE_PHYSICS_MODEL_LIBRARY_INCLUDED__
