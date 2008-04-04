/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include <maya/MFnMatrixData.h>
#include <maya/MAnimUtil.h>
#include <maya/MAnimControl.h>
#include <maya/MFnAnimCurve.h>
#include "TranslatorHelpers/CDagHelper.h"
#include "TranslatorHelpers/CAnimationHelper.h"
#include "CExportOptions.h"
#include "DaeAnimationCurve.h"
#include "DaeAnimClipLibrary.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDAnimation.h"
#include "FCDocument/FCDAnimationChannel.h"
#include "FCDocument/FCDAnimationCurve.h"
#include "FCDocument/FCDAnimationMultiCurve.h"
#include "FCDocument/FCDAnimationKey.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDSceneNodeTools.h"
#include "FCDocument/FCDTransform.h"
//#include "FUtils/FUDaeParser.h"
//using namespace FUDaeParser;

//
// DaeAnimationLibrary
//

DaeAnimationLibrary::DaeAnimationLibrary(DaeDoc* _doc, bool _isImport)
:	DaeBaseLibrary(_doc), isImport(_isImport)
{
}

DaeAnimationLibrary::~DaeAnimationLibrary()
{
	CLEAR_POINTER_VECTOR(animatedElements);
	importedDrivenCurves.clear();
}

bool DaeAnimationLibrary::ExportAnimation(DaeAnimatedElement* animation)
{
	if (!CExportOptions::ExportAnimations()) return false;

	const MPlug& plug = animation->plug;
	FCDAnimated* animated = animation->animatedValue;

	// Create a channel for these curves.
	FCDAnimation* animationNode = animation->animatedValue->GetDocument()->GetAnimationLibrary()->AddEntity();
	animationNode->SetDaeId(doc->MayaNameToColladaName(plug.info()).asChar());
	colladaChannel = animationNode->AddChannel();

	SampleType typeValue = (SampleType) ((uint) animation->sampleType & kValueMask);
	bool isQualifiedAngle = (animation->sampleType & kQualifiedAngle) == kQualifiedAngle;

	size_t elementCount = animated->GetValueCount();
	if (!plug.isCompound() || elementCount == 1)
	{
		// Export the single curve without fuss
		FCDAnimationCurveList curves;
		CreateAnimationCurve(plug, typeValue, animation->conversion, curves);
		if (!curves.empty())
		{
			if (typeValue == kMatrix)
			{
				for (size_t i = 0; i < curves.size(); ++i)
				{
					animated->AddCurve(i, curves[i]);
				}
			}
			else if (isQualifiedAngle)
			{
				// Special type for skew/angleAxis rotations.
				size_t animatedIndex = animated->FindQualifier(".ANGLE");
				animated->AddCurve(animatedIndex, curves);
			}
			else
			{
				animated->AddCurve(0, curves);
			}
		}
	}
	else
	{
		// Get the animation data for each child plug
		elementCount = min((size_t) plug.numChildren(), elementCount);
		for (uint i = 0; i < elementCount; ++i)
		{
			FCDAnimationCurveList curves;
			CreateAnimationCurve(plug.child(i), typeValue, animation->conversion, curves);
			if (!curves.empty()) animated->AddCurve(i, curves);
		}
	}

	if (!animated->HasCurve())
	{
		delete animationNode;
		animationNode = NULL;
	}

	return animationNode != NULL;
}

// Export any animation associated with the specified plug
void DaeAnimationLibrary::CreateAnimationCurve(const MPlug& plug, SampleType sampleType, FCDConversionFunctor* conversion, FCDAnimationCurveList& curves)
{
	MStatus status;
	SampleType typeValue = (SampleType) ((uint) sampleType & kValueMask);

	// Check if this plug is animated
	// Differentiate between expression, IK, and animCurve-based curves here.
	bool animatedMatrix = typeValue == kMatrix;
	MPlugArray connections, sourcePlugs;
	CAnimationResult aresult = CAnimationHelper::IsAnimated(doc->GetAnimationCache(), plug);
	if (aresult == kISANIM_None && !animatedMatrix) return;
	if (!CExportOptions::IsSampling() && (aresult == kISANIM_Curve || aresult == kISANIM_Character))
	{
		// Create a DaeAnimationCurve for the corresponding Maya animation curve node
		if (aresult == kISANIM_Curve)
		{
			MObject curveObject = CDagHelper::GetSourceNodeConnectedTo(plug);
			FCDAnimationCurve* curve = DaeAnimationCurve::CreateFromMayaNode(doc, curveObject, colladaChannel);
			if (curve != NULL) curves.push_back(curve);
		}
		else if (aresult == kISANIM_Character)
		{
			doc->GetAnimClipLibrary()->CreateCurve(plug, conversion, colladaChannel->GetParent(), curves);

			// it is possible that the character has some curves as well as clips
			MPlug plugIntermediate;
			CDagHelper::GetPlugConnectedTo(plug, plugIntermediate);

			// clips will not have curves directly attached like this
			CreateAnimationCurve(plugIntermediate, sampleType, NULL, curves); // don't convert since we'll do it later
		}

		// For a boolean-typed curve, back out early. The curve is valid, but doesn't 
		// include interpolations or tangents
		if (!curves.empty())
		{
			for (FCDAnimationCurveList::iterator itC = curves.begin(); itC != curves.end(); ++itC)
			{
				FCDAnimationCurve* curve = (*itC);
				curve->ConvertValues(conversion, conversion);
			}
		}
	}
	else
	{
		// Sample the animation.
		CAnimCache* cache = doc->GetAnimationCache();
		if (animatedMatrix)
		{
			curves.reserve(16);
			for (size_t i = 0; i < 16; ++i) curves.push_back(colladaChannel->AddCurve());
			CAnimationHelper::SampleAnimatedTransform(cache, plug, curves);
		}
		else
		{
			FCDAnimationCurve* curve = colladaChannel->AddCurve();
			CAnimationHelper::SampleAnimatedPlug(cache, plug, curve, conversion);
			curves.push_back(curve);
		}

		// Verify that there is, in fact, an animation in this curve.
		bool equals = true;
		for (size_t i = 0; i < curves.size() && equals; ++i)
		{
			FCDAnimationCurve* curve = curves[i];
			FCDAnimationKey** values = curve->GetKeys();
			size_t valueCount = curve->GetKeyCount();
			if (valueCount > 1)
			{
				for (size_t j = 0; j < valueCount - 1 && equals; ++j)
				{
					equals = curve->GetKey(j)->output == curve->GetKey(j+1)->output;
				}
			}
		}

		if (equals) { CLEAR_POINTER_VECTOR(curves); }
	}
}

void DaeAnimationLibrary::ConnectDrivers()
{
	for (DaeAnimationDrivenList::iterator it = importedDrivenCurves.begin(); it != importedDrivenCurves.end(); ++it)
	{
		MFnAnimCurve mayaCurve((*it).curve);
		const FCDAnimated* animatedInput = (*it).driver;
		if (animatedInput == NULL) continue;

		// Find the driver's input information
		DaeAnimatedElement* inputUrlInfo = FindAnimated(animatedInput);
		int32 index = (*it).driverIndex;
		if (inputUrlInfo == NULL || index >= (int32) animatedInput->GetValueCount()) continue;

		// Connect the driver input
		MPlug inputPlug = CAnimationHelper::GetTargetedPlug(inputUrlInfo->plug, (index >= 0) ? (uint) index : 0u);
		CDagHelper::Connect(inputPlug, mayaCurve.object(), "input");

		// Transform the driven curve's input keys
		uint keyCount = mayaCurve.numKeys();
		for (uint j = 0; j < keyCount; j++)
		{
			double inputValue = mayaCurve.unitlessInput(j);
			if (inputUrlInfo->conversion != NULL)
			{
				inputValue = (*(inputUrlInfo->conversion))((float)inputValue);
			}
			mayaCurve.setUnitlessInput(j, inputValue);
		}
	}
	importedDrivenCurves.clear();
}

// Import an animation associated with a plug
//
void DaeAnimationLibrary::ImportAnimation(DaeAnimatedElement* animation)
{
	MStatus rc;
	const MPlug& plug = animation->plug;
	FCDAnimated* animated = const_cast<FCDAnimated*>(animation->animatedValue);

	// Check the sample type information for possible animation targets
	SampleType typeValue = (SampleType) ((uint) animation->sampleType & kValueMask);
	SampleType typeFlag = (SampleType) ((uint) animation->sampleType & kFlagMask);

	size_t stride;
	switch (typeValue)
	{
	case kSingle: stride = 1; break;
	case kVector: stride = 3; break;
	case kColour: stride = 4; break;
	case kBoolean: stride = 1; break;
	case kMatrix: return; // Unsupported on import. Use ImportAnimatedSceneNode instead, after transform bucketing
	default: stride = 1; break;
	}

	// Apply the conversion onto the curve.
	// NOTE: each curve should only be converted once, but there is no need YET for a flag
	FCDAnimationCurveListList& curves = animated->GetCurves();
	bool isQualifiedAngle = (typeFlag & kQualifiedAngle) == kQualifiedAngle;
	stride = min(curves.size(), stride);
	if (plug.isCompound()) stride = stride = min((size_t) plug.numChildren(), stride);
	if (isQualifiedAngle) stride = 1;
	for (size_t child = 0; child < stride; ++child)
	{
		if (isQualifiedAngle)
		{
			// Qualified angles are a little trickier: you have to look for the correct curve index.
			child = animated->FindQualifier(".ANGLE");
			if (child == ((size_t)-1)) break;
		}

		for (FCDAnimationCurveTrackList::iterator itC = curves[child].begin(); itC != curves[child].end(); ++itC)
		{
			FCDAnimationCurve* curve = (*itC);

			// Retrieve and verify the plug to which we want to attach this curve.
			MPlug childPlug;
			if (!isQualifiedAngle)
			{
				childPlug = CAnimationHelper::GetTargetedPlug(plug, (int) child);
				if (childPlug.node().isNull()) continue;
			}
			else childPlug = plug;

			// Apply the conversion functions
			curve->ConvertValues(animation->conversion, animation->conversion);

			MObject animCurveNode = DaeAnimationCurve::CreateMayaNode(doc, curve, typeFlag);
			if (animCurveNode.isNull()) continue;
			if (curve->GetClipCount() == 0)
			{
				// This curve belongs on the global timeline
				CDagHelper::Connect(animCurveNode, "output", childPlug);
			}
			else
			{
				// The animation clip library will attach this curve through the character/clip nodes.
				doc->GetAnimClipLibrary()->ImportClipCurve(curve, childPlug, animCurveNode);
			}

			if (curve->HasDriver())
			{
				// Buffer this curve to be linked with its driver, later on
				DaeAnimationDriven driven;
				driven.curve = animCurveNode;
				curve->GetDriver(driven.driver, driven.driverIndex);
				importedDrivenCurves.push_back(driven);
			}
		}
	}
}

// From ColladaMax:

void AngleApproach(double pval, double& val)
{
	while (val - pval > FMath::Pi) val -= FMath::Pi * 2.0f;
	while (val - pval < -FMath::Pi) val += FMath::Pi * 2.0f;
}

void PatchEuler(double* pval, double* val)
{
	// Approach these Eulers to the previous value.
	for (int i = 0; i < 3; ++i) AngleApproach(pval[i], val[i]);
	double distanceSq = 0.0f; for (int i = 0; i < 3; ++i) distanceSq += (val[i] - pval[i]) * (val[i] - pval[i]);

	// All quaternions can be expressed two ways. Check if the second way is better.
	double alternative[3] = { val[0] + FMath::Pi, FMath::Pi - val[1], val[2] + FMath::Pi };
	for (int i = 0; i < 3; ++i) AngleApproach(pval[i], alternative[i]);
	double alternateDistanceSq = 0.0f; for (int i = 0; i < 3; ++i) alternateDistanceSq += (alternative[i] - pval[i]) * (alternative[i] - pval[i]);

	if (alternateDistanceSq < distanceSq)
	{
		// Pick the alternative
		for (int i = 0; i < 3; ++i) val[i] = alternative[i];
	}
}

bool DaeAnimationLibrary::ImportAnimatedSceneNode(const MObject& transformNode, FCDSceneNode* colladaSceneNode)
{
	if (!transformNode.hasFn(MFn::kTransform)) return false;

	MFnTransform transformFn(transformNode);
	MTransformationMatrix::RotationOrder rotationOrder = transformFn.rotationOrder();

	// Ask FCollada to generate the sampled transform animation
	FCDSceneNodeTools::GenerateSampledAnimation(colladaSceneNode);
	const FloatList& keys = FCDSceneNodeTools::GetSampledAnimationKeys();
	const FMMatrix44List& values = FCDSceneNodeTools::GetSampledAnimationMatrices();
	if (keys.empty() || values.empty()) return false;

	size_t keyCount = keys.size();

	MTransformationMatrix* tm = new MTransformationMatrix[keyCount];
	for (size_t i = 0; i < keyCount; ++i)
	{
		tm[i] = MConvert::ToMMatrix(values[i]);
	}

	bool haveTransX = false, haveTransY = false, haveTransZ = false;
	bool haveRotateX = false, haveRotateY = false, haveRotateZ = false;
	bool haveScaleX = false, haveScaleY = false, haveScaleZ = false;
	bool haveShearXY = false, haveShearXZ = false, haveShearYZ = false;
	double oldTransX, oldTransY, oldTransZ;
	double oldRotateX, oldRotateY, oldRotateZ;
	double oldScaleX, oldScaleY, oldScaleZ;
	double oldShearXY, oldShearXZ, oldShearYZ;
	for (size_t i = 0; i < keyCount; ++i)
	{
		MVector translation = tm[i].translation(MSpace::kTransform);
		double rotation[3]; tm[i].getRotation(rotation, rotationOrder);
		double scale[3]; tm[i].getScale(scale, MSpace::kWorld);
		double shear[3]; tm[i].getShear(shear, MSpace::kWorld);
		if (i == 0)
		{
			oldTransX = translation.x;
			oldTransY = translation.y;
			oldTransZ = translation.z;
			oldRotateX = rotation[0];
			oldRotateY = rotation[1];
			oldRotateZ = rotation[2];
			oldScaleX = scale[0];
			oldScaleY = scale[1];
			oldScaleZ = scale[2];
			oldShearXY = shear[0];
			oldShearXZ = shear[1];
			oldShearYZ = shear[2];
		}
		else 
		{
			if (!IsEquivalent(oldTransX, translation.x)) haveTransX = true;
			if (!IsEquivalent(oldTransY, translation.y)) haveTransY = true;
			if (!IsEquivalent(oldTransZ, translation.z)) haveTransZ = true;
			if (!IsEquivalent(oldRotateX, rotation[0])) haveRotateX = true;
			if (!IsEquivalent(oldRotateY, rotation[1])) haveRotateY = true;
			if (!IsEquivalent(oldRotateZ, rotation[2])) haveRotateZ = true;
			if (!IsEquivalent(oldScaleX, scale[0])) haveScaleX = true;
			if (!IsEquivalent(oldScaleY, scale[1])) haveScaleY = true;
			if (!IsEquivalent(oldScaleZ, scale[2])) haveScaleZ = true;
			if (!IsEquivalent(oldShearXY, shear[0])) haveShearXY = true;
			if (!IsEquivalent(oldShearXZ, shear[1])) haveShearXZ = true;
			if (!IsEquivalent(oldShearYZ, shear[2])) haveShearYZ = true;
		}
	}

#define IMPORT_NON_ANIMATED(plugName, value) { \
	MPlug p = MFnDependencyNode(transformNode).findPlug(plugName); \
	CDagHelper::SetPlugValue(p, value); }

	// Create the receiving animation curves; none if not needed
	MFnAnimCurve tXCurve, tYCurve, tZCurve, rXCurve, rYCurve, rZCurve;
	MFnAnimCurve sXCurve, sYCurve, sZCurve, hXYCurve, hXZCurve, hYZCurve;
	if (haveTransX)
	{
		tXCurve.setObject(CDagHelper::CreateAnimationCurve(transformNode, 
				"translateX", "animCurveTL"));
	}
	else 
	{
		IMPORT_NON_ANIMATED("translateX", oldTransX);
	}
	if (haveTransY)
	{
		tYCurve.setObject(CDagHelper::CreateAnimationCurve(transformNode, 
				"translateY", "animCurveTL"));
	}
	else
	{
		IMPORT_NON_ANIMATED("translateY", oldTransY);
	}
	if (haveTransZ)
	{
		tZCurve.setObject(CDagHelper::CreateAnimationCurve(transformNode, 
				"translateZ", "animCurveTL"));
	}
	else
	{
		IMPORT_NON_ANIMATED("translateZ", oldTransZ);
	}

	if (haveRotateX)
	{
		rXCurve.setObject(CDagHelper::CreateAnimationCurve(transformNode, 
				"rotateX", "animCurveTA"));
	}
	else
	{
		IMPORT_NON_ANIMATED("rotateX", oldRotateX);
	}
	if (haveRotateY)
	{
		rYCurve.setObject(CDagHelper::CreateAnimationCurve(transformNode, 
				"rotateY", "animCurveTA"));
	}
	else
	{
		IMPORT_NON_ANIMATED("rotateY", oldRotateY);
	}
	if (haveRotateZ)
	{
		rZCurve.setObject(CDagHelper::CreateAnimationCurve(transformNode, 
				"rotateZ", "animCurveTA"));
	}
	else
	{
		IMPORT_NON_ANIMATED("rotateZ", oldRotateZ);
	}

	if (haveScaleX)
	{
		sXCurve.setObject(CDagHelper::CreateAnimationCurve(transformNode, 
				"scaleX", "animCurveTU"));
	}
	else
	{
		IMPORT_NON_ANIMATED("scaleX", oldScaleX);
	}
	if (haveScaleY)
	{
		sYCurve.setObject(CDagHelper::CreateAnimationCurve(transformNode, 
				"scaleY", "animCurveTU"));
	}
	else
	{
		IMPORT_NON_ANIMATED("scaleY", oldScaleY);
	}
	if (haveScaleZ)
	{
		sZCurve.setObject(CDagHelper::CreateAnimationCurve(transformNode, 
				"scaleZ", "animCurveTU"));
	}
	else
	{
		IMPORT_NON_ANIMATED("scaleZ", oldScaleZ);
	}

	if (haveShearXY)
	{
		hXYCurve.setObject(CDagHelper::CreateAnimationCurve(transformNode, 
				"shearXY", "animCurveTU"));
	}
	else
	{
		IMPORT_NON_ANIMATED("shearXY", oldShearXY);
	}
	if (haveShearXZ)
	{
		hXZCurve.setObject(CDagHelper::CreateAnimationCurve(transformNode, 
				"shearXZ", "animCurveTU"));
	}
	else
	{
		IMPORT_NON_ANIMATED("shearXZ", oldShearXZ);
	}
	if (haveShearYZ)
	{
		hYZCurve.setObject(CDagHelper::CreateAnimationCurve(transformNode, 
				"shearYZ", "animCurveTU"));
	}
	else
	{
		IMPORT_NON_ANIMATED("shearYZ", oldShearYZ);
	}
#undef IMPORT_NON_ANIMATED

	// Be careful when converting animated matrices into Euler angles:
	// you need to verify that we have the closest Euler angle.

	// Sample the scene node transform
	double previousRotation[3] = { 0.0, 0.0, 0.0 };
	for (size_t i = 0; i < keyCount; ++i)
	{
		// Extract the transformation from the sampled matrix
		MVector translation = tm[i].translation(MSpace::kTransform);
		double rotation[3]; tm[i].getRotation(rotation, rotationOrder);
		double scale[3]; tm[i].getScale(scale, MSpace::kWorld);
		double shear[3]; tm[i].getShear(shear, MSpace::kWorld);

		// Since we're coming from a matrix, which doesn't care for angle's loop-around property: patch these Euler rotations
		// according to the previous angles.
		if (i > 0) PatchEuler(previousRotation, rotation);
		for (int j = 0; j < 3; ++j) previousRotation[j] = rotation[j];

		// Setup the keys
		MTime t(keys[i], MTime::kSeconds);
		if (haveTransZ) tXCurve.addKey(t, translation.x);
		if (haveTransY) tYCurve.addKey(t, translation.y);
		if (haveTransZ) tZCurve.addKey(t, translation.z);
		if (haveRotateX) rXCurve.addKey(t, rotation[0]);
		if (haveRotateY) rYCurve.addKey(t, rotation[1]);
		if (haveRotateZ) rZCurve.addKey(t, rotation[2]);
		if (haveScaleX) sXCurve.addKey(t, scale[0]);
		if (haveScaleY) sYCurve.addKey(t, scale[1]);
		if (haveScaleZ) sZCurve.addKey(t, scale[2]);
		if (haveShearXY) hXYCurve.addKey(t, shear[0]);
		if (haveShearXZ) hXZCurve.addKey(t, shear[1]);
		if (haveShearYZ) hYZCurve.addKey(t, shear[2]);
	}

	FCDSceneNodeTools::ClearSampledAnimation();
	return true;
}

// Buffer the plug arrays and plugs to export animations
//
void DaeAnimationLibrary::AddPlugAnimation(MObject node, const MString& attr, FCDParameterAnimatable& animatedValue, uint sampleType, FCDConversionFunctor* conversion)
{ AddPlugAnimation(node, attr, animatedValue, (SampleType) sampleType, conversion); }
void DaeAnimationLibrary::AddPlugAnimation(MObject node, const MString& attr, FCDParameterAnimatable& animatedValue, SampleType sampleType, FCDConversionFunctor* conversion)
{
	if (node == MObject::kNullObj) return;

	MFnDependencyNode dgFn(node);
	MPlug plug = dgFn.findPlug(attr);

	AddPlugAnimation(plug, animatedValue, sampleType, conversion);
}

void DaeAnimationLibrary::AddPlugAnimation(MObject node, const MString& attr, FCDAnimated* animatedValue, uint sampleType, FCDConversionFunctor* conversion)
{ AddPlugAnimation(node, attr, animatedValue, (SampleType) sampleType, conversion); }
void DaeAnimationLibrary::AddPlugAnimation(MObject node, const MString& attr, FCDAnimated* animatedValue, SampleType sampleType, FCDConversionFunctor* conversion)
{
	if (node == MObject::kNullObj || animatedValue == NULL) return;

	MFnDependencyNode dgFn(node);
	MPlug plug = dgFn.findPlug(attr);

	AddPlugAnimation(plug, animatedValue, sampleType, conversion);
}

void DaeAnimationLibrary::AddPlugAnimation(const MPlug& plug, FCDParameterAnimatable& animatedValue, uint sampleType, FCDConversionFunctor* conversion)
{ AddPlugAnimation(plug, animatedValue, (SampleType) sampleType, conversion); }
void DaeAnimationLibrary::AddPlugAnimation(const MPlug& plug, FCDParameterAnimatable& animatedValue, SampleType sampleType, FCDConversionFunctor* conversion)
{
	if (plug.node() == MObject::kNullObj) return;
	if (isImport && !animatedValue.IsAnimated()) return;
	AddPlugAnimation(plug, animatedValue.GetAnimated(), sampleType, conversion);
}

void DaeAnimationLibrary::AddPlugAnimation(const MPlug& plug, FCDAnimated* animatedValue, uint sampleType, FCDConversionFunctor* conversion)
{ AddPlugAnimation(plug, animatedValue, (SampleType) sampleType, conversion); }
void DaeAnimationLibrary::AddPlugAnimation(const MPlug& plug, FCDAnimated* animatedValue, SampleType sampleType, FCDConversionFunctor* conversion)
{
	if (animatedValue == NULL) return;

	bool isSampling = false;
	if (!isImport)
	{
		CAnimationHelper::CheckForSampling(doc->GetAnimationCache(), sampleType, plug);
		isSampling = doc->GetAnimationCache()->MarkPlugWanted(plug);
	}

	// Buffer the call to support driven-key animations
	DaeAnimatedElement* animatedPlug = new DaeAnimatedElement();
	animatedPlug->plug = plug;
	animatedPlug->animatedValue = animatedValue;
	animatedPlug->sampleType = sampleType;
	animatedPlug->isExportSampling = isSampling;

	// Copy the conversion functor. If none is provided: generate one.
	if (conversion != NULL)
	{
		animatedPlug->conversion = conversion;
	}
	else if ((sampleType & kAngle) == kAngle)
	{
		animatedPlug->conversion = new FCDConversionScaleFunctor(isImport ? FMath::DegToRad(1.0f) : FMath::RadToDeg(1.0f));
	}

	if (!isImport)
	{
		if (isSampling)
		{
			animatedElements.push_back(animatedPlug);
		}
		else if (ExportAnimation(animatedPlug))
		{
			animatedElements.push_back(animatedPlug);

			// check with the unset driven curves to see if new curve matches what they are waiting for
			size_t unsetDrivenCurvesCount = unsetDrivenCurves.size();
			for (size_t i = unsetDrivenCurvesCount; i > 0; i --)
			{
				DaeUnsetDriven* unsetDriven = unsetDrivenCurves.at(i-1);
				int index = -1;
				if (FindAnimated(unsetDriven->plug, index, animatedPlug))
				{
					unsetDriven->curve->SetDriver(animatedPlug->animatedValue, index);
					unsetDrivenCurves.erase(i-1);
					SAFE_DELETE(unsetDriven);
				}
			}
		}
		else
		{
			SAFE_DELETE(animatedPlug);
		}
	}
	else
	{
		animatedElements.push_back(animatedPlug);
		if (animatedValue->HasCurve())
		{
			ImportAnimation(animatedPlug);
		}
	}
}

void DaeAnimationLibrary::PostSampling()
{
	for (DaeAnimatedElement** it = animatedElements.begin(); it != animatedElements.end(); ++it)
	{
		if ((*it)->isExportSampling) ExportAnimation(*it);
	}
}

void DaeAnimationLibrary::AddUnsetDrivenCurve(FCDAnimationCurve* curve, const MPlug& plug)
{
#ifdef _DEBUG
	for (FCDAnimationCurvePlugList::iterator it = unsetDrivenCurves.begin(); it != unsetDrivenCurves.end(); it++)
	{
		FUAssert((*it)->curve != curve, return;);
	}
#endif
	
	DaeUnsetDriven* unsetDriven = new DaeUnsetDriven();
	unsetDriven->curve = curve;
	unsetDriven->plug = plug;
	unsetDrivenCurves.push_back(unsetDriven);
}

// Look-ups
DaeAnimatedElement* DaeAnimationLibrary::FindAnimated(const MPlug& plug, int& index)
{
	// Check the known plugs and their child plugs
	for (DaeAnimatedElementList::iterator it = animatedElements.begin(); it != animatedElements.end(); ++it)
	{
		if (FindAnimated(plug, index, *it)) return *it;
	}
	return NULL;
}

bool DaeAnimationLibrary::FindAnimated(const MPlug& plug, int& index, DaeAnimatedElement* animation)
{
	if (animation->plug == plug)
	{
		index = -1;
		return true;
	}
	else
	{
		uint childCount = animation->plug.numChildren();
		for (uint j = 0; j < childCount; ++j)
		{
			if (animation->plug.child(j) == plug)
			{
				index = (int) j;
				return true;
			}
		}
	}
	return false;
}

DaeAnimatedElement* DaeAnimationLibrary::FindAnimated(const FCDAnimated* value)
{
	// Check the known plug references
	for (DaeAnimatedElementList::iterator it = animatedElements.begin(); it != animatedElements.end(); ++it)
	{
		if ((*it)->animatedValue == value) return (*it);
	}
	return NULL;
}

void DaeAnimationLibrary::OffsetAnimationWithAnimation(FCDParameterAnimatableFloat& oldValue, FCDParameterAnimatableFloat& offsetValue)
{
	if (!oldValue.IsAnimated() && !offsetValue.IsAnimated()) return;

	// find the old animation curve
	FCDAnimated* oldAnimated = oldValue.GetAnimated();
	if (oldAnimated == NULL) return;
	FCDAnimationCurve* oldCurve = oldAnimated->GetCurve(0);
	if (oldCurve == NULL) return;

	// find the offset animation curve, if it doens't exist, just offset the old curve by offsetValue and exit
#define VALUE_OFFSET FCDConversionOffsetFunctor functor(-*offsetValue); oldCurve->ConvertValues(&functor, &functor);
	FCDAnimated* offsetAnimated = offsetValue.GetAnimated();
	if (offsetAnimated == NULL) { VALUE_OFFSET; return; }
	FCDAnimationCurve* offsetCurve = offsetAnimated->GetCurve(0);
	if (offsetCurve == NULL) { VALUE_OFFSET; return; }
#undef VALUE_OFFSET

	// offset the values and their tangents if necessary
	size_t oldKeysCount = oldCurve->GetKeyCount();
	FCDAnimationKey** offsetKeys = offsetCurve->GetKeys();
	FCDAnimationKey** oldKeys = oldCurve->GetKeys();
	for (size_t i = 0; i < oldKeysCount; i++)
	{
		float inputValue = oldKeys[i]->input;

		if (IsEquivalent(offsetKeys[i]->input, inputValue)) // assume 1 to 1 relationship
		{
			float offset = -offsetKeys[i]->output;
			oldKeys[i]->output += offset;

			if (oldKeys[i]->interpolation == FUDaeInterpolation::BEZIER)
			{
				if (offsetKeys[i]->interpolation == FUDaeInterpolation::BEZIER)
				{
					FCDAnimationKeyBezier* bkey = (FCDAnimationKeyBezier*)oldKeys[i];
					FCDAnimationKeyBezier* offsetBkey = (FCDAnimationKeyBezier*)offsetKeys[i];

					float outTangentOffset = (offsetBkey->outTangent.y - offsetBkey->output) /
							(offsetBkey->outTangent.x - offsetBkey->input);
					float inTangentOffset = (offsetBkey->inTangent.y - offsetBkey->output) /
							(offsetBkey->inTangent.x - offsetBkey->input);					

					bkey->outTangent.y += offset - outTangentOffset * (bkey->outTangent.x - bkey->input);
					bkey->inTangent.y += offset - inTangentOffset * (bkey->inTangent.x - bkey->input);
				}
			}
		}
		else
		{
			float offset = -offsetCurve->Evaluate(inputValue);
			oldKeys[i]->output += offset;

			if (oldKeys[i]->interpolation == FUDaeInterpolation::BEZIER)
			{
				// FIXME: we should estimate the tangents
				FUFail(;);

				FCDAnimationKeyBezier* bkey = (FCDAnimationKeyBezier*)oldKeys[i];
				bkey->outTangent.y += offset;
				bkey->inTangent.y += offset;
			}
		}
	}
}

//
// DaeAnimatedElement
//

DaeAnimatedElement::DaeAnimatedElement()
{
	sampleType = kSingle;
	animatedValue = NULL;
	conversion = NULL;
}

DaeAnimatedElement::~DaeAnimatedElement()
{
	SAFE_DELETE(conversion);
	animatedValue = NULL;
}
