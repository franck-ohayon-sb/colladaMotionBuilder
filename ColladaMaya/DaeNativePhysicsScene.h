/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_PHYSICS_NATIVE_SCENE_INCLUDED__
#define __DAE_PHYSICS_NATIVE_SCENE_INCLUDED__

class DaeDoc;
class MDagPath;
class MFnDagNode;
class MFnDependencyNode;
class MObjectArray;
class MBoundingBox;

#include "DaePhysicsScene.h"

class DaeNativePhysicsScene : public DaePhysicsScene
{
public:
	DaeNativePhysicsScene(DaeDoc* pDoc)
		: DaePhysicsScene(pDoc)
	{ 
		fRegularShapeAttributeOnRigidBody = "inputGeometryMsg";
		fOverrideMassOrDensityAttrName = "useMassOrDensity";
		fConstraintPositionAttrName = "initialPosition";
		fConstraintOrientationAttrName = "initialOrientation";
	}

	virtual MString engineName() { return "MayaNative"; }

	// Add a rigid body to this library and return the xml node.
	FCDEntity* ExportRigidBody(FCDSceneNode* sceneNode, const MDagPath& dagPath);

	virtual bool isRigidConstraint(const MFnDependencyNode& depFn) const;

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
	bool				findPhysicsShapesForRigidBody(const MDagPath& rigidBodyDagPath, MDagPathArray& physicsShapeDagPaths);

	void				exportBoundingGeometry(FCDPhysicsShape* shape, ShapeType shapeType, const MBoundingBox& bbox);

	MMatrix				includeRegularShapeGeometry(FCDPhysicsShape* shape, const MDagPath& rigidBodyDagPath, 
											   const MDagPath& shapeDagPath);
	
	void				exportMassOrDensity(FCDPhysicsShape* shape, const MFnDagNode& dagFn,
											bool useExtendedPhysicsModel);

	// Returns true if the rigid body overrides the geometry stand-in, in which
	// case there is no need to export shapes.
	bool				exportRigidBodyOverrides(FCDPhysicsRigidBody* rigidBody, const MDagPath& dagPath, 
												 const MObjectArray& shapeObjects, bool useExtendedPhysicsModel);

	MBoundingBox		computeShapeBoundingBoxInRigidBodySpace(const MDagPath& shapeDagPath, 
																const MDagPath& rigidBodyDagPath);

	void				exportConstraintSpringParams(FCDPhysicsRigidConstraint* rigidConstraint, MFnDagNode& constraintDagFn);
};


#endif // __DAE_PHYSICS_NATIVE_SCENE_INCLUDED__
