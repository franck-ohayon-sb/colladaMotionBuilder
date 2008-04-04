/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"

#include <maya/MAnimUtil.h>
#include <maya/MAnimControl.h>
#include <maya/MFnMatrixData.h>

#include "../CExportOptions.h"
#include "CAnimationHelper.h"
#include "CDagHelper.h"
#include "HIAnimCache.h"
#include "FCDocument/FCDAnimationCurve.h"
#include "FCDocument/FCDAnimationKey.h"
#include "FUtils/FUDaeEnum.h"
//#include "FUtils/FUDaeParser.h"
//using namespace FUDaeParser;

FloatList CAnimationHelper::samplingTimes;

MObject CAnimationHelper::GetAnimatingNode(const MPlug& plug)
{
	// Early withdrawal: check for no direct connections on plug
	MObject animating = CDagHelper::GetSourceNodeConnectedTo(plug);
	if (animating.isNull()) return animating;

	// By-pass any unit conversion nodes
	while (animating.hasFn(MFn::kUnitConversion))
	{
		animating = CDagHelper::GetSourceNodeConnectedTo(animating, "input");
	}
	return animating;
}

// Figure out whether a given plug is animated
CAnimationResult CAnimationHelper::IsAnimated(CAnimCache *acache, const MObject& node, const MString& attribute)
{
	MStatus status;
	MPlug p = MFnDependencyNode(node).findPlug(attribute, &status);
	return (status != MStatus::kSuccess) ? kISANIM_None : IsAnimated(acache, p);
}

CAnimationResult CAnimationHelper::IsAnimated(CAnimCache* cache, const MPlug& plug)
{
	// First check for sampling in our cache -- if this exists, it overrides
	// all other considerations.  We could have sampling on a node without any
	// connections (for example, IK driven nodes).
	bool animated;
	if (cache->FindCachePlug(plug, animated))
	{
		return (!animated) ? kISANIM_None : kISANIM_Sample;
	}
	else
	{
		// Get the connected animating object
		MObject connectedNode = GetAnimatingNode(plug);
		if (connectedNode == MObject::kNullObj) return kISANIM_None;
		else if (connectedNode.hasFn(MFn::kAnimCurve))
		{
			MFnAnimCurve curveFn(connectedNode);
			CAnimationResult result = curveFn.numKeys() >= 1 ? kISANIM_Curve : kISANIM_None;
			if (CExportOptions::RemoveStaticCurves())
			{
				if (result == kISANIM_Curve && curveFn.isStatic()) result = kISANIM_None;
			}
			return result;
		}
		else if (connectedNode.hasFn(MFn::kCharacter))
		{
			return kISANIM_Character;
		}
	}
	
	return kISANIM_None;
}

class CSamplingInterval
{
public:
	float start;
	float end;
	float period;
};

// set the sampling function from the UI/command line.
// Returns validity of the function
bool CAnimationHelper::SetSamplingFunction(const MFloatArray& function)
{
	fm::vector<CSamplingInterval> parsedFunction;

	// Order and parse the given floats as a function
	uint elementCount = function.length();
	if (elementCount > 1 && elementCount % 3 != 0) return false;
	if (elementCount == 0)
	{
		GenerateSamplingFunction();
		return true;
	}
	else if (elementCount == 1)
	{
		CSamplingInterval interval;
		interval.start = (float) AnimationStartTime().as(MTime::kSeconds);
		interval.end = (float) AnimationEndTime().as(MTime::kSeconds);
		interval.period = function[0];
		parsedFunction.push_back(interval);
	}
	else
	{
        uint intervalCount = elementCount / 3;
		parsedFunction.resize(intervalCount);
		for (uint i = 0; i < intervalCount; ++i)
		{
			CSamplingInterval& interval = parsedFunction[i];
			interval.start = function[i * 3];
			interval.end = function[i * 3 + 1];
			interval.period = function[i * 3 + 2];
		}
	}

	// Check for a valid sampling function
	uint parsedFunctionSize = (uint) parsedFunction.size();
	for (uint i = 0; i < parsedFunctionSize; ++i)
	{
		CSamplingInterval& interval = parsedFunction[i];
		if (interval.end <= interval.start) return false;
		if (interval.period > interval.end - interval.start) return false;
		if (i > 0 && parsedFunction[i - 1].end > interval.start) return false;
		if (interval.period <= 0.0f) return false;
	}

	// Gather the sampling times
	samplingTimes.clear();
	float previousTime = (float) AnimationStartTime().as(MTime::kSeconds);
	float previousPeriodTakenRatio = 1.0f;
	for (fm::vector<CSamplingInterval>::iterator it = parsedFunction.begin(); it != parsedFunction.end(); ++it)
	{
		CSamplingInterval& interval = (*it);
		float time = interval.start;
		if (IsEquivalent(time, previousTime))
		{
			// Continuity in the sampling, calculate overlap start time
			time = time + (1.0f - previousPeriodTakenRatio) * interval.period;
		}

		for (; time <= interval.end - interval.period + FLT_TOLERANCE; time += interval.period)
		{
			samplingTimes.push_back(time);
		}
		samplingTimes.push_back(time);

		previousTime = interval.end;
		previousPeriodTakenRatio = (interval.end - time) / interval.period;
	}

	return true;
}

void CAnimationHelper::GenerateSamplingFunction()
{
	samplingTimes.clear();

	// [NMartz] Avoid any potential precision accumulation problems by using the MTime class as an iterator
	MTime startT = AnimationStartTime();
	MTime endT = AnimationEndTime(); 
	for (MTime currentT = startT; currentT <= endT; ++currentT)
	{
		samplingTimes.push_back((float) currentT.as(MTime::kSeconds));        
	}
}

// Sample animated values for a given plug
bool CAnimationHelper::SampleAnimatedPlug(CAnimCache* cache, const MPlug& plug, FCDAnimationCurve* curve, FCDConversionFunctor* converter)
{
	MStatus status;
	if (cache == NULL) return false;

	FloatList* inputs = NULL;
	FloatList* outputs = NULL;

	// Buffer temporarly the inputs and outputs, so they can be sorted
	if (!cache->FindCachePlug(plug, inputs, outputs) || inputs == NULL || outputs == NULL) return false;

	uint inputCount = (uint) inputs->size();

	// Look for a matching plug in the animation cache...
	if (CExportOptions::CurveConstrainSampling())
	{
		// Drop the keys and their outputs that don't belong to the attached animation curve
		MFnAnimCurve curveFn(plug, &status);
		if (status == MStatus::kSuccess && curveFn.numKeys() > 0)
		{
			float startTime = (float) curveFn.time(0).as(MTime::kSeconds);
			float endTime = (float) curveFn.time(curveFn.numKeys() - 1).as(MTime::kSeconds);
			uint count = 0;

			// To avoid memory re-allocations, start by counting the number of keys that are within the curve
			for (uint i = 0; i < inputCount; ++i)
			{
				if (inputs->at(i) + FLT_TOLERANCE >= startTime && inputs->at(i) - FLT_TOLERANCE <= endTime) ++count;
			}

			curve->SetKeyCount(count, FUDaeInterpolation::LINEAR);

			// Copy over the keys belonging to the curve's timeframe
			for (uint i = 0; i < inputCount; ++i)
			{
				if (inputs->at(i) + FLT_TOLERANCE >= startTime && inputs->at(i) - FLT_TOLERANCE <= endTime)
				{
					curve->GetKey(i)->input = inputs->at(i);
					curve->GetKey(i)->output = outputs->at(i);
				}
			}
		}
		else if (status != MStatus::kSuccess)
		{
			// No curve found, so use the sampled inputs/outputs directly
			curve->SetKeyCount(inputs->size(), FUDaeInterpolation::LINEAR);
			for (uint i = 0; i < inputCount; ++i)
			{
				curve->GetKey(i)->input = inputs->at(i);
				curve->GetKey(i)->output = outputs->at(i);
			}
		}
	}
	else 
	{
		curve->SetKeyCount(inputs->size(), FUDaeInterpolation::LINEAR);
		for (uint i = 0; i < inputCount; ++i)
		{
			curve->GetKey(i)->input = inputs->at(i);
			curve->GetKey(i)->output = outputs->at(i);
		}
	}

	// Convert the samples
	if (converter != NULL)
	{
		curve->ConvertValues(converter, converter);
	}
	return true;
}

void CAnimationHelper::SampleAnimatedTransform(CAnimCache* cache, const MPlug& plug, FCDAnimationCurveList& curves)
{
	if (curves.size() != 16) return;

	FloatList* inputs = NULL,* outputs = NULL;
	if (!cache->FindCachePlug(plug, inputs, outputs) || inputs == NULL || outputs == NULL) return;

	size_t keyCount = inputs->size();
	for (size_t i = 0; i < 16; ++i)
	{
		curves[i]->SetKeyCount(keyCount, FUDaeInterpolation::LINEAR);
		for (size_t j = 0; j < keyCount; ++j)
		{
			curves[i]->GetKey(j)->input = inputs->at(j);
			curves[i]->GetKey(j)->output = outputs->at(j*16 + i);
		}
	}
}

// Since Maya 5.0 doesn't support MAnimControl::animation[Start/End]Time(), wrap it with the MEL command
//
// [JHoerner]: use minTime/maxTime rather than animationStartTime/animationEndTime.  Usually our artists
// use the former (narrower) range of the time slider and put "junk" beyond the edges.
#define TSTART "animationStartTime"
#define TEND   "animationEndTime"
//#define TSTART "minTime"
//#define TEND   "maxTime"

MTime CAnimationHelper::AnimationStartTime()
{
	MTime time(MAnimControl::currentTime());
	double result;
	MGlobal::executeCommand("playbackOptions -q -" TSTART, result);
	time.setValue(result);
	return time;
}

MTime CAnimationHelper::AnimationEndTime()
{
	MTime time(MAnimControl::currentTime());
	double result;
	MGlobal::executeCommand("playbackOptions -q -" TEND, result);
	time.setValue(result);
	return time;
}

void CAnimationHelper::SetAnimationStartTime(float _time)
{
	MTime time(_time, MTime::kSeconds);
	double t = time.as(MTime::uiUnit());
	MGlobal::executeCommand(MString("playbackOptions -" TSTART " ") + t);
}

void CAnimationHelper::SetAnimationEndTime(float _time)
{
	MTime time(_time, MTime::kSeconds);
	double t = time.as(MTime::uiUnit());
	MGlobal::executeCommand(MString("playbackOptions -" TEND " ") + t);
}

void CAnimationHelper::GetCurrentTime(MTime& time)
{
	time = MAnimControl::currentTime();
}

void CAnimationHelper::SetCurrentTime(const MTime& time)
{
	MAnimControl::setCurrentTime(time);
}

MPlug CAnimationHelper::GetTargetedPlug(MPlug parentPlug, int index)
{
	if (index >= 0 && parentPlug.isCompound())
	{
		return parentPlug.child(index);
	}
	else if (index >= 0 && parentPlug.isArray())
	{
		return parentPlug.elementByLogicalIndex(index);
	}
	else return parentPlug;
}

// Interpolation Type Handling
//
FUDaeInterpolation::Interpolation CAnimationHelper::ToInterpolation(MFnAnimCurve::TangentType outType)
{
	switch (outType)
	{
	case MFnAnimCurve::kTangentGlobal: return FUDaeInterpolation::BEZIER;
	case MFnAnimCurve::kTangentFixed: return FUDaeInterpolation::BEZIER;
	case MFnAnimCurve::kTangentLinear: return FUDaeInterpolation::LINEAR;
	case MFnAnimCurve::kTangentFlat: return FUDaeInterpolation::BEZIER;
	case MFnAnimCurve::kTangentSmooth: return FUDaeInterpolation::BEZIER;
	case MFnAnimCurve::kTangentStep: return FUDaeInterpolation::STEP;
	case MFnAnimCurve::kTangentClamped: return FUDaeInterpolation::BEZIER;
	default: return FUDaeInterpolation::BEZIER;
	}
}

MFnAnimCurve::TangentType CAnimationHelper::ToTangentType(FUDaeInterpolation::Interpolation type)
{
	switch (type)
	{
	case FUDaeInterpolation::STEP: return MFnAnimCurve::kTangentStep;
	case FUDaeInterpolation::LINEAR: return MFnAnimCurve::kTangentLinear;
	case FUDaeInterpolation::BEZIER: return MFnAnimCurve::kTangentFixed;
	default: return MFnAnimCurve::kTangentClamped;
	}
}

// Pre/Post-Infinity Type Handling
FUDaeInfinity::Infinity CAnimationHelper::ConvertInfinity(MFnAnimCurve::InfinityType type)
{
	switch (type)
	{
	case MFnAnimCurve::kConstant: return FUDaeInfinity::CONSTANT;
	case MFnAnimCurve::kLinear: return FUDaeInfinity::LINEAR;
	case MFnAnimCurve::kCycle: return FUDaeInfinity::CYCLE;
	case MFnAnimCurve::kCycleRelative: return FUDaeInfinity::CYCLE_RELATIVE;
	case MFnAnimCurve::kOscillate: return FUDaeInfinity::OSCILLATE;
	default: return FUDaeInfinity::UNKNOWN;
	}
}

MFnAnimCurve::InfinityType CAnimationHelper::ConvertInfinity(FUDaeInfinity::Infinity type)
{
	switch (type)
	{
	case FUDaeInfinity::CONSTANT: return MFnAnimCurve::kConstant;
	case FUDaeInfinity::LINEAR: return MFnAnimCurve::kLinear;
	case FUDaeInfinity::CYCLE: return MFnAnimCurve::kCycle;
	case FUDaeInfinity::CYCLE_RELATIVE: return MFnAnimCurve::kCycleRelative;
	case FUDaeInfinity::OSCILLATE: return MFnAnimCurve::kOscillate;
	default: return MFnAnimCurve::kConstant;
	}
}

bool CAnimationHelper::IsPhysicsAnimation(const MObject& o)
{
	if (o.hasFn(MFn::kChoice))
	{
		MFnDependencyNode n(o);
		MPlug p = n.findPlug("input");
		uint choiceCount = p.numElements();
		for (uint i = 0; i < choiceCount; ++i)
		{
			MPlug child = p.elementByPhysicalIndex(i);
			MObject connection = CDagHelper::GetSourceNodeConnectedTo(child);
			if (!connection.isNull() && connection != o)
			if (IsPhysicsAnimation(connection)) return true;
		}
	}
	else if (o.hasFn(MFn::kRigidSolver) || o.hasFn(MFn::kRigid)) return true;
	return false;
}

void CAnimationHelper::CheckForSampling(CAnimCache* cache, SampleType sampleType, const MPlug& plug)
{
	switch (sampleType & kValueMask)
	{
	case kBoolean: 
	case kSingle: {
		bool forceSampling = CExportOptions::IsSampling();
		if (!forceSampling)
		{
			MObject connection = CAnimationHelper::GetAnimatingNode(plug);
			forceSampling |= !connection.isNull() && !connection.hasFn(MFn::kCharacter) && !connection.hasFn(MFn::kAnimCurve);
			forceSampling &= !IsPhysicsAnimation(connection);
		}
		if (forceSampling) cache->CachePlug(plug, false);
		break; }

	case kMatrix: {
		bool forceSampling = cache->FindCacheNode(plug.node());
		if (forceSampling) cache->CachePlug(plug, false);
		break; }

	case kVector:
	case kColour: {
		// Check for one node affecting the whole value.
		bool forceSampling = CExportOptions::IsSampling();
		if (!forceSampling)
		{
			MObject connection = CAnimationHelper::GetAnimatingNode(plug);
			forceSampling |= !connection.isNull() && !connection.hasFn(MFn::kCharacter) && !connection.hasFn(MFn::kAnimCurve);
			forceSampling &= !IsPhysicsAnimation(connection);
		}
		if (forceSampling) cache->CachePlug(plug, false);
		else
		{
			// Check for nodes affecting the children.
			uint childCount = plug.numChildren();
			for (uint i = 0; i < childCount; ++i)
			{
				MObject connection = CAnimationHelper::GetAnimatingNode(plug.child(i));
				bool sampleChild = !connection.isNull() && !connection.hasFn(MFn::kCharacter) && !connection.hasFn(MFn::kAnimCurve);
				sampleChild &= !IsPhysicsAnimation(connection);
				if (sampleChild) cache->CachePlug(plug.child(i), false);
			}
		}
		break; }
	}
}
