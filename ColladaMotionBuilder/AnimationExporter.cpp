/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "AnimationExporter.h"
#include "ColladaExporter.h"
#include "NodeExporter.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDAnimation.h"
#include "FCDocument/FCDAnimationChannel.h"
#include "FCDocument/FCDAnimationCurve.h"
#include "FCDocument/FCDAnimationKey.h"
#include "FCDocument/FCDEntity.h"
#include "FCDocument/FCDLibrary.h"

//
// Helpers
//

template <class Type>
class Holder
{
public:
	Type value;
	Holder& operator= (const Type& _value) { _value = value; return *this; }
	Type& operator[] (int index) { return value; }
};

//
// AnimationExporter
//

AnimationExporter::AnimationExporter(ColladaExporter* base)
:	EntityExporter(base)
{
}

AnimationExporter::~AnimationExporter()
{
	CLEAR_POINTER_VECTOR(propertiesToSample);
	CLEAR_POINTER_VECTOR(modelsToSample);
}

bool AnimationExporter::IsAnimated(FBProperty* property)
{
	if (!property->IsAnimatable()) return false;
	FBPropertyAnimatable* animatable = (FBPropertyAnimatable*) property;
	FBAnimationNode* animation = animatable->GetAnimationNode();
	if (animation == NULL) return false;
	int subNodeCount = animation->Nodes.GetCount();
	if (subNodeCount == 0)
	{
		FBFCurve* curve = animation->FCurve;
		return curve != NULL && curve->Keys.GetCount() > 0;
	}
	for (int i = 0; i < subNodeCount; ++i)
	{
		FBAnimationNode* subAnim = animation->Nodes[i];
		FBFCurve* curve = subAnim->FCurve;
		if (curve != NULL && curve->Keys.GetCount() > 0) return true;
	}
	return false;
}

void AnimationExporter::ExportProperty(FBProperty* property, FCDEntity* colladaEntity, FCDAnimated* animated, FCDConversionFunctor* functor)
{
	// Retrieve the list of curves for this property.
	FBCurveList curveList;
	GetPropertyCurves(property, curveList, (int) animated->GetValueCount());
	size_t subNodeCount = curveList.size();
	if (subNodeCount == 0) return;

	// Create the animation entity and its channel.
	FCDAnimation* colladaAnimation = CDOC->GetAnimationLibrary()->AddEntity();
	colladaAnimation->SetDaeId(colladaEntity->GetDaeId() + "-" + property->GetName());
	FCDAnimationChannel* colladaChannel = colladaAnimation->AddChannel();
	
	// Process each FBCurve and export it.
	for (size_t i = 0; i < subNodeCount; ++i)
	{
		FBFCurve* curve = curveList[i];
		if (curve == NULL || curve->Keys.GetCount() == 0) continue;

		FCDAnimationCurve* colladaCurve = colladaChannel->AddCurve();
		ExportCurve(curve, colladaCurve, functor);
		animated->AddCurve(i, colladaCurve);
	}
}

void AnimationExporter::ExportProperty(FBProperty* property, size_t index, FCDEntity* colladaEntity, FCDAnimated* colladaAnimated, int animatedIndex, FCDConversionFunctor* functor)
{
	// Retrieve the list of curves for this property.
	FBCurveList curveList;
	GetPropertyCurves(property, curveList, INT_MAX);
	if (curveList.size() <= index || curveList[index] == NULL || curveList[index]->Keys.GetCount() == 0) return;
	FBFCurve* exportCurve = curveList[index];

	// Create the animation entity and its channel.
	FCDAnimation* colladaAnimation = CDOC->GetAnimationLibrary()->AddEntity();
	colladaAnimation->SetDaeId(colladaEntity->GetDaeId() + "-" + property->GetName());
	FCDAnimationChannel* colladaChannel = colladaAnimation->AddChannel();
	
	// Export the curve.
	FCDAnimationCurve* colladaCurve = colladaChannel->AddCurve();
	ExportCurve(exportCurve, colladaCurve, functor);
	colladaAnimated->AddCurve(animatedIndex, colladaCurve);
}

void AnimationExporter::GetPropertyCurves(FBProperty* property, FBCurveList& curveList, int maxCount)
{
	// Retrieve the animation node for this property, if valid.
	if (!property->IsAnimatable()) return;
	FBPropertyAnimatable* animatable = (FBPropertyAnimatable*) property;
	FBAnimationNode* mainNode = animatable->GetAnimationNode();
	if (mainNode == NULL) return;

	// Make a list of the animation curves available for this animation node.
	bool hasCurve = false;
	int subNodeCount = mainNode->Nodes.GetCount();
	if (subNodeCount == 0)
	{
		curveList.push_back(mainNode->FCurve);
		subNodeCount = 1;
		hasCurve = mainNode->FCurve != NULL && mainNode->FCurve->Keys.GetCount() > 0;
	}
	else
	{
		// Size-up the curves list
		subNodeCount = min(subNodeCount, maxCount);
		for (int i = 0; i < subNodeCount; ++i)
		{
			FBAnimationNode* subNode = mainNode->Nodes[i];
			FBFCurve* curve = subNode->FCurve;
			int subNodeCount2 = subNode->Nodes.GetCount(); subNodeCount2;
			curveList.push_back(curve);
			hasCurve |= curve != NULL && curve->Keys.GetCount() > 0;
		}
	}
	if (!hasCurve) curveList.clear();
}

void AnimationExporter::ExportCurve(FBFCurve* curve, FCDAnimationCurve* colladaCurve, FCDConversionFunctor* functor)
{
	int keyCount = curve->Keys.GetCount();
	for (int i = 0; i < keyCount; ++i)
	{
		// Process each key independently.
		FBFCurveKey key = curve->Keys[i];
		FCDAnimationKey* colladaKey = NULL;
		switch (key.Interpolation)
		{
		case kFBInterpolationConstant: colladaKey = colladaCurve->AddKey(FUDaeInterpolation::STEP); break;
		case kFBInterpolationLinear: colladaKey = colladaCurve->AddKey(FUDaeInterpolation::LINEAR); break;
		case kFBInterpolationCubic:
			if (key.TangentMode == kFBTangentModeTCB)
			{
				colladaKey = colladaCurve->AddKey(FUDaeInterpolation::TCB);
			}
			else
			{
				colladaKey = colladaCurve->AddKey(FUDaeInterpolation::BEZIER);
			}
			break;
		default:
			colladaKey = colladaCurve->AddKey(FUDaeInterpolation::LINEAR);
			FUFail("Unknown enum value. ");
			break;
		}

		// Common key parameters.
		colladaKey->input = ToSeconds(key.Time);
		colladaKey->output = key.Value;

		if (colladaKey->interpolation == FUDaeInterpolation::BEZIER)
		{
			FCDAnimationKeyBezier* bkey = (FCDAnimationKeyBezier*) colladaKey;

			// Motion Builder has 2D tangents, using an angle-weight system, like Maya's.
			// But I cannot figure out how to get these weights or the 2D control points.
			// So, set the left and right tangents to one-third of the way to the previous/next keys.
			bkey->inTangent.x = (i == 0) ? (colladaKey->input - 1.0f) : ((2.0f * colladaKey->input + ToSeconds(curve->Keys[i-1].Time)) / 3.0f);
			bkey->outTangent.x = (i == keyCount - 1) ? (colladaKey->input + 1.0f) : ((2.0f * colladaKey->input + ToSeconds(curve->Keys[i+1].Time)) / 3.0f);
			FUAssert(key.TangentMode != kFBTangentModeTCB, continue);
			bkey->inTangent.y = bkey->output - (key.LeftDerivative * (bkey->input - bkey->inTangent.x));
			bkey->outTangent.y = bkey->output + (key.RightDerivative * (bkey->outTangent.x - bkey->input));
		}
		else if (colladaKey->interpolation == FUDaeInterpolation::TCB)
		{
			FCDAnimationKeyTCB* tkey = (FCDAnimationKeyTCB*) colladaKey;
			tkey->tension = key.Tension;
			tkey->continuity = key.Continuity;
			tkey->bias = key.Bias;
		}
	}

	// Apply the wanted conversion.
	if (functor != NULL)
	{
		colladaCurve->ConvertValues(functor, functor);
	}
}

void AnimationExporter::AddModelToSample(FBModel* node, FCDEntity* colladaNode, FCDParameterAnimatableMatrix44& colladaTransform)
{
	if (!IsAnimated(&node->Translation) && !IsAnimated(&node->Rotation) && !IsAnimated(&node->Scaling)) return;

	// Create the animated object.
	FCDAnimated* animated = colladaTransform.GetAnimated();
	FCDAnimation* colladaAnimation = CDOC->GetAnimationLibrary()->AddEntity();
	colladaAnimation->SetDaeId(colladaNode->GetDaeId() + "-transform");
	FCDAnimationChannel* colladaChannel = colladaAnimation->AddChannel();

	// Create 16 animation curves.
	for (size_t i = 0; i < 16; ++i)
	{
		animated->AddCurve(i, colladaChannel->AddCurve());
	}

	// Add to our list.
	FBSampledModel* sampledModel = new FBSampledModel();
	sampledModel->animated = animated;
	sampledModel->animation = colladaAnimation;
	sampledModel->model = node;
	modelsToSample.push_back(sampledModel);
}

void AnimationExporter::AddPropertyToSample(FBProperty* property, int index, FCDEntity* colladaEntity, FCDAnimated* colladaAnimated, int animatedIndex, bool isAngle)
{
	if (!property->IsAnimatable()) return;
	FBPropertyAnimatable* animatable = (FBPropertyAnimatable*) property;

	// Create the animated object.
	FCDAnimation* colladaAnimation = CDOC->GetAnimationLibrary()->AddEntity();
	colladaAnimation->SetDaeId(colladaEntity->GetDaeId() + "-" + property->GetName());
	FCDAnimationChannel* colladaChannel = colladaAnimation->AddChannel();

	// Create the necessary animation curves.
	size_t valueCount = colladaAnimated->GetValueCount();
	for (size_t i = 0; i < valueCount; ++i)
	{
		colladaAnimated->AddCurve(i, colladaChannel->AddCurve());
	}

	// Add to our list.
	FBSampledProperty* sampledProperty = new FBSampledProperty();
	sampledProperty->animated = colladaAnimated;
	sampledProperty->animation = colladaAnimation;
	sampledProperty->animatedIndex = animatedIndex;
	sampledProperty->index = index;
	sampledProperty->property = animatable;
	sampledProperty->isAngle = isAngle;
	propertiesToSample.push_back(sampledProperty);
}

void AnimationExporter::DoSampling()
{
	FBSystem system;
	float sampleTime = ToSeconds(system.LocalTime);

	for (FBSampledModel** it = modelsToSample.begin(); it != modelsToSample.end();)
	{
		FBModel* node = (*it)->model;
		FCDAnimated* animated = (*it)->animated;
		if (animated == NULL)
		{
			// Something deleted this sample point...
			SAFE_RELEASE((*it)->animation);
			SAFE_DELETE(*it);
			it = modelsToSample.erase(it);
			continue;
		}

		// Take a sample.
		FMMatrix44 t;
		FBMatrix t1;
		if (node->Is(FBCamera::TypeInfo))
		{
			// Cameras are special and nasty nodes.
			FMMatrix44 parentTransform = NODE->GetParentTransform(node->Parent, false);

			
//			t = ((FBCamera*)node)->GetMatrix(kFBModelView);
			((FBCamera*)node)->GetCameraMatrix(t1, kFBModelView);
			t = ToFMMatrix44(t1);

			t = parentTransform.Inverted() * t.Transposed().Inverted();
		}
		else
		{
			FBMatrix localTransformation;
			node->GetMatrix(localTransformation, kModelTransformation, false);
			t = ToFMMatrix44(localTransformation);
		}

		// Write out the matrix values.
		for (size_t j = 0; j < 16; ++j)
		{
			FCDAnimationCurve* colladaCurve = animated->GetCurve(j);
			FCDAnimationKey* key = colladaCurve->AddKey(FUDaeInterpolation::LINEAR);
			key->input = sampleTime;
			key->output = t.m[j & 0x3][j >> 2];
		}

		++it;
	}

	for (FBSampledProperty** it = propertiesToSample.begin(); it != propertiesToSample.end();)
	{
		if ((*it)->animated == NULL)
		{
			// Something deleted this sample point...
			SAFE_RELEASE((*it)->animation);
			it = propertiesToSample.erase(it);
			continue;
		}

		switch ((*it)->property->GetPropertyType())
		{
		case kFBPT_int: SampleProperty<Holder<int>, 1>(*it, sampleTime); break;
		case kFBPT_bool: SampleProperty<Holder<bool>, 1>(*it, sampleTime); break;
		case kFBPT_float: SampleProperty<Holder<float>, 1>(*it, sampleTime); break;
		case kFBPT_double: SampleProperty<Holder<double>, 1>(*it, sampleTime); break;
		case kFBPT_Vector4D: SampleProperty<FBVector4d, 4>(*it, sampleTime); break;
		case kFBPT_Vector3D: SampleProperty<FBVector3d, 3>(*it, sampleTime); break;
		case kFBPT_Vector2D: SampleProperty<FBVector2d, 2>(*it, sampleTime); break;
		case kFBPT_ColorRGB: SampleProperty<FBVector3d, 3>(*it, sampleTime); break;
		case kFBPT_ColorRGBA: SampleProperty<FBVector4d, 4>(*it, sampleTime); break;

			// Not animatables:
		case kFBPT_unknown: case kFBPT_charptr: case kFBPT_enum: case kFBPT_Time:
		/*case kFBPT_String:*/ case kFBPT_object: case kFBPT_event: case kFBPT_stringlist:
		case kFBPT_Action: case kFBPT_Reference: case kFBPT_TimeSpan: case kFBPT_kReference:
		/*case kFBPT_last:*/ default: break;
		}

		++it;
	}
}

void AnimationExporter::TerminateSampling()
{
	// Check all the sample curves to see if they are real.
	for (FBSampledModel** it = modelsToSample.begin(); it != modelsToSample.end(); ++it)
	{
		TerminateSampling((*it)->animated);
		if ((*it)->animation->GetChannel(0)->GetCurveCount() == 0)
		{
			SAFE_RELEASE((*it)->animation);
		}
	}
	for (FBSampledProperty** it = propertiesToSample.begin(); it != propertiesToSample.end(); ++it)
	{
		TerminateSampling((*it)->animated);
		if ((*it)->animation->GetChannel(0)->GetCurveCount() == 0)
		{
			(*it)->animation->Release();
			FUAssert((*it)->animation == NULL, continue);
		}
	}
}

void AnimationExporter::TerminateSampling(FCDAnimated* animated)
{
	size_t valueCount = animated->GetValueCount();
	for (size_t i = 0; i < valueCount; ++i)
	{
		bool isCurveValid = false;
		FCDAnimationCurve* curve = animated->GetCurve(i);
		if (curve == NULL) continue;
		size_t keyCount = curve->GetKeyCount();
		if (keyCount > 0)
		{
			float value = curve->GetKey(0)->output;
			for (size_t j = 1; j < keyCount && !isCurveValid; ++j)
			{
				isCurveValid = !IsEquivalent(value, curve->GetKey(j)->output);
			}
		}

		if (!isCurveValid)
		{
			// This curve contains no actual animation.
			SAFE_RELEASE(curve);
		}
	}
}

template <class Type, size_t TypeValueCount>
void AnimationExporter::SampleProperty(AnimationExporter::FBSampledProperty* it, float sampleTime)
{
	Type value;
	it->property->GetData(&value, sizeof(Type));
	if (it->index == -1)
	{
		int valueCount = (int) min(TypeValueCount, it->animated->GetValueCount());
		for (int i = 0; i < valueCount; ++i)
		{
			FCDAnimationCurve* curve = it->animated->GetCurve(i);
			FUAssert(curve != NULL, continue);
			FCDAnimationKey* key = curve->AddKey(FUDaeInterpolation::LINEAR);
			key->input = sampleTime;
			key->output = (float) value[i];
		}
	}
	else if (it->index < TypeValueCount)
	{
		FCDAnimationCurve* curve = it->animated->GetCurve(it->animatedIndex);
		FCDAnimationKey* key = curve->AddKey(FUDaeInterpolation::LINEAR);
		key->input = sampleTime;
		key->output = (float) value[it->index];
		if (it->isAngle && curve->GetKeyCount() > 1)
		{
			FCDAnimationKey* previousKey = curve->GetKey(curve->GetKeyCount() - 2);
			while (key->output > previousKey->output + 180.0f) key->output -= 360.0f;
			while (key->output < previousKey->output - 180.0f) key->output += 360.0f;
		}
	}
}
