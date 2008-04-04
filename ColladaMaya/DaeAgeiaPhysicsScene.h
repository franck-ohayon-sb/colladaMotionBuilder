/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_AGEIA_PHYSICS_SCENE_INCLUDED__
#define __DAE_AGEIA_PHYSICS_SCENE_INCLUDED__

class DaeDoc;
class MDagPath;
class MFnDagNode;
class MFnDependencyNode;
class MObjectArray;
class MBoundingBox;

#include "DaePhysicsScene.h"

class DaeAgeiaPhysicsScene : public DaePhysicsScene
{
public:
	DaeAgeiaPhysicsScene(DaeDoc* pDoc)
		: DaePhysicsScene(pDoc)
	{ 
		fRegularShapeAttributeOnRigidBody = "shapes";
		fOverrideMassOrDensityAttrName = "overrideMassOrDensity";
		fConstraintPositionAttrName = "translate";
		fConstraintOrientationAttrName = "rotate";
	}

	virtual MString engineName() { return "Ageia"; }

	// Add a rigid body to this library and return the xml node.
	FCDEntity* ExportRigidBody(FCDSceneNode* sceneNode, const MDagPath& dagPath);

	virtual bool isRigidBody(const MFnDependencyNode& depFn);
	virtual bool isRigidSolver(const MFnDependencyNode& depFn);
	virtual bool isRigidConstraint(const MFnDependencyNode& depFn) const;

	virtual void exportShapeGeometries(FCDSceneNode* sceneNode, const MDagPath& shapeDagPath, FCDPhysicsShape* rigidBody, bool convex);
	

protected:

	//--------------------------------------------------------
	// Virtual methods
	//
	virtual MDagPath getTransformFromRigidBody(const MDagPath& rigidBodyDagPath);
	virtual void exportSceneParameters();
	virtual void exportConstraintParams(FCDPhysicsRigidConstraint* rigidConstraint, MFnDagNode& constraintDagFn);
	virtual MString getRigidBodySid(const MString& dagName);
	virtual void exportAttachment(FCDPhysicsRigidConstraint* rigidConstraint,
									FCDPhysicsRigidBody* rigidBody, 
									const MDagPath& constraintDagPath,
									const MDagPath& rigidBodyDagPath,
									bool isReferenceAttachment);

	//--------------------------------------------------------
	// Engine-specific methods.
	//
	bool findPhysicsShapesForRigidBody(const MDagPath& rigidBodyDagPath, MDagPathArray& physicsShapeDagPaths);
	void exportMassOrDensity(FCDPhysicsShape* shape, const MFnDagNode& dagFn);
};


#endif // __DAE_AGEIA_PHYSICS_SCENE_INCLUDED__
