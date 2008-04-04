/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "FCDocument/FCDPhysicsMaterial.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDGeometryMesh.h"
#include "FCDocument/FCDPhysicsShape.h"
#include "FCDocument/FCDPhysicsAnalyticalGeometry.h"
#include "FCDocument/FCDPhysicsRigidBodyInstance.h"
#include "FCDocument/FCDPhysicsRigidBodyParameters.h"
#include "maya/MFnDagNode.h"
#include "maya/MDagPath.h"
#include "maya/MDagModifier.h"
#include "maya/MFnAttribute.h"
#include <limits>

const MTypeId PhysicsShapeTypeId(0x0010BC01);

void DaePhysicsModelLibrary::createRigidBody(FCDSceneNode* colladaNode, 
		FCDPhysicsRigidBodyInstance* instance)
{
	MStatus status;
	MFnDagNode rigidBodyDagFn;
	MObject rigidBodyObj;
	fstring nodeName;

	FCDPhysicsRigidBody* rigidBody = instance->GetRigidBody();
	MString rigidBodyName = rigidBody->GetName().c_str();
	
	// create the rigid body
	if ((colladaNode->GetParent(0) == NULL) ||
			(colladaNode->GetParent(0) == CDOC->GetVisualSceneInstance()))
	{
		rigidBodyObj = rigidBodyDagFn.create("nxRigidBody", rigidBodyName, 
				MObject::kNullObj, &status);
		nodeName = colladaNode->GetName();
	}
	else
	{
		fstring parentNodeName;
		FCDSceneNode* parent = colladaNode->GetParent(0);
		nodeName.append(FC("|") + colladaNode->GetName());
		while (parent && (parent != CDOC->GetVisualSceneInstance()))
		{
			parentNodeName.insert(0, FC("|") + parent->GetName());
			nodeName.insert(0, FC("|") + parent->GetName());
			parent = parent->GetParent(0);
		}
		
		MObject parentNodeObj = CDagHelper::GetNode(parentNodeName.c_str());
		rigidBodyObj = rigidBodyDagFn.create("nxRigidBody", rigidBodyName, parentNodeObj, &status);
	}
		
	if (!status) 
	{
		MGlobal::displayWarning(MString("Unable to create rigid body: ") + 
				rigidBodyName);
		return;
	}

	// get the visual scene's transforms
	MObject transformObj = CDagHelper::GetNode(nodeName.c_str());
	
	// get previous rotations
	float vx, vy, vz;
	float vx1, vy1, vz1;
	CDagHelper::GetPlugValue(transformObj, "rotateX", vx);
	CDagHelper::GetPlugValue(transformObj, "rotateY", vy);
	CDagHelper::GetPlugValue(transformObj, "rotateZ", vz);
	CDagHelper::GetPlugValue(transformObj, "rotateAxisX", vx1);
	CDagHelper::GetPlugValue(transformObj, "rotateAxisY", vy1);
	CDagHelper::GetPlugValue(transformObj, "rotateAxisZ", vz1);
	const FMMatrix44& xRot = FMMatrix44::AxisRotationMatrix(FMVector3::XAxis, vx);
	const FMMatrix44& yRot = FMMatrix44::AxisRotationMatrix(FMVector3::YAxis, vy);
	const FMMatrix44& zRot = FMMatrix44::AxisRotationMatrix(FMVector3::ZAxis, vz);
	const FMMatrix44& xRotAxis = FMMatrix44::AxisRotationMatrix(FMVector3::XAxis, vx1);
	const FMMatrix44& yRotAxis = FMMatrix44::AxisRotationMatrix(FMVector3::YAxis, vy1);
	const FMMatrix44& zRotAxis = FMMatrix44::AxisRotationMatrix(FMVector3::ZAxis, vz1);
	FMMatrix44 prevTransformMatrix;
	int order;
	CDagHelper::GetPlugValue(transformObj, "rotateOrder", order);
	switch (order)
	{
	case 0: // xyz
		prevTransformMatrix = 
				zRot * yRot * xRot * zRotAxis * yRotAxis * xRotAxis;
		break;
	case 1: // yzx
		prevTransformMatrix = 
				xRot * zRot * yRot * xRotAxis * zRotAxis * yRotAxis;
		break;
	case 2: // zxy
		prevTransformMatrix = 
				yRot * xRot * zRot * yRotAxis * xRotAxis * zRotAxis;
		break;
	case 3: // xzy
		prevTransformMatrix = 
				yRot * zRot * xRot * yRotAxis * zRotAxis * xRotAxis;
		break;
	case 4: // yxz
		prevTransformMatrix = 
				zRot * xRot * yRot * zRotAxis * xRotAxis * yRotAxis;
		break;
	case 5: // zyx
		prevTransformMatrix = 
				xRot * yRot * zRot * xRotAxis * yRotAxis * zRotAxis;
		break;
	default:
		MGlobal::displayWarning("Invalid rotation order. Default to xyz");
		prevTransformMatrix = 
				zRot * yRot * xRot * zRotAxis * yRotAxis * xRotAxis;
		break;
	}

	// get previous translations
	CDagHelper::GetPlugValue(transformObj, "translateX", vx);
	CDagHelper::GetPlugValue(transformObj, "translateY", vy);
	CDagHelper::GetPlugValue(transformObj, "translateZ", vz);
	prevTransformMatrix = FMMatrix44::TranslationMatrix(FMVector3(vx, vy, vz))
			* prevTransformMatrix;

	// get the previous scales
	CDagHelper::GetPlugValue(transformObj, "scaleX", vx);
	CDagHelper::GetPlugValue(transformObj, "scaleY", vy);
	CDagHelper::GetPlugValue(transformObj, "scaleZ", vz);
	FMVector3 prevScale(vx, vy, vz);

	// this is where the first shape is created
	// connect the shapes to the rigid body
	const FCDPhysicsShape* shape = rigidBody->GetParameters()->GetPhysicsShape(0);
	if (shape)
	{
		setupShape(shape, transformObj, rigidBodyDagFn, rigidBodyObj, 
				rigidBody->GetName(), false, prevTransformMatrix, prevScale);
	}
	
	// this is where the subsequent shapes are created under the first shape
	size_t shapeCount = rigidBody->GetParameters()->GetPhysicsShapeCount();
	for (size_t i = 1; i < shapeCount; i++)
	{
		MFnDagNode shapeDagFn;
		MObject shapeObj;

		shapeObj = shapeDagFn.create("transform", "physics1", rigidBodyObj, 
				&status);
		if (!status)
		{
			MGlobal::displayWarning(MString("Unable to create transform: ") + 
					MString(rigidBodyName));
			continue;
		}
		rigidBodyDagFn.addChild(shapeObj);
		
		const FCDPhysicsShape* shape = rigidBody->GetParameters()->GetPhysicsShape(i);
		
		setupShape(shape, shapeObj, rigidBodyDagFn, rigidBodyObj, 
				rigidBody->GetName(), true, prevTransformMatrix, prevScale);
	}
	
	// connect to the solver
	MObject rigidSolverObj = CDagHelper::GetNode("nxRigidSolver1");
	MFnDependencyNode solverFn(rigidSolverObj);
		
	MPlug desrPlug = solverFn.findPlug("rigidBodies");
	int rigidBodyId = CDagHelper::GetFirstEmptyElementId(desrPlug);
	MPlug rigidBodyIdPlug = rigidBodyDagFn.findPlug("rigidBodyId");
	rigidBodyIdPlug.setValue(rigidBodyId);

	MPlug curRigidBodyPlug = desrPlug.elementByLogicalIndex(rigidBodyId);
	CDagHelper::Connect(rigidBodyObj, "outRigidBody", curRigidBodyPlug);	

	MPlug forcePlug = solverFn.findPlug("generalForce");
	MPlug curForcePlug = forcePlug.elementByLogicalIndex(rigidBodyId);
	CDagHelper::Connect(rigidBodyObj, "generalForce", curForcePlug);

	CDagHelper::Connect(rigidSolverObj, "simulate", rigidBodyObj, 
			"inSimulate");
	
	MPlug parentInversePlug = rigidBodyDagFn.findPlug("parentInverseMatrix");
	MPlug firstParentInversePlug = parentInversePlug.elementByLogicalIndex(0);
	MPlug copyPlug = rigidBodyDagFn.findPlug("parentInverseMatrixCopy");
	MPlug firstCopyPlug = copyPlug.elementByLogicalIndex(0);
	CDagHelper::Connect(firstParentInversePlug, firstCopyPlug);

	CDagHelper::Connect(rigidBodyObj, "scale", rigidBodyObj, "scaleCopy");
	
	MObject timeObj = CDagHelper::GetNode("time1");
	CDagHelper::Connect(timeObj, "outTime", rigidBodyObj, "currentTime");
	
	MFnDagNode fdummy;
	MObject internalPhysicsObj = CDagHelper::GetNode("nimaInternalPhysics");
	MObject dummyTransform = fdummy.create("transform", "dummyTransform", internalPhysicsObj);
	CDagHelper::Connect(rigidBodyObj, "translate", dummyTransform, 
			"translate");

	// connect animations
	MPlugArray connections;
	MFnDagNode transformDagFn(transformObj);
	if (transformDagFn.getConnections(connections))
	{
		unsigned int connectionsCount = connections.length();
		for (unsigned int i = 0; i < connectionsCount; i++)
		{
			MPlug plug = connections[i];
			MPlugArray connectedToPlugs;
			plug.connectedTo(connectedToPlugs, true, false);
			unsigned int connectedToCount = connectedToPlugs.length();
			for (unsigned int j = 0; j < connectedToCount; j++)
			{
				MPlug connectedToPlug = connectedToPlugs[j];
				MObject nodeObj = connectedToPlug.node();
				MFnDependencyNode nodeFn(nodeObj);
				MString typeName = nodeFn.typeName();
				if ((typeName == "animCurveTU") || (typeName == "animCurveTL")
						|| (typeName == "animCurveTA"))
				{
					MObject attributeObj = plug.attribute();
					MFnAttribute attribute(attributeObj);
					CDagHelper::Connect(connectedToPlug, rigidBodyObj,
							attribute.name());
				}
			}
		}
	}

	// remove old transform and rename the rigid body
	status = MGlobal::removeFromModel(transformObj);
	if (!status)
	{
		MGlobal::displayWarning(MString("Unable to remove ") + 
				((MFnDagNode)transformObj).name());
	}
	rigidBodyDagFn.setName(colladaNode->GetName().c_str());
	
	// set rigid body instance parameters
	const FMVector3& vel = instance->GetVelocity();
	CDagHelper::SetPlugValue(rigidBodyObj, "initialVelocity", 
				MVector(vel.x, vel.y, vel.z));

	const FMVector3& angVel = instance->GetAngularVelocity();
	CDagHelper::SetPlugValue(rigidBodyObj, "initialSpin",
			MVector(FMath::DegToRad(angVel.x), FMath::DegToRad(angVel.y), 
					FMath::DegToRad(angVel.z)));

	FCDParameterAnimatableFloat& mass = instance->GetParameters()->GetMass();
	CDagHelper::SetPlugValue(rigidBodyObj, "mass", mass);
	ANIM->AddPlugAnimation(rigidBodyObj, "mass", mass, kSingle);

	FCDParameterAnimatableVector3& com = instance->GetParameters()->GetMassFrameTranslate();
	CDagHelper::SetPlugValue(rigidBodyObj, "centerOfMass", MConvert::ToMVector(com));
	ANIM->AddPlugAnimation(rigidBodyObj, "centerOfMass", com, kVector);

	// mass_frame rotate element using: DAE_ROTATE_ELEMENT, not supported yet

	FCDParameterAnimatableVector3& inertiaTensor = instance->GetParameters()->GetInertia();
	if (!instance->GetParameters()->IsInertiaAccurate()) inertiaTensor = FMVector3::Zero;
	CDagHelper::SetPlugValue(rigidBodyObj, "inertiaTensor", MConvert::ToMVector(inertiaTensor));
	ANIM->AddPlugAnimation(rigidBodyObj, "inertiaTensor", inertiaTensor, kVector);

	FCDParameterAnimatableFloat& pdynamic = instance->GetParameters()->GetDynamic();
	CDagHelper::SetPlugValue(rigidBodyObj, "active", pdynamic);
	ANIM->AddPlugAnimation(rigidBodyObj, "active", pdynamic, kSingle);

	FCDPhysicsMaterial* material = instance->GetParameters()->GetPhysicsMaterial();
	if (material == NULL)
	{
		material = rigidBody->GetParameters()->GetPhysicsMaterial();
	}
	if (material == NULL)
	{
		MGlobal::displayInfo(MString("Failed to get physics material from rigid body: ") + colladaNode->GetName().c_str());
		return;
	}

	CDagHelper::SetPlugValue(rigidBodyObj, "staticFriction", material->GetStaticFriction());
	CDagHelper::SetPlugValue(rigidBodyObj, "dynamicFriction", material->GetDynamicFriction());
	CDagHelper::SetPlugValue(rigidBodyObj, "bounciness", material->GetRestitution());
}

void DaePhysicsModelLibrary::setupShape(const FCDPhysicsShape* shape, 
		const MObject& transformObj, MFnDagNode rigidBodyDagFn, const MObject& rigidBodyObj,
		const fstring& name, bool connectTransform, const FMMatrix44& prevTransformMatrix,
		const FMVector3& prevScale)
{
	// find the mesh shape	
	MFnDagNode transformDagFn(transformObj);
	MDagPath meshPath = CDagHelper::GetShortestDagPath(transformObj);
	meshPath.extendToShape();
	MObject meshObj = meshPath.node();

	// take care of instances
	unsigned int parentCount = transformDagFn.parentCount();
	for (unsigned int i = 0; i < parentCount; i++)
	{
		MObject parentObj = transformDagFn.parent(i);
		MFnDagNode parentDagFn = MFnDagNode(parentObj);
		if (!parentDagFn.isDefaultNode() && (parentObj != rigidBodyObj))
		{
			parentDagFn.addChild(const_cast<MObject&>(rigidBodyObj), MFnDagNode::kNextPos, true);
		}
	}
	
	if (!shape->IsAnalyticalGeometry() && meshObj.hasFn(MFn::kMesh))
	{
		// save shader group connections before breaking it
		PlugInfoList connectionList;
		MStatus status;
		MPlug instPlug = MFnDagNode(meshObj).findPlug("instObjGroups", status);
		if (status)
		{
			unsigned int elementCount = instPlug.numElements();
			for (unsigned int i = 0; i < elementCount; i++)
			{
				MPlug connectedPlug;
				MPlug elementPlug = instPlug.elementByPhysicalIndex(i);

				if (CDagHelper::GetPlugConnectedTo(elementPlug, connectedPlug))
				{
					MFnAttribute attribute(connectedPlug.attribute());

					PlugInfo info;
					info.inAttributeName = attribute.name();
					info.inLogicalIndex = connectedPlug.logicalIndex();
					info.inNode = connectedPlug.node();
					info.outLogicalIndex = elementPlug.logicalIndex();
					connectionList.push_back(info);
				}
			}
		}

		// parent mesh to rigid body
		rigidBodyDagFn.addChild(meshObj);

		for (PlugInfoList::const_iterator it = connectionList.begin();
				it != connectionList.end(); it++)
		{
			PlugInfo info = *it;
			MPlug elementPlug = instPlug.elementByLogicalIndex(
					info.outLogicalIndex);
			MFnDependencyNode inNode(info.inNode);
			MPlug attributePlug = inNode.findPlug(info.inAttributeName);
			MPlug connectedPlug = attributePlug.elementByLogicalIndex(
					info.inLogicalIndex);
			CDagHelper::Connect(elementPlug, connectedPlug);
		}

		MFnDependencyNode meshNode = MFnDependencyNode(meshObj);

		// connect mesh to rigid body
		MPlug shapesPlug = rigidBodyDagFn.findPlug("shapes");
		MPlug firstShapesPlug = shapesPlug.elementByLogicalIndex(0);
		CDagHelper::Connect(meshObj, "outMesh", firstShapesPlug);

		MPlug shapeMatPlug = rigidBodyDagFn.findPlug("shapeMatrices");
		MPlug firstShapeMatPlug = shapeMatPlug.elementByLogicalIndex(0);
		CDagHelper::Connect(rigidBodyObj, "identityMatrix", firstShapeMatPlug);
		if (connectTransform)
		{
			CDagHelper::Connect(transformObj, "xformMatrix", meshObj, 
					"shapeMatrix");
			((MFnDagNode)transformObj).addChild(meshObj);
		}
	}
	else
	{
		MObject analyticShape = setupAnalyticShape(shape, rigidBodyDagFn,
				rigidBodyObj, name);
		if ((connectTransform) && !analyticShape.isNull())
		{
			CDagHelper::Connect(transformObj, "xformMatrix", analyticShape,
					"shapeMatrix");
			((MFnDagNode)transformObj).addChild(analyticShape);
		}
	}

	// get transforms from the shape node to add to the rest
	FMMatrix44 transformMatrix = FMMatrix44::Identity;
	const FCDTransformContainer& transforms = shape->GetTransforms();
	for (FCDTransformContainer::const_iterator it = transforms.begin(); it != transforms.end(); ++it)
	{
		transformMatrix = transformMatrix * (*it)->ToMatrix();
	}
	transformMatrix = prevTransformMatrix * transformMatrix;

	MObject parentObj = MObject::kNullObj;
	if (transformDagFn.hasParent(rigidBodyObj))
	{
		// FIXME: find a cleaner way of getting correct transforms
		parentObj = transformDagFn.parent(0);
		MGlobal::executeCommand("parent -w " + transformDagFn.fullPathName());
	}

	FMVector3 scale;
	FMVector3 translation;
	FMVector3 rotation;
	float invert;
	transformMatrix.Decompose(scale, rotation, translation, invert);

	MVector r(rotation.x, rotation.y, rotation.z);
	MVector t(translation.x, translation.y, translation.z);
	
	if (connectTransform)
	{
		CDagHelper::SetPlugValue(transformObj, "rotate", r);
		CDagHelper::SetPlugValue(transformObj, "translate", t);
		CDagHelper::SetPlugValue(transformObj, "scale", 
				MVector(prevScale.x, prevScale.y, prevScale.z));
	}
	else
	{
		CDagHelper::SetPlugValue(rigidBodyObj, "rotate", r);
		CDagHelper::SetPlugValue(rigidBodyObj, "initialOrientation", r);

		CDagHelper::SetPlugValue(rigidBodyObj, "translate", t);
		CDagHelper::SetPlugValue(rigidBodyObj, "initialPosition", t);

		CDagHelper::SetPlugValue(rigidBodyObj, "scale", 
				MVector(prevScale.x, prevScale.y, prevScale.z));
	}

	if (!parentObj.isNull())
	{
		MGlobal::executeCommand("parent " + transformDagFn.fullPathName() + 
				" " + rigidBodyDagFn.fullPathName());
	}

	FCDEntityInstance* eInstance = 
			(FCDEntityInstance*) shape->GetGeometryInstance();
	if (eInstance)
	{
		FCDGeometry* geom = (FCDGeometry*)(eInstance->GetEntity());
		if (geom->IsMesh() && !geom->GetMesh()->IsConvex())
		{
			// non convex
			CDagHelper::SetPlugValue(rigidBodyObj, "geometryType", 1);
		}
	}
}

MObject DaePhysicsModelLibrary::setupAnalyticShape(
		const FCDPhysicsShape* shape, MFnDagNode rigidBodyDagFn, 
		const MObject& rigidBodyObj, const fstring& name)
{
	MStatus status;

	const FCDPhysicsAnalyticalGeometry* aGeometry = 
			shape->GetAnalyticalGeometry();
	if (!aGeometry) return MObject::kNullObj;

	MFnDagNode physicsShapeDagFn;
	MObject physicsShapeObj;

	physicsShapeObj = physicsShapeDagFn.create("physicsShape", "physicsShape1",
			const_cast<MObject&>(rigidBodyObj), &status);
	if (!status)
	{
		MGlobal::displayWarning(MString("Unable to create physicsShape for ") +
				MString(name.c_str()));
		return MObject::kNullObj;
	}

	// parent physicsShape to rigid body
	rigidBodyDagFn.addChild(physicsShapeObj);

	// set the physicsShape parameters
	int shapeType = 0;
	FCDPASCapsule* capsule;
	FCDPASCylinder* cylinder;
	FMVector3 halfExtents;
	float height;
	float radius;
	switch (aGeometry->GetGeomType())
	{
	// ellipsoid and convex hull not in COLLADA
	// plane, tapered capsule, tapered cylinder not in NIMA
	case FCDPhysicsAnalyticalGeometry::BOX:
		halfExtents = ((FCDPASBox*)aGeometry)->halfExtents;
		CDagHelper::SetPlugValue(physicsShapeObj, "scaleX", halfExtents.x * 2);
		CDagHelper::SetPlugValue(physicsShapeObj, "scaleY", halfExtents.y * 2);
		CDagHelper::SetPlugValue(physicsShapeObj, "scaleZ", halfExtents.z * 2);

		shapeType = 1;
		break;
	case FCDPhysicsAnalyticalGeometry::SPHERE:
		CDagHelper::SetPlugValue(physicsShapeObj, "radius",
				((FCDPASSphere*)aGeometry)->radius);

		shapeType = 2;
		break;
	case FCDPhysicsAnalyticalGeometry::CAPSULE:
		capsule = (FCDPASCapsule*)aGeometry;
		height = capsule->height;
		radius = capsule->radius.x;

		CDagHelper::SetPlugValue(physicsShapeObj, "radius", radius);
		CDagHelper::SetPlugValue(physicsShapeObj, "point1Y", -height/2);
		CDagHelper::SetPlugValue(physicsShapeObj, "point2Y", height/2);
		// hack: NIMA bug where geometry does not show if only y axis
		CDagHelper::SetPlugValue(physicsShapeObj, "point1X", -0.001);
		CDagHelper::SetPlugValue(physicsShapeObj, "point2X", 0.000);

		shapeType = 4;
		break;
	case FCDPhysicsAnalyticalGeometry::CYLINDER:
		cylinder = (FCDPASCylinder*)aGeometry;
		height = cylinder->height;
		radius = cylinder->radius.x;

		CDagHelper::SetPlugValue(physicsShapeObj, "radius", radius);
		CDagHelper::SetPlugValue(physicsShapeObj, "point1Y", -height/2);
		CDagHelper::SetPlugValue(physicsShapeObj, "point2Y", height/2);
		// hack: NIMA bug where geometry does not show if only y axis
		CDagHelper::SetPlugValue(physicsShapeObj, "point1X", -0.001);
		CDagHelper::SetPlugValue(physicsShapeObj, "point2X", 0.000);

		shapeType = 5;
		break;
	}

	CDagHelper::SetPlugValue(physicsShapeObj, "shapeType", shapeType);

	// connect physics shape to rigid body
	MPlug physicsShapesPlug = rigidBodyDagFn.findPlug("physicsShapes");
	int physicsShapeId = CDagHelper::GetFirstEmptyElementId(physicsShapesPlug);
	MPlug curPhysicsShapesPlug = 
			physicsShapesPlug.elementByLogicalIndex(physicsShapeId);
	CDagHelper::Connect(physicsShapeObj, "outPhysicsShape", 
			curPhysicsShapesPlug);

	return physicsShapeObj;
}

void DaePhysicsModelLibrary::createRigidConstraint(
		FCDPhysicsRigidConstraint* rigidConstraint)
{
	MStatus status;
	
	FCDPhysicsRigidBody* refBody = rigidConstraint->GetReferenceRigidBody();
	MObject refObj = CDagHelper::GetNode(refBody->GetName().c_str());
	MFnDependencyNode referenceFn(refObj);

	FCDPhysicsRigidBody* targetBody = rigidConstraint->GetTargetRigidBody();
	MObject targetObj = CDagHelper::GetNode(targetBody->GetName().c_str());
	MFnDependencyNode targetFn(targetObj);

	MObject rigidSolverObj = CDagHelper::GetNode("nxRigidSolver1");
	MFnDependencyNode solverFn(rigidSolverObj);
	
	// create the constraint node
	MFnDagNode rigidConstraintDagFn;
	MObject rigidConstraintObj = rigidConstraintDagFn.create(
			"nxRigidConstraint", rigidConstraint->GetName().c_str(), refObj,
			&status);
	if (!status) return;
	
	MFnDagNode constraintLocaterFn;
	MObject constraintLocatorObj = constraintLocaterFn.create("nxRigidConstraintLocator", rigidConstraintObj, &status);
	if (!status) return;

	// make connections
	MPlug parentPlug = rigidConstraintDagFn.findPlug("parentInverseMatrix");
	MPlug firstParentPlug = parentPlug.elementByLogicalIndex(0);
	MPlug copyPlug = rigidConstraintDagFn.findPlug("parentInverseMatrixCopy");
	MPlug firstCopyPlug = copyPlug.elementByLogicalIndex(0);
	CDagHelper::Connect(firstParentPlug, firstCopyPlug);
	CDagHelper::Connect(rigidConstraintObj, "translate", rigidConstraintObj,
			"translateCopy");
	CDagHelper::Connect(rigidConstraintObj, "rotate", rigidConstraintObj,
			"rotateCopy");
	
	CDagHelper::Connect(refObj, "outRigidBody", rigidConstraintObj, 
			"rigidBody1");	
	CDagHelper::Connect(targetObj, "outRigidBody", rigidConstraintObj,
			"rigidBody2");
	CDagHelper::Connect(rigidSolverObj, "simulate", rigidConstraintObj,
			"inSimulate");
	
	MPlug rigidConstraintsPlug = solverFn.findPlug("rigidConstraints");
	int rigidConstraintId = CDagHelper::GetFirstEmptyElementId(
			rigidConstraintsPlug);

	MPlug curRigidConstraintsPlug = 
			rigidConstraintsPlug.elementByLogicalIndex(rigidConstraintId);
	CDagHelper::Connect(rigidConstraintObj, "outRigidConstraint", 
			curRigidConstraintsPlug);	
	
	MFnDagNode fdummy;
	MObject internalPhysicsObj = CDagHelper::GetNode("nimaInternalPhysics");
	MObject dummyTransform = fdummy.create("transform", "dummyTransform", 
			internalPhysicsObj);
	//CDagHelper::Connect(rigidConstraintObj, "drivePosition", dummyTransform,
	//		"translate");
	
	// set parameters
#define SET_PARAMETERS(transforms, translateName, rotateXName, rotateYName, rotateZName, translateBool, rotateBool) \
	for (FCDTransformContainer::iterator it = transforms.begin(); it != transforms.end(); ++it) \
	{ \
		switch ((*it)->GetType()) \
		{ \
			case FCDTransform::TRANSLATION: \
			{ \
				FMVector3 vt = ((FCDTTranslation*)(*it))->GetTranslation(); \
				CDagHelper::SetPlugValue(rigidConstraintObj, translateName, MVector(vt.x, vt.y, vt.z)); \
				translateBool = true; \
				break; \
			} \
			case FCDTransform::ROTATION: \
			{ \
				FMVector3 axis = ((FCDTRotation*)(*it))->GetAxis(); \
				MVector angles; \
				if (axis==FMVector3(0,0,1)) CDagHelper::SetPlugValue(rigidConstraintObj, rotateZName, FMath::DegToRad(((FCDTRotation*)(*it))->GetAngle())); \
				else if (axis==FMVector3(0,1,0)) CDagHelper::SetPlugValue(rigidConstraintObj, rotateYName, FMath::DegToRad(((FCDTRotation*)(*it))->GetAngle())); \
				else if (axis==FMVector3(1,0,0)) CDagHelper::SetPlugValue(rigidConstraintObj, rotateXName, FMath::DegToRad(((FCDTRotation*)(*it))->GetAngle())); \
				rotateBool = true; \
			} \
			default: break; \
		} \
	}

	bool dummy1, dummy2;
	FCDTransformContainer& transformsRef = rigidConstraint->GetTransformsRef();
	SET_PARAMETERS(transformsRef, "translate", "rotateX", "rotateY", "rotateZ", dummy1, dummy2);

	bool haveTranslate = false;
	bool haveRotate = false;
	FCDTransformContainer& transformsTar = rigidConstraint->GetTransformsTar();
	SET_PARAMETERS(transformsTar, "localPosition2", "localOrientation2X", "localOrientation2Y", "localOrientation2Z", haveTranslate, haveRotate);

#define UPDATE_CHECK(plugName, type, tarXName, tarYName, tarZName, refXName, refYName, refZName)\
	bool check = false; \
	float vx, vy, vz; \
	float vx1, vy1, vz1; \
	CDagHelper::GetPlugValue(targetObj, tarXName, vx); \
	CDagHelper::GetPlugValue(targetObj, tarYName, vy); \
	CDagHelper::GetPlugValue(targetObj, tarZName, vz); \
	CDagHelper::GetPlugValue(rigidConstraintObj, refXName, vx1); \
	CDagHelper::GetPlugValue(rigidConstraintObj, refYName, vy1); \
	CDagHelper::GetPlugValue(rigidConstraintObj, refZName, vz1); \
	/* negative because we want to go back to origin */ \
	if (!IsEquivalent(vx, -vx1) || !IsEquivalent(vy, -vy1) || !IsEquivalent(vz, -vz1)) \
	{ \
		/* it is not the same as attachment's values */ \
		check = true; \
	} \
	else \
	{ \
		const FCDTransformContainer& transformsTar = rigidConstraint->GetTransformsTar(); \
		for (FCDTransformContainer::const_iterator it = transformsTar.begin(); it != transformsTar.end(); ++it) \
		{ \
			/* it is animated */ \
			if ((*it)->GetAnimated() && ((*it)->GetType() == type)) \
			{ \
				check = true; \
				break; \
			} \
		} \
	} \
	\
	if (check) \
	{ \
		CDagHelper::SetPlugValue(rigidConstraintObj, plugName, 0); \
	}

	if (haveTranslate)
	{
		UPDATE_CHECK("samePosition", FCDTransform::TRANSLATION, "translateX", "translateY", "translateZ", "localPosition2X", "localPosition2Y", "localPosition2Z");
	}
	if (haveRotate)
	{
		UPDATE_CHECK("sameOrientation", FCDTransform::ROTATION, "rotateX", "rotateY", "rotateZ", "localOrientation2X", "localOrientation2Y", "localOrientation2Z");
		CDagHelper::SetPlugValue(rigidConstraintObj, "sameOrientation", 0);
	}
#undef UPDATE_CHECK
#undef SET_PARAMETERS

	setDegreesOfFreedom(rigidConstraint, rigidConstraintObj);

	CDagHelper::SetPlugValue(rigidConstraintObj, "constrain", 
			rigidConstraint->GetEnabled());
	CDagHelper::SetPlugValue(rigidConstraintObj, "interpenetrate",
			rigidConstraint->GetInterpenetrate());
	
	animateConstraint(rigidConstraint, rigidConstraintObj);
}

void DaePhysicsModelLibrary::animateConstraint(
		FCDPhysicsRigidConstraint* rigidConstraint, const MObject& rigidConstraintObj)
{
#define ANIMATE_TRANSFORMS(transforms, translateName, rotateXName, \
						   rotateYName, rotateZName) \
	for (FCDTransformContainer::iterator it = transforms.begin(); \
			it != transforms.end(); ++it) \
	{ \
		FCDAnimated* animated = (*it)->GetAnimated(); \
		if (animated) \
		{ \
			switch ((*it)->GetType()) \
			{ \
			case FCDTransform::TRANSLATION: \
			{ \
				ANIM->AddPlugAnimation(rigidConstraintObj, translateName, \
						animated, kVector | kLength); \
				break; \
			} \
			case FCDTransform::ROTATION: \
			{ \
				FMVector3 axis = ((FCDTRotation*)(*it))->GetAxis(); \
				if (axis == FMVector3::ZAxis) \
				{ \
					ANIM->AddPlugAnimation(rigidConstraintObj, rotateXName, \
						animated, kSingle | kQualifiedAngle); \
				} \
				else if (axis == FMVector3::YAxis) \
				{ \
					ANIM->AddPlugAnimation(rigidConstraintObj, rotateYName, \
						animated, kSingle | kQualifiedAngle); \
				} \
				else if (axis == FMVector3::XAxis) \
				{ \
					ANIM->AddPlugAnimation(rigidConstraintObj, rotateZName, \
						animated, kSingle | kQualifiedAngle); \
				} \
				break; \
			} \
			default: break; \
			} \
		} \
	}

	FCDTransformContainer& transformsRef = rigidConstraint->GetTransformsRef();
	ANIMATE_TRANSFORMS(transformsRef, "translate", "rotateX", "rotateY", "rotateZ");
	FCDTransformContainer& transformsTar = rigidConstraint->GetTransformsTar();
	ANIMATE_TRANSFORMS(transformsTar, "localPosition2", "localOrientation2X", "localOrientation2Y", "localOrientation2Z");
#undef ANIMATE_TRANSFORMS

	ANIM->AddPlugAnimation(rigidConstraintObj, "constrain", rigidConstraint->GetEnabled(), kSingle);
	ANIM->AddPlugAnimation(rigidConstraintObj, "interpenetrate", rigidConstraint->GetInterpenetrate(), kSingle);
}

void DaePhysicsModelLibrary::setDegreesOfFreedom(
		FCDPhysicsRigidConstraint* rigidConstraint, const MObject& rigidConstraintObj)
{
	// set the linear values
	const FMVector3& linearMin = rigidConstraint->GetLimitsLinearMin();
	const FMVector3& linearMax = rigidConstraint->GetLimitsLinearMax();
	float linearLimit = 0.0f;
	bool hasLinearLimit = false;

#define SET_LINEAR(minValue, maxValue, plugName) \
	if ((minValue != 0.0f) || (maxValue != 0.0f)) \
	{ \
		if (maxValue == std::numeric_limits<float>::infinity()) \
		{ \
			CDagHelper::SetPlugValue(rigidConstraintObj, plugName, \
					2); /* free */ \
		} \
		else \
		{ \
			if (!hasLinearLimit) \
			{ \
				linearLimit = maxValue; \
				hasLinearLimit = true; \
			} \
			else \
			{ \
				linearLimit = max(linearLimit, maxValue); \
			} \
			CDagHelper::SetPlugValue(rigidConstraintObj, plugName, \
					1); /* limited */ \
		} \
	} 

	SET_LINEAR(linearMin.x, linearMax.x, "motionX");
	SET_LINEAR(linearMin.y, linearMax.y, "motionY");
	SET_LINEAR(linearMin.z, linearMax.z, "motionZ");
#undef SET_LINEAR

	CDagHelper::SetPlugValue(rigidConstraintObj, "linearLimitValue",
			linearLimit);
	CDagHelper::SetPlugValue(rigidConstraintObj, "linearLimitRestitution",
			rigidConstraint->GetSpringLinearStiffness());
	CDagHelper::SetPlugValue(rigidConstraintObj, "linearLimitSpring", 
			rigidConstraint->GetSpringLinearTargetValue());
	CDagHelper::SetPlugValue(rigidConstraintObj, "linearLimitDamping",
			rigidConstraint->GetSpringLinearDamping());

	// set the angular values
	const FMVector3& angularMin = rigidConstraint->GetLimitsSCTMin();
	const FMVector3& angularMax = rigidConstraint->GetLimitsSCTMax();

	float minTwist = angularMin.x;
	float maxTwist = angularMax.x;
	float minSwing1 = angularMin.y;
	float maxSwing1 = angularMax.y;
	float minSwing2 = angularMin.z;
	float maxSwing2 = angularMax.z;

#define SET_ANGULAR(minValue, maxValue, plugName, limitName, stiffName, \
					springName, dampName) \
	if ((minValue <= -180) && (maxValue >= 180)) \
	{ \
		CDagHelper::SetPlugValue(rigidConstraintObj, plugName, 2); /* free */ \
	} \
	else if ((minValue != 0.0f) || (maxValue != 0.0f)) \
	{ \
		CDagHelper::SetPlugValue(rigidConstraintObj, plugName, \
				1); /* limited */ \
	} \
	CDagHelper::SetPlugValue(rigidConstraintObj, limitName, \
			min(maxValue, 180.0f)); \
	CDagHelper::SetPlugValue(rigidConstraintObj, stiffName, \
			rigidConstraint->GetSpringAngularStiffness()); \
	CDagHelper::SetPlugValue(rigidConstraintObj, springName, \
			rigidConstraint->GetSpringAngularTargetValue()); \
	CDagHelper::SetPlugValue(rigidConstraintObj, dampName, \
			rigidConstraint->GetSpringAngularDamping());

	SET_ANGULAR(minTwist, maxTwist, "motionTwist", "twistHighLimitRestitution",
				"twistHighLimitRestitution", "twistHighLimitSpring", 
				"twistHighLimitDamping");
	SET_ANGULAR(minSwing1, maxSwing1, "motionSwing1", "swing1LimitValue", 
				"swing1LimitRestitution", "swing1LimitSpring", 
				"swing1LimitDamping");
	SET_ANGULAR(minSwing2, maxSwing2, "motionSwing2", "swing2LimitValue", 
				"swing2LimitRestitution", "swing2LimitSpring", 
				"swing2LimitDamping");
#undef SET_ANGULAR

	CDagHelper::SetPlugValue(rigidConstraintObj, "twistLowLimitValue",
			max(minTwist, -180.0f));
	CDagHelper::SetPlugValue(rigidConstraintObj, "twistLowLimitRestitution",
			rigidConstraint->GetSpringAngularStiffness());
	CDagHelper::SetPlugValue(rigidConstraintObj, "twistLowLimitSpring",
			rigidConstraint->GetSpringAngularTargetValue());
	CDagHelper::SetPlugValue(rigidConstraintObj, "twistLowLimitDamping",
			rigidConstraint->GetSpringAngularDamping());

}
