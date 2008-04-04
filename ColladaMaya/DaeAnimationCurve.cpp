/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include <maya/MAnimControl.h>
#include "DaeAnimationCurve.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDAnimationChannel.h"
#include "FCDocument/FCDAnimationCurve.h"
#include "FCDocument/FCDAnimationKey.h"

// Create a curve from a Maya node, which should be an 'animCurveXX' type
FCDAnimationCurve* DaeAnimationCurve::CreateFromMayaNode(DaeDoc* doc, const MObject& curveObject, FCDAnimationChannel* colladaChannel)
{
	MStatus status;
	MFnAnimCurve animCurveFn(curveObject, &status);
	if (status != MStatus::kSuccess) return NULL;

	// Check for a supported curve type
	bool isLengthCurve = false;
	MFnAnimCurve::AnimCurveType curveType = animCurveFn.animCurveType();
	if (curveType == MFnAnimCurve::kAnimCurveTT || curveType == MFnAnimCurve::kAnimCurveUT || curveType == MFnAnimCurve::kAnimCurveUnknown)
	{
		// Unsupported curve types.
		return NULL;
	}
	isLengthCurve = curveType == MFnAnimCurve::kAnimCurveTL || curveType == MFnAnimCurve::kAnimCurveUL;

	// Create the output, single-dimensional curve
	FCDAnimationCurve* curve = colladaChannel->AddCurve();
	curve->SetPreInfinity(CAnimationHelper::ConvertInfinity(animCurveFn.preInfinityType()));
	curve->SetPostInfinity(CAnimationHelper::ConvertInfinity(animCurveFn.postInfinityType()));

	DaeAnimatedElement* info = NULL; // used for driven key only
	if (!animCurveFn.isTimeInput() && animCurveFn.isUnitlessInput())
	{
		int index = -1;

		// Figure out the real curve input
		MPlug connected;
		bool connection = CDagHelper::GetPlugConnectedTo(animCurveFn.object(), "input", connected);
		if (connection && connected.node() != MObject::kNullObj)
		{
			info = ANIM->FindAnimated(connected, index);
			if (info != NULL)
			{
				curve->SetDriver(info->animatedValue, index);
			}
			else
			{
				ANIM->AddUnsetDrivenCurve(curve, connected); // cache until we find the real curve
			}
		}
	}

	// Create the FCollada animation keys
	uint keyCount = animCurveFn.numKeys();
	for (uint k = 0; k < keyCount; ++k)
	{
		FCDAnimationKey* key = curve->AddKey(CAnimationHelper::ToInterpolation(animCurveFn.outTangentType(k)));

		// Gather the key time values
		if (animCurveFn.isTimeInput())
		{
			key->input = (float) animCurveFn.time(k).as(MTime::kSeconds);
			key->output = (float) animCurveFn.value(k);
		}
		else if (animCurveFn.isUnitlessInput())
		{
			// Driven-key animation curve!
			
			// Transform the input values into something known by COLLADA 
			float value = (float) animCurveFn.unitlessInput(k);
			if (info != NULL)
			{
				if (info->conversion != NULL)
				{
					MDGContext context;
					value = (*(info->conversion))(value);
				}
				if ((info->sampleType & kAngle) == kAngle) value = FMath::RadToDeg(value);
			}
			key->input = value;
			key->output = (float) animCurveFn.value(k);
		}

		if (key->interpolation == FUDaeInterpolation::BEZIER)
		{
			// Get tangent control points and fill in the animation data structure
			FCDAnimationKeyBezier* bkey = (FCDAnimationKeyBezier*) key;
			bool isWeightedCurve = animCurveFn.isWeighted();
			float prevTime = k > 0 ? (float) animCurveFn.time(k-1).as(MTime::kSeconds) : (float) (animCurveFn.time(0).as(MTime::kSeconds) - 1.0);
			float curTime = (float) animCurveFn.time(k).as(MTime::kSeconds);
			float nextTime = k < keyCount - 1 ? (float) animCurveFn.time(k+1).as(MTime::kSeconds) : (float) (animCurveFn.time(k).as(MTime::kSeconds) + 1.0);

			// In-tangent
			float slopeX, slopeY;
			animCurveFn.getTangent(k, slopeX, slopeY, k > 0);
			if (slopeX < 0.01f && slopeX > -0.01f) slopeX = FMath::Sign(slopeX) * 0.01f;
			if (!isWeightedCurve)
			{
				// Extend the slope to be one third of the time-line in the X-coordinate.
				slopeY *= (curTime - prevTime) / 3.0f / slopeX;
				slopeX = (curTime - prevTime) / 3.0f;
			}
			else
			{
				// Take out the strange 3.0 factor.
				slopeX /= 3.0f; slopeY /= 3.0f;
			}
			bkey->inTangent = FMVector2(bkey->input - slopeX, bkey->output - slopeY);

			// Out-tangent
			animCurveFn.getTangent(k, slopeX, slopeY, k >= keyCount - 1);
			if (slopeX < 0.01f && slopeX > -0.01f) slopeX = FMath::Sign(slopeX) * 0.01f;
			if (!isWeightedCurve)
			{
				// Extend the slope to be one third of the time-line in the X-coordinate.
				slopeY *= (nextTime - curTime) / 3.0f / slopeX;
				slopeX = (nextTime - curTime) / 3.0f;
			}
			else
			{
				// Take out the strange 3.0 factor.
				slopeX /= 3.0f; slopeY /= 3.0f;
			}
			bkey->outTangent = FMVector2(bkey->input + slopeX, bkey->output + slopeY);
		}
	}

	return curve;
}

MObject DaeAnimationCurve::CreateMayaNode(DaeDoc* doc, FCDAnimationCurve* curve, SampleType typeFlag)
{
	if (curve == NULL) return MObject::kNullObj;

	MFnAnimCurve animCurveFn;
	bool hasDriver = curve->HasDriver();

	// Forcefully create a new animation curve. This ensures we have the type we want!
	bool isAngular = (typeFlag & kAngle) == kAngle;
	bool isLength = (typeFlag & kLength) == kLength;
	MString curveType = "animCurve";
	curveType += (!hasDriver) ? "T" : "U";
	curveType += isAngular ? "A" : (isLength ? "L" : "U");
	MFnDependencyNode bypassFn;
	animCurveFn.setObject(bypassFn.create(curveType));

	// Set the pre-infinity and post-infinity type
	animCurveFn.setPreInfinityType(CAnimationHelper::ConvertInfinity(curve->GetPreInfinity()));
	animCurveFn.setPostInfinityType(CAnimationHelper::ConvertInfinity(curve->GetPostInfinity()));

    // Add the Maya animation keys
	uint keyCount = (uint) curve->GetKeyCount();
	if (!hasDriver)
	{
		for (uint j = 0; j < keyCount; j++)
		{
			FCDAnimationKey* key = curve->GetKey(j);
			MTime keyTime(key->input, MTime::kSeconds);
			animCurveFn.addKey(keyTime, key->output);
		}
	}
	else
	{
		for (uint j = 0; j < keyCount; j++)
		{
			// Need to convert the input: will be done when linking with the driver
			FCDAnimationKey* key = curve->GetKey(j);
			animCurveFn.addKey(key->input, key->output);
		}
	}

	// Read in the tangents and interpolation values
	MTime curTime = animCurveFn.time(0), prevTime, nextTime;
	float timeFactor = 3.0f * (float) MTime(1.0, MTime::kSeconds).as(MTime::uiUnit());
	float valueFactor = isLength ? 1.0f / doc->GetUIUnitFactor() : isAngular ? FMath::RadToDeg(1.0f) : 1.0f;
	for (uint k = 0; k < keyCount; k++)
	{
		FCDAnimationKeyBezier* bkey = (FCDAnimationKeyBezier*) curve->GetKey(k);

		// Compute time intervals, since the tangents are 
		// stored as increments.
		prevTime = curTime;
		curTime = animCurveFn.time(k);

		// Retrieve the tangent control points.
		MFnAnimCurve::TangentType tangentType = CAnimationHelper::ToTangentType((FUDaeInterpolation::Interpolation) bkey->interpolation);

		// Unlock the tangents/weights and set both in/out-tangents and their weights.
		animCurveFn.setTangentsLocked(k, false);
		float inTX = (bkey->input - bkey->inTangent.x) * timeFactor;
		float inTY = (bkey->output - bkey->inTangent.y) * 3.0f * valueFactor;
		float outTX = (bkey->outTangent.x - bkey->input) * timeFactor;
		float outTY = (bkey->outTangent.y - bkey->output) * 3.0f * valueFactor;
		animCurveFn.setTangent(k, inTX, inTY, true);
		animCurveFn.setInTangentType(k, tangentType);
        animCurveFn.setTangent(k, outTX, outTY, false);
		animCurveFn.setOutTangentType(k, tangentType);
	}

	return animCurveFn.object();
}

