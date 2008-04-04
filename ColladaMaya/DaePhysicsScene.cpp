/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include <maya/MItDependencyNodes.h>
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDPhysicsAnalyticalGeometry.h"
#include "FCDocument/FCDPhysicsMaterial.h"
#include "FCDocument/FCDPhysicsModel.h"
#include "FCDocument/FCDPhysicsModelInstance.h"
#include "FCDocument/FCDPhysicsRigidBody.h"
#include "FCDocument/FCDPhysicsRigidBodyInstance.h"
#include "FCDocument/FCDPhysicsRigidBodyParameters.h"
#include "FCDocument/FCDPhysicsRigidConstraint.h"
#include "FCDocument/FCDPhysicsRigidConstraintInstance.h"
#include "FCDocument/FCDPhysicsScene.h"
#include "FCDocument/FCDPhysicsShape.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDAnimated.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "CRotateHelper.h"

DaePhysicsScene::DaePhysicsScene(DaeDoc* pDoc)
:	doc(pDoc), 
	fpPhysicsModel(NULL), fpPhysicsModelInstance(NULL)
{
}

//virtual 
DaePhysicsScene::~DaePhysicsScene()
{
	doc = NULL; 
	fpPhysicsModel = NULL;
	fpPhysicsModelInstance = NULL;
}

bool DaePhysicsScene::isPhysicsShape(const MFnDependencyNode& depFn)
{
	const MTypeId PhysicsShapeTypeId(0x0010BC01);
	return (depFn.typeId() == PhysicsShapeTypeId);
}

// Create a physics model definition node, if it doesn't already exist.
FCDPhysicsModel* DaePhysicsScene::physicsModel()
{
	// If the physics model hasn't been created already,
	// do it now.
	if (fpPhysicsModel == NULL)
	{
		fpPhysicsModel = CDOC->GetPhysicsModelLibrary()->AddEntity();
		fpPhysicsModel->SetDaeId(physicsModelId().asChar());
	}
	return fpPhysicsModel;
}

FCDPhysicsModelInstance* DaePhysicsScene::physicsModelInstance()
{
	// If the physics scene's first node hasn't been created already,
	// do it now.
	if (fpPhysicsModelInstance == NULL)
	{
		fpPhysicsModelInstance = physicsScene()->AddPhysicsModelInstance(physicsModel());
	}
	return fpPhysicsModelInstance;
}

FCDPhysicsScene* DaePhysicsScene::physicsScene()
{
	FCDPhysicsScene* scene = CDOC->GetPhysicsSceneInstance();
	if (scene == NULL)
	{
		scene = CDOC->AddPhysicsScene();
		scene->SetDaeId(physicsSceneId().asChar());
	}
	return scene;
}

//--------------------------------------------------------
// Methods to add elements shared across engines.
//

FCDPhysicsRigidBodyInstance* DaePhysicsScene::addInstanceRigidBody(FCDPhysicsRigidBody* rigidBody, FCDSceneNode* targetNode)
{
	// Create the rigid body instance.
	FCDPhysicsRigidBodyInstance* instance = physicsModelInstance()->AddRigidBodyInstance(rigidBody);
	instance->SetTargetNode(targetNode);
	return instance;
}

FCDPhysicsRigidConstraintInstance* DaePhysicsScene::addInstanceRigidConstraint(FCDPhysicsRigidConstraint* rigidConstraint)
{
	return physicsModelInstance()->AddRigidConstraintInstance(rigidConstraint);
}

// Add a physics material in the physics material library. Give it an
// id which depends on the name of the provided dagpath.
//
FCDPhysicsMaterial* DaePhysicsScene::addPhysicsMaterial(const MDagPath& dagPath)
{
	FCDPhysicsMaterial* material = CDOC->GetPhysicsMaterialLibrary()->AddEntity();
	material->SetDaeId(doc->DagPathToColladaId(dagPath).asChar() + fm::string("-material"));
	material->SetName(MConvert::ToFChar(doc->DagPathToColladaName(dagPath)) + FS("-material"));
	return material;
}

FCDPhysicsRigidBody* DaePhysicsScene::addRigidBody(const MDagPath& dagPath)
{
	FCDPhysicsRigidBody* rigidBody = physicsModel()->AddRigidBody();
	rigidBody->SetSubId(doc->DagPathToColladaId(dagPath).asChar());
	rigidBody->SetName(MConvert::ToFChar(doc->DagPathToColladaName(dagPath)));
	return rigidBody;
}

FCDPhysicsRigidConstraint* DaePhysicsScene::addRigidConstraint(const MDagPath& dagPath)
{
	FCDPhysicsRigidConstraint* rigidConstraint = physicsModel()->AddRigidConstraint();
	rigidConstraint->SetSubId(doc->DagPathToColladaId(dagPath).asChar());
	rigidConstraint->SetName(MConvert::ToFChar(doc->DagPathToColladaName(dagPath)));
	return rigidConstraint;
}

FCDPhysicsShape* DaePhysicsScene::addShape(FCDPhysicsRigidBody* rigidBody)
{
	return rigidBody->GetParameters()->AddPhysicsShape();
}

//--------------------------------------------------------
// Methods to add low-level XML elements.
//

void DaePhysicsScene::matrixToTranslateRotate(const MMatrix& matrix,
											  MVector& translate, MEulerRotation& rotate)
{
	// Extract and export the translation.
	translate.x = matrix[3][0];
	translate.y = matrix[3][1];
	translate.z = matrix[3][2];

	// Extract and export the rotations.
	MTransformationMatrix finalMatrix(matrix);

	// Find minimal rotation.
	MEulerRotation rotateSol1 = finalMatrix.eulerRotation();
	MEulerRotation rotateSol2 = finalMatrix.eulerRotation().alternateSolution();
	double totalSquareAngles1 = rotateSol1.x * rotateSol1.x + rotateSol1.y * rotateSol1.y + rotateSol1.z * rotateSol1.z;
	double totalSquareAngles2 = rotateSol2.x * rotateSol2.x + rotateSol2.y * rotateSol2.y + rotateSol2.z * rotateSol2.z;
	rotate = (totalSquareAngles1 <= totalSquareAngles2) ? rotateSol1 : rotateSol2;
}

void DaePhysicsScene::addTranslateRotates(FCDTransformContainer* transforms, 
		const MVector& _translate, const MEulerRotation& rotate, 
		bool addEmptyTranslate, bool addEmptyRotate)
{
	if (transforms == NULL) return;

	// Export the translation.
	FMVector3 translation = MConvert::ToFMVector(_translate);
	if (addEmptyTranslate || (!IsEquivalent(translation, FMVector3::Zero)))
	{
		FCDTTranslation* t = new FCDTTranslation(CDOC, NULL);
		transforms->push_back(t);
		t->SetTranslation(translation);
	}

	// Export the rotations.
	CRotateHelper rotateHelper(doc, transforms, rotate);
	rotateHelper.SetEliminateEmptyRotations(!addEmptyRotate);
	rotateHelper.Output();
}

void DaePhysicsScene::addTranslateRotates(FCDTransformContainer* transforms, const MMatrix& matrix)
{
	MVector translate;
	MEulerRotation rotate;
	matrixToTranslateRotate(matrix, translate, rotate);
	addTranslateRotates(transforms, translate, rotate);
}

//--------------------------------------------------------
// Methods to add shape nodes.
//
	
void DaePhysicsScene::addBox(FCDPhysicsShape* shape, const MVector& size)
{
	FCDPASBox* box = (FCDPASBox*) shape->CreateAnalyticalGeometry(FCDPhysicsAnalyticalGeometry::BOX);
	box->halfExtents = MConvert::ToFMVector(size / 2.0);
}

void DaePhysicsScene::addSphere(FCDPhysicsShape* shape, float radius)
{
	FCDPASSphere* sphere = (FCDPASSphere*) shape->CreateAnalyticalGeometry(FCDPhysicsAnalyticalGeometry::SPHERE);
	sphere->radius = radius;
}

void DaePhysicsScene::addCapsule(FCDPhysicsShape* shape, float radius, const MVector& point1, const MVector& point2, MMatrix& intrinsicShapeMatrix)
{
	FCDPASCapsule* capsule = (FCDPASCapsule*) shape->CreateAnalyticalGeometry(FCDPhysicsAnalyticalGeometry::CAPSULE);
	capsule->radius.x = radius;
	capsule->radius.y = radius;

	// Compute derived properties: intrinsic matrix, height and radius.
	MVector cylinderVector = point2 - point1;
	capsule->height = (float) cylinderVector.length();
	MVector cylinderDir = cylinderVector.normal();

	float yRotation = (float) atan(cylinderDir.x / cylinderDir.z);
	if (cylinderDir.z < 0) 
		yRotation += (float) FMath::Pi;

	float elevationAngle = (float) asin(cylinderDir.y);

	MTransformationMatrix rot0TM, rot1TM, rot2TM, translateTM;
	rot0TM.setToRotationAxis(MVector(-1.0, 0.0, 0.0), ((float) FMath::Pi) / 2.0f + elevationAngle);
	rot1TM.setToRotationAxis(MVector(0.0, 1.0, 0.0), yRotation);

	MVector centerPoint = point1 + (point2 - point1)/2;
	translateTM.setTranslation(centerPoint, MSpace::kTransform);

	intrinsicShapeMatrix = rot0TM.asMatrix();
	intrinsicShapeMatrix *= rot1TM.asMatrix();
	intrinsicShapeMatrix *= translateTM.asMatrix();
}


//--------------------------------------------------------
// Methods to add common techniques.
//

void DaePhysicsScene::addMassPropertiesInCommonTechnique(const MFnDagNode& dagFn, FCDPhysicsRigidBody* rigidBody, const MVector& centerOfMass)
{
	MPlug centerXPlug = dagFn.findPlug("centerOfMassX");
	MPlug centerYPlug = dagFn.findPlug("centerOfMassY");
	MPlug centerZPlug = dagFn.findPlug("centerOfMassZ");
	if (!centerOfMass.isEquivalent(MVector::zero) 
		|| CDagHelper::PlugHasAnimation(centerXPlug)
		|| CDagHelper::PlugHasAnimation(centerYPlug)
		|| CDagHelper::PlugHasAnimation(centerZPlug))
	{
		FMVector3 translation = MConvert::ToFMVector(centerOfMass);
		rigidBody->GetParameters()->SetMassFrameTranslate(translation);
		FCDParameterAnimatableVector3& rbTranslate = rigidBody->GetParameters()->GetMassFrameTranslate();
		MPlug centerPlug = dagFn.findPlug("centerOfMass");
		doc->GetAnimationLibrary()->AddPlugAnimation(centerPlug, rbTranslate, kVector);
	}
}

void DaePhysicsScene::addInertiaPropertiesInCommonTechnique(const MFnDagNode& dagFn, 
		FCDPhysicsRigidBody* rigidBody, const MVector& inertiaTensor)
{
	MPlug inertiaXPlug = dagFn.findPlug("inertiaTensorX");
	MPlug inertiaYPlug = dagFn.findPlug("inertiaTensorY");
	MPlug inertiaZPlug = dagFn.findPlug("inertiaTensorZ");
	if (!inertiaTensor.isEquivalent(MVector::zero) 
		|| CDagHelper::PlugHasAnimation(inertiaXPlug)
		|| CDagHelper::PlugHasAnimation(inertiaYPlug)
		|| CDagHelper::PlugHasAnimation(inertiaZPlug))
	{
		FMVector3 inertia = MConvert::ToFMVector(inertiaTensor);
		rigidBody->GetParameters()->SetInertia(inertia);
		FCDParameterAnimatableVector3& rbInertia = rigidBody->GetParameters()->GetInertia();
		MPlug inertiaPlug = dagFn.findPlug("inertiaTensor");
		doc->GetAnimationLibrary()->AddPlugAnimation(inertiaPlug, rbInertia, kVector);
	}
}

void DaePhysicsScene::setRigidBodyDynamic(FCDPhysicsRigidBody* rigidBody, bool isDynamic, MPlug activePlug)
{
	rigidBody->GetParameters()->SetDynamic(isDynamic);
	FCDParameterAnimatableFloat& rbDynamic = rigidBody->GetParameters()->GetDynamic();
	doc->GetAnimationLibrary()->AddPlugAnimation(activePlug, rbDynamic, kSingle);
}

void DaePhysicsScene::setRigidConstraintInfo(FCDPhysicsRigidConstraint* rigidConstraint, bool enabled, bool interpenetrate, const MVector& transLimitMin, const MVector& transLimitMax, const MVector& rotLimitMin, const MVector& rotLimitMax)
{
	rigidConstraint->SetEnabled(enabled);
	rigidConstraint->SetInterpenetrate(interpenetrate);
	rigidConstraint->SetLimitsSCTMin(MConvert::ToFMVector(rotLimitMin));
	rigidConstraint->SetLimitsSCTMax(MConvert::ToFMVector(rotLimitMax));
	rigidConstraint->SetLimitsLinearMin(MConvert::ToFMVector(transLimitMin));
	rigidConstraint->SetLimitsLinearMax(MConvert::ToFMVector(transLimitMax));
}

void DaePhysicsScene::setSceneInfo(float timeStep, const MVector& gravityMagnitude)
{
	FCDPhysicsScene* scene = physicsScene();
	scene->SetGravity(MConvert::ToFMVector(gravityMagnitude));
	scene->SetTimestep(timeStep);
}

//--------------------------------------------------------
// Export methods that involve some Maya structure traversal
// but that happen to be shared between the Native and Ageia
// implementations.
//

FCDPhysicsMaterial* DaePhysicsScene::includePhysicsMaterial(const MDagPath& dagPath)
{
	FCDPhysicsMaterial* material = addPhysicsMaterial(dagPath);

	// Set values of friction and restitution from Maya.
	MFnDagNode dagFn(dagPath);
	material->SetStaticFriction(floatAttribute(dagFn, "staticFriction"));
	material->SetDynamicFriction(floatAttribute(dagFn, "dynamicFriction"));
	material->SetRestitution(floatAttribute(dagFn, "bounciness"));
	return material;
}

void DaePhysicsScene::instantiateRigidBody(const MDagPath& rigidBodyDagPath, FCDPhysicsRigidBody* rigidBody)
{
	// Find the rigid body target transform.
	MDagPath transformDagPath = getTransformFromRigidBody(rigidBodyDagPath);
	DaeEntity* element = doc->GetSceneGraph()->FindElement(transformDagPath);
	if (element == NULL || element->entity == NULL || element->entity->GetObjectType() != FCDSceneNode::GetClassType()) return;

	// Create the instance
	FCDPhysicsRigidBodyInstance* instance = addInstanceRigidBody(rigidBody, (FCDSceneNode*) &*(element->entity));

	// Set the initial parameter values
	MFnDagNode dagFn(rigidBodyDagPath);

	// Export initial velocity.
	MVector vel;
	vel.x = floatAttribute(dagFn, "initialVelocityX");
	vel.y = floatAttribute(dagFn, "initialVelocityY");
	vel.z = floatAttribute(dagFn, "initialVelocityZ");
	
	// Export initial spin.
	MVector angVel;
	angVel.x = FMath::RadToDeg(floatAttribute(dagFn, "initialSpinX"));
	angVel.y = FMath::RadToDeg(floatAttribute(dagFn, "initialSpinY"));
	angVel.z = FMath::RadToDeg(floatAttribute(dagFn, "initialSpinZ"));

	if (!angVel.isEquivalent(MVector::zero))
	{
		instance->SetAngularVelocity(MConvert::ToFMVector(angVel));
	}
	if (!vel.isEquivalent(MVector::zero))
	{
		instance->SetVelocity(MConvert::ToFMVector(vel));
	}
}

// Return true to indicate success, if there is at least
// one supported shape within the rigid body.
bool DaePhysicsScene::findRegularShapesForRigidBody(const MDagPath& rigidBodyDagPath, MObjectArray& shapeObjects)
{
	MStatus status;
	MFnDependencyNode dgFn(rigidBodyDagPath.node());
	MString attribute = fRegularShapeAttributeOnRigidBody;
	MPlug plug = dgFn.findPlug(attribute, &status);
	if (status != MS::kSuccess)
	{
		MGlobal::displayWarning(MString("couldn't find plug on attribute ") + 
			attribute + MString(" on object ") + dgFn.name());
		return false;
	}

	if (plug.numElements() < 1)
	{
		MGlobal::displayWarning(MString("plug array doesn't have any connection on attribute ") + 
			attribute + MString(" on object ") + dgFn.name());
		return false;
	}

	// Iterate on all geometry nodes that are
	// part of this rigid body.
	//
	for (unsigned int i = 0; i < plug.numElements(); i++)
	{
		MPlug elementPlug = plug.connectionByPhysicalIndex(i);

		// Get the connection - there can be at most one input to a plug
		//
		MPlugArray connections;
		elementPlug.connectedTo(connections, true, true); // [claforte] I think this should be "false, true"
		if (connections.length() < 1)
		{
			MGlobal::displayWarning(MString("rigid body plug connected to an empty array on attribute ") + 
				attribute + MString(" on object ") + dgFn.name());
			return false;
		}

		MPlug connectedPlug = connections[0];
		MObject geometryNode = connectedPlug.node();

		// Verify that the object is of a supported shape type. Right now,
		// this means MFnMesh only.
		if (geometryNode.hasFn(MFn::kMesh))
		{
			shapeObjects.append(geometryNode);
		}
		else
		{
			MGlobal::displayWarning(MString("Rigid bodies are only supported on polygon meshes.\n") +
				MString("Unsupported rigid body on shape: ") + 
				geometryNode.apiTypeStr() + "... skipping.");
			continue; // skip other
		}
	}

	return (shapeObjects.length() >= 1);
}

DaePhysicsScene::ShapeType DaePhysicsScene::getShapeType(const MFnDependencyNode& depFn, 
														 MStatus* pOutStatus /* = NULL */)
{
	int shapeTypeInt = 0;

	// Look for the following attributes, in this order:
	// "shapeType" (used in physicsShape)
	// "physicsStandIn" (used in extended regular shapes and rigid bodies)
	// "standIn" (used in Maya's native rigid bodies, limited to none, box and sphere.)
	//
	MStatus status;
	shapeTypeInt = intAttribute(depFn, "shapeType", &status);
	
	if (!status)
		shapeTypeInt = intAttribute(depFn, "physicsStandIn", &status);

	if (!status)
		shapeTypeInt = intAttribute(depFn, "standIn", &status);

	if (!status && pOutStatus)
	{
		// Exhausted all possibilities.
		*pOutStatus = MS::kFailure;
	}

	return (ShapeType) shapeTypeInt;
}

// Return a shape intrinsic matrix if applicable 
// (e.g. matrix to place capsule along y axis),
// otherwise returns identity.
//
MMatrix DaePhysicsScene::exportPhysicsShapeGeometry(FCDPhysicsShape* shape, const MFnDagNode& shapeDagFn)
{
	MMatrix intrinsicShapeMatrix = MMatrix::identity;
	ShapeType shapeType = getShapeType(shapeDagFn);

	switch (shapeType)
	{
	case kBox:
		addBox(shape, vectorAttribute(shapeDagFn, "scale"));
		break;

	case kSphere:
		addSphere(shape, floatAttribute(shapeDagFn, "radius"));
		break;

	case kCapsule:
		addCapsule(shape, floatAttribute(shapeDagFn, "radius"),
			vectorAttribute(shapeDagFn, "point1"), 
			vectorAttribute(shapeDagFn, "point2"), intrinsicShapeMatrix);
		break;
		
	case kNone:
	case kObsoleteConvexHull:
	case kUnsupportedCylinder:
	case kObsoleteEllipsoid:
	default:
		break;
	}

	return intrinsicShapeMatrix;
}

MMatrix DaePhysicsScene::computeShapeTransform(const MDagPath& shapeDagPath,
											   const MDagPath& rigidBodyDagPath)
{
	// If a rigid body R is the direct child of a "parent transform" PT, then
	// a shape S that is part of R will have a flattened local space ST that will
	// be computed by concatenating the intermediate transforms T2, T3, ... , TN
	// where TN is the direct parent transform of S.
	//
	// An example follows. S1 is a direct child of PT. In this case, TN is the same
	// as RT, so S1 has an identity shape transform which doesn't need to be exported.
	//
	// In contrast, S2's direct parent is T3, whose direct parent is T2, whose direct
	// parent is PT. So the shape transform for S2 will be T3 * T2.
	//
	// --- PT
	//		|
	//		|--- R
	//		|
	//		|--- S1
	//		|
	//		|--- T2
	//		|	 |
	//		|	 |--- T3
	//		|		  |
	//				  |--- S2
	//

	// Find PT, the direct parent of the rigid body.
	MDagPath rigidBodyTransformDagPath = getTransformFromRigidBody(rigidBodyDagPath);
	
	MDagPath shapeAncestorDagPath = shapeDagPath;
	// remove the shape, otherwise we'll concatenate 
	// the shape local transform twice.
	// [claforte] TODO: DOES THIS REQUIRES A CHANGE FOR AGEIA-SPECIFIC BEHAVIOUR?
	shapeAncestorDagPath.pop(); 
		
	MMatrix concatenatedMatrix;

	// Climb up from the shape, so concatenate the transform matrices
	// for each parent T3, T2, ...
	while (!(shapeAncestorDagPath == rigidBodyTransformDagPath))
	{
		if (shapeAncestorDagPath.length() < rigidBodyTransformDagPath.length())
		{
			// The shape and the rigid body don't have a transform in common!
			// Strange.
			MGlobal::displayWarning(MString("Shape ") + shapeDagPath.partialPathName() +
				MString(" doesn't have common ancestor with rigid body ") + 
				rigidBodyDagPath.partialPathName() + ". Exporting with identity transform.");
			return MMatrix::identity;
		}

		MFnTransform currentTransform(shapeAncestorDagPath.transform());

		// [claforte] Verify that this code supports hierarchies.
		concatenatedMatrix *= currentTransform.transformationMatrix();

		// Go to the next parent.
		shapeAncestorDagPath.pop();
	}

	return concatenatedMatrix;
}

void DaePhysicsScene::exportShapeTransform(FCDPhysicsShape* shape, const MDagPath& shapeDagPath, const MDagPath& rigidBodyDagPath, const MMatrix& intrinsicShapeMatrix)
{
	// [claforte] TODO: Verify ordering.
	MMatrix transform = intrinsicShapeMatrix * computeShapeTransform(shapeDagPath, rigidBodyDagPath);
	addTranslateRotates(&shape->GetTransforms(), transform);
}

void DaePhysicsScene::finalize()
{
	// Iterate over all nodes in the scene.
	for (MItDependencyNodes it; !it.isDone(); it.next())
	{
		MObject	obj = it.item();
		MFnDagNode constraintDagFn(obj);

		if (!isRigidConstraint(constraintDagFn))
			continue; // skip non-rigid constraints

		// [claforte] TODO: Replace these ugly calls to getAPathTo() by
		// iterating over the DAG instead of the DG. This will be safer too.
		MDagPath constraintDagPath;
		MDagPath::getAPathTo(obj, constraintDagPath);

		// Create a new rigid constraint, then instance it.
		exportConstraint(constraintDagPath);
	}

	exportSceneParameters();
}

void DaePhysicsScene::exportConstraint(const MDagPath& constraintDagPath)
{
	// Verify that the connections are proper before creating the XML nodes.
	//
	MFnDagNode constraintDagFn(constraintDagPath);
	MObject rigidBody1Obj = CDagHelper::GetNodeConnectedTo(constraintDagFn.object(), "rigidBody1");
	MObject rigidBody2Obj = CDagHelper::GetNodeConnectedTo(constraintDagFn.object(), "rigidBody2");
	if (rigidBody1Obj.isNull())
	{
		MGlobal::displayWarning(MString("rigidBody1 is not connected to rigid constraint '") + constraintDagFn.name() + "'. Skipping constraint.");
		return; // skip constraint
	}

	// Create the rigid constraint definition.
	FCDPhysicsRigidConstraint* rigidConstraint = addRigidConstraint(constraintDagPath);
	addInstanceRigidConstraint(rigidConstraint);

	// Find the attached rigid bodies
	DagPathRigidBodyMap::iterator itR1 = rigidBodies.find(MDagPath::getAPathTo(rigidBody1Obj));
	DagPathRigidBodyMap::iterator itR2 = rigidBodies.find(MDagPath::getAPathTo(rigidBody2Obj));

	// Finally export the attachments. Note that this may automatically add
	// a target="#someVisualNode" to the instance.
	exportAttachment(rigidConstraint, (itR1 != rigidBodies.end()) ? (*itR1).second : NULL, constraintDagPath, (itR1 != rigidBodies.end()) ? (*itR1).first : MDagPath(), true);
	exportAttachment(rigidConstraint, (itR2 != rigidBodies.end()) ? (*itR2).second : NULL, constraintDagPath, (itR2 != rigidBodies.end()) ? (*itR2).first : MDagPath(), false);
	exportConstraintParams(rigidConstraint, constraintDagFn);
}

//
// [claforte] TODO: Code the import part. For now, since the Physics support
// is at a prototype stage, it's not a priority.
//


//---------------------------------------------------------------
// [claforte] Utility functions, which belong somewhere else
// but I don't have the time to figure out where. Note that are more
// complete and recent version of these utility methods is
// part of Nima.
//

float floatAttribute(const MFnDependencyNode& node, const MString& attributeName, MStatus* pReturnStatus /* = NULL */)
{
	MPlug plug = node.findPlug(attributeName, pReturnStatus);
	if (pReturnStatus && *pReturnStatus != MS::kSuccess)
		return 0.0f;

	double val = 0.0;
	plug.getValue(val);
	return (float) val;
}

int intAttribute(const MFnDependencyNode& node, const MString& attributeName, MStatus* pReturnStatus /* = NULL */)
{
	MPlug plug = node.findPlug(attributeName, pReturnStatus);
	if (pReturnStatus && *pReturnStatus != MS::kSuccess)
		return 0;

	int val = 0;
	plug.getValue(val);
	return val;
}

bool boolAttribute(const MFnDependencyNode& node, const MString& attributeName, MStatus* pReturnStatus /* = NULL */)
{
	MPlug plug = node.findPlug(attributeName, pReturnStatus);
	if (pReturnStatus && *pReturnStatus != MS::kSuccess)
		return 0;

	int val = 0;
	plug.getValue(val);

	if (val < 0 || val > 1)
	{
		MString warning = "Expected bool value for attribute '";
		warning += attributeName;
		warning += "' on node '";
		warning += node.name() + "', but instead found value: ";
		warning += val;
		MGlobal::displayWarning(warning);
	}
	
	return val != 0;
}

MVector vectorAttribute(const MFnDependencyNode& node, const MString& attributeName, MStatus* pReturnStatus /* = NULL */)
{
	MVector v(0.0, 0.0, 0.0);

	MPlug plug = node.findPlug(attributeName, pReturnStatus);
	if (pReturnStatus && *pReturnStatus != MS::kSuccess)
		return v;

	// Indicate failure by default.
	if (pReturnStatus)
		*pReturnStatus = MS::kFailure;

	// Verify that this is a vector.
	if (!plug.isCompound() || plug.numChildren() != 3)
	{
		MGlobal::displayWarning(MString("plug on node ") + node.name() + ", attribute " + attributeName + " is not a vector.");
		return v;
	}

	// Get the plugs to the children attributes.
	MPlug plug_x = plug.child(0);
	MPlug plug_y = plug.child(1);
	MPlug plug_z = plug.child(2);

	// Get the values from the children plugs.
	float x, y, z;
	plug_x.getValue(x);
	plug_y.getValue(y);
	plug_z.getValue(z);

	v.x = x; v.y = y; v.z = z;

	// Success.
	if (pReturnStatus)
		*pReturnStatus = MS::kSuccess;

	return v;
}

