/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_ANIMATION_LIBRARY_INCLUDED__
#define __DAE_ANIMATION_LIBRARY_INCLUDED__

#ifndef _DAE_ANIMATION_TYPES_H_
#include "DaeAnimationTypes.h"
#endif // _DAE_ANIMATION_TYPES_H_
#ifndef __DAE_BASE_LIBRARY_INCLUDED__
#include "DaeBaseLibrary.h"
#endif // __DAE_BASE_LIBRARY_INCLUDED__
#ifndef _FCD_PARAMETER_ANIMATABLE_H_
#include "FCDocument/FCDParameterAnimatable.h"
#endif // _FCD_PARAMETER_ANIMATABLE_H_

class DaeDoc;
class FCDAnimated;
class DaeAnimation;
class DaeAnimationCurve;
class FCDSceneNode;
class FCDAnimationChannel;
class FCDAnimationCurve;
struct DaeUnsetDriven;
typedef fm::pvector<FCDAnimationCurve> FCDAnimationCurveList;
typedef fm::pvector<DaeAnimatedElement> DaeAnimatedElementList;
typedef fm::pvector<FCDAnimated> FCDAnimatedList;
typedef fm::pvector<DaeUnsetDriven> FCDAnimationCurvePlugList;

struct DaeUnsetDriven
{
	FCDAnimationCurve* curve;
	MPlug plug;
};

struct DaeAnimationDriven
{
	MObject curve;
	FCDAnimated* driver;
	int32 driverIndex;
};
typedef fm::vector<DaeAnimationDriven> DaeAnimationDrivenList;

class DaeAnimationLibrary : public DaeBaseLibrary
{
public:
	DaeAnimationLibrary(DaeDoc* doc, bool isImport);
	~DaeAnimationLibrary();

	// Export a scene element animation
	bool ExportAnimation(DaeAnimatedElement* animation);
	void CreateAnimationCurve(const MPlug& plug, SampleType sampleType, FCDConversionFunctor* conversion, FCDAnimationCurveList& curves);
	void AddUnsetDrivenCurve(FCDAnimationCurve* curve, const MPlug& plug);

	// Import a scene element animation
	void ImportAnimation(DaeAnimatedElement* animation);
	bool ImportAnimatedSceneNode(const MObject& transformNode, FCDSceneNode* sceneNode);
	void ConnectDrivers();
	void PostSampling();

	// Animated plug support
	void AddPlugAnimation(MObject node, const MString& attr, FCDParameterAnimatable& animatedValue, uint sampleType, FCDConversionFunctor* conversion = NULL);
	void AddPlugAnimation(MObject node, const MString& attr, FCDParameterAnimatable& animatedValue, SampleType sampleType, FCDConversionFunctor* conversion = NULL);
	void AddPlugAnimation(MObject node, const MString& attr, FCDAnimated* animatedValue, uint sampleType, FCDConversionFunctor* conversion = NULL);
	void AddPlugAnimation(MObject node, const MString& attr, FCDAnimated* animatedValue, SampleType sampleType, FCDConversionFunctor* conversion = NULL);
	void AddPlugAnimation(const MPlug& plug, FCDParameterAnimatable& animatedValue, uint sampleType, FCDConversionFunctor* conversion = NULL);
	void AddPlugAnimation(const MPlug& plug, FCDParameterAnimatable& animatedValue, SampleType sampleType, FCDConversionFunctor* conversion = NULL);
	void AddPlugAnimation(const MPlug& plug, FCDAnimated* animatedValue, uint sampleType, FCDConversionFunctor* conversion = NULL);
	void AddPlugAnimation(const MPlug& plug, FCDAnimated* animatedValue, SampleType sampleType, FCDConversionFunctor* conversion = NULL);

	void OffsetAnimationWithAnimation(FCDParameterAnimatableFloat& oldValue, FCDParameterAnimatableFloat& offsetValue);

	DaeAnimatedElement* FindAnimated(const MPlug& plug, int& index);
	bool FindAnimated(const MPlug& plug, int& index, DaeAnimatedElement* animation);
	DaeAnimatedElement* FindAnimated(const FCDAnimated* value);

private:
	bool isImport;
	DaeAnimationDrivenList importedDrivenCurves;
	FCDAnimationChannel* colladaChannel;
	DaeAnimatedElementList animatedElements;
	FCDAnimationCurvePlugList unsetDrivenCurves;
};

#endif 
