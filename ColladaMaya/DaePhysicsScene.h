/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_PHYSICS_SCENE_INCLUDED__
#define __DAE_PHYSICS_SCENE_INCLUDED__

class DaeDoc;
class DaePhysicsModelLibrary;
class FCDTransform;
class FCDPhysicsMaterial;
class FCDPhysicsModel;
class FCDPhysicsModelInstance;
class FCDPhysicsScene;
class FCDPhysicsShape;
class FCDPhysicsRigidBody;
class FCDPhysicsRigidBodyInstance;
class FCDPhysicsRigidConstraint;
class FCDPhysicsRigidConstraintInstance;
class FCDSceneNode;
class MFnDependencyNode;
typedef fm::map<MDagPath, FCDPhysicsRigidBody*> DagPathRigidBodyMap;
typedef FUObjectContainer<FCDTransform> FCDTransformContainer;

class DaePhysicsScene
{
public:
	DaePhysicsScene(DaeDoc* pDoc);
	
	virtual ~DaePhysicsScene();


	// Cloned from Nima's PhysicsShape::shapeType. Defined here since
	// physics shapes are supported for extended Maya native dynamics.
	//
	typedef enum {
		kNone,	// used for stand-in.
		kBox,
		kSphere,
		kObsoleteConvexHull,
		kCapsule,
		kUnsupportedCylinder,
		kObsoleteEllipsoid,
	} ShapeType;

	// Returns true if the node is of type "physicsShape", defined in
	// the ColladaPhysics plug-in.
	bool isPhysicsShape(const MFnDependencyNode& node);

	// Name of the underlying physics engine, e.g. "Ageia" or
	// "MayaNative".
	virtual MString engineName() = 0;

	MString physicsSceneId() { return engineName() + "PhysicsScene"; }
	MString physicsModelId() { return engineName() + "PhysicsModel"; }

	void finalize();

	virtual bool isRigidConstraint(const MFnDependencyNode& depFn) const = 0;

protected:
	// Returns a pointer to the only physics model, from which
	// rigid bodies and constraints are defined.
	FCDPhysicsModel*			physicsModel();

	// Returns a pointer to the only <instance_physics_model>, from which
	// rigid bodies and constraints are instanced.
	FCDPhysicsModelInstance*	physicsModelInstance();

	FCDPhysicsScene*		physicsScene();

	DaeDoc*						doc;

	// A lookup table of elements we've already processed
	DagPathRigidBodyMap			rigidBodies;

	// Currently, Maya and ColladaMaya don't exploit
	// the concept of hierarchical physics models and separate
	// physics scenes. A single physics model is used to
	// export all rigid bodies and constraints for a given engine
	// (i.e. Nima or Native). Likewise, we use a single physics 
	// scene per document.
	//
	FCDPhysicsModel*			fpPhysicsModel;

	// A pointer to the only <instance_physics_model> used
	// for this scene, from which rigid bodies and constraints 
	// are instanced and associated with nodes in the visual scene.
	FCDPhysicsModelInstance*	fpPhysicsModelInstance;

	
	//--------------------------------------------------------
	// Virtual methods that should generally be overloaded by
	// derived, engine-specific scene classes.
	//
	virtual MDagPath getTransformFromRigidBody(const MDagPath& rigidBodyDagPath) = 0;
	
	// Called during the creation of the physics scene, to
	// export engine-specific scene parameters.
	virtual void exportSceneParameters() = 0;

	virtual void exportConstraintParams(FCDPhysicsRigidConstraint* rigidConstraint, MFnDagNode& constraintDagFn) = 0;

	virtual MString getRigidBodySid(const MString& dagName) = 0;

	virtual void exportAttachment(FCDPhysicsRigidConstraint* rigidConstraint,
									FCDPhysicsRigidBody* rigidBody,
									const MDagPath& constraintDagPath,
									const MDagPath& rigidBodyDagPath,
									bool isReferenceAttachment) = 0;


	//--------------------------------------------------------
	// Methods to add shared across engines.
	//
	FCDPhysicsRigidBodyInstance* addInstanceRigidBody(FCDPhysicsRigidBody* rigidBody, FCDSceneNode* target);
	FCDPhysicsRigidConstraintInstance* addInstanceRigidConstraint(FCDPhysicsRigidConstraint* rigidConstraint);
	FCDPhysicsMaterial* addPhysicsMaterial(const MDagPath& dagPath);
	FCDPhysicsRigidBody* addRigidBody(const MDagPath& dagPath);
	FCDPhysicsRigidConstraint* addRigidConstraint(const MDagPath& dagPath);
	FCDPhysicsShape* addShape(FCDPhysicsRigidBody* rigidBody);


	//--------------------------------------------------------
	// Methods to add low-level XML elements.
	//

	void matrixToTranslateRotate(const MMatrix& matrix, MVector& translate, MEulerRotation& rotate);
	void addTranslateRotates(FCDTransformContainer* transforms, const MVector& translate, const MEulerRotation& rotate, bool addEmptyTranslate = false, bool addEmptyRotate = false);
	void addTranslateRotates(FCDTransformContainer* transforms, const MMatrix& matrix);
	void addMassPropertiesInCommonTechnique(const MFnDagNode& dagFn, FCDPhysicsRigidBody* rigidBody, const MVector& centerOfMass = MVector());
	void addInertiaPropertiesInCommonTechnique(const MFnDagNode& dagFn, FCDPhysicsRigidBody* rigidBody, 
			const MVector& inertia = MVector());


	//--------------------------------------------------------
	// Methods to add shape nodes.
	//
		
	void addBox(FCDPhysicsShape* shape, const MVector& size);
	void addSphere(FCDPhysicsShape* shape, float radius);
	void addCapsule(FCDPhysicsShape* shape, float radius,
							const MVector& point1, const MVector& point2,
							MMatrix& intrinsicShapeMatrix);

	//--------------------------------------------------------
	// Methods to add common techniques.
	//
	void setRigidBodyDynamic(FCDPhysicsRigidBody* rigidBody, bool isDynamic, 
			MPlug activePlug);
	void setRigidConstraintInfo(FCDPhysicsRigidConstraint* rigidConstraint, bool enabled, bool interpenetrate, const MVector& transLimitMin, const MVector& transLimitMax, const MVector& rotLimitMin, const MVector& rotLimitMax);
	void setSceneInfo(float timestep, const MVector& gravity);

	//--------------------------------------------------------
	// Export methods that involve some Maya structure traversal
	// but that happen to be shared between the Native and Ageia
	// implementations.
	//
	bool findRegularShapesForRigidBody(const MDagPath& rigidBodyDagPath, MObjectArray& shapeObjects);
	FCDPhysicsMaterial* includePhysicsMaterial(const MDagPath& dagPath);
	void instantiateRigidBody(const MDagPath& rigidBodyDagPath, FCDPhysicsRigidBody* rigidBody);
	MMatrix exportPhysicsShapeGeometry(FCDPhysicsShape* shape, const MFnDagNode& shapeDagFn);
	MMatrix	computeShapeTransform(const MDagPath& shapeDagPath, const MDagPath& rigidBodyDagPath);
	void exportShapeTransform(FCDPhysicsShape* shape, const MDagPath& shapeDagPath, const MDagPath& rigidBodyDagPath, const MMatrix& shapeIntrinsicMatrix);
	ShapeType getShapeType(const MFnDependencyNode& depFn, MStatus* pOutStatus = NULL);
	virtual void exportConstraint(const MDagPath& dagPath);

	//--------------------------------------------------------
	// Engine-specific constants.
	//

	// Attribute that can be used to find regular shapes
	// from a rigid body.
	MString fRegularShapeAttributeOnRigidBody;
	MString fOverrideMassOrDensityAttrName;
	MString fConstraintPositionAttrName;
	MString fConstraintOrientationAttrName;
};


// [claforte] Utility functions, which belong somewhere else
// but I don't have the time to figure out where. Note that are more
// complete and recent version of these utility methods is
// part of Nima.
//
float floatAttribute(const MFnDependencyNode& node, const MString& attributeName, MStatus* pReturnStatus = NULL);
int intAttribute(const MFnDependencyNode& node, const MString& attributeName, MStatus* pReturnStatus = NULL);
bool boolAttribute(const MFnDependencyNode& node, const MString& attributeName, MStatus* pReturnStatus = NULL);
MVector vectorAttribute(const MFnDependencyNode& node, const MString& attributeName, MStatus* pReturnStatus = NULL);

#endif // __DAE_PHYSICS_SCENE_INCLUDED__
