/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/
/*
	Based on the 3dsMax COLLADA Tools:
	Copyright (C) 2005-2006 Autodesk Media Entertainment
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

// Scene Node Transform Importer Class

#include "StdAfx.h"
#include "decomp.h"
#include "AnimationImporter.h"
#include "DocumentImporter.h"
#include "TransformImporter.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDTransform.h"
#include "ColladaModifiers.h"
#include "Controller/ColladaController.h"

TransformImporter::TransformImporter(DocumentImporter* _document, INode* _iNode)
:	document(_document), iNode(_iNode)
,	forceSampling(false), isAnimated(false)
,	colladaSceneNode(NULL)
{
	for (uint32 i = 0; i < BUCKET_COUNT; ++i) buckets[i] = NULL;

	// Overwrite the node's TM controller.
	// GL: Needs to be under an import option.
	// _iNode->SetTMController(new ColladaController());
}

TransformImporter::~TransformImporter()
{
	document = NULL;
	iNode = NULL;
	colladaSceneNode = NULL;

	for (uint32 i = 0; i < BUCKET_COUNT; ++i) buckets[i] = NULL;
}

void TransformImporter::ImportSceneNode(FCDSceneNode* _colladaSceneNode)
{
	colladaSceneNode = _colladaSceneNode;

	// Verify that there is an animation on the transform
	isAnimated = false;
	size_t transformCount = colladaSceneNode->GetTransformCount();
	for (size_t t = 0; t < transformCount && !isAnimated; ++t)
	{
		isAnimated = colladaSceneNode->GetTransform(t)->IsAnimated();
	}

	// Set the default transformation matrix onto the transform
	iNode->SetNodeTM(TIME_INITIAL_POSE, ToMatrix3(colladaSceneNode->ToMatrix()));

	// For unanimated transforms, just patching in the matrix is enough
	if (isAnimated)
	{
        // For animated transforms, first verify that the Max transform stack can express the animation
		// by bucketing the transforms in order
		forceSampling = !BucketTransforms(colladaSceneNode->GetTransforms(), colladaSceneNode->GetTransformCount());

		// The animation(s) will be imported later, during finalization, since the
		// animations are destroyed when a targeted node is attached to its target
		// [staylor] node attachment can be non-destructive, if we wanted to import here...
	}
}

// Attempt to bucket the COLLADA transforms in the slots of the 3dsMax transform
bool TransformImporter::BucketTransforms(const FCDTransform** transforms, size_t transformCount)
{
	// Initialize the buckets
	for (uint32 i = 0; i < BUCKET_COUNT; ++i) buckets[i] = NULL;
	int bucketDepth = -1;

	// Attempt to fill in the buckets with the COLLADA scene node transforms
	for (size_t t = 0; t < transformCount; ++t)
	{
		// Verify that this transformation is either animated or does an actual real transform.
		const FCDTransform* transform = transforms[t];
		FMMatrix44 testTransform = transform->ToMatrix();
		bool isValid = !IsEquivalent(testTransform, FMMatrix44::Identity);
		const FCDAnimated* animated = transform->GetAnimated();
		if (animated != NULL) isValid |= animated->HasCurve();
		if (!isValid) continue; // This transform is considered irrelevant.

		// Attempt to bucket this transformation.
		switch (transform->GetType())
		{
		case FCDTransform::TRANSLATION:
			if (bucketDepth >= TRANSLATION || buckets[TRANSLATION] != NULL)
			{
				// Only one translation transform is allowed per node.
				return false;
			}
			buckets[TRANSLATION] = transform;
			bucketDepth = TRANSLATION;
			break;

		case FCDTransform::SCALE:
			if (bucketDepth >= SCALE || buckets[SCALE] != NULL)
			{
				// Only one scale transform is allowed per node.
				return false;
			}
			buckets[SCALE] = transform;
			bucketDepth = SCALE;
			break;

		case FCDTransform::ROTATION: {
			FCDTRotation* rotation = (FCDTRotation*) transform;
			AngAxis aa(ToPoint3(rotation->GetAxis()), FMath::DegToRad(rotation->GetAngle()));

			if (bucketDepth >= SCALE_AXIS_ROTATE_R)
			{
				// Only one rotation is allowed after the scale transform: the reverse scale-axis rotation.
				return false;
			}
			else if (bucketDepth == SCALE)
			{
				// Check for reverse of scale axis transformation
				Bucket reverseBucket = BUCKET_COUNT;
				if (buckets[SCALE_AXIS_ROTATE] != NULL) reverseBucket = SCALE_AXIS_ROTATE;
				else if (buckets[ROTATE_AXIS] != NULL) reverseBucket = ROTATE_AXIS;
				else if (buckets[ROTATE_X] != NULL) reverseBucket = ROTATE_X;
				else if (buckets[ROTATE_Y] != NULL) reverseBucket = ROTATE_Y;
				else if (buckets[ROTATE_Z] != NULL) reverseBucket = ROTATE_Z;

				if (reverseBucket < BUCKET_COUNT)
				{
					const FCDTRotation* reverseRotation = (const FCDTRotation*) buckets[reverseBucket];
					float reverseAngle = FMath::DegToRad(reverseRotation->GetAngle());
					if (IsEquivalent(reverseRotation->GetAxis(), aa.axis) && IsEquivalent(reverseAngle, -aa.angle))
					{
						// Bucket as the reverse scale axis rotation
						if (reverseBucket != SCALE_AXIS_ROTATE)
						{
							buckets[SCALE_AXIS_ROTATE] = buckets[reverseBucket];
							buckets[reverseBucket] = NULL;
						}
						buckets[SCALE_AXIS_ROTATE_R] = rotation;
						bucketDepth = SCALE_AXIS_ROTATE_R;
						break;
					}
				}
				
				// No rotation to reverse as the scale-axis rotation: force sampling
				return false;
			}
			else if (bucketDepth < ROTATE_X && aa.axis == Point3::XAxis)
			{
				buckets[ROTATE_X] = rotation;
				bucketDepth = ROTATE_X;
			}
			else if (bucketDepth < ROTATE_Y && aa.axis == Point3::YAxis)
			{
				buckets[ROTATE_Y] = rotation;
				bucketDepth = ROTATE_Y;
			}
			else if (bucketDepth < ROTATE_Z && aa.axis == Point3::ZAxis)
			{
				buckets[ROTATE_Z] = rotation;
				bucketDepth = ROTATE_Z;
			}
			else if (bucketDepth < ROTATE_Z) // Only use this if we have no rotations yet.
			{
				buckets[ROTATE_AXIS] = rotation;
				bucketDepth = ROTATE_AXIS;
			}
			else if (bucketDepth < SCALE_AXIS_ROTATE)
			{
				buckets[SCALE_AXIS_ROTATE] = rotation;
				bucketDepth = SCALE_AXIS_ROTATE;
			}
			else return false;

			break; } 

		case FCDTransform::MATRIX:
		case FCDTransform::LOOKAT:
		case FCDTransform::SKEW:
		default:
			// No place for these in the Max transform stack: force sampling
			return false;
		}
	}

	// Verify that if and only if there is a scale-axis rotation, there is its reverse
	if (buckets[SCALE_AXIS_ROTATE] != NULL)
	{
		if (buckets[SCALE_AXIS_ROTATE_R] == NULL) return false;
		const FCDTRotation* scaleAxisRotation = (const FCDTRotation*) buckets[SCALE_AXIS_ROTATE];
		const FCDTRotation* scaleAxisRotationR = (const FCDTRotation*) buckets[SCALE_AXIS_ROTATE_R];
		if (!IsEquivalent(scaleAxisRotation->GetAxis(), scaleAxisRotationR->GetAxis())) return false;
		if (!IsEquivalent(scaleAxisRotation->GetAngle(), -scaleAxisRotationR->GetAngle())) return false;
	}
	else if (buckets[SCALE_AXIS_ROTATE_R] != NULL) return false;

	return true;
}

void TransformImporter::ImportPivot(FCDSceneNode* colladaSceneNode)
{
	// Pivot transforms are not animated, so use the transform matrix directly
	Matrix3 pivotTransform = ToMatrix3(colladaSceneNode->ToMatrix());
	ImportPivot(iNode, pivotTransform);
}

void TransformImporter::ImportPivot(INode* node, Matrix3& tm)
{
	// Decompose this matrix.
	AffineParts ap;
	decomp_affine(tm, &ap);

	// Import each of the matrix components individually
	node->SetObjOffsetPos(ap.t);
	node->SetObjOffsetRot(ap.q);
	ScaleValue s(ap.k * ap.f, ap.u);
	node->SetObjOffsetScale(s);
}

void TransformImporter::Finalize()
{
	if (!isAnimated) return;
	if (!forceSampling)
	{
		// The buckets contain all the COLLADA transforms, in the correct order
		// So, create the controllers and fill them in with the keys
		INode* node = iNode;
		Control* baseController = node->GetTMController();
		
		if (buckets[SCALE] != NULL)
		{
			// If the scale axis rotation differs from its reverse, then well, the second animation is squashed.
			const FCDTRotation* scaleAxisRotation = (const FCDTRotation*) buckets[SCALE_AXIS_ROTATE];
			const FCDTScale* scale = (const FCDTScale*) buckets[SCALE];
			ANIM->ImportAnimatedScale(baseController, "Scale", scale, scaleAxisRotation);
		}
		if (buckets[ROTATE_AXIS] != NULL)
		{
			// Import the animated angle-axis rotation
			// Max uses Quaternions without tangents, internally, for this animation.
			// You may loose some information
			const FCDTRotation* axisRotation = (const FCDTRotation*) buckets[ROTATE_AXIS];
			ANIM->ImportAnimatedAxisRotation(baseController, "Rotation", axisRotation);
		}
		else if (buckets[ROTATE_X] != NULL || buckets[ROTATE_Y] != NULL || buckets[ROTATE_Z] != NULL)
		{
			// Import the animated Euler rotations
			const FCDTRotation* xRotation = (const FCDTRotation*) buckets[ROTATE_X];
			const FCDTRotation* yRotation = (const FCDTRotation*) buckets[ROTATE_Y];
			const FCDTRotation* zRotation = (const FCDTRotation*) buckets[ROTATE_Z];
			ANIM->ImportAnimatedEulerRotation(baseController, "Rotation", xRotation, yRotation, zRotation);
		}
		if (buckets[TRANSLATION] != NULL)
		{
			const FCDTTranslation* translation = (const FCDTTranslation*) buckets[TRANSLATION];
			ANIM->ImportAnimatedTranslation(baseController, translation);
		}
	}
	else
	{
		ANIM->ImportAnimatedSceneNode(iNode->GetTMController(), colladaSceneNode);
	}
}