/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include <maya/MFnCamera.h>
#include <maya/MFnTransform.h>
#include <maya/MFnGeometryFilter.h>
#include "DaeTransform.h"
#include "CExportOptions.h"
#include "CRotateHelper.h"
#include "CSkewHelper.h"
#include "TranslatorHelpers/HIAnimCache.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "FCDocument/FCDTransform.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDAnimated.h"
//#include "FUtils/FUDaeParser.h"
//using namespace FUDaeParser;

#define kAxisTolerance 0.001

DaeTransform::DaeTransform(DaeDoc* _doc, FCDSceneNode* _colladaNode)
{
	doc = _doc;
	isJoint = false;
	isFirstRotation = true;
	colladaNode = _colladaNode;
	element = NULL;
}

DaeTransform::~DaeTransform()
{
	doc = NULL;
	colladaNode = NULL;
}

//
// Create a new transform node
//
MObject DaeTransform::CreateTransform(const MObject& parent, const MString& name)
{
	MFnTransform fn;
	transform = fn.create(parent);
	if (name.length() > 0) fn.setName(name);
	return transform;
}

void DaeTransform::SetTransform(const MObject& transform)
{
	this->transform = transform;
	if (!transform.isNull())
	{
		MFnTransform fn(transform);
		transformation = fn.transformation();

		if (fn.parentCount() > 0)
		{
			MFnTransform t(fn.parent(0));
			if (t.hasObj(MFn::kClusterFilter) || t.hasObj(MFn::kSkinClusterFilter))
			{
				transformation = MTransformationMatrix(transformation.asMatrix() * t.transformationMatrix());
			}
		}
	}
	else
	{
		transformation = MTransformationMatrix::identity;
	}
}

void DaeTransform::SetTransformation(const MTransformationMatrix& transformation)
{
	this->transformation = transformation;
}

// Add this transform's manipulations to the xml node
//
void DaeTransform::ExportTransform()
{
	MFnTransform transform(this->transform);
	MDagPath dagPath = MDagPath::getAPathTo(this->transform);

	if (CExportOptions::BakeTransforms())
	{
		ExportMatrix(transformation.asMatrix());
	}
	else if (CExportOptions::ExportCameraAsLookat() && dagPath.hasFn(MFn::kCamera))
	{
		ExportLookatTransform();
	}
	else
	{
		ExportDecomposedTransform();
	}

	ExportVisibility();
}

void DaeTransform::ExportLookatTransform()
{
	// Compute local space parameters and export them. These parameters are:
	// - Eye position
	// - Interest point
	// - Up-axis direction
	//
	// TODO: camera animations for look-at transform are not implemented yet.
	//
	
	// Locate the camera in the dagPath
	MFnTransform transform(this->transform);
	MObject cameraObject(MObject::kNullObj);
	uint pathChildCount = transform.childCount();
	for (uint i = 0; i < pathChildCount; ++i)
	{
		MObject child = transform.child(i);
		if (child.hasFn(MFn::kCamera)) { cameraObject = child; break; }
	}
	if (cameraObject == MObject::kNullObj)
	{
		// Revert to using decomposed transforms.
		ExportDecomposedTransform();
	}
	else
	{
		// Get the camera matrix from which the other parameters
		// are computed.
		MFnCamera camera(cameraObject);
		MMatrix matrix = transform.transformationMatrix();
		matrix.homogenize();

		// Get the position of the camera in local space.
		MVector eye(matrix[3][0], matrix[3][1], matrix[3][2]);

		// Compute center of interest.
		double centerOfInterestDistance = camera.centerOfInterestPoint(MSpace::kObject).z;
		MVector front(matrix[2][0], matrix[2][1], matrix[2][2]);
		MVector centerOfInterest = eye + (front * centerOfInterestDistance);

		FCDTLookAt* lookAtTransform = (FCDTLookAt*) colladaNode->AddTransform(FCDTransform::LOOKAT);
		lookAtTransform->SetPosition(MConvert::ToFMVector(eye));
		lookAtTransform->SetTarget(MConvert::ToFMVector(centerOfInterest));

		// Extract the up direction, which corresponds to the second row.
		lookAtTransform->SetUp((float) matrix[1][0], (float) matrix[1][1], (float) matrix[1][2]);
	}
}

void DaeTransform::ExportDecomposedTransform()
{
	MVector translation = transformation.translation(MSpace::kTransform);
	MPoint rotatePivotTranslation = transformation.rotatePivotTranslation(MSpace::kTransform);
	MPoint rotatePivot = transformation.rotatePivot(MSpace::kTransform, NULL);
	MVector scalePivotTranslation = transformation.scalePivotTranslation(MSpace::kTransform);
	MVector scalePivot = transformation.scalePivot(MSpace::kTransform);
	double shear[3] = {0, 0, 0}; transformation.getShear(shear, MSpace::kTransform);

	MEulerRotation jointOrientation, rotation, rotationAxis;
	bool isJoint;
	if (transform != MObject::kNullObj)
	{
		isJoint = CDagHelper::GetPlugValue(transform, "jointOrient", jointOrientation);
		if (!CDagHelper::GetPlugValue(transform, "rotate", rotation)) rotation.setValue(0, 0, 0);
		if (!CDagHelper::GetPlugValue(transform, "rotateAxis", rotationAxis)) rotationAxis.setValue(0, 0, 0);

		rotation.order = (MEulerRotation::RotationOrder)((int) transformation.rotationOrder() - MTransformationMatrix::kXYZ + MEulerRotation::kXYZ);
		rotationAxis.order = jointOrientation.order = MEulerRotation::kXYZ;
	}
	else
	{
		rotation = transformation.eulerRotation();
		rotation.order = (MEulerRotation::RotationOrder)((int) transformation.rotationOrder() - MTransformationMatrix::kXYZ + MEulerRotation::kXYZ);
		isJoint = false;
	}

	// This is the order of the transforms:
	//
	// matrix = [SP-1 * S * SH * SP * ST] * [RP-1 * RA * R * JO * RP * RT] * T
	//          [        scale          ] * [          rotation          ] * translation
	//
	// Where SP is scale pivot translation, S is scale, SH is shear, ST is scale pivot translation
	// RP is rotation pivot, RA is rotation axis, R is rotation, RP is rotation pivot,
	// RT is rotation pivot translation, T is translation, JO is joint orientation
	//
	// references: Maya documentation - transform node, Maya documentation - joint node
	// NOTE: Left multiplying, column-order matrices
	//
	ExportTranslation("translate", translation, true);
	ExportTranslation("rotatePivotTranslation", rotatePivotTranslation, false);
	ExportTranslation("rotatePivot", rotatePivot, false);
	if (isJoint) ExportRotation("jointOrient", jointOrientation, presence.jointOrient, presence.jointOrientNode);
	ExportRotation("rotate", rotation, presence.rot, presence.rotNode);
	ExportRotation("rotateAxis", rotationAxis, presence.rotAxis, presence.rotAxisNode); // Also called rotate orient in documentation
	ExportTranslation("rotatePivotInverse", rotatePivot * -1, false);
	ExportTranslation("scalePivotTranslation", scalePivotTranslation, false);
	ExportTranslation("scalePivot", scalePivot, false);
	CSkewHelper::ExportSkew(colladaNode, shear);

	bool exportScale = true;


	if (exportScale)
		ExportScale();
	ExportTranslation("scalePivotInverse", scalePivot * -1, false);
}

//
// Element export
//
void DaeTransform::ExportTranslation(const char* name, const MPoint& translation, bool animation)
{ ExportTranslation(name, MVector(translation), animation); }
void DaeTransform::ExportTranslation(const char* name, const MVector& _translation, bool animation)
{
	FMVector3 translation = MConvert::ToFMVector(_translation);
	bool isZero = IsEquivalent(translation, FMVector3::Zero);
	if (animation || !isZero)
	{
		FCDTTranslation* colladaTranslate = (FCDTTranslation*) colladaNode->AddTransform(FCDTransform::TRANSLATION);
		colladaTranslate->SetSubId(name);
		colladaTranslate->SetTranslation(translation);
		if (animation)
		{
			ANIM->AddPlugAnimation(transform, name, colladaTranslate->GetTranslation(), kVector | kLength);
			presence.trans = isZero ? DaeTransformPresence::kPresent : DaeTransformPresence::kNecessary;
			presence.transNode = colladaTranslate;

			if (transform == MObject::kNullObj && presence.trans == DaeTransformPresence::kPresent)
			{
				SAFE_RELEASE(presence.transNode);
				presence.trans = DaeTransformPresence::kUnused;
			}
		}
	}
}

void DaeTransform::ExportRotation(const char* name, const MEulerRotation& rotation, DaeTransformPresence::EPresence* p, FCDTRotation** rotationTransforms)
{
	MString str(name);

	CRotateHelper rotateHelper(doc, NULL, rotation);
	rotateHelper.SetTransformId(name);
	rotateHelper.SetParentNode(colladaNode);
	rotateHelper.Output();

	bool isZero[3] = { IsEquivalent(rotation.x, 0.0), IsEquivalent(rotation.y, 0.0), IsEquivalent(rotation.z, 0.0) };
	const char* components[3] = { "X", "Y", "Z" };
	for (size_t i = 0; i < 3; ++i)
	{
		if (rotateHelper.colladaRotations[i] != NULL)
		{
			FCDAnimated* animated = rotateHelper.colladaRotations[i]->GetAngleAxis().GetAnimated();
			ANIM->AddPlugAnimation(transform, str + components[i], animated, kSingle | kQualifiedAngle);
			p[i] = (!isFirstRotation && isZero[i]) ? DaeTransformPresence::kPresent : DaeTransformPresence::kNecessary;
			rotationTransforms[i] = rotateHelper.colladaRotations[i];

			if (transform == MObject::kNullObj && p[i] == DaeTransformPresence::kPresent)
			{
				SAFE_RELEASE(rotationTransforms[i]);
				p[i] = DaeTransformPresence::kUnused;
			}
		}
	}

	isFirstRotation = false;
}

void DaeTransform::ExportScale()
{
	double scale[3] = {1, 1, 1};
	transformation.getScale(scale, MSpace::kTransform);
	FMVector3 scaleV((float) scale[0], (float) scale[1], (float) scale[2]);

	presence.scale = IsEquivalent(scaleV, FMVector3::One) ? DaeTransformPresence::kPresent : DaeTransformPresence::kNecessary;
	if (transform != MObject::kNullObj || presence.scale == DaeTransformPresence::kNecessary)
	{
		presence.scaleNode = (FCDTScale*) colladaNode->AddTransform(FCDTransform::SCALE);
		presence.scaleNode->SetSubId("scale");
		presence.scaleNode->SetScale(scaleV);
		
		FCDAnimated* animated = presence.scaleNode->GetScale().GetAnimated();
		ANIM->AddPlugAnimation(transform, "scale", animated, kVector);
	}
	else
	{
		presence.scale = DaeTransformPresence::kUnused;
		presence.scaleNode = NULL;
	}
}

void DaeTransform::ExportMatrix(const MMatrix& matrix)
{
	FCDTMatrix* colladaTransform = (FCDTMatrix*) colladaNode->AddTransform(FCDTransform::MATRIX);
	colladaTransform->SetSubId("transform");
	colladaTransform->SetTransform(MConvert::ToFMatrix(matrix));

	// For animations, sampling is always enforced for baked transforms.
	MPlug p = MFnDagNode(transform).findPlug("matrix");
	doc->GetAnimationCache()->CachePlug(p, true);
	
	FCDAnimated* animated = colladaTransform->GetTransform().GetAnimated();
	ANIM->AddPlugAnimation(p, animated, kMatrix);
}

void DaeTransform::ExportVisibility()
{
	bool isVisible;
	if (transform != MObject::kNullObj && CDagHelper::GetPlugValue(transform, "visibility", isVisible))
	{
		// Add an <extra> node with a visibility parameters that the animation can target
		colladaNode->SetVisibility(isVisible);
		FCDAnimated* animated = colladaNode->GetVisibility().GetAnimated();
		ANIM->AddPlugAnimation(transform, "visibility", animated, kBoolean);
	}
}

//
// Load the xml manipulations into the Maya transform node
//
void DaeTransform::ImportTransform(FCDSceneNode* colladaNode)
{
	isJoint = colladaNode->GetJointFlag();

	// Import the visibility and user-defined parameters
	FCDParameterAnimatableFloat& visibility = colladaNode->GetVisibility();
	CDagHelper::SetPlugValue(transform, "visibility", !IsEquivalent(*visibility, 0.0f));
	ANIM->AddPlugAnimation(transform, "visibility", visibility, kBoolean);

	// Attempt to bucket the transformations, forcing them to match Maya's transform stack
	if (BucketTransforms(colladaNode)) return;

	// The transforms for this node does not match Maya's transform stack, so read them in as matrices:
	// while sampling any animations
	if (!ANIM->ImportAnimatedSceneNode(transform, colladaNode))
	{
		// Scene node transforms are not animated, so simply import as a matrix.
		MTransformationMatrix tm(MConvert::ToMMatrix(colladaNode->ToMatrix()));
		MFnTransform transformFn(transform);
		transformFn.set(tm);
	}
}

void DaeTransform::CalculateRotationOrder()
{
	MFnTransform transform(this->transform);

	// When dealing with mixed angle-axis and known angle rotations, don't import a rotation order.
	if (noRotationOrder) return;

	// Up to three different rotation orders might have been extracted from the bucketed rotations.
	// We deal with whether we can support these, afterwards.
	MTransformationMatrix::RotationOrder rotationOrders[3];
	int offset = 0;
	for (int k = 0; k < 3; ++k)
	{
		rotationOrders[k] = MTransformationMatrix::kInvalid;

		// Set the found rotation order
		// Following the bucketing algorithm, the rotation order count should never be greater than three.
		int xIndex = -1, yIndex = -1, zIndex = -1, i = offset;
		for (; i < rotationOrderCount; ++i)
		{
			bool alreadyCovered = false;
			switch (rotationOrder[i])
			{
				case 0: if (zIndex == -1) zIndex = i; else alreadyCovered = true; break;
				case 1: if (yIndex == -1) yIndex = i; else alreadyCovered = true; break;
				case 2: if (xIndex == -1) xIndex = i; else alreadyCovered = true; break;
				default: { FUBreak; }
			}
			if (alreadyCovered) break;
		}
		offset = i;

		if (xIndex == -1 && yIndex == -1 && zIndex == -1) continue;
		if (zIndex == -1) zIndex = rotationOrderCount + 0; // The prefered order to assume.
		if (yIndex == -1) yIndex = rotationOrderCount + 1;
		if (xIndex == -1) xIndex = rotationOrderCount + 2;

		if (xIndex > yIndex && yIndex > zIndex) rotationOrders[k] = MTransformationMatrix::kXYZ;
		else if (xIndex > zIndex && zIndex > yIndex) rotationOrders[k] = MTransformationMatrix::kXZY;
		else if (yIndex > xIndex && xIndex > zIndex) rotationOrders[k] = MTransformationMatrix::kYXZ;
		else if (yIndex > zIndex && zIndex > xIndex) rotationOrders[k] = MTransformationMatrix::kYZX;
		else if (zIndex > xIndex && xIndex > zIndex) rotationOrders[k] = MTransformationMatrix::kZXY;
		else if (zIndex > yIndex && yIndex > xIndex) rotationOrders[k] = MTransformationMatrix::kZYX;
	}

	// Now, look at this information and see whether we can support it.
	int validRotationOrderCount = 0;
	for (; validRotationOrderCount < 3; ++validRotationOrderCount)
	{
		if (rotationOrders[validRotationOrderCount] == MTransformationMatrix::kInvalid) break;
	}

	if (validRotationOrderCount == 0) transform.setRotationOrder(MTransformationMatrix::kXYZ, false);
	else if (validRotationOrderCount == 1) transform.setRotationOrder(rotationOrders[0], false);
	else
	{
		// The old versions of ColladaMaya (3.05A and before) would have all the same rotation order.
		bool allSame = true;
		for (int k = 1; k < validRotationOrderCount; ++k)
		{
			allSame &= rotationOrders[k-1] == rotationOrders[k];
		}
		if (allSame) transform.setRotationOrder(rotationOrders[0], false);

		// The newer versions of ColladaMaya should have XYZ on all except one. This one would be the first or the second.
		int firstNonXYZ = 0;
		for (; firstNonXYZ < validRotationOrderCount; ++firstNonXYZ)
		{
			if (rotationOrders[firstNonXYZ] != MTransformationMatrix::kXYZ) break;
		}

		// Not feeling fancy. Otherwise, if there is unexpected rotation orders,
		// we should stop the transform bucket process and switch to sampling.
		if (firstNonXYZ < validRotationOrderCount) transform.setRotationOrder(rotationOrders[firstNonXYZ], false);
		else transform.setRotationOrder(rotationOrders[0], false);
	}
}

void DaeTransform::ImportTranslation(FCDTTranslation* translation, const char* plugName)
{
	MFnTransform transform(this->transform);
	MVector v = MConvert::ToMVector(translation->GetTranslation());

	MPlug translatePlug = transform.findPlug(plugName);
	CDagHelper::SetPlugValue(translatePlug, v);
	ANIM->AddPlugAnimation(transform.object(), plugName, translation->GetAnimated(), kVector | kLength);
}

void DaeTransform::ImportRotation(FCDTRotation* rotation, const char* plugName)
{
	MFnTransform transform(this->transform);
	MPlug rotationPlug = transform.findPlug(plugName);

	const FMVector3& axis = rotation->GetAxis();
	const float& angle = rotation->GetAngle();
	bool isAxisX = IsEquivalent(axis, FMVector3::XAxis);
	bool isAxisY = IsEquivalent(axis, FMVector3::YAxis);
	bool isAxisZ = IsEquivalent(axis, FMVector3::ZAxis);
	if (!isAxisX && !isAxisY && !isAxisZ)
	{
		// Angle-axis rotation: support only unanimated
		MTransformationMatrix mT(MConvert::ToMMatrix(rotation->ToMatrix()));
		MEulerRotation er = mT.eulerRotation();
		MVector vv = MVector(er.x, er.y, er.z);
		CDagHelper::SetPlugValue(rotationPlug, vv);
	}
	else
	{
		// Euler rotation: set the angle on the correct axis
		int index = isAxisX ? 0 : isAxisY ? 1 : 2;
		MPlug rotatePlug = rotationPlug.child(index);
		rotatePlug.setValue(FMath::DegToRad(angle));
		ANIM->AddPlugAnimation(rotatePlug, rotation->GetAngleAxis().GetAnimated(), kSingle | kQualifiedAngle);
	}
}

void DaeTransform::ImportScale(FCDTScale* scale)
{
	MFnTransform transform(this->transform);

	const FMVector3& scaleFactor = scale->GetScale();
	double s[3] = { scaleFactor.x, scaleFactor.y, scaleFactor.z };
	transform.setScale(s);
	ANIM->AddPlugAnimation(transform.object(), "scale", scale->GetAnimated(), kVector);
}

void DaeTransform::ImportSkew(FCDTSkew* skew, const char* plugName)
{
	MFnTransform transform(this->transform);

	const float& angle = skew->GetAngle();
	MPlug plug = transform.findPlug(plugName);
	plug.setValue(tan(FMath::DegToRad(angle)));
	ANIM->AddPlugAnimation(plug, skew->GetAnimated(), kSingle | kQualifiedAngle);
}

void DaeTransform::ClearTransform()
{
#define CHECK_PRESENCE(presenceV, presenceP) { \
	if (presenceV != NULL && !presenceV->IsAnimated() && presenceP != DaeTransformPresence::kNecessary) { \
	SAFE_RELEASE(presenceV); } }

	CHECK_PRESENCE(presence.transNode, presence.trans);
	CHECK_PRESENCE(presence.scaleNode, presence.scale);
	CHECK_PRESENCE(presence.rotNode[0], presence.rot[0]);
	CHECK_PRESENCE(presence.rotNode[1], presence.rot[1]);
	CHECK_PRESENCE(presence.rotNode[2], presence.rot[2]);
	CHECK_PRESENCE(presence.rotAxisNode[0], presence.rotAxis[0]);
	CHECK_PRESENCE(presence.rotAxisNode[1], presence.rotAxis[1]);
	CHECK_PRESENCE(presence.rotAxisNode[2], presence.rotAxis[2]);
	CHECK_PRESENCE(presence.jointOrientNode[0], presence.jointOrient[0]);
	CHECK_PRESENCE(presence.jointOrientNode[1], presence.jointOrient[1]);
	CHECK_PRESENCE(presence.jointOrientNode[2], presence.jointOrient[2]);
#undef CHECK_PRESENCE
}

#define BUCKET(level) { buckets[level] = t; bucketDepth = level; return true; }
#define MOVE_BUCKET(level0, level1) { buckets[level1] = buckets[level0]; buckets[level0] = NULL; }

// Attempt to bucket the COLLADA transforms in the slots of the 3dsMax transform
bool DaeTransform::BucketTransforms(FCDSceneNode* node)
{
	// Transform buckets: matches Maya's transform stack
	// Any deviation in the COLLADA scene node from this will force baking of the transform
	// Yes, Maya's transform stack is that NASTY.
	// See Maya's API doc on "All about: transform node" and "All about: joint node".
	// Remember that you can change the rotation order :(

	// Initialize the buckets and the rotation order
	for (uint32 i = 0; i < BUCKET_COUNT; ++i) buckets[i] = NULL;
	bucketDepth = -1;
	rotationOrderCount = 0;
	noRotationOrder = false;
	for (uint i = 0; i < 3; ++i) rotationOrder[i] = -1;

	// Attempt to fill in the buckets with the COLLADA scene node transforms
	size_t transformCount = node->GetTransformCount();
	for (size_t t = 0; t < transformCount; ++t)
	{
		FCDTransform* transform = node->GetTransform(t);
		switch (transform->GetType())
		{
		case FCDTransform::TRANSLATION: if (!BucketTranslation((FCDTTranslation*) transform)) return false; break;
		case FCDTransform::ROTATION: if (!BucketRotation((FCDTRotation*) transform)) return false; break;
		case FCDTransform::SCALE: if (!BucketScale((FCDTScale*) transform)) return false; break;
		case FCDTransform::SKEW: if (!BucketSkew((FCDTSkew*) transform)) return false; break;

		case FCDTransform::MATRIX:
		case FCDTransform::LOOKAT:
		default:
			// No place for these in the Maya transform stack: force sampling
			return false;
		}
	}

	// Verify that if and only if there is a pivot, there is its reverse
	while (buckets[ROTATE_PIVOT] != NULL && buckets[ROTATE_PIVOT_R] == NULL)
	{
		// One possibility is no rotations and rotate pivot is in the pivot_translation
		if (buckets[ROTATE_PIVOT_TRANSLATION] != NULL && buckets[ROTATE_PIVOT_TRANSLATION]->IsInverse(buckets[ROTATE_PIVOT]))
		{
			bool badBucketFound = false;
			for (int b = ROTATE_PIVOT + 1; b < SCALE_PIVOT_TRANSLATION && !badBucketFound; ++b) badBucketFound = buckets[b] != NULL;
			if (!badBucketFound)
			{
				MOVE_BUCKET(ROTATE_PIVOT, ROTATE_PIVOT_R)
				MOVE_BUCKET(ROTATE_PIVOT_TRANSLATION, ROTATE_PIVOT)
				break;
			}
		}

		// One possibility is moving the translation over to SCALE_PIVOT_TRANSLATION
		if (buckets[ROTATE_PIVOT]->IsAnimated()) return false;
		for (int b = ROTATE_PIVOT + 1; b < SCALE_PIVOT_TRANSLATION; ++b) if (buckets[b] != NULL) return false;
		buckets[SCALE_PIVOT_TRANSLATION] = buckets[ROTATE_PIVOT];
		buckets[ROTATE_PIVOT] = NULL;
		break;
	}

	if (buckets[SCALE_PIVOT] != NULL && buckets[SCALE_PIVOT_R] == NULL)
	{
		// One possibility is no scale and scale pivot is in the pivot_translation
		if (buckets[SCALE] != NULL || buckets[SCALE_PIVOT_TRANSLATION] == NULL) return false;
		if (!buckets[SCALE_PIVOT_TRANSLATION]->IsInverse(buckets[SCALE_PIVOT])) return false;
		MOVE_BUCKET(SCALE_PIVOT, SCALE_PIVOT_R)
		MOVE_BUCKET(SCALE_PIVOT_TRANSLATION, SCALE_PIVOT)
	}

	// If there is a joint orientation, but not normal rotation,
	// move the joint orientation to the normal rotation bucket
	if (buckets[ROTATE_A] == NULL && buckets[ROTATE_1] == NULL && buckets[ROTATE_2] == NULL && buckets[ROTATE_3] == NULL)
	{
		MOVE_BUCKET(JOINT_ORIENT_1, ROTATE_1)
		MOVE_BUCKET(JOINT_ORIENT_2, ROTATE_2)
		MOVE_BUCKET(JOINT_ORIENT_3, ROTATE_3)
	}

	// The buckets contain all the COLLADA transforms, in the correct order
	// Now, import the transformations!
#define IMPORT_T(level, plugName) if (buckets[level] != NULL) ImportTranslation((FCDTTranslation*) buckets[level], plugName);
#define IMPORT_R(level, plugName) if (buckets[level] != NULL) ImportRotation((FCDTRotation*) buckets[level], plugName);
#define IMPORT_S(level) if (buckets[level] != NULL) ImportScale((FCDTScale*) buckets[level]);
#define IMPORT_K(level, plugName) if (buckets[level] != NULL) ImportSkew((FCDTSkew*) buckets[level], plugName);
	IMPORT_T(TRANSLATION, "translate")
	IMPORT_T(ROTATE_PIVOT_TRANSLATION, "rotatePivotTranslate")
	IMPORT_T(ROTATE_PIVOT, "rotatePivot")
	IMPORT_R(JOINT_ORIENT_1, "jointOrient")
	IMPORT_R(JOINT_ORIENT_2, "jointOrient")
	IMPORT_R(JOINT_ORIENT_3, "jointOrient")
	IMPORT_R(ROTATE_1, "rotate")
	IMPORT_R(ROTATE_2, "rotate")
	IMPORT_R(ROTATE_3, "rotate")
	IMPORT_R(ROTATE_A, "rotate")
	IMPORT_R(ROTATE_AXIS_1, "rotateAxis")
	IMPORT_R(ROTATE_AXIS_2, "rotateAxis")
	IMPORT_R(ROTATE_AXIS_3, "rotateAxis")
	IMPORT_T(SCALE_PIVOT_TRANSLATION, "scalePivotTranslate")
	IMPORT_T(SCALE_PIVOT, "scalePivot")
	IMPORT_S(SCALE)
	IMPORT_K(SKEW_XY, "shearXY")
	IMPORT_K(SKEW_XZ, "shearXZ")
	IMPORT_K(SKEW_YZ, "shearYZ")
#undef IMPORT_T
#undef IMPORT_R
#undef IMPORT_S
#undef IMPORT_K

	CalculateRotationOrder();
	return true;
}

bool DaeTransform::BucketTranslation(FCDTTranslation* t)
{
	bool isAnimated = t->IsAnimated();

	if (bucketDepth < TRANSLATION) BUCKET(TRANSLATION)
	else if (bucketDepth < ROTATE_PIVOT_TRANSLATION && !isAnimated) BUCKET(ROTATE_PIVOT_TRANSLATION)
	else if (bucketDepth < ROTATE_PIVOT) BUCKET(ROTATE_PIVOT)
	else if (bucketDepth < ROTATE_PIVOT_R)
	{
		// Verify that this is the reverse pivot translation.
		if (buckets[ROTATE_PIVOT] != NULL)
		{
			if (buckets[ROTATE_PIVOT]->IsInverse(t)) BUCKET(ROTATE_PIVOT_R)
			else if (bucketDepth == ROTATE_PIVOT && !buckets[ROTATE_PIVOT]->IsAnimated())
			{
				MOVE_BUCKET(ROTATE_PIVOT, SCALE_PIVOT_TRANSLATION);
				BUCKET(SCALE_PIVOT)
			}
			else return false;
		}
		else if (buckets[ROTATE_PIVOT_TRANSLATION] != NULL && buckets[ROTATE_PIVOT_TRANSLATION]->IsInverse(t))
		{
			MOVE_BUCKET(ROTATE_PIVOT_TRANSLATION, ROTATE_PIVOT)
			BUCKET(ROTATE_PIVOT_R)
		}
		else if (!isAnimated) BUCKET(SCALE_PIVOT_TRANSLATION)
		else BUCKET(SCALE_PIVOT)
	}
	else if (bucketDepth < SCALE_PIVOT_TRANSLATION && !isAnimated) BUCKET(SCALE_PIVOT_TRANSLATION)
	else if (bucketDepth < SCALE_PIVOT) BUCKET(SCALE_PIVOT)
	else if (bucketDepth < SCALE_PIVOT_R)
	{
		// Verify that this is the reverse scale translation. Easier since there is no bucket advance spots
		if (buckets[SCALE_PIVOT] != NULL)
		{
			if (buckets[SCALE_PIVOT]->IsInverse(t)) BUCKET(SCALE_PIVOT_R)
			else return false;
		}
		else if (buckets[SCALE_PIVOT_TRANSLATION] != NULL && buckets[SCALE_PIVOT_TRANSLATION]->IsInverse(t))
		{
			MOVE_BUCKET(SCALE_PIVOT_TRANSLATION, SCALE_PIVOT)
			BUCKET(SCALE_PIVOT_R)
		}
		else return false;
	}
	else return false; // No more translation buckets to fill
}

bool DaeTransform::BucketRotation(FCDTRotation* t)
{
	const FMVector3& axis = t->GetAxis();
	bool isAxisX = IsEquivalent(axis, FMVector3::XAxis);
	bool isAxisY = IsEquivalent(axis, FMVector3::YAxis);
	bool isAxisZ = IsEquivalent(axis, FMVector3::ZAxis);
	if (!isAxisX && !isAxisY && !isAxisZ)
	{
		// Only one spot for angle-axis, and must be un-animated
		if (bucketDepth < ROTATE_A && !t->IsAnimated() && buckets[ROTATE_1] == NULL
			&& buckets[ROTATE_2] == NULL && buckets[ROTATE_3] == NULL)
		{
			noRotationOrder = true;
			BUCKET(ROTATE_A)
		}
		else return false;
	}

	// Check/Update the rotation order
	int order = isAxisX ? 2 : isAxisY ? 1 : 0;
	rotationOrder[rotationOrderCount++] = order;

	// Bucket the Euler rotation
	if (buckets[JOINT_ORIENT_1 + order] != NULL) return false;
	else if (isJoint && bucketDepth < JOINT_ORIENT_1 + 3) BUCKET(JOINT_ORIENT_1 + order)
	else if (bucketDepth < ROTATE_1 + 3) BUCKET(ROTATE_1 + order)
	else if (bucketDepth < ROTATE_AXIS_1 + 3) BUCKET(ROTATE_AXIS_1 + order)
	else return false;
}

bool DaeTransform::BucketScale(FCDTScale* t)
{
	// Only one scale transform is allowed per node.
	if (bucketDepth < SCALE) BUCKET(SCALE)
	else return false;
}

bool DaeTransform::BucketSkew(FCDTSkew* t)
{
	int wantedBucket;
	if (IsEquivalent(t->GetRotateAxis(), FMVector3::XAxis) && IsEquivalent(t->GetAroundAxis(), FMVector3::YAxis)) wantedBucket = SKEW_XY;
	else if (IsEquivalent(t->GetRotateAxis(), FMVector3::XAxis) && IsEquivalent(t->GetAroundAxis(), FMVector3::ZAxis)) wantedBucket = SKEW_XZ;
	else if (IsEquivalent(t->GetRotateAxis(), FMVector3::YAxis) && IsEquivalent(t->GetAroundAxis(), FMVector3::ZAxis)) wantedBucket = SKEW_YZ;
	else return false;

	if (bucketDepth < wantedBucket) BUCKET(wantedBucket)
	else return false;
}
