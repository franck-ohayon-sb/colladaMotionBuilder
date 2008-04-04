/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _ANIMATION_EXPORTER_H_
#define _ANIMATION_EXPORTER_H_

#ifndef _ENTITY_EXPORTER_H_
#include "EntityExporter.h"
#endif // _ENTITY_EXPORTER_H_
#ifndef _FCD_PARAMETER_ANIMATABLE_H_
#include "FCDocument/FCDParameterAnimatable.h"
#endif // _FCD_PARAMETER_ANIMATABLE_H_

class FCDAnimated;
class FCDAnimation;
class FCDAnimationCurve;
class FCDConversionFunctor;
class FCDEntity;

class AnimationExporter : public EntityExporter
{
private:
	struct FBSampledProperty
	{
		FBPropertyAnimatable* property;
		int index;
		FUTrackedPtr<FCDAnimation> animation;
		FUTrackedPtr<FCDAnimated> animated;
		int animatedIndex;
		bool isAngle;
	};

	struct FBSampledModel
	{
		FBModel* model;
		FUTrackedPtr<FCDAnimation> animation;
		FUTrackedPtr<FCDAnimated> animated;
	};

private:
	typedef fm::pvector<FBFCurve> FBCurveList;
	typedef fm::pvector<FBSampledModel> FBSampledModelList;
	typedef fm::pvector<FBSampledProperty> FBSampledPropertyList;
	FBSampledModelList modelsToSample;
	FBSampledPropertyList propertiesToSample;

public:
	AnimationExporter(ColladaExporter* base);
	~AnimationExporter();

	// Verify whether an actual animation exists on the given property.
	bool IsAnimated(FBProperty* property);

	// Export animatable properties.
	void ExportProperty(FBProperty* property, FCDEntity* colladaEntity, FCDAnimated* colladaAnimated, FCDConversionFunctor* functor = NULL);
	void ExportProperty(FBProperty* property, size_t index, FCDEntity* colladaEntity, FCDAnimated* colladaAnimated, int animatedIndex, FCDConversionFunctor* functor = NULL);

	// Export a single curve.
	void ExportCurve(FBFCurve* curve, FCDAnimationCurve* colladaCurve, FCDConversionFunctor* functor);

	// Export a sampled node animation.
	bool HasSampling() { return !modelsToSample.empty() || !propertiesToSample.empty(); }
	void DoSampling();
	void TerminateSampling();

	void AddModelToSample(FBModel* node, FCDEntity* colladaNode, FCDParameterAnimatableMatrix44& colladaTransform);
	void AddPropertyToSample(FBProperty* property, int index, FCDEntity* colladaEntity, FCDAnimated* colladaAnimated, int animatedIndex, bool isAngle);

private:
	// Helpers
	void GetPropertyCurves(FBProperty* property, FBCurveList& curveList, int maxCount);
	void TerminateSampling(FCDAnimated* animated);
	template <class Type, size_t TypeValueCount>
	void SampleProperty(AnimationExporter::FBSampledProperty* it, float sampleTime);
};

#ifdef WIN32
#pragma warning(disable:4127) // conditional expression is constant..
#endif

#define ANIMATABLE(property, colladaEntity, colladaParameter, arrayElement, functor, sampling) { \
	FCDAnimated* colladaAnimated = colladaParameter.GetAnimated(); \
	if (sampling || GetOptions()->ForceSampling()) { \
		ANIM->AddPropertyToSample(property, -1, colladaEntity, colladaAnimated, 0, false); } \
	else { ANIM->ExportProperty(property, colladaEntity, colladaAnimated, functor); } }

#define ANIMATABLE_INDEXED(property, propertyIndex, colladaEntity, colladaParameter, arrayElement, functor, sampling) { \
	FCDAnimated* colladaAnimated = colladaParameter.GetAnimated(); \
	if (sampling || GetOptions()->ForceSampling()) { \
		ANIM->AddPropertyToSample(property, propertyIndex, colladaEntity, colladaAnimated, 0, false); } \
	else { ANIM->ExportProperty(property, propertyIndex, colladaEntity, colladaAnimated, 0, functor); } }

#define ANIMATABLE_INDEXED_ANGLE(property, propertyIndex, colladaEntity, colladaParameter, arrayElement, functor, sampling) { \
	FCDAnimated* colladaAnimated = colladaParameter.GetAnimated(); \
	if (sampling || GetOptions()->ForceSampling()) { \
		ANIM->AddPropertyToSample(property, propertyIndex, colladaEntity, colladaAnimated, 3, true); } \
	else { ANIM->ExportProperty(property, propertyIndex, colladaEntity, colladaAnimated, 3, functor); } }

#define ANIMATABLE_CUSTOM(property, propertyIndex, colladaEntity, colladaAnimatedCustom, functor) { \
	if (GetOptions()->ForceSampling()) { \
		ANIM->AddPropertyToSample(property, propertyIndex, colladaEntity, colladaAnimatedCustom, 0, false); } \
	else { ANIM->ExportProperty(property, propertyIndex, colladaEntity, colladaAnimatedCustom, 0, functor); } }

#endif // _ANIMATION_EXPORTER_H_
