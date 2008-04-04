/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_ANIMATION_HELPER___
#define __DAE_ANIMATION_HELPER___

#include <maya/MFnAnimCurve.h>
#include <maya/MTime.h>
#include <maya/MDGContext.h>
#include "DaeAnimationTypes.h"

#ifndef _FU_DAE_ENUM_H_
#include "FUtils/FUDaeEnum.h"
#endif // _FU_DAE_ENUM_H_

#undef GetCurrentTime

class CAnimCache;
class FCDAnimationCurve;
typedef fm::pvector<FCDAnimationCurve> FCDAnimationCurveList;

class CAnimationHelper
{
public:
	static FloatList samplingTimes;

public:
	// Returns whether the plug has any sort of animation, keyed or expression-wise.
	static CAnimationResult IsAnimated(CAnimCache* cache, const MObject& node, const MString& attribute);
	static CAnimationResult IsAnimated(CAnimCache* cache, const MPlug& plug);
	static MObject GetAnimatingNode(const MPlug& plug);

	// Sample animated data
	static bool IsPhysicsAnimation(const MObject& o);
	static void CheckForSampling(CAnimCache* cache, SampleType sampleType, const MPlug& plug);
	static bool SetSamplingFunction(const MFloatArray& function);
	static void GenerateSamplingFunction();
	static bool SampleAnimatedPlug(CAnimCache* cache, const MPlug& plug, FCDAnimationCurve* curve, FCDConversionFunctor* converter);
	static void SampleAnimatedTransform(CAnimCache* cache, const MPlug& plug, FCDAnimationCurveList& curves);

	// Since Maya 5.0 doesn't support MAnimControl::animation[Start/End]Time(), wrap it with the MEL command
	static MTime AnimationStartTime();
	static MTime AnimationEndTime();
	static void SetAnimationStartTime(float time);
	static void SetAnimationEndTime(float time);
	static void GetCurrentTime(MTime& time);
	static void SetCurrentTime(const MTime& time);

	// Handle animation targetting
	static MPlug GetTargetedPlug(MPlug parentPlug, int index);

	// Interpolation Type Handling
	static FUDaeInterpolation::Interpolation ToInterpolation(MFnAnimCurve::TangentType outType);
	static MFnAnimCurve::TangentType ToTangentType(FUDaeInterpolation::Interpolation type);

	// Pre/Post-Infinity Type Handling
	static FUDaeInfinity::Infinity ConvertInfinity(MFnAnimCurve::InfinityType type);
	static MFnAnimCurve::InfinityType ConvertInfinity(FUDaeInfinity::Infinity type);
};

#endif
