/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"

// COLLADA Physics Library
//
#include <maya/MItDependencyNodes.h>
#include <maya/MFnGravityField.h>
#include <maya/MFnTransform.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnMesh.h>
#include "TranslatorHelpers/CDagHelper.h"
#include "FCDocument/FCDController.h"
#include "FCDocument/FCDEntity.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDPhysicsRigidConstraint.h"
#include "FCDocument/FCDPhysicsRigidBody.h"
#include "FCDocument/FCDPhysicsRigidBodyParameters.h"
#include "FCDocument/FCDPhysicsShape.h"

#if MAYA_API_VERSION >= 650
#include <maya/MIteratorType.h>
#endif

#include <limits>

MDagPath DaeNativePhysicsScene::getTransformFromRigidBody(const MDagPath& rigidBodyDagPath)
{
	// In Native, the rigid body's parent is a transform.
	MDagPath transformDagPath(rigidBodyDagPath);
	transformDagPath.pop();
	return transformDagPath;
}

bool DaeNativePhysicsScene::findPhysicsShapesForRigidBody(const MDagPath& rigidBodyDagPath, MDagPathArray& physicsShapeDagPaths)
{
	MDagPath transformDagPath = getTransformFromRigidBody(rigidBodyDagPath);
	
	// Search the DAG from the parent transform, for nodes of type
	// "physicsNodes".
	//
	MItDag dagIt(MItDag::kDepthFirst);
	dagIt.reset(transformDagPath, MItDag::kDepthFirst);
	for (; !dagIt.isDone(); dagIt.next()) 
	{
		MDagPath thisPath;
		if (dagIt.getPath(thisPath) != MS::kSuccess) 
			continue;

		MFnDagNode dagFn(thisPath);

		// Skip any non-physicsShape nodes.
		if (!isPhysicsShape(dagFn))
			continue;

		// Add to the array.
		physicsShapeDagPaths.append(thisPath);
	}

	return (physicsShapeDagPaths.length() > 0);
}

MString DaeNativePhysicsScene::getRigidBodySid(const MString& dagName)
{
	return doc->MayaNameToColladaName(dagName, false);
}	

FCDEntity* DaeNativePhysicsScene::ExportRigidBody(FCDSceneNode* sceneNode, const MDagPath& rigidBodyDagPath)
{
	FUAssert(sceneNode != NULL, return NULL);

	// Before we start exporting anything, first check
	// that the rigid body is pointing to at least one supported 
	// shape object (e.g. polygon mesh).
	MObjectArray shapeObjects;
	bool attachedToShape = findRegularShapesForRigidBody(rigidBodyDagPath, shapeObjects);
	if (!attachedToShape)
		return NULL; // probably attached to unsupported shape (e.g. NURBS)

	// Have we seen this rigid body before? 
	//
	DagPathRigidBodyMap::iterator itRigidBody = rigidBodies.find(rigidBodyDagPath);
	if (itRigidBody != rigidBodies.end()) return (*itRigidBody).second;
    MFnDagNode rigidBodyDagFn(rigidBodyDagPath);

	// If the transform doesn't have a unit scale, warn the user.
	MDagPath transformDagPath = getTransformFromRigidBody(rigidBodyDagPath);
	MTransformationMatrix transformationMatrix(transformDagPath.inclusiveMatrix());
	double scale[3];
	transformationMatrix.getScale(scale, MSpace::kTransform);
	if (scale[0] != 1 || scale[1] != 1 || scale[2] != 1)
	{
		MGlobal::displayWarning("non-unit scale may not export properly: '" + transformDagPath.partialPathName() + "'");
	}

	// Create structure for the rigid body definition.
	FCDPhysicsRigidBody* rigidBody = addRigidBody(rigidBodyDagPath);

	// Check if the rigid body is marked to use the COLLADA-specific features,
	// e.g. mass and density per shape, extended standIns (e.g. capsule or convex mesh),
	// etc.
	MStatus status = MS::kSuccess;
	bool useExtendedNativePhysics = (intAttribute(rigidBodyDagFn, "enableColladaPhysics", &status) == 1);
	if (status != MS::kSuccess)
		useExtendedNativePhysics = false;

	// Is the rigid body dynamic (aka "active" in Maya)?
	MPlug activePlug = rigidBodyDagFn.findPlug("active");
	setRigidBodyDynamic(rigidBody, boolAttribute(rigidBodyDagFn, "active"), activePlug);

	// Include one physics material, shared by all objects, since
	// Maya's physics material is set per rigid body, not per shape or per shape polygon.
	FCDPhysicsMaterial* physicsMaterial = includePhysicsMaterial(rigidBodyDagPath);
	rigidBody->GetParameters()->SetPhysicsMaterial(physicsMaterial);

	// Override center-of-mass if appropriate.
	bool lockCenterOfMass = (intAttribute(rigidBodyDagFn, "lockCenterOfMass") != 0);
	if (lockCenterOfMass)
	{
		MVector centerOfMass = vectorAttribute(rigidBodyDagFn, "centerOfMass");
		addMassPropertiesInCommonTechnique(rigidBodyDagFn, rigidBody, centerOfMass);
	}

	instantiateRigidBody(rigidBodyDagPath, rigidBody);

	// Override inertia if appropriate.
	bool lockInertia = (intAttribute(rigidBodyDagFn, "lockInertia") != 0);
	if (lockInertia)
	{
		MVector inertiaTensor = vectorAttribute(rigidBodyDagFn, "inertiaTensor");
		addInertiaPropertiesInCommonTechnique(rigidBodyDagFn, rigidBody, inertiaTensor);
	}

	bool rigidBodyOverridesStandIn = exportRigidBodyOverrides(rigidBody, rigidBodyDagPath, shapeObjects, useExtendedNativePhysics);
	if (!rigidBodyOverridesStandIn)
	{
		// If a physicsShape node is found, then only export physics shapes.
		// Otherwise, export regular shapes.
		//
		MDagPathArray physicsShapeDagPaths;
		if (findPhysicsShapesForRigidBody(rigidBodyDagPath, physicsShapeDagPaths))
		{
			for (unsigned int i = 0; i < physicsShapeDagPaths.length(); i++)
			{
				MFnDagNode shapeDagFn(physicsShapeDagPaths[i]);

				// Create a shape used for physics.
				FCDPhysicsShape* shape = addShape(rigidBody);

				// Include geometry used for physics.
				MMatrix shapeIntrinsicMatrix = includeRegularShapeGeometry(shape, rigidBodyDagPath, physicsShapeDagPaths[i]);
				exportShapeTransform(shape, physicsShapeDagPaths[i], rigidBodyDagPath, shapeIntrinsicMatrix);
				exportMassOrDensity(shape, shapeDagFn, useExtendedNativePhysics);
			}
		}
		else
		{
			// Export one shape for each geometry node used by the rigid body.
			//
			for (unsigned int i = 0; i < shapeObjects.length(); i++)
			{
				MObject shapeObject = shapeObjects[i];
				MFnDagNode shapeDagFn(shapeObject);

				// For each shape within the rigid body, export the transform
				// and physical properties.

				// [claforte] TODO: Refactor the higher-level code
				// to pass shapes as dag paths, instead of MFnDependencyNode.
				MDagPath shapeDagPath;
				MDagPath::getAPathTo(shapeDagFn.object(), shapeDagPath);

				// Create a shape used for physics.
				FCDPhysicsShape* shape = addShape(rigidBody);
				
				// Include geometry used for physics.
				MMatrix shapeIntrinsicMatrix = includeRegularShapeGeometry(shape, rigidBodyDagPath, shapeDagPath);
				exportShapeTransform(shape, shapeDagPath, rigidBodyDagPath, shapeIntrinsicMatrix);

				if (useExtendedNativePhysics)
					exportMassOrDensity(shape, shapeDagFn, useExtendedNativePhysics);
			}
		}
	}

	// Now add this to our list of exported rigid geometries and return the result.
	//
	rigidBodies[rigidBodyDagPath] = rigidBody;
	doc->GetEntityManager()->ExportEntity(rigidBodyDagFn.object(), rigidBody);
	return rigidBody;
}

// If the bounding box is empty, do not export the dimensions of the shape.
void DaeNativePhysicsScene::exportBoundingGeometry(FCDPhysicsShape* shape, ShapeType shapeType, const MBoundingBox& bbox)
{
	if (shapeType == kBox)
	{
		addBox(shape, MVector(bbox.width(), bbox.height(), bbox.depth()));
	}
	else if (shapeType == kSphere)
	{
		double diameter = max(bbox.width(), bbox.height());
		diameter = max(diameter, bbox.depth());
		addSphere(shape, (float) diameter / 2.0f);
	}
}

// Export a shape's geometry parameters. The shape must be
// included within the given rigid body.
//
// rigidBodyDagPath: dag path of the rigid body.
// dagPath: dag path of the object whose geometry should be
//			exported. This should be a mesh or other type of regular shape,
//			or a physics shape (e.g. capsule).
//
// Return: a shape's intrinsic matrix, if applicable. Otherwise identity.
//
MMatrix DaeNativePhysicsScene::includeRegularShapeGeometry(FCDPhysicsShape* shape, const MDagPath& rigidBodyDagPath,  const MDagPath& shapeDagPath)
{
	MFnDagNode shapeDagFn(shapeDagPath);

	// Check the stand in geometry associated with this
	// rigid body. Note that the enum is represented as:
	//
	//	1. none (use actual non-convex geometry)
	//	2. box (note: in native Maya, limited to cube)
	//	3. sphere
	//	4. convex hull (OBSOLETE/NOT SUPPORTED)
	//	5. capsule
	//	6. cylinder
	//	7. ellipsoid (OBSOLETE/NOT SUPPORTED)
	//
	// The first three (none, box and sphere) are supported by
	// Maya (on the rigid body only). The other types are
	// only supported through dynamic attributes.
	//
	ShapeType shapeType = getShapeType(shapeDagFn);
	if (shapeType == kNone)
	{
		// No stand-in, i.e. actual mesh.
		DaeEntity* element = doc->GetSceneGraph()->FindElement(shapeDagPath);
		if (element != NULL && element->entity != NULL)
		{
			FCDGeometry* geometry = NULL;
			if (element->entity->GetObjectType() == FCDController::GetClassType())
			{
				FCDController* controller = (FCDController*) &*(element->entity);
				geometry = controller->GetBaseGeometry();
			}
			else if (element != NULL && element->entity->GetObjectType() == FCDGeometry::GetClassType())
			{
				geometry = (FCDGeometry*) &*(element->entity);
			}
			if (geometry != NULL)
			{
				shape->CreateGeometryInstance((FCDGeometry*) &*(element->entity));
			}
		}
	}
	else
	{
		if (isPhysicsShape(shapeDagFn))
		{
			return exportPhysicsShapeGeometry(shape, shapeDagFn);
		}
		else
		{
			// We have a stand-in geometry. Create and fill it.
			// Find the bounding box of the shape, expressed in the rigid body's space.
			MBoundingBox bbox = computeShapeBoundingBoxInRigidBodySpace(shapeDagPath, rigidBodyDagPath);
			exportBoundingGeometry(shape, shapeType, bbox);
		}
	}

	return MMatrix::identity;
}

void DaeNativePhysicsScene::exportMassOrDensity(FCDPhysicsShape* shape, const MFnDagNode& dagFn,
												bool useExtendedNativePhysics)
{
	int useMassOrDensity = intAttribute(dagFn, "useMassOrDensity");

	// Set default values.
	float mass = 1;
	float density = 1;
	MVector centerOfMass;
	
	// Override mass if appropriate.
	if ((dagFn.typeName() == "rigidBody" && !useExtendedNativePhysics) || useMassOrDensity == 1)
		shape->SetMass(floatAttribute(dagFn, "mass"));

	// Override density if appropriate.
	bool overrideDensity = (useMassOrDensity == 2);
	if (useExtendedNativePhysics && overrideDensity)
		shape->SetDensity(floatAttribute(dagFn, "density"));
}

// Returns true if the rigid body overrides the geometry stand-in, in which
// case there is no need to export shapes.
bool DaeNativePhysicsScene::exportRigidBodyOverrides(FCDPhysicsRigidBody* rigidBody, const MDagPath& rigidBodyDagPath, 
													 const MObjectArray& shapeObjects, bool useExtendedNativePhysics)
{
	MFnDagNode rigidBodyDagFn(rigidBodyDagPath);

	// Override geometry if the stand-in is different than 'None'.
	ShapeType shapeType = getShapeType(rigidBodyDagFn); 
	if (shapeType != kBox && shapeType != kSphere)
		return false;

	// Compute the overall bounding box of all children.
	// bbox is the bounding box expressed in rigid body space.
	// We start with an empty bounding box, and for each shape
	// we find the local-space bounding box, then transform it
	// in rigid body space.
	//
	MBoundingBox bbox;
	for (uint i = 0; i < shapeObjects.length(); i++)
	{
		MObject shapeObject = shapeObjects[i];
		MFnDagNode shapeDagFn(shapeObject);

		// [claforte] TODO: Make sure this code is safe for instances.
		MDagPath shapeDagPath;
		MDagPath::getAPathTo(shapeDagFn.object(), shapeDagPath);

		// Expand the rigid body's bounding box to account for this shape.
		bbox.expand(computeShapeBoundingBoxInRigidBodySpace(shapeDagPath, rigidBodyDagPath));
	}

	// Export a shape stand-in.
	FCDPhysicsShape* shape = addShape(rigidBody);
	exportBoundingGeometry(shape, shapeType, bbox);

	return true;
}

void DaeNativePhysicsScene::exportSceneParameters()
{
	// Iterate over all solvers, fields.
	// [claforte] NOTE: this is a hack, since there may be more than
	// one gravity field and more than one solver in the scene.
	// For now, just assume that every rigid dynamics object is
	// connected to each other.
	//

	MVector gravityMagnitude(0.0, -980.0, 0.0);
	float timeStep = 0.083f;

#if MAYA_API_VERSION >= 650
	// Iterate only over the nodes we're interested in.
	// Only supported since Maya version 6.5?
	MIntArray typesArray;
	typesArray.append(MFn::kRigidSolver);
	typesArray.append(MFn::kGravity);
	
	MIteratorType iteratorTypes;
	iteratorTypes.setFilterList(typesArray);
	MItDependencyNodes it(iteratorTypes);
#else
	// Iterate over all nodes in the scene. Much less efficient.
	MItDependencyNodes it;
#endif

	// Export one gravity and one rigid solver's step size
	bool hasFoundGravity = false, hasFoundSolver = false;
	for (; !it.isDone() && (!hasFoundGravity || !hasFoundSolver); it.next())
	{
		MObject	obj = it.item();
		MFnDependencyNode objFn(obj);

		if (obj.apiType() == MFn::kGravity && !hasFoundGravity)
		{
			MFnGravityField gravField(obj);
			gravityMagnitude = gravField.magnitude() * gravField.direction();
			hasFoundGravity = true;
		}
		else if (obj.apiType() == MFn::kRigidSolver && !hasFoundSolver)
		{
			timeStep = floatAttribute(objFn, "stepSize");
			hasFoundSolver = true;
		}
	}

	// Export the found values (or default).
	setSceneInfo(timeStep, gravityMagnitude);
}

MBoundingBox DaeNativePhysicsScene::computeShapeBoundingBoxInRigidBodySpace(const MDagPath& shapeDagPath, const MDagPath& rigidBodyDagPath)
{
	// Get the matrix from rigid body space to shape local space.
	MMatrix shapeTransform = computeShapeTransform(shapeDagPath, rigidBodyDagPath);

	// Get the shape's local-space bounding box, then transform it
	// in rigid body space.
	MFnDagNode shapeDagFn(shapeDagPath);
	MBoundingBox shapeBbox = shapeDagFn.boundingBox();

	shapeBbox.transformUsing(shapeTransform);

	return shapeBbox;
}

void DaeNativePhysicsScene::exportAttachment(FCDPhysicsRigidConstraint* rigidConstraint, FCDPhysicsRigidBody* rigidBody, const MDagPath& constraintDagPath, const MDagPath& rigidBodyDagPath, bool isRefAttachment)
{
	MFnDagNode constraintDagFn(constraintDagPath);

	// Link the rigid constraint instance attachment to the rigid body or <scene>/<node>.
	bool attachedToScene = rigidBody == NULL;
	if (attachedToScene)
	{
		// The rigid constraint is attached to a visual node,
		// which happens to be the parent of the constraint in Maya.
		MDagPath constraintParentTransform = constraintDagPath;
		constraintParentTransform.pop();
		if (constraintParentTransform.partialPathName().length() > 0)
		{
			DaeEntity* sceneNode = doc->GetSceneGraph()->FindElement(constraintParentTransform);
			if (sceneNode != NULL && sceneNode->entity != NULL && sceneNode->entity->GetObjectType() == FCDSceneNode::GetClassType())
			{
				if (isRefAttachment)
				{
					rigidConstraint->SetReferenceNode((FCDSceneNode*) &*(sceneNode->entity));
				}
				else
				{
					rigidConstraint->SetTargetNode((FCDSceneNode*) &*(sceneNode->entity));
				}
			}
		}
		// else: parent is the world, should we export something?
	}
	else if (isRefAttachment)
	{
		rigidConstraint->SetReferenceRigidBody(rigidBody);
	}
	else
	{
		rigidConstraint->SetTargetRigidBody(rigidBody);
	}

	// Get the constraint's initial position and orientation. Those are
	// specified in the local space of the constraint.
	MVector constraintInitPos = vectorAttribute(constraintDagFn, "initialPosition");
	MVector constraintInitOri = vectorAttribute(constraintDagFn, "initialOrientation");

	// Assemble the initial, rigid constraint local space matrix.
	MTransformationMatrix transformationMatrix;
	double rot[3];
	rot[0] = constraintInitOri.x; 
	rot[1] = constraintInitOri.y;
	rot[2] = constraintInitOri.z;
	transformationMatrix.setTranslation(constraintInitPos, MSpace::kTransform);
	transformationMatrix.setRotation(rot, MTransformationMatrix::kXYZ, MSpace::kTransform);

	MMatrix matrix = transformationMatrix.asMatrix();	 // in local space

	// [claforte] TODO: Transform in world space. (only done for nail/spring?)
	// Reversed?
	//matrix = matrix * constraintDagFn.transformationMatrix();
	
	if (!attachedToScene)
	{
		// Transform in rigid body space.
		MDagPath rigidBodyTransformDagPath = getTransformFromRigidBody(rigidBodyDagPath);
		matrix = matrix * rigidBodyTransformDagPath.inclusiveMatrixInverse();
	}

	// Export translate/rotates.
	FCDTransformContainer* transforms = isRefAttachment ? &rigidConstraint->GetTransformsRef() : &rigidConstraint->GetTransformsTar();
	addTranslateRotates(transforms, matrix);	
}

bool DaeNativePhysicsScene::isRigidConstraint(const MFnDependencyNode& depFn) const
{
	if (depFn.typeName() != "rigidConstraint")
		return false;

	// Not all constraints are supported for export.
	// Verify that this type of constraint is supported.
	MStatus status;
	int constraintType = intAttribute(depFn, "constraintType", &status);
	if (status != MS::kSuccess || constraintType < 0 || constraintType > 8)
		return false;
	
	switch (constraintType)
	{
	case 1:	// pin
		
	case 2: // nail

	case 3: // directionalHinge

	case 4: // old hinge.. no longer used?
	case 8: // new hinge.

	case 6: // old spring, now replaced by 7
	case 7: // "multispring", now default spring.

		return true;

	case 5: // barrier, not supported yet.
	default:
		return false;
	}
}

void DaeNativePhysicsScene::exportConstraintParams(FCDPhysicsRigidConstraint* rigidConstraint, MFnDagNode& constraintDagFn)
{
	// Defaults.
	MVector defaultRotLimitMin(0.0, 0.0, 0.0);
	MVector defaultRotLimitMax(0.0, 0.0, 0.0);
	MVector defaultTransLimitMin(0, 0, 0);
	MVector defaultTransLimitMax(0, 0, 0);
	bool defaultInterpenetrate = false;
	bool defaultEnabled = true;

	// Current values.
	MVector rotLimitMin = defaultRotLimitMin;
	MVector rotLimitMax = defaultRotLimitMax;
	MVector transLimitMin = defaultTransLimitMin;
	MVector transLimitMax = defaultTransLimitMax;
	bool interpenetrate = defaultInterpenetrate;
	bool enabled = defaultEnabled;

	// Find the type of constraint.
	MStatus status;
	int constraintType = intAttribute(constraintDagFn, "constraintType", &status);
	if (status != MS::kSuccess || constraintType < 0 || constraintType > 8)
	{
		MGlobal::displayWarning("Invalid constraint type detected. Skipping.");
		return;
	}

	double infinity = std::numeric_limits<double>::infinity();

	// set up parameters dependent on the constraint type.
	//
	bool exportSpring = false;
	switch (constraintType)
	{
	case 1:	// pin
	case 2: // nail
		rotLimitMin.x = rotLimitMin.y = rotLimitMin.z = -infinity;
		rotLimitMax.x = rotLimitMax.y = rotLimitMax.z =  infinity;
		break;	// default case, nothing to do.

	case 3: // directionalHinge
	case 4: // old hinge.. no longer used?
	case 8: // new hinge.
		rotLimitMin.z = -infinity;
		rotLimitMax.z =  infinity;
		break;

	case 6: // old spring, now replaced by 7
	case 7: // "multispring", now default spring.
		rotLimitMin.x = rotLimitMin.y = rotLimitMin.z = -infinity;
		rotLimitMax.x = rotLimitMax.y = rotLimitMax.z =  infinity;
		exportSpring = true;
		break;	// nothing for now.
	}

	// set interpenetrate flag.
	interpenetrate = boolAttribute(constraintDagFn, "interpenetrate", &status);
	if (status != MS::kSuccess)
		MGlobal::displayWarning("Inexistent rigid constraint 'interpenetrate' attribute.");

	// set enabled flag.
	enabled = boolAttribute(constraintDagFn, "constrain", &status);
	if (status != MS::kSuccess)
		MGlobal::displayWarning("Inexistent rigid constraint 'constrain' attribute.");

	setRigidConstraintInfo(rigidConstraint, enabled, interpenetrate, transLimitMin, transLimitMax, rotLimitMin, rotLimitMax);
	exportConstraintSpringParams(rigidConstraint, constraintDagFn);
}

void DaeNativePhysicsScene::exportConstraintSpringParams(FCDPhysicsRigidConstraint* rigidConstraint, MFnDagNode& constraintDagFn)
{
	MStatus status;
	float stiffness = floatAttribute(constraintDagFn, "springStiffness", &status);
	if (status != MS::kSuccess)
	{
		MGlobal::displayWarning("Inexistent rigid constraint 'springStiffness' attribute.");
		stiffness = 1.0;
	}
	rigidConstraint->SetSpringLinearStiffness(stiffness);
	
	float damping = floatAttribute(constraintDagFn, "springDamping", &status);
	if (status != MS::kSuccess)
	{
		MGlobal::displayWarning("Inexistent rigid constraint 'springDamping' attribute.");
		damping = 1.0;
	}
	rigidConstraint->SetSpringLinearDamping(damping);

	float restLength = floatAttribute(constraintDagFn, "springRestLength", &status);
	if (status != MS::kSuccess)
	{
		MGlobal::displayWarning("Inexistent rigid constraint 'springRestLength' attribute.");
		restLength = 1.0;
	}
	rigidConstraint->SetSpringLinearTargetValue(restLength);
}


//
// [claforte] TODO: Code the import part. For now, since the Physics support
// is at a prototype stage, it's not a priority.
//


