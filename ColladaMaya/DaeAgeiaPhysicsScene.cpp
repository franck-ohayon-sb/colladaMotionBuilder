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
#include "FCDocument/FCDPhysicsRigidConstraint.h"
#include "FCDocument/FCDPhysicsRigidBody.h"
#include "FCDocument/FCDPhysicsRigidBodyParameters.h"
#include "FCDocument/FCDPhysicsShape.h"
#include "FCDocument/FCDAnimated.h"

#if MAYA_API_VERSION >= 650
#include <maya/MIteratorType.h>
#endif
#include <limits>

MDagPath DaeAgeiaPhysicsScene::getTransformFromRigidBody(const MDagPath& rigidBodyDagPath)
{
	// In Nima, the rigid body is a transform.
	MDagPath transformDagPath(rigidBodyDagPath);
	return transformDagPath;
}

bool DaeAgeiaPhysicsScene::findPhysicsShapesForRigidBody(const MDagPath& rigidBodyDagPath, MDagPathArray& physicsShapeDagPaths)
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

MString DaeAgeiaPhysicsScene::getRigidBodySid(const MString& dagName)
{
	return doc->MayaNameToColladaName(dagName, false) + "-RB";
}	

void DaeAgeiaPhysicsScene::exportShapeGeometries(FCDSceneNode* sceneNode, const MDagPath& shapeDagPath, FCDPhysicsShape* shape, bool convex)
{
	if (doc->GetSceneGraph()->EnterDagNode(sceneNode, shapeDagPath, false))
	{
		doc->GetGeometryLibrary()->ExportMesh(sceneNode, shapeDagPath, true, false);
	}
	DaeEntity* element = doc->GetSceneGraph()->FindElement(shapeDagPath);
	if (element != NULL && element->entity != NULL)
	{
		FCDGeometry* geometry = NULL;
		if (element->entity->HasType(FCDController::GetClassType()))
		{
			FCDController* controller = (FCDController*) &*(element->entity);
			geometry = controller->GetBaseGeometry();
		}
		else if (element != NULL && element->entity->HasType(FCDGeometry::GetClassType()))
		{
			geometry = (FCDGeometry*) &*(element->entity);
		}
		if (geometry != NULL)
		{
			shape->CreateGeometryInstance((FCDGeometry*) &*(element->entity), convex);
		}
	}
}

FCDEntity* DaeAgeiaPhysicsScene::ExportRigidBody(FCDSceneNode* sceneNode, const MDagPath& rigidBodyDagPath)
{
	// Before we start exporting anything, first check
	// that the rigid body is pointing to at least one supported 
	// regular shape (e.g. polygon mesh) or physics shape.
	MObjectArray shapeObjects;
	MDagPathArray physicsShapeDagPaths;
	findRegularShapesForRigidBody(rigidBodyDagPath, shapeObjects);
	findPhysicsShapesForRigidBody(rigidBodyDagPath, physicsShapeDagPaths);

	// are there any shapes?
	if ((shapeObjects.length() <= 0) && (physicsShapeDagPaths.length() <= 0))
	{
		return NULL;
	}

	// Have we seen this rigid body before?
    MFnDagNode rigidBodyDagFn(rigidBodyDagPath);
	
	DagPathRigidBodyMap::iterator itRigidBody = rigidBodies.find(rigidBodyDagPath);
	if (itRigidBody != rigidBodies.end()) return (*itRigidBody).second;

	// If the transform doesn't have a unit scale, warn the user.
	MDagPath transformDagPath = getTransformFromRigidBody(rigidBodyDagPath);
	MTransformationMatrix transformationMatrix(transformDagPath.inclusiveMatrix());
	double scale[3];
	transformationMatrix.getScale(scale, MSpace::kTransform);
	if (!IsEquivalent(FMVector3((float) scale[0], (float) scale[1], (float) scale[2]), FMVector3::One))
		MGlobal::displayWarning("non-unit scale may not export properly: '" + transformDagPath.partialPathName() + "'");

	// Create XML structure for the rigid body definition.
	FCDPhysicsRigidBody* rigidBody = addRigidBody(rigidBodyDagPath);

	// Is the rigid body dynamic (aka "active" in Maya)?
	MPlug activePlug = rigidBodyDagFn.findPlug("active");
	setRigidBodyDynamic(rigidBody, boolAttribute(rigidBodyDagFn, "active"),
			activePlug);
	
	// Include one physics material, shared by all objects, since
	// Maya's physics material is set per rigid body, not per shape or per shape polygon.
	FCDPhysicsMaterial* material = includePhysicsMaterial(rigidBodyDagPath);
	rigidBody->GetParameters()->SetPhysicsMaterial(material);

	// For each shape within the rigid body, export the transform
	// and physical properties. Start with physics shapes, then
	// regular shapes.
	//

	for (uint i = 0; i < physicsShapeDagPaths.length(); i++)
	{
		MFnDagNode shapeDagFn(physicsShapeDagPaths[i]);

		// Create a shape used for physics.
		FCDPhysicsShape* shape = addShape(rigidBody);
		MMatrix shapeIntrinsicMatrix = exportPhysicsShapeGeometry(shape, shapeDagFn);
		exportShapeTransform(shape, physicsShapeDagPaths[i], rigidBodyDagPath, shapeIntrinsicMatrix);
	}

	float totalMass = 0.0f;
	bool convex = (intAttribute(rigidBodyDagFn, "geometryType") == 0); //convex: 0, non-convex: 1
	for (uint i = 0; i < shapeObjects.length(); i++)
	{
		MObject shapeObject = shapeObjects[i];
		MFnDagNode shapeDagFn(shapeObject);

		// [claforte] TODO: Refactor the higher-level code
		// to pass shapes as dag paths, instead of MFnDependencyNode.
		MDagPath shapeDagPath;
		MDagPath::getAPathTo(shapeDagFn.object(), shapeDagPath);

		// Create a shape used for physics.
		FCDPhysicsShape* shape = addShape(rigidBody);
		exportShapeGeometries(sceneNode, shapeDagPath, shape, convex);
		exportMassOrDensity(shape, rigidBodyDagFn);
		exportShapeTransform(shape, shapeDagPath, rigidBodyDagPath, MMatrix::identity);
		totalMass += shape->GetMass();
	}
	rigidBody->GetParameters()->SetMass(totalMass);

	// Export the center of mass, which may be animated.
	MVector centerOfMass = vectorAttribute(rigidBodyDagFn, "centerOfMass");
	addMassPropertiesInCommonTechnique(rigidBodyDagFn, rigidBody, centerOfMass);

	// Export the inertia tensor, which may be animated.
	MVector inertiaTensor = vectorAttribute(rigidBodyDagFn, "inertiaTensor");
	addInertiaPropertiesInCommonTechnique(rigidBodyDagFn, rigidBody, inertiaTensor);

	instantiateRigidBody(rigidBodyDagPath, rigidBody);

	// Now add this to our list of exported rigid geometries and return the result.
	//
	rigidBodies[rigidBodyDagPath] = rigidBody;
	doc->GetEntityManager()->ExportEntity(rigidBodyDagFn.object(), rigidBody);
	return rigidBody;
}

bool DaeAgeiaPhysicsScene::isRigidBody(const MFnDependencyNode& depFn)
{
	const MTypeId RigidBodyTypeId(0x0010BC04);
	return (depFn.typeId() == RigidBodyTypeId);
}

bool DaeAgeiaPhysicsScene::isRigidSolver(const MFnDependencyNode& depFn)
{
	const MTypeId RigidSolverTypeId(0x0010BC00);
	return (depFn.typeId() == RigidSolverTypeId);
}

bool DaeAgeiaPhysicsScene::isRigidConstraint(const MFnDependencyNode& depFn) const
{
	const MTypeId RigidConstraintTypeId(0x0010BC06);
	return (depFn.typeId() == RigidConstraintTypeId);
}

void DaeAgeiaPhysicsScene::exportSceneParameters()
{
	// Iterate over all nodes in the scene.
	MItDependencyNodes it;

	for (; !it.isDone(); it.next())
	{
		MObject	obj = it.item();
		MFnDependencyNode objFn(obj);

		if (isRigidSolver(objFn))
		{
			MVector gravity = vectorAttribute(objFn, "gravityDirection") * floatAttribute(objFn, "gravityMagnitude");
			setSceneInfo(floatAttribute(objFn, "stepSize"), gravity);
			break;
		}
	}
}

void DaeAgeiaPhysicsScene::exportMassOrDensity(FCDPhysicsShape* shape, const MFnDagNode& dagFn)
{
	int useMassOrDensity = intAttribute(dagFn, fOverrideMassOrDensityAttrName);

	// Set default values.
	MVector centerOfMass;
	
	float volume = shape->CalculateVolume();
	if (useMassOrDensity == 0 || useMassOrDensity == 1)
	{
		shape->SetMass(floatAttribute(dagFn, "mass"));
		shape->SetDensity(shape->GetMass() / volume);
	}
	else if (useMassOrDensity == 2)
	{
		shape->SetDensity(floatAttribute(dagFn, "density"));
		shape->SetMass(shape->GetDensity() * volume);
	}
	else
	{
		shape->SetDensity(1.0f);
		shape->SetMass(shape->GetDensity() * volume);
	}

#if 0 
	// [claforte] Not yet supported by Nima.
	// Override center-of-mass if appropriate.
	bool lockCenterOfMass = (intAttribute(dagFn, "lockCenterOfMass") != 0);
	if (lockCenterOfMass)
		centerOfMass = vectorAttribute(dagFn, "centerOfMass");
#endif //0
}

void getConstraintParam(const MString& attr, int component, bool isAngular,
						MFnDagNode& constraintDagFn,
						MVector& minVect, MVector& maxVect)
{
	MStatus status;

	int motionType = intAttribute(constraintDagFn, attr, &status);
	if (status != MS::kSuccess)
	{
		MString errMsg = "attribute ";
		errMsg += attr;
		errMsg += " not found in rigid constraint '";
		errMsg += constraintDagFn.name();
		errMsg += "'.";
		MGlobal::displayWarning(errMsg);
		
		// leave minVal and maxVal unmodified.
		return;
	}

	if (motionType == 0) // locked
	{
		minVect[component] = maxVect[component] = 0.0;
	}
	else if (motionType == 1) // limited
	{
		// Find the actual limit. The attribute name depends on the
		// type of motion.
		if (isAngular)
		{
			if (component == 1 || component == 2)	// swing1 or swing2
			{
				MString limitValueAttr = "swing";
				limitValueAttr += component;
				limitValueAttr += "LimitValue";
				maxVect[component] = floatAttribute(constraintDagFn, limitValueAttr, &status);
				minVect[component] = -maxVect[component];
			}
			else // component == 0, which means twist
			{
				maxVect[component] = floatAttribute(constraintDagFn, "twistHighLimitValue", &status);
				minVect[component] = floatAttribute(constraintDagFn, "twistLowLimitValue", &status);
			}
		}
		else
		{
			maxVect[component] = floatAttribute(constraintDagFn, "linearLimitValue", &status);
			minVect[component] = -maxVect[component];	
		}
	}
	else // free
	{
		const double infinity = std::numeric_limits<double>::infinity();
		minVect[component] = -infinity;
		maxVect[component] = infinity;
	}
}

void DaeAgeiaPhysicsScene::exportConstraintParams(FCDPhysicsRigidConstraint* rigidConstraint, MFnDagNode& constraintDagFn)
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

	MStatus status;

	// Export the motionX, Y, Z and twist/swing1/swing2.
	//
	getConstraintParam("motionX", 0, false, constraintDagFn, transLimitMin, transLimitMax);
	getConstraintParam("motionY", 1, false, constraintDagFn, transLimitMin, transLimitMax);
	getConstraintParam("motionZ", 2, false, constraintDagFn, transLimitMin, transLimitMax);
	getConstraintParam("motionTwist", 0, true, constraintDagFn, rotLimitMin, rotLimitMax);
	getConstraintParam("motionSwing1", 1, true, constraintDagFn, rotLimitMin, rotLimitMax);
	getConstraintParam("motionSwing2", 2, true, constraintDagFn, rotLimitMin, rotLimitMax);

	// Set interpenetrate flag.
	interpenetrate = boolAttribute(constraintDagFn, "interpenetrate", &status);
	if (status != MS::kSuccess)
		MGlobal::displayWarning("Inexistent rigid constraint 'interpenetrate' attribute.");
	else
	{
		MPlug interpenetratePlug = constraintDagFn.findPlug("interpenetrate");
		doc->GetAnimationLibrary()->AddPlugAnimation(interpenetratePlug, rigidConstraint->GetInterpenetrate(), kSingle);
	}

	// Set enabled flag.
	enabled = boolAttribute(constraintDagFn, "constrain", &status);
	if (status != MS::kSuccess) MGlobal::displayWarning("Inexistent rigid constraint 'constrain' attribute.");
	else
	{
		MPlug enabledPlug = constraintDagFn.findPlug("constrain");
		doc->GetAnimationLibrary()->AddPlugAnimation(enabledPlug, rigidConstraint->GetEnabled(), kSingle);
	}

	setRigidConstraintInfo(rigidConstraint, enabled, interpenetrate, transLimitMin, transLimitMax, rotLimitMin, rotLimitMax);

	// [claforte] TODO.
	//exportConstraintSpringParams(rigidConstraint, constraintDagFn);
}


// Note: Nima currently only supports constraints attached to two rigid bodies.
//
void DaeAgeiaPhysicsScene::exportAttachment(FCDPhysicsRigidConstraint* rigidConstraint, FCDPhysicsRigidBody* rigidBody, const MDagPath& constraintDagPath, const MDagPath& rigidBodyDagPath, bool isRefAttachment)
{
	if (isRefAttachment)
	{
		rigidConstraint->SetReferenceRigidBody(rigidBody);
	}
	else
	{
		rigidConstraint->SetTargetRigidBody(rigidBody);
	}

	// Get the constraint's initial position and orientation. Those are
	// specified in the local space of the constraint.
	MFnDagNode constraintDagFn(constraintDagPath);
	MVector constraintInitPos = vectorAttribute(constraintDagFn, isRefAttachment ? "translate" : "localPosition2");
	MVector constraintInitOri = vectorAttribute(constraintDagFn, isRefAttachment ? "rotate" : "localOrientation2");

	// Export translate/rotates.
	MEulerRotation localOrientation(constraintInitOri);
	FCDTransformContainer* transforms = isRefAttachment ? &rigidConstraint->GetTransformsRef() : &rigidConstraint->GetTransformsTar();

	MPlug translatePlug;
	MPlug translateXPlug;
	MPlug translateYPlug;
	MPlug translateZPlug;
	MPlug rotatePlug;
	MPlug rotateXPlug;
	MPlug rotateYPlug;
	MPlug rotateZPlug;
	if (isRefAttachment)
	{
		translatePlug = constraintDagFn.findPlug("translate");
		translateXPlug = constraintDagFn.findPlug("translateX");
		translateYPlug = constraintDagFn.findPlug("translateY");
		translateZPlug = constraintDagFn.findPlug("translateZ");

		rotatePlug = constraintDagFn.findPlug("rotate");
		rotateXPlug = constraintDagFn.findPlug("rotateX");
		rotateYPlug = constraintDagFn.findPlug("rotateY");
		rotateZPlug = constraintDagFn.findPlug("rotateZ");
	}
	else
	{
		translatePlug = constraintDagFn.findPlug("localPosition2");
		translateXPlug = constraintDagFn.findPlug("localPosition2X");
		translateYPlug = constraintDagFn.findPlug("localPosition2Y");
		translateZPlug = constraintDagFn.findPlug("localPosition2Z");

		rotatePlug = constraintDagFn.findPlug("localOrientation2");
		rotateXPlug = constraintDagFn.findPlug("localOrientation2X");
		rotateYPlug = constraintDagFn.findPlug("localOrientation2Y");
		rotateZPlug = constraintDagFn.findPlug("localOrientation2Z");
	}
	bool hasTranslateAnimation = 
			CDagHelper::PlugHasAnimation(translateXPlug) ||
			CDagHelper::PlugHasAnimation(translateYPlug) ||
			CDagHelper::PlugHasAnimation(translateZPlug);
	bool hasRotateAnimation = CDagHelper::PlugHasAnimation(rotateXPlug) ||
							  CDagHelper::PlugHasAnimation(rotateYPlug) ||
							  CDagHelper::PlugHasAnimation(rotateZPlug);

	addTranslateRotates(transforms, constraintInitPos, localOrientation, hasTranslateAnimation, hasRotateAnimation);	

	FCDTransformContainer& transformsList = (isRefAttachment) ? rigidConstraint->GetTransformsRef() : rigidConstraint->GetTransformsTar();
	for (FCDTransformContainer::iterator it = transformsList.begin(); it != transformsList.end(); ++it)
	{
		if ((*it)->GetAnimated())
		{
			switch ((*it)->GetType())
			{
			case FCDTransform::TRANSLATION:
			{
				FCDParameterAnimatableVector3& translation = ((FCDTTranslation*)(*it))->GetTranslation();
				doc->GetAnimationLibrary()->AddPlugAnimation(translatePlug, translation, kVector);
				break;
			}
			case FCDTransform::ROTATION:
			{
				FCDParameterAnimatableAngleAxis& angleAxis = ((FCDTRotation*)(*it))->GetAngleAxis();
				MPlug rotateSinglePlug;
				if (angleAxis->axis == FMVector3::ZAxis) rotateSinglePlug = rotateXPlug;
				else if (angleAxis->axis == FMVector3::YAxis) rotateSinglePlug = rotateYPlug;
				else if (angleAxis->axis == FMVector3::XAxis) rotateSinglePlug = rotateZPlug;
				doc->GetAnimationLibrary()->AddPlugAnimation(rotateSinglePlug, angleAxis, kSingle | kQualifiedAngle);
				break;
			}
			default: break;
			}
		}
	}
}

#if 0

void DaeAgeiaPhysicsScene::exportConstraintSpringParams(FCDPhysicsRigidConstraint* rigidConstraint, 
													 MFnDagNode& constraintDagFn)
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

#endif //0 


//
// [claforte] TODO: Code the import part. For now, since the Physics support
// is at a prototype stage, it's not a priority.
//
