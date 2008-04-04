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

#include "StdAfx.h"
#include "AnimationExporter.h"
#include "AnimationImporter.h"
#include "DocumentExporter.h"
#include "ColladaOptions.h"
#include "Common/ConversionFunctions.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDAnimation.h"
#include "FCDocument/FCDAnimationClip.h"
#include "FCDocument/FCDAnimationChannel.h"
#include "FCDocument/FCDAnimationCurve.h"
#include "FCDocument/FCDAnimationKey.h"
#include "FCDocument/FCDExtra.h"
#include "FCDocument/FCDLibrary.h"
#include "FUtils/FUCrc32.h"
#include <decomp.h>

//
// Static Macros
// 

#define FLOATWAVE_CONTROL_CLASS_ID	0x930Abc78

inline FUDaeInfinity::Infinity ConvertORT(int ort)
{
	switch (ort)
	{
		case ORT_CONSTANT: return FUDaeInfinity::CONSTANT;
		case ORT_CYCLE: return FUDaeInfinity::CYCLE;
		case ORT_LOOP: return FUDaeInfinity::CYCLE;
		case ORT_OSCILLATE: return FUDaeInfinity::OSCILLATE;
		case ORT_LINEAR: return FUDaeInfinity::LINEAR;
		case ORT_RELATIVE_REPEAT: return FUDaeInfinity::CYCLE_RELATIVE;
		default: return FUDaeInfinity::CONSTANT;
	}
}

template <class T, class F, class L>
inline FUDaeInterpolation::Interpolation PatchBezierKey(T& k, const F& UNUSED(flat), const L& length, TimeValue previousTime, TimeValue nextTime)
{
	FUDaeInterpolation::Interpolation interp = FUDaeInterpolation::BEZIER;
	if (k.flags & BEZKEY_UNCONSTRAINHANDLE) 
	{
		k.inLength *= GetTicksPerFrame() / (float) (k.time - previousTime); 
		k.outLength *= GetTicksPerFrame() / (float) (nextTime - k.time); 
	}
	if (GetInTanType(k.flags) != BEZKEY_USER) k.inLength = length;
	if (GetOutTanType(k.flags) != BEZKEY_USER) k.outLength = length;
	if (GetOutTanType(k.flags) == BEZKEY_LINEAR) interp = FUDaeInterpolation::LINEAR;
	else if (GetOutTanType(k.flags) == BEZKEY_STEP) interp = FUDaeInterpolation::STEP;
	return interp;
}

//
// AnimationExporter::Clip
//

AnimationExporter::Clip::Clip()
:	colladaClip(NULL), colladaContainer(NULL), controller(NULL)
{
}

//
// AnimationExporter
//

AnimationExporter::AnimationExporter(DocumentExporter* _document)
:	BaseExporter(_document)
,	isRelativeAnimation(false), controlType(ColladaMaxAnimatable::ROTATION_X)
{
	for (size_t i = 0; i < maxInitialValues; ++i) initialValue[i] = 0.0f;
}

AnimationExporter::~AnimationExporter()
{
	clips.clear();
}

void AnimationExporter::FinalizeExport()
{
	for (ClipMap::iterator it = clips.begin(); it != clips.end(); ++it)
	{
		if (it->second.colladaClip != NULL)
		{
			// If the clip is empty, remove it because empty clips are not valid COLLADA.
			if (it->second.colladaClip->GetClipCurves().empty())
			{
				SAFE_RELEASE(it->second.colladaClip);
				SAFE_RELEASE(it->second.colladaContainer);
			}
		}
	}
}

AnimationExporter::ClipList AnimationExporter::GetAnimationClips(Control* controller)
{
	// Create the global clip, if it does not yet exists.
	if (clips.empty())
	{
		ClipMap::iterator it = clips.insert(0, Clip());
		if (OPTS->ExportAnimClip())
		{
			Interval animRange = GetCOREInterface()->GetAnimRange();
			float timeFactor = 1.0f / AnimationImporter::GetTimeFactor();
			it->second.colladaClip = CDOC->GetAnimationClipLibrary()->AddEntity();
			it->second.colladaClip->SetStart(animRange.Start() * timeFactor);
			it->second.colladaClip->SetEnd(animRange.End() * timeFactor);
		}
	}

	ClipList out;

#if MAX_VERSION_MAJOR >= 9
	Class_ID cid = (controller != NULL) ? controller->ClassID() : Class_ID(0, 0);
	if (cid == FLOATLAYER_CONTROL_CLASS_ID || cid == POINT3LAYER_CONTROL_CLASS_ID || cid == POSLAYER_CONTROL_CLASS_ID || cid == ROTLAYER_CONTROL_CLASS_ID || cid == SCALELAYER_CONTROL_CLASS_ID || cid == POINT4LAYER_CONTROL_CLASS_ID || cid == LAYEROUTPUT_CONTROL_CLASS_ID)
	{
		// Retrieve the animation layer controller interface in order to extract the animation clip information.
		ILayerControl* layerController = GetILayerControlInterface(controller);
		FUAssert(layerController != NULL, return out);
		int layerCount = layerController->GetLayerCount();
		for (int i = 0; i < layerCount; ++i)
		{
			TSTR name = layerController->GetLayerName(i);
			uint32 crc = FUCrc32::CRC32(name.data());
			ClipMap::iterator it = clips.find(crc);
			if (it == clips.end())
			{
				// This is a new animation layer.
				Interval animRange = GetCOREInterface()->GetAnimRange();
				float timeFactor = 1.0f / AnimationImporter::GetTimeFactor();
				it = clips.insert(crc, Clip());
				it->second.colladaClip = CDOC->GetAnimationClipLibrary()->AddEntity();
				it->second.colladaClip->SetStart(animRange.Start() * timeFactor);
				it->second.colladaClip->SetEnd(animRange.End() * timeFactor);
				it->second.colladaClip->SetDaeId(name.data());
				it->second.colladaContainer = CDOC->GetAnimationLibrary()->AddEntity();
				it->second.colladaContainer->SetDaeId(fm::string(name.data()) + "-clip");
			}
			it->second.controller = layerController->GetSubCtrl(i);
			if (it->second.controller != NULL)
			{
				// Add this sub-controller to the list of controllers to attempt to export.
				out.push_back(&it->second);
			}
		}
	}
	else
#endif // Max 9.0+
	{
		// Attempt to export this standard controller.
		Clip* baseClip = &clips.find(0)->second;
		baseClip->controller = controller;
		out.push_back(baseClip);
	}
	return out;
}

FCDAnimation* AnimationExporter::CreateAnimation(Clip* clip, const fchar* id)
{
	FCDAnimation* animation;
	if (clip == NULL || clip->colladaContainer == NULL)
	{
		animation = CDOC->GetAnimationLibrary()->AddEntity();
	}
	else
	{
		animation = clip->colladaContainer->AddChild();
	}
	animation->SetDaeId(TO_STRING(id));
	return animation;
}

FCDAnimationCurve* AnimationExporter::CreateCurve(FCDAnimationChannel* colladaChannel, Control* controller)
{
	FCDAnimationCurve* colladaCurve = colladaChannel->AddCurve();
	if (controller != NULL)
	{
		colladaCurve->SetPreInfinity(ConvertORT(controller->GetORT(ORT_BEFORE)));
		colladaCurve->SetPostInfinity(ConvertORT(controller->GetORT(ORT_AFTER)));
	}
	return colladaCurve;
}

FCDAnimated* AnimationExporter::ExportAnimatedPoint3(const fm::string& id, Control* controller, FCDParameterAnimatableVector3& colladaValue, int UNUSED(arrayIndex), FCDConversionFunctor* conversion, bool forceSampling)
{
	if (!OPTS->ExportAnimations()) return NULL;
	FCDAnimated* animated = colladaValue.GetAnimated();
	ExportAnimatedPoint3(id, controller, animated, conversion, forceSampling);
	if (!animated->HasCurve()) SAFE_RELEASE(animated);
	return animated;
}

void AnimationExporter::ExportAnimatedPoint3(const fm::string& id, Control* controller, FCDAnimated* animated, FCDConversionFunctor* conversion, bool forceSampling)
{
	if (controller == NULL || animated == NULL || animated->GetValueCount() < 3) return; 

	// Create the animation clip iterator.
	ClipList clips = GetAnimationClips(controller);
	for (Clip** it = clips.begin(); it != clips.end(); ++it)
	{
		controller = (*it)->controller;
		FCDAnimation* animation = CreateAnimation(*it, id.c_str());
		FCDAnimationChannel* channel = animation->AddChannel();

		// Relative animation support
		if (isRelativeAnimation)
		{
			SClass_ID ctrlSuperId = controller->SuperClassID();
			Point3 v;
			if (ctrlSuperId == CTRL_POINT3_CLASS_ID || ctrlSuperId == CTRL_POSITION_CLASS_ID) 
			{
				// Get our initial value
				controller->GetValue(OPTS->AnimStart(), &v, FOREVER, CTRL_ABSOLUTE);
			}
			else if (ctrlSuperId == CTRL_SCALE_CLASS_ID)
			{
				// For COLLADA, a scale is just another P3
				ScaleValue sv;	// A scale value holds a scale and orientation
				controller->GetValue(OPTS->AnimStart(), &sv, FOREVER, CTRL_ABSOLUTE);
				v = sv.s;		// Keep the scale, we dont care about the orientation.
			}
			else
			{
				FUFail(return);
			}
			
			initialValue[0] = v[0];
			initialValue[1] = v[1];
			initialValue[2] = v[2];
		}

		Control* subControllers[3] = { controller->GetXController(), controller->GetYController(), controller->GetZController() };

		// First, Try to extract animations from the component controllers
		bool animationValid = false;
		if (subControllers[0] != NULL && subControllers[1] != NULL && subControllers[2] != NULL)
		{
			for (size_t i = 0; i < 3; ++i)
			{
				FCDAnimationCurve* curve = CreateCurve(channel, subControllers[i]);
				ExportCurve(subControllers[i], curve, conversion, forceSampling);
				if (curve != NULL)
				{
					animated->AddCurve(i, curve);
					if ((*it)->colladaClip != NULL) (*it)->colladaClip->AddClipCurve(curve);
					animationValid = true;
				}
			}
		}
		else 
		{
			// Else, with no subs, try and export ourselves as keyframes
			animationValid = ExportAnimation(*it, ColladaMaxAnimatable::FLOAT3, animated, channel, conversion, forceSampling);
		}

		if (!animationValid) SAFE_RELEASE(animation);
	}
}

FCDAnimated* AnimationExporter::ExportAnimatedPoint4(const fm::string& id, Control* controller, FCDParameterAnimatableVector4& colladaValue, int UNUSED(arrayIndex), FCDConversionFunctor* conversion, bool forceSampling)
{
	if (!OPTS->ExportAnimations()) return NULL;
	FCDAnimated* animated = colladaValue.GetAnimated();
	ExportAnimatedPoint4(id, controller, animated, conversion, forceSampling);
	if (!animated->HasCurve()) SAFE_RELEASE(animated);
	return animated;
}

void AnimationExporter::ExportAnimatedPoint4(const fm::string& id, Control* controller, FCDAnimated* animated, FCDConversionFunctor* conversion, bool forceSampling)
{
	if (!OPTS->ExportAnimations() || controller == NULL || animated == NULL || animated->GetValueCount() < 4)
		return;
	
	SClass_ID superId = controller->SuperClassID();
	FUAssert(superId == CTRL_POINT4_CLASS_ID, return);

	// treat it as a RGBA color
	ClipList clips = GetAnimationClips(controller);
	for (Clip** it = clips.begin(); it != clips.end(); ++it)
	{
		controller = (*it)->controller;
		FCDAnimation* animation = CreateAnimation(*it, id.c_str());
		FCDAnimationChannel* channel = animation->AddChannel();

		Point4 v;
		controller->GetValue(OPTS->AnimStart(), &v, FOREVER, CTRL_ABSOLUTE);

		initialValue[0] = v.x;
		initialValue[1] = v.y;
		initialValue[2] = v.z;
		initialValue[3] = v.w;

		// Direct controller not keyed. Try to extract animations from the component controllers
		bool animationValid = false;
		Control* subControllers[4] = { controller->GetXController(), controller->GetYController(), controller->GetZController(), controller->GetWController() };
		if (subControllers[0] != NULL && subControllers[1] != NULL && subControllers[2] != NULL && subControllers[3] != NULL)
		{	
			for (int i = 0; i < 4; ++i)
			{
				FCDAnimationCurve* curve = CreateCurve(channel, subControllers[i]);
				ExportCurve(subControllers[i], curve, conversion, forceSampling);
				if (curve != NULL)
				{
					animated->AddCurve(i, curve);
					if ((*it)->colladaClip != NULL) (*it)->colladaClip->AddClipCurve(curve);
					animationValid = true;
				}
			}
		}
		else
		{
			animationValid = ExportAnimation(*it, ColladaMaxAnimatable::FLOAT4, animated, channel, conversion, forceSampling);
		}

		if (!animationValid)
		{
			SAFE_RELEASE(animation);
		}
	}
}

FCDAnimated* AnimationExporter::ExportAnimatedAngle(const fm::string& id, Control* controller, ColladaMaxAnimatable::Type type, FCDParameterAnimatableAngleAxis& colladaValue, FCDConversionFunctor* conversion, bool forceSampling)
{
	if (!OPTS->ExportAnimations() || controller == NULL) return NULL;

	// maybe this rotation controller doesn't have XYZ components
	Control* xyzController = NULL;

	if (type == ColladaMaxAnimatable::ROTATION_X) xyzController = controller->GetXController();
	else if (type == ColladaMaxAnimatable::ROTATION_Y) xyzController = controller->GetYController();
	else if (type == ColladaMaxAnimatable::ROTATION_Z) xyzController = controller->GetZController();
	if (xyzController != NULL) controller = xyzController;

	FCDAnimated* animated = colladaValue.GetAnimated();
	ClipList clips = GetAnimationClips(controller);
	for (Clip** it = clips.begin(); it != clips.end(); ++it)
	{
		controller = (*it)->controller;
		FCDAnimation* animation = CreateAnimation(*it, id.c_str());
		FCDAnimationChannel* channel = animation->AddChannel();
		FCDAnimationCurve* curve = CreateCurve(channel, controller);

		// set the type parameter
		controlType = type;
		ExportCurve(controller, curve, conversion, forceSampling);
		if (curve != NULL)
		{
			animated->AddCurve(3, curve);
			if ((*it)->colladaClip != NULL) (*it)->colladaClip->AddClipCurve(curve);
		}
		else
		{
			SAFE_RELEASE(animation);
		}
	}

	if (!animated->HasCurve()) SAFE_RELEASE(animated);
	return animated;
}

void AnimationExporter::ExportAnimatedFloat(const fm::string& id, Control* controller, FCDAnimated* animated, FCDConversionFunctor* conversion, bool forceSampling)
{
	if (!OPTS->ExportAnimations() || controller == NULL || animated == NULL || animated->GetValueCount() < 1)
		return;

	// Relative animation support
	if (isRelativeAnimation)
	{
		SClass_ID ctrlSuperId = controller->SuperClassID();
		if (ctrlSuperId == CTRL_POINT3_CLASS_ID || ctrlSuperId == CTRL_POSITION_CLASS_ID) 
		{
			// Get our initial value
			Point3 v;
			controller->GetValue(OPTS->AnimStart(), &v, FOREVER, CTRL_ABSOLUTE);
			initialValue[0] = v[controlType - ColladaMaxAnimatable::ROTATION_X];
		}
		else if (ctrlSuperId == CTRL_SCALE_CLASS_ID)
		{
			// For COLLADA, a scale is just another P3
			ScaleValue sv;	// A scale value holds a scale and orientation
			controller->GetValue(OPTS->AnimStart(), &sv, FOREVER, CTRL_ABSOLUTE);
			initialValue[0] = sv.s[controlType - ColladaMaxAnimatable::ROTATION_X];
		}
		else if (ctrlSuperId == CTRL_FLOAT_CLASS_ID)
		{
			controller->GetValue(OPTS->AnimStart(), initialValue, FOREVER, CTRL_ABSOLUTE);
		}
		else
		{
			FUFail(return);
		}
	}

	// Now, write out the curves.
	ClipList clips = GetAnimationClips(controller);
	for (Clip** it = clips.begin(); it != clips.end(); ++it)
	{
		controller = (*it)->controller;
		FCDAnimation* animation = CreateAnimation(*it, id.c_str());
		FCDAnimationChannel* channel = animation->AddChannel();
		FCDAnimationCurve* curve = CreateCurve(channel, controller);

		ExportCurve(controller, curve, conversion, forceSampling);
		if (curve != NULL)
		{
			animated->AddCurve(0, curve);
			if ((*it)->colladaClip != NULL) (*it)->colladaClip->AddClipCurve(curve);
		}
		else
		{
			SAFE_RELEASE(animation);
		}
	}
}

FCDAnimated* AnimationExporter::ExportAnimatedFloat(const fm::string& id, Control* controller, FCDParameterAnimatableFloat& colladaValue, int UNUSED(arrayIndex), FCDConversionFunctor* conversion, bool forceSampling)
{
	if (!OPTS->ExportAnimations()) return NULL;
	FCDAnimated* animated = colladaValue.GetAnimated();
	ExportAnimatedFloat(id, controller, animated, conversion, forceSampling);
	if (!animated->HasCurve()) SAFE_RELEASE(animated);
	return animated;
}

FCDAnimated* AnimationExporter::ExportAnimatedAngAxis(const fm::string& id, Control* controller, ColladaMaxAnimatable::Type type, FCDParameterAnimatableAngleAxis& colladaValue, FCDConversionFunctor* conversion, bool forceSampling)
{
	if (!OPTS->ExportAnimations() || controller == NULL) return NULL;
	
	FCDAnimated* animated = colladaValue.GetAnimated();
	ClipList clips = GetAnimationClips(controller);
	for (Clip** it = clips.begin(); it != clips.end(); ++it)
	{
		controller = (*it)->controller;
		FCDAnimation* animation = CreateAnimation(*it, id.c_str());
		FCDAnimationChannel* channel = animation->AddChannel();

		if (!ExportAnimation(*it, type, animated, channel, conversion, forceSampling))
		{
			SAFE_RELEASE(animation);
		}
	}

	if (!animated->HasCurve())
	{
		SAFE_RELEASE(animated);
	}
	return animated;
}

FCDAnimated* AnimationExporter::ExportAnimatedTransform(const fm::string& id, INode* node, FCDParameterAnimatableMatrix44& colladaValue)
{
	if (!OPTS->ExportAnimations() || node == NULL) return NULL;

	FCDAnimated* animated = colladaValue.GetAnimated();
	ClipList clips = GetAnimationClips(NULL);
	FCDAnimation* animation = CreateAnimation(clips.front(), id.c_str());
	FCDAnimationChannel* channel = animation->AddChannel();

	// Create the sixteen needed curves
	FCDAnimationCurveList curves;
    FUAssert(animated->GetValueCount() >= 12, return NULL); // checks against programmer error?
	static const size_t dimensions = 12;
	curves.reserve(dimensions);
	for (size_t i = 0; i < dimensions; ++i)
	{
		FCDAnimationCurve* curve = CreateCurve(channel);
		animated->AddCurve(i, curve);
		curves.push_back(curve);
		if (clips.front()->colladaClip != NULL) clips.front()->colladaClip->AddClipCurve(curve);
	}

	// Setup the curve keys and interpolations. Pre-buffer the key values.
	int tpf = GetTicksPerFrame();
	TimeValue startTime = OPTS->AnimStart();
	TimeValue endTime = OPTS->AnimEnd() + 1;
	for (FCDAnimationCurveList::iterator it = curves.begin(); it != curves.end(); ++it)
	{
		FCDAnimationCurve* curve = (*it);
		for (TimeValue t = startTime; t < endTime; t += tpf)
		{
			FCDAnimationKey* k = curve->AddKey(FUDaeInterpolation::LINEAR);
			k->input = (float) FSConvert::MaxTickToSecs(t);
		}
	}

	// Actual sampling
	Matrix3 childTM, parentTM;
	INode* parentNode = node->GetParentNode();
	if (parentNode != NULL && parentNode->IsRootNode()) parentNode = NULL;

	size_t k = 0;
	for (TimeValue t = startTime; t < endTime; t += tpf, k++)
	{
		// Export the base NODE TM
		childTM = node->GetNodeTM(t);

		if (parentNode != NULL)
		{
			// export a relative TM
			// We have to use whatever value we exported for this parent
			// in order to remain consistent in collada
			parentTM = parentNode->GetNodeTM(t);
			parentTM.Invert();
			childTM = childTM * parentTM;
		}

		// Push this matrix as separate animation curves
		const Matrix3& _tm = childTM;
		for (int y = 0; y < 3; ++y)
		{
			for (int x = 0; x < 4; ++x)
			{
				curves[y * 4 + x]->GetKey(k)->output = _tm[x][y];
			}
		}
	}

	// Verify that the samples create an animation
	for (int d = 0; d < dimensions; ++d)
	{
		FCDAnimationCurve* curve = curves[d];
		bool keepCurve = false;
		size_t keyCount = curve->GetKeyCount();
		if (keyCount > 0)
		{
			float _out = curve->GetKey(0)->output;
			for (size_t i = 0; i < keyCount && !keepCurve; ++i)
			{
				keepCurve |= !IsEquivalent(_out, curve->GetKey(i)->output);
			}
		}

		if (!keepCurve)
		{
			delete curve;
			curves[d] = NULL;
		}
	}

	// Delete empty animations
	if (!animated->HasCurve())
	{
		SAFE_RELEASE(animated);
		SAFE_RELEASE(animation);
	}
	return animated;
}


FCDAnimated* AnimationExporter::ExportWSMPivot(const fm::string& id, INode* node, FCDParameterAnimatableMatrix44& colladaValue)
{
	if (!OPTS->ExportAnimations() || node == NULL) return NULL;

	// This can only be animated if we have a WSM object
	if (node->GetWSMDerivedObject() == NULL) return NULL;

	FCDAnimated* animated = colladaValue.GetAnimated();
	ClipList clips = GetAnimationClips(NULL);
	FCDAnimation* animation = CreateAnimation(clips.front(), id.c_str());
	FCDAnimationChannel* channel = animation->AddChannel();

	// Create the sixteen needed curves
	FCDAnimationCurveList curves;
	size_t dimensions = animated->GetValueCount();
	curves.reserve(dimensions);
	for (size_t i = 0; i < dimensions; ++i)
	{
		FCDAnimationCurve* curve = CreateCurve(channel);
		animated->AddCurve(i, curve);
		curves.push_back(curve);
		if (clips.front()->colladaClip != NULL) clips.front()->colladaClip->AddClipCurve(curve);
	}

	// Setup the curve keys and interpolations. Pre-buffer the key values.
	int tpf = GetTicksPerFrame();
	TimeValue startTime = OPTS->AnimStart();
	TimeValue endTime = OPTS->AnimEnd() + 1;
	for (FCDAnimationCurveList::iterator it = curves.begin(); it != curves.end(); ++it)
	{
		FCDAnimationCurve* curve = (*it);
		for (TimeValue t = startTime; t < endTime; t += tpf)
		{
			FCDAnimationKey* k = curve->AddKey(FUDaeInterpolation::LINEAR);
			k->input = (float) FSConvert::MaxTickToSecs(t);
		}
	}

	size_t k = 0;
	for (TimeValue t = startTime; t < endTime; t += tpf, k++)
	{
		// The matrix we are sampling is the tm from NodeTM to the final world TM of the object.
		// This is the ObjOffset TM and any WSM occuring on the node.
		Matrix3 objWSMTM = node->GetObjTMAfterWSM(t);
		Matrix3 nodeTM = node->GetNodeTM(t);
		nodeTM.Invert();
		const Matrix3& _tm = objWSMTM * nodeTM;
		for (int y = 0; y < 3; ++y)
		{
			for (int x = 0; x < 4; ++x)
			{
				curves[y * 4 + x]->GetKey(k)->output = _tm[x][y];
			}
		}
	}

	// Verify that the samples create an animation
	for (size_t d = 0; d < dimensions; ++d)
	{
		FCDAnimationCurve* curve = curves[d];
		bool keepCurve = false;
		size_t keyCount = curve->GetKeyCount();
		if (keyCount > 0)
		{
			float _out = curve->GetKey(0)->output;
			for (size_t i = 0; i < keyCount && !keepCurve; ++i)
			{
				keepCurve |= !IsEquivalent(_out, curve->GetKey(i)->output);
			}
		}

		if (!keepCurve)
		{
			delete curve;
			curves[d] = NULL;
		}
	}

	// Delete empty animations
	if (!animated->HasCurve())
	{
		SAFE_RELEASE(animated);
		SAFE_RELEASE(animation);
	}
	return animated;
}

void AnimationExporter::ExportCurve(Control* controller, FCDAnimationCurve*& curve, FCDConversionFunctor* conversion, bool forceSampling)
{
	if (!OPTS->ExportAnimations() || controller == NULL)
	{
		SAFE_RELEASE(curve);
		return;
	}

	bool isSampling = OPTS->SampleAnim() || forceSampling;
	if (!isSampling)
	{
		IKeyControl* keyInterface = GetKeyControlInterface(controller);
		Class_ID clid = controller->ClassID();
		if (clid.PartB() != 0)
		{
			// This is not a Max controller, sample it.
			// The only max controllers that have non-zero
			// values are not keyable (attach, link, etc).
			isSampling = true;
		}
		else if (keyInterface != NULL && keyInterface->GetNumKeys() > 0)
		{
			float timeFactor = 1.0f / AnimationImporter::GetTimeFactor();
			int keyCount = keyInterface->GetNumKeys();

#define PROCESS_KEYS(keyClassName, interpolation) { \
				keyClassName previousValue, nextValue; \
				for (int i = 0; i < keyCount; ++i) { \
					TimeValue previousTime = (i > 0) ? controller->GetKeyTime(i - 1) : controller->GetKeyTime(i) - 1.0f / timeFactor; (void)previousTime; \
					TimeValue nextTime = (i < keyCount - 1) ? controller->GetKeyTime(i + 1) : controller->GetKeyTime(i) + 1.0f / timeFactor; (void)nextTime; \
					keyClassName k; \
					keyInterface->GetKey(i, &k); \
					FUDaeInterpolation::Interpolation interp = FUDaeInterpolation::interpolation;
#define VAL(value) FCDAnimationKey* key = curve->AddKey(interp); key->input = k.time * timeFactor; key->output = value
#define INTAN(valueX, valueY) ((FCDAnimationKeyBezier*) key)->inTangent = FMVector2(valueX, valueY)
#define OUTTAN(valueX, valueY) ((FCDAnimationKeyBezier*) key)->outTangent = FMVector2(valueX, valueY)
#define TCBPARAM(t, c, b, ei, eo) \
				((FCDAnimationKeyTCB*) key)->tension = t; \
				((FCDAnimationKeyTCB*) key)->continuity = c; \
				((FCDAnimationKeyTCB*) key)->bias = b; \
				((FCDAnimationKeyTCB*) key)->easeIn = ei; \
				((FCDAnimationKeyTCB*) key)->easeOut = eo

#define PROCESS_KEYS_END } break; }

			int type = clid.PartA();
			switch (type)
			{
			case LININTERP_FLOAT_CLASS_ID:
				PROCESS_KEYS(ILinFloatKey, LINEAR)
					VAL(k.val);
				PROCESS_KEYS_END

			case LININTERP_POSITION_CLASS_ID:
				controlType = FMath::Clamp(controlType, ColladaMaxAnimatable::ROTATION_X, ColladaMaxAnimatable::ROTATION_Z);
				PROCESS_KEYS(ILinPoint3Key, LINEAR)
					VAL(k.val[controlType - ColladaMaxAnimatable::ROTATION_X]);
				PROCESS_KEYS_END
			
			case LININTERP_ROTATION_CLASS_ID: {
				controlType = FMath::Clamp(controlType, ColladaMaxAnimatable::ROTATION_X, ColladaMaxAnimatable::ROTATION_Z);

				float pval[3];
				PROCESS_KEYS(ILinRotKey, LINEAR)
					float v[3];
					k.val.GetEuler(&v[0],&v[1],&v[2]);
					
					if (i > 0) FSConvert::PatchEuler(pval, v);
					pval[0] = v[0]; pval[1] = v[1]; pval[2] = v[2];

					VAL(v[controlType - ColladaMaxAnimatable::ROTATION_X]);
				PROCESS_KEYS_END }

			case HYBRIDINTERP_FLOAT_CLASS_ID:
				PROCESS_KEYS(IBezFloatKey, BEZIER)
					interp = PatchBezierKey(k, 0.0f, 0.333f, previousTime, nextTime);
					VAL(k.val);
					// Dont forget! Max Bezier handles multiple curve types!
					if (interp == FUDaeInterpolation::BEZIER)
					{
						float inInterval = (k.time - previousTime) * k.inLength;
						INTAN((k.time - inInterval) * timeFactor, k.val + k.intan * inInterval);
						float outInterval = (nextTime - k.time) * k.outLength;
						OUTTAN((k.time + outInterval) * timeFactor, k.val + k.outtan * outInterval);
					}
				PROCESS_KEYS_END

			case HYBRIDINTERP_POINT3_CLASS_ID:
			case HYBRIDINTERP_POSITION_CLASS_ID: {
				controlType = FMath::Clamp(controlType, ColladaMaxAnimatable::ROTATION_X, ColladaMaxAnimatable::ROTATION_Z);
				int ii = controlType - ColladaMaxAnimatable::ROTATION_X;
				PROCESS_KEYS(IBezPoint3Key, BEZIER)
					interp = PatchBezierKey(k, Point3::Origin, Point3(0.333f, 0.333f, 0.33f), previousTime, nextTime);
					VAL(k.val[ii]);
					// Dont forget! Max Bezier handles multiple curve types!
					if (interp == FUDaeInterpolation::BEZIER)
					{
						float inInterval = (k.time - previousTime) * k.inLength[ii];
						INTAN((k.time - inInterval) * timeFactor, k.val[ii] + k.intan[ii] * inInterval);
						float outInterval = (nextTime - k.time) * k.outLength[ii];
						OUTTAN((k.time + outInterval) * timeFactor, k.val[ii] + k.outtan[ii] * outInterval);
					}
				PROCESS_KEYS_END }

			case HYBRIDINTERP_ROTATION_CLASS_ID: {
				controlType = FMath::Clamp(controlType, ColladaMaxAnimatable::ROTATION_X, ColladaMaxAnimatable::ROTATION_Z);
				
				float pval[3];
				PROCESS_KEYS(IBezQuatKey, BEZIER)
					float v[3];
					k.val.GetEuler(&v[0],&v[1],&v[2]);

					if (i > 0) FSConvert::PatchEuler(pval, v);
					pval[0] = v[0]; pval[1] = v[1]; pval[2] = v[2];

					float value = v[controlType - ColladaMaxAnimatable::ROTATION_X];
					VAL(value);

					float inInterval = (k.time - previousTime) * 0.333f;
					float outInterval = (nextTime - k.time) * 0.333f;
					INTAN((k.time - inInterval) * timeFactor, value);
					OUTTAN((k.time + outInterval) * timeFactor, value);
				PROCESS_KEYS_END }

			case TCBINTERP_FLOAT_CLASS_ID:
				PROCESS_KEYS(ITCBFloatKey, TCB)
					VAL(k.val);
					TCBPARAM(k.tens, k.cont, k.bias, k.easeIn, k.easeOut);
				PROCESS_KEYS_END

			case TCBINTERP_POINT3_CLASS_ID:
			case TCBINTERP_POSITION_CLASS_ID: {
				controlType = FMath::Clamp(controlType, ColladaMaxAnimatable::ROTATION_X, ColladaMaxAnimatable::ROTATION_Z);
				int ii = controlType - ColladaMaxAnimatable::ROTATION_X;
				PROCESS_KEYS(ITCBPoint3Key, BEZIER)
					VAL(k.val[ii]);
					TCBPARAM(0.5f, 0.5f, 0.5f, 0.5f, 0.5f);
				PROCESS_KEYS_END }

			case TCBINTERP_ROTATION_CLASS_ID:
				controlType = FMath::Clamp(controlType, ColladaMaxAnimatable::ROTATION_X, ColladaMaxAnimatable::ROTATION_Z);
				float pval[3];
				PROCESS_KEYS(ITCBRotKey, TCB)
					float v[3];
					Quat quat(k.val);
					quat.GetEuler(&v[0],&v[1],&v[2]);
					if (i > 0) FSConvert::PatchEuler(pval, v);
					pval[0] = v[0]; pval[1] = v[1]; pval[2] = v[2];
					VAL(v[controlType - ColladaMaxAnimatable::ROTATION_X]);
					TCBPARAM(0.5f, 0.5f, 0.5f, 0.5f, 0.5f);
				PROCESS_KEYS_END
		
			default:
				isSampling = true;
				break;
			}
		}
		else if (clid.PartA() == LININTERP_FLOAT_CLASS_ID ||
				clid.PartA() == HYBRIDINTERP_FLOAT_CLASS_ID)
		{
			SAFE_RELEASE(curve);
			return;
		}
		else
		{
			isSampling = true;
		}

#undef PROCESS_KEYS
#undef VAL
#undef INTAN
#undef OUTTAN
#undef TCBPARAM
#undef PROCESS_KEYS_END


	}

	if (isSampling)
	{
		// Reserve the necessary memory and generate the key times
		int tpf = GetTicksPerFrame();
		TimeValue startTime = OPTS->AnimStart();
		TimeValue endTime = OPTS->AnimEnd() + 1;
		for (TimeValue t = startTime; t < endTime; t += tpf)
		{
			FCDAnimationKey* key = curve->AddKey(FUDaeInterpolation::LINEAR);
			key->input = (float) FSConvert::MaxTickToSecs(t);
		}

#define PROCESS_KEYS(valueClassName, defaultValue) { \
			valueClassName value(defaultValue); \
			size_t i = 0; \
			for (TimeValue t = startTime; t < endTime; t += tpf, ++i) {  \
				FCDAnimationKey* key = curve->GetKey(i); \
				controller->GetValue(t, &value, FOREVER, CTRL_ABSOLUTE);
#define VAL(value) key->output = value;
#define PROCESS_KEYS_END } break; } 

		SClass_ID type = controller->SuperClassID();
		switch (type)
		{
		case CTRL_FLOAT_CLASS_ID:
			PROCESS_KEYS(float, 1.0f)
				VAL(value);
			PROCESS_KEYS_END

		case CTRL_ROTATION_CLASS_ID:
			controlType = FMath::Clamp(controlType, ColladaMaxAnimatable::ROTATION_X, ColladaMaxAnimatable::ROTATION_Z);

			float pval[3], v[3];
			PROCESS_KEYS(Quat, 1)
				//value.GetEuler(&v[0],&v[1],&v[2]);
				QuatToEuler(value, v, EULERTYPE_XYZ);
				if (i > 0) FSConvert::PatchEuler(pval, v);
				pval[0] = v[0]; pval[1] = v[1]; pval[2] = v[2];

				VAL(v[controlType - ColladaMaxAnimatable::ROTATION_X]);
			PROCESS_KEYS_END

		case CTRL_POINT3_CLASS_ID:
			controlType = FMath::Clamp(controlType, ColladaMaxAnimatable::ROTATION_X, ColladaMaxAnimatable::ROTATION_Z);
			PROCESS_KEYS(Point3, Point3::Origin)
				VAL(value[controlType - ColladaMaxAnimatable::ROTATION_X]);
			PROCESS_KEYS_END

		default:
			FUFail(;);
		}

#undef PROCESS_KEYS
#undef VAL
#undef PROCESS_KEYS_END

		// Verify that the samples create an animation
		bool keepCurve = true;
		size_t keyCount = curve->GetKeyCount(); 
		if (keyCount > 0 && isSampling)
		{
			keepCurve = false;

			float firstOut = curve->GetKey(0)->output;
			for (size_t i = 1; !keepCurve && i < keyCount; ++i)
			{
				keepCurve |= !IsEquivalent(firstOut, curve->GetKey(i)->output);
			}
		}

		if (!keepCurve)
		{
			SAFE_RELEASE(curve);
		}
	}
	else
	{
		if (curve->GetKeyCount() == 0)
		{
			// no sampling and no keys -> no need for a curve.
			SAFE_RELEASE(curve);
		}
	}

	if (curve != NULL)
	{
		if (conversion != NULL)
		{
			curve->ConvertValues(conversion, conversion);
		}
		if (isRelativeAnimation && curve->GetKeyCount() > 0)
		{
			FCDConversionOffsetFunctor functor(-initialValue[0]);
			curve->ConvertValues(&functor, &functor);
		}
	}
}

bool AnimationExporter::ExportAnimation(Clip* clip, ColladaMaxAnimatable::Type type, FCDAnimated* animated, FCDAnimationChannel* channel, FCDConversionFunctor* conversion, bool forceSampling)
{
	Control* controller = clip->controller;
	if (!OPTS->ExportAnimations() || controller == NULL || !controller->IsAnimated()) return false;

	// Check that the information necessary for a keyframe animation is available.
	bool isSampling = OPTS->SampleAnim() || forceSampling;
	IKeyControl* keyInterface = NULL;
	if (!isSampling)
	{
		keyInterface = GetKeyControlInterface(controller);
		if (keyInterface == NULL) isSampling = true;
		else if (keyInterface->GetNumKeys() <= 1) return false;
	}

	// Create the necessary curves
	FCDAnimationCurveList curves;
	size_t dimensions = animated->GetValueCount();
	curves.reserve(dimensions);
	for (size_t i = 0; i < dimensions; ++i)
	{
		FCDAnimationCurve* curve = CreateCurve(channel, controller);
		animated->AddCurve(i, curve);
		curves.push_back(curve);
		if (clip->colladaClip != NULL) clip->colladaClip->AddClipCurve(curve);
	}

	float timeFactor = 1.0f / AnimationImporter::GetTimeFactor();
	if (!isSampling)
	{
		int keyCount = keyInterface->GetNumKeys();

#define PROCESS_KEYS(expectedDimensions, keyClassName, interpolation) { \
			if (expectedDimensions > dimensions) break; \
			for (int i = 0; i < keyCount; ++i) {  \
				TimeValue previousTime = (i > 0) ? controller->GetKeyTime(i - 1) : controller->GetKeyTime(i) - 1.0f / timeFactor; (void) previousTime; \
				TimeValue nextTime = (i < keyCount - 1) ? controller->GetKeyTime(i + 1) : controller->GetKeyTime(i) + 1.0f / timeFactor; (void) nextTime; \
				keyClassName k; \
				keyInterface->GetKey(i, &k); \
				FUDaeInterpolation::Interpolation interp = FUDaeInterpolation::interpolation;
#define CREATEKEY \
			for (size_t d = 0; d < dimensions; ++d) { \
				FCDAnimationKey* key = curves[d]->AddKey(interp); \
				key->input = k.time * timeFactor; }
#define VAL(dim, value) curves[dim]->GetKey(i)->output = value;
#define INTAN(dim, valueX, valueY) ((FCDAnimationKeyBezier*) curves[dim]->GetKey(i))->inTangent = FMVector2(valueX, valueY);
#define OUTTAN(dim, valueX, valueY) ((FCDAnimationKeyBezier*) curves[dim]->GetKey(i))->outTangent = FMVector2(valueX, valueY);
#define TCBPARAM(dim, t, c, b, ei, eo) \
				((FCDAnimationKeyTCB*) curves[dim]->GetKey(i))->tension = t; \
				((FCDAnimationKeyTCB*) curves[dim]->GetKey(i))->continuity = c; \
				((FCDAnimationKeyTCB*) curves[dim]->GetKey(i))->bias = b; \
				((FCDAnimationKeyTCB*) curves[dim]->GetKey(i))->easeIn = ei; \
				((FCDAnimationKeyTCB*) curves[dim]->GetKey(i))->easeOut = eo

#define PROCESS_KEYS_END } } break;
			
		int controllerType = controller->ClassID().PartA();
		switch (controllerType)
		{
		case LININTERP_FLOAT_CLASS_ID:
			PROCESS_KEYS(1, ILinFloatKey, LINEAR)
				CREATEKEY;
				VAL(0, k.val);
			PROCESS_KEYS_END

		case HYBRIDINTERP_FLOAT_CLASS_ID:
			PROCESS_KEYS(1, IBezFloatKey, BEZIER)
				interp = PatchBezierKey(k, 0.0f, 0.333f, previousTime, nextTime);

				CREATEKEY;

				if (interp == FUDaeInterpolation::BEZIER)
				{
					float inInterval = (k.time - previousTime) * k.inLength;
					float outInterval = (nextTime - k.time) * k.outLength;
					VAL(0, k.val);
					INTAN(0, (k.time - inInterval) * timeFactor, k.val + k.intan * inInterval);
					OUTTAN(0, (k.time + outInterval) * timeFactor, k.val + k.outtan * outInterval);
				}
			PROCESS_KEYS_END

		case LININTERP_POSITION_CLASS_ID:
			PROCESS_KEYS(3, ILinPoint3Key, LINEAR)
				CREATEKEY;
				VAL(0, k.val.x); VAL(1, k.val.y); VAL(2, k.val.z);
			PROCESS_KEYS_END

		case LININTERP_ROTATION_CLASS_ID: {
			float pval[3];
			PROCESS_KEYS(3, ILinRotKey, LINEAR)
				CREATEKEY;

				float v[3];
				k.val.GetEuler(&v[0], &v[1], &v[2]);

				if (i > 0) FSConvert::PatchEuler(pval, v);
				pval[0] = v[0]; pval[1] = v[1]; pval[2] = v[2];

				VAL(0, v[0]); VAL(1, v[1]); VAL(2, v[2]);

			PROCESS_KEYS_END }

		case LININTERP_SCALE_CLASS_ID:
			if (type == ColladaMaxAnimatable::SCALE)
			{
				PROCESS_KEYS(3, ILinScaleKey, LINEAR)
					CREATEKEY;
					VAL(0, k.val.s.x); VAL(1, k.val.s.y); VAL(2, k.val.s.z);
				PROCESS_KEYS_END
			}
			else if (type == ColladaMaxAnimatable::SCALE_ROT_AXIS)
			{
				PROCESS_KEYS(4, ILinScaleKey, LINEAR)
					CREATEKEY;
					AngAxis aa(k.val.q);
					if (aa.axis.Equals(Point3::Origin, TOLERANCE)) aa.axis = Point3::XAxis;
					VAL(0, aa.axis.x); VAL(1, aa.axis.y); VAL(2, aa.axis.z); VAL(3, FMath::RadToDeg(aa.angle));
				PROCESS_KEYS_END
			}
			else if (type == ColladaMaxAnimatable::SCALE_ROT_AXIS_R)
			{
				PROCESS_KEYS(4, ILinScaleKey, LINEAR)
					CREATEKEY;
					AngAxis aa(k.val.q);
					if (aa.axis.Equals(Point3::Origin, TOLERANCE)) aa.axis = Point3::XAxis;
					VAL(0, aa.axis.x); VAL(1, aa.axis.y); VAL(2, aa.axis.z); VAL(3, -FMath::RadToDeg(aa.angle));
				PROCESS_KEYS_END
			}
			break;

		case TCBINTERP_FLOAT_CLASS_ID:
			PROCESS_KEYS(1, ITCBFloatKey, TCB)
				CREATEKEY;
				VAL(0, k.val);
				TCBPARAM(0, k.tens, k.cont, k.bias, k.easeIn, k.easeOut);
			PROCESS_KEYS_END

		case TCBINTERP_POSITION_CLASS_ID:
		case TCBINTERP_POINT3_CLASS_ID:
			PROCESS_KEYS(3, ITCBPoint3Key, TCB)
				CREATEKEY;
				VAL(0, k.val.x); 
				VAL(1, k.val.y); 
				VAL(2, k.val.z);
				TCBPARAM(0, k.tens, k.cont, k.bias, k.easeIn, k.easeOut);
				TCBPARAM(1, k.tens, k.cont, k.bias, k.easeIn, k.easeOut);
				TCBPARAM(2, k.tens, k.cont, k.bias, k.easeIn, k.easeOut);
			PROCESS_KEYS_END

		case TCBINTERP_POINT4_CLASS_ID:
			PROCESS_KEYS(4, ITCBPoint4Key, TCB)
				CREATEKEY;
				VAL(0, k.val.x); 
				VAL(1, k.val.y); 
				VAL(2, k.val.z);
				VAL(2, k.val.w);
				TCBPARAM(0, k.tens, k.cont, k.bias, k.easeIn, k.easeOut);
				TCBPARAM(1, k.tens, k.cont, k.bias, k.easeIn, k.easeOut);
				TCBPARAM(2, k.tens, k.cont, k.bias, k.easeIn, k.easeOut);
				TCBPARAM(3, k.tens, k.cont, k.bias, k.easeIn, k.easeOut);
			PROCESS_KEYS_END

		case TCBINTERP_ROTATION_CLASS_ID:
			float pval[3];
			PROCESS_KEYS(3, ITCBRotKey, TCB)
				CREATEKEY;
				float v[3];
				Quat quat(k.val);
				quat.GetEuler(&v[0], &v[1], &v[2]);
				if (i > 0) FSConvert::PatchEuler(pval, v);
				pval[0] = v[0]; pval[1] = v[1]; pval[2] = v[2];
			
				VAL(0, v[0]); VAL(1, v[1]); VAL(2, v[2]);
				TCBPARAM(1, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f);
				TCBPARAM(2, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f);
				TCBPARAM(3, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f);
			PROCESS_KEYS_END
		
		case TCBINTERP_SCALE_CLASS_ID:
			if (type == ColladaMaxAnimatable::SCALE)
			{
				PROCESS_KEYS(3, ITCBScaleKey, TCB)
					CREATEKEY;
					VAL(0, k.val.s.x); VAL(1, k.val.s.y); VAL(2, k.val.s.z);
					TCBPARAM(0, k.tens, k.cont, k.bias, k.easeIn, k.easeOut);
					TCBPARAM(1, k.tens, k.cont, k.bias, k.easeIn, k.easeOut);
					TCBPARAM(2, k.tens, k.cont, k.bias, k.easeIn, k.easeOut);
				PROCESS_KEYS_END
			}
			else if (type == ColladaMaxAnimatable::SCALE_ROT_AXIS)
			{
				PROCESS_KEYS(4, ITCBScaleKey, TCB)
					CREATEKEY;
					AngAxis aa(k.val.q);
					if (aa.axis.Equals(Point3::Origin, TOLERANCE)) aa.axis = Point3::XAxis;
					VAL(0, aa.axis.x); VAL(1, aa.axis.y); VAL(2, aa.axis.z); VAL(3, FMath::RadToDeg(aa.angle));
					TCBPARAM(0, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f);
					TCBPARAM(1, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f);
					TCBPARAM(2, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f);
					TCBPARAM(3, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f);
				PROCESS_KEYS_END
			}
			else if (type == ColladaMaxAnimatable::SCALE_ROT_AXIS_R)
			{
				PROCESS_KEYS(4, ITCBScaleKey, TCB)
					CREATEKEY;
					AngAxis aa(k.val.q);
					if (aa.axis.Equals(Point3::Origin, TOLERANCE)) aa.axis = Point3::XAxis;
					VAL(0, aa.axis.x); VAL(1, aa.axis.y); VAL(2, aa.axis.z); VAL(3, -FMath::RadToDeg(aa.angle));
					TCBPARAM(0, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f);
					TCBPARAM(1, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f);
					TCBPARAM(2, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f);
					TCBPARAM(3, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f);
				PROCESS_KEYS_END
			}
			break;

		case HYBRIDINTERP_POSITION_CLASS_ID:
		case HYBRIDINTERP_POINT3_CLASS_ID:
		case HYBRIDINTERP_COLOR_CLASS_ID:
			PROCESS_KEYS(3, IBezPoint3Key, BEZIER)
				interp = PatchBezierKey(k, Point3::Origin, Point3(0.333, 0.333, 0.333), previousTime, nextTime);
				CREATEKEY;

				VAL(0, k.val.x); VAL(1, k.val.y); VAL(2, k.val.z);

				if (interp == FUDaeInterpolation::BEZIER)
				{
					float inInterval = (k.time - previousTime) * k.inLength.x;
					float outInterval = (nextTime - k.time) * k.outLength.x;
					INTAN(0, (k.time - inInterval) * timeFactor, k.val.x + k.intan.x * inInterval);
					OUTTAN(0, (k.time + outInterval) * timeFactor, k.val.x + k.outtan.x * outInterval);

					inInterval = (k.time - previousTime) * k.inLength.y;
					outInterval = (nextTime - k.time) * k.outLength.y;
					INTAN(1, (k.time - inInterval) * timeFactor, k.val.y + k.intan.y * inInterval);
					OUTTAN(1, (k.time + outInterval) * timeFactor, k.val.y + k.outtan.y * outInterval);

					inInterval = (k.time - previousTime) * k.inLength.z;
					outInterval = (nextTime - k.time) * k.outLength.z;
					INTAN(2, (k.time - inInterval) * timeFactor, k.val.z + k.intan.z * inInterval);
					OUTTAN(2, (k.time + outInterval) * timeFactor, k.val.z + k.outtan.z * outInterval);
				}
			PROCESS_KEYS_END

		case HYBRIDINTERP_POINT4_CLASS_ID:
		case HYBRIDINTERP_FRGBA_CLASS_ID:
			PROCESS_KEYS(4, IBezPoint4Key, BEZIER)
				interp = PatchBezierKey(k, Point4::Origin, Point4(0.333f, 0.333f, 0.333f, 0.333f), previousTime, nextTime);
				CREATEKEY;

				VAL(0, k.val.x); VAL(1, k.val.y); VAL(2, k.val.z); VAL(3, k.val.w);

				if (interp == FUDaeInterpolation::BEZIER)
				{
					float inInterval = (k.time - previousTime) * k.inLength.x;
					float outInterval = (nextTime - k.time) * k.outLength.x;
					INTAN(0, (k.time - inInterval) * timeFactor, k.val.x + k.intan.x * inInterval);
					OUTTAN(0, (k.time + outInterval) * timeFactor, k.val.x + k.outtan.x * outInterval);

					inInterval = (k.time - previousTime) * k.inLength.y;
					outInterval = (nextTime - k.time) * k.outLength.y;
					INTAN(1, (k.time - inInterval) * timeFactor, k.val.y + k.intan.y * inInterval);
					OUTTAN(1, (k.time + outInterval) * timeFactor, k.val.y + k.outtan.y * outInterval);

					inInterval = (k.time - previousTime) * k.inLength.z;
					outInterval = (nextTime - k.time) * k.outLength.z;
					INTAN(2, (k.time - inInterval) * timeFactor, k.val.z + k.intan.z * inInterval);
					OUTTAN(2, (k.time + outInterval) * timeFactor, k.val.z + k.outtan.z * outInterval);

					inInterval = (k.time - previousTime) * k.inLength.w;
					outInterval = (nextTime - k.time) * k.outLength.w;
					INTAN(3, (k.time - inInterval) * timeFactor, k.val.w + k.intan.w * inInterval);
					OUTTAN(3, (k.time + outInterval) * timeFactor, k.val.w + k.outtan.w * outInterval);
				}
			PROCESS_KEYS_END

		case HYBRIDINTERP_ROTATION_CLASS_ID: {
			float pval[3];
			PROCESS_KEYS(3, IBezQuatKey, BEZIER)
				CREATEKEY;

				float v[3];
				k.val.GetEuler(&v[0], &v[1], &v[2]);

				if (i > 0) FSConvert::PatchEuler(pval, v);
				pval[0] = v[0]; pval[1] = v[1]; pval[2] = v[2];
			
				VAL(0, v[0]); VAL(1, v[1]); VAL(2, v[2]);

				float inInterval = (k.time - previousTime) * 0.333f;
				float outInterval = (nextTime - k.time) * 0.333f;
				INTAN(0, (k.time - inInterval) * timeFactor, v[0]);
				OUTTAN(0, (k.time + outInterval) * timeFactor, v[0]);
				INTAN(1, (k.time - inInterval) * timeFactor, v[1]);
				OUTTAN(1, (k.time + outInterval) * timeFactor, v[1]);
				INTAN(2, (k.time - inInterval) * timeFactor, v[2]);
				OUTTAN(2, (k.time + outInterval) * timeFactor, v[2]);
			PROCESS_KEYS_END }

		case HYBRIDINTERP_SCALE_CLASS_ID:
			if (type == ColladaMaxAnimatable::SCALE)
			{
				PROCESS_KEYS(3, IBezScaleKey, BEZIER)
					interp = PatchBezierKey(k, Point3::Origin, Point3(0.333f, 0.333f, 0.333f), previousTime, nextTime);
					CREATEKEY;

					VAL(0, k.val.s.x); VAL(1, k.val.s.y); VAL(2, k.val.s.z);

					if (interp == FUDaeInterpolation::BEZIER)
					{
						float inInterval = (k.time - previousTime) * k.inLength.x;
						float outInterval = (nextTime - k.time) * k.outLength.x;
						INTAN(0, (k.time - inInterval) * timeFactor, k.val.s.x + k.intan.x * inInterval);
						OUTTAN(0, (k.time + outInterval) * timeFactor, k.val.s.x + k.outtan.x * outInterval);

						inInterval = (k.time - previousTime) * k.inLength.y;
						outInterval = (nextTime - k.time) * k.outLength.y;
						INTAN(1, (k.time - inInterval) * timeFactor, k.val.s.y + k.intan.y * inInterval);
						OUTTAN(1, (k.time + outInterval) * timeFactor, k.val.s.y + k.outtan.y * outInterval);

						inInterval = (k.time - previousTime) * k.inLength.z;
						outInterval = (nextTime - k.time) * k.outLength.z;
						INTAN(2, (k.time - inInterval) * timeFactor, k.val.s.z + k.intan.z * inInterval);
						OUTTAN(2, (k.time + outInterval) * timeFactor, k.val.s.z + k.outtan.z * outInterval);
					}
				PROCESS_KEYS_END
			}
			else if (type == ColladaMaxAnimatable::SCALE_ROT_AXIS)
			{
				PROCESS_KEYS(4, IBezScaleKey, BEZIER)
					interp = PatchBezierKey(k, Point3::Origin, Point3(0.333f, 0.333f, 0.333f), previousTime, nextTime);
					CREATEKEY;

					AngAxis aa(k.val.q);
					if (aa.axis.Equals(Point3::Origin, TOLERANCE)) aa.axis = Point3::XAxis;
					float degAngle = FMath::RadToDeg(aa.angle);
					VAL(0, aa.axis.x); VAL(1, aa.axis.y); VAL(2, aa.axis.z); VAL(3, FMath::RadToDeg(aa.angle));
					
					if (interp == FUDaeInterpolation::BEZIER)
					{
						float inInterval = (k.time - previousTime) * 0.333f;
						float outInterval = (nextTime - k.time) * 0.333f;
						INTAN(0, (k.time - inInterval) * timeFactor, aa.axis.x);
						OUTTAN(0, (k.time + outInterval) * timeFactor, aa.axis.x);
						INTAN(1, (k.time - inInterval) * timeFactor, aa.axis.y);
						OUTTAN(1, (k.time + outInterval) * timeFactor, aa.axis.y);
						INTAN(2, (k.time - inInterval) * timeFactor, aa.axis.z);
						OUTTAN(2, (k.time + outInterval) * timeFactor, aa.axis.z);
						INTAN(3, (k.time - inInterval) * timeFactor, degAngle);
						OUTTAN(3, (k.time + outInterval) * timeFactor, degAngle);
					}
				PROCESS_KEYS_END
			}
			else if (type == ColladaMaxAnimatable::SCALE_ROT_AXIS_R)
			{
				PROCESS_KEYS(4, IBezScaleKey, BEZIER)
					interp = PatchBezierKey(k, Point3::Origin, Point3(0.333f, 0.333f, 0.333f), previousTime, nextTime);
					CREATEKEY;

					AngAxis aa(k.val.q);
					if (aa.axis.Equals(Point3::Origin, TOLERANCE)) aa.axis = Point3::XAxis;
					float degAngle = -FMath::RadToDeg(aa.angle);
					VAL(0, aa.axis.x); VAL(1, aa.axis.y); VAL(2, aa.axis.z); VAL(3, degAngle);
					
					if (interp == FUDaeInterpolation::BEZIER)
					{
						float inInterval = (k.time - previousTime) * 0.333f;
						float outInterval = (nextTime - k.time) * 0.333f;
						INTAN(0, (k.time - inInterval) * timeFactor, aa.axis.x);
						OUTTAN(0, (k.time + outInterval) * timeFactor, aa.axis.x);
						INTAN(1, (k.time - inInterval) * timeFactor, aa.axis.y);
						OUTTAN(1, (k.time + outInterval) * timeFactor, aa.axis.y);
						INTAN(2, (k.time - inInterval) * timeFactor, aa.axis.z);
						OUTTAN(2, (k.time + outInterval) * timeFactor, aa.axis.z);
						INTAN(3, (k.time - inInterval) * timeFactor, degAngle);
						OUTTAN(3, (k.time + outInterval) * timeFactor, degAngle);
					}
				PROCESS_KEYS_END
			}
			break;

		case FLOATWAVE_CONTROL_CLASS_ID:
		default:
			isSampling = true;
			break;
		}
	}

#undef PROCESS_KEYS
#undef VAL
#undef INTAN
#undef OUTTAN
#undef TCBPARAM
#undef PROCESS_KEYS_END

	if (isSampling)
	{
		// Reserve the necessary memory and generate the key times
		int tpf = GetTicksPerFrame();
		TimeValue startTime = OPTS->AnimStart();
		TimeValue endTime = OPTS->AnimEnd() + 1;
		for (FCDAnimationCurveList::iterator it = curves.begin(); it != curves.end(); ++it)
		{
			FCDAnimationCurve* curve = (*it);
			for (int i = startTime; i < endTime; i += tpf)
			{
				FCDAnimationKey* key = curve->AddKey(FUDaeInterpolation::LINEAR);
				key->input = FSConvert::MaxTickToSecs(i);
			}
		}

#define PROCESS_KEYS(expectedDimensions, valueClassName) { \
			if (expectedDimensions > dimensions) break; \
			valueClassName value; \
			size_t i = 0; \
			for (TimeValue t = startTime; t < endTime; t += tpf, ++i) {  \
				controller->GetValue(t, &value, FOREVER, CTRL_ABSOLUTE);
#define VAL(dim, value) curves[dim]->GetKey(i)->output = value;
#define PROCESS_KEYS_END } } break;

		int sclassID = controller->SuperClassID();
		switch (sclassID)
		{
		case CTRL_FLOAT_CLASS_ID:
			PROCESS_KEYS(1, float)
				VAL(0, value);
			PROCESS_KEYS_END

		case CTRL_POINT3_CLASS_ID:
		case CTRL_POSITION_CLASS_ID:
			PROCESS_KEYS(3, Point3)
				VAL(0, value.x); VAL(1, value.y); VAL(2, value.z);
			PROCESS_KEYS_END;
		case CTRL_ROTATION_CLASS_ID :{
			float pval[3];
			PROCESS_KEYS(3, AngAxis)
				Quat q(value);
				float v[3];
				q.GetEuler(&v[0], &v[1], &v[2]);

				if (i > 0) FSConvert::PatchEuler(pval, v);
				pval[0] = v[0]; pval[1] = v[1]; pval[2] = v[2];

				VAL(0, v[0]); VAL(1, v[1]); VAL(2, v[2]);
			PROCESS_KEYS_END }

		case CTRL_SCALE_CLASS_ID:
			if (type == ColladaMaxAnimatable::SCALE)
			{
				PROCESS_KEYS(3, ScaleValue)
					VAL(0, value.s.x); VAL(1, value.s.y); VAL(2, value.s.z);
				PROCESS_KEYS_END
			}
			else if (type == ColladaMaxAnimatable::SCALE_ROT_AXIS)
			{
				PROCESS_KEYS(4, ScaleValue)
					AngAxis aa(value.q);
					if (aa.axis.Equals(Point3::Origin, TOLERANCE)) aa.axis = Point3::XAxis;
					VAL(0, aa.axis.x); VAL(1, aa.axis.y); VAL(2, aa.axis.z); VAL(3, FMath::RadToDeg(aa.angle));
				PROCESS_KEYS_END
			}
			else if (type == ColladaMaxAnimatable::SCALE_ROT_AXIS_R)
			{
				PROCESS_KEYS(4, ScaleValue)
					AngAxis aa(value.q);
					if (aa.axis.Equals(Point3::Origin, TOLERANCE)) aa.axis = Point3::XAxis;
					VAL(0, aa.axis.x); VAL(1, aa.axis.y); VAL(2, aa.axis.z); VAL(3, -FMath::RadToDeg(aa.angle));
				PROCESS_KEYS_END
			}
			break;
		default: break;
		}

#undef PROCESS_KEYS
#undef VAL
#undef PROCESS_KEYS_END

	}

	// Verify that the samples create an animation
	bool anyCurvesLeft = false;
	for (size_t i = 0; i < dimensions; ++i)
	{
		FCDAnimationCurve* curve = curves[i];
		size_t keyCount = curve->GetKeyCount();
		bool keepCurve = keyCount > 0;
		if (keepCurve && (isSampling || type == ColladaMaxAnimatable::SCALE_ROT_AXIS_R || type == ColladaMaxAnimatable::SCALE_ROT_AXIS))
		{
			keepCurve = false;
			float startValue = curve->GetKey(0)->output;
			for (size_t j = 1; j < keyCount && !keepCurve; ++j)
			{
				keepCurve |= startValue != curve->GetKey(j)->output;
			}
		}

		if (!keepCurve)
		{
			SAFE_RELEASE(curve);
			curves[i] = NULL;
		}
		else
		{
			if (conversion != NULL)
			{
				curve->ConvertValues(conversion, conversion);
			}
			if (isRelativeAnimation && i < maxInitialValues && !IsEquivalent(initialValue[i], 0.0f))
			{
				FCDConversionOffsetFunctor functor(-initialValue[i]);
				curve->ConvertValues(&functor, &functor);
			}
			anyCurvesLeft = true;
		}
	}

	return anyCurvesLeft;
}

/** IParamBlock ExportProperty methods */

void AnimationExporter::ExportProperty(const fm::string& id, IParamBlock* parameters, int parameterId, FCDParameterAnimatableFloat& colladaValue, FCDConversionFunctor* conversion)
{
	if (parameters == NULL) return;
	ParamType type = parameters->GetParameterType(parameterId); // support both int and floats here.
	if (type == TYPE_INT || 
		type == TYPE_TIMEVALUE || 
		type == TYPE_BOOL ||
		type == TYPE_INDEX) 
	{
		colladaValue = (float) parameters->GetInt(parameterId, TIME_EXPORT_START);
	}
	else if (type == TYPE_FLOAT || 
			 type == TYPE_PCNT_FRAC || 
			 type == TYPE_WORLD || 
			 type == TYPE_ANGLE ||
			 type == TYPE_COLOR_CHANNEL) 
	{
		colladaValue = parameters->GetFloat(parameterId, TIME_EXPORT_START);
	}
	else
	{
		FUAssert(type == TYPE_FLOAT,); // This will always fail
		colladaValue = 0.0f;
		return;
	}
	if (conversion != NULL) colladaValue = (*conversion)(colladaValue);

	ExportProperty(id, parameters, parameterId, colladaValue.GetAnimated(), conversion);
}

void AnimationExporter::ExportProperty(const fm::string& id, IParamBlock* parameters, int parameterId, FCDAnimated* colladaAnimated, FCDConversionFunctor* conversion)
{
	if (OPTS->ExportAnimations())
	{
		Control* controller = parameters->GetController((ParamID)parameterId);
		ExportAnimatedFloat(id, controller, colladaAnimated, conversion);
	}
}

void AnimationExporter::ExportProperty(const fm::string& id, IParamBlock* parameters, int parameterId, FCDParameterAnimatableVector3& colladaValue, FCDConversionFunctor* conversion)
{
	if (parameters == NULL) return;
	ParamType type = parameters->GetParameterType(parameterId);
	switch (type)
	{
	case TYPE_RGBA: {
		Color c = parameters->GetColor(parameterId, TIME_EXPORT_START);
		if (conversion != NULL)
		{
			c.r = (*conversion)(c.r);
			c.g = (*conversion)(c.g);
			c.b = (*conversion)(c.b);
		}
		colladaValue = ToFMVector3(c);
		break; }

	case TYPE_POINT3: {
		Point3 p = parameters->GetPoint3(parameterId, TIME_EXPORT_START);
		if (conversion != NULL)
		{
			p.x = (*conversion)(p.x);
			p.y = (*conversion)(p.y);
			p.z = (*conversion)(p.z);
		}
		colladaValue = ToFMVector3(p);
		break; }

	default:
		// should never happen
		FUFail(return);
	}

	if (OPTS->ExportAnimations())
	{
		Control* controller = parameters->GetController((ParamID)parameterId);
		ExportAnimatedPoint3(id, controller, colladaValue, -1, conversion);
	}
}

void AnimationExporter::ExportProperty(const fm::string& id, IParamBlock* parameters, int parameterId, FCDParameterAnimatableVector4& colladaValue, FCDConversionFunctor* conversion)
{
	if (parameters == NULL) return;
	colladaValue->w = 1.0f;
	ExportProperty(id, parameters, parameterId, (FCDParameterAnimatableVector3&) colladaValue, conversion);
}

/** Export named parameter, for cases where we are unsure of the structure of the parameter block */

void AnimationExporter::ExportProperty(const fm::string& id, IParamBlock2* parameters, TSTR name, int tabIndex, FCDParameterAnimatableFloat& colladaValue, FCDConversionFunctor* conversion)
{
	for (int i = 0; i < parameters->NumParams(); ++i)
	{
		ParamID parameterId = parameters->IndextoID(i);
		TSTR localName = parameters->GetLocalName(parameterId);
		if (localName == name)
		{
			ExportProperty(id, parameters, parameterId, tabIndex, colladaValue, conversion);
			return;
		}
	}
}


void AnimationExporter::ExportProperty(const fm::string& id, IParamBlock2* parameters, TSTR name, int tabIndex, FCDParameterAnimatableVector3& colladaValue, FCDConversionFunctor* conversion)
{
	for (int i = 0; i < parameters->NumParams(); ++i)
	{
		ParamID parameterId = parameters->IndextoID(i);
		if (parameters->GetLocalName(parameterId) == name)
		{
			ExportProperty(id, parameters, parameterId, tabIndex, colladaValue, conversion);
		}
	}
}


void AnimationExporter::ExportProperty(const fm::string& id, IParamBlock2* parameters, TSTR name, int tabIndex, FCDParameterAnimatableVector4& colladaValue, FCDConversionFunctor* conversion)
{
	for (int i = 0; i < parameters->NumParams(); ++i)
	{
		ParamID parameterId = parameters->IndextoID(i);
		TSTR localName = parameters->GetLocalName(parameterId);
		if (localName == name)
		{
			ExportProperty(id, parameters, parameterId, tabIndex, colladaValue, conversion);
			return;
		}
	}
}

/** IParamBlock2 ID-based methods */

void AnimationExporter::ExportProperty(const fm::string& id, IParamBlock2* parameters, int parameterId, int tabIndex, FCDParameterAnimatableFloat& colladaValue, FCDConversionFunctor* conversion)
{
	if (parameters == NULL) return;
	FUAssert(parameters->IDtoIndex(parameterId) >= 0, return);

	Interval dummy;
	ParamType2 type = parameters->GetParameterType(parameterId); // support both int and floats here.
	FUAssert(tabIndex == 0 || is_tab(type), tabIndex = 0); // Only allow access to proper whatsits here
	type = base_type(type);
	if (type == TYPE_INT || 
		type == TYPE_TIMEVALUE || 
		type == TYPE_BOOL ||
		type == TYPE_INDEX) 
	{
		colladaValue = (float) parameters->GetInt(parameterId, TIME_EXPORT_START, tabIndex);
	}
	else if (type == TYPE_FLOAT || 
			 type == TYPE_PCNT_FRAC || 
			 type == TYPE_WORLD || 
			 type == TYPE_ANGLE ||
			 type == TYPE_COLOR_CHANNEL) 
	{
		colladaValue = parameters->GetFloat(parameterId, TIME_EXPORT_START, tabIndex);
	}
	else
	{
#ifdef _DEBUG
		ParamDef& pd = parameters->GetParamDef(parameterId);
		pd;
#endif
		colladaValue = 0.0f;
		FUFail(return);
	}
	if (conversion != NULL) colladaValue = (*conversion)(colladaValue);
	if (OPTS->ExportAnimations())
	{
		Control* controller = parameters->GetController((ParamID)parameterId);
		ExportAnimatedFloat(id, controller, colladaValue, -1, conversion);
	}
}


void AnimationExporter::ExportProperty(const fm::string& id, IParamBlock2* parameters, int parameterId, int tabIndex, FCDParameterAnimatableVector3& colladaValue, FCDConversionFunctor* conversion)
{
	if (parameters == NULL) return;
	FUAssert(parameters->IDtoIndex(parameterId) >= 0, return);

	int type = parameters->GetParameterType(parameterId); // (ParamType2)
	int baseType = base_type(type);
	// If we have a tab index, make sure that our type was a TAB type.
	if (tabIndex != 0) FUAssert(baseType == type, tabIndex = 0);
	switch (baseType)
	{
	case TYPE_RGBA:
	case TYPE_FRGBA:
	case TYPE_POINT4: {
		Point4 c = parameters->GetPoint4(parameterId, TIME_EXPORT_START, tabIndex);
		if (conversion != NULL)
		{
			c.x = (*conversion)(c.x);
			c.y = (*conversion)(c.y);
			c.z = (*conversion)(c.z);
			c.w = (*conversion)(c.w);
		}
		colladaValue = ToFMVector3(c);
		break; }

	case TYPE_POINT3: {
		Point3 p = parameters->GetPoint3(parameterId, TIME_EXPORT_START, tabIndex);
		if (conversion != NULL)
		{
			p.x = (*conversion)(p.x);
			p.y = (*conversion)(p.y);
			p.z = (*conversion)(p.z);
		}
		colladaValue = ToFMVector3(p);
		break; }
	default:
		FUFail(return);
	}

	if (OPTS->ExportAnimations())
	{
		Control* controller = parameters->GetController((ParamID)parameterId);
		ExportAnimatedPoint3(id, controller, colladaValue, -1, conversion);
	}
}

void AnimationExporter::ExportProperty(const fm::string& id, IParamBlock2* parameters, int parameterId, int tabIndex, FCDParameterAnimatableVector4& colladaValue, FCDConversionFunctor* conversion)
{
	if (parameters == NULL) return;
	FUAssert(parameters->IDtoIndex(parameterId) >= 0, return);

	int type = parameters->GetParameterType(parameterId); // (ParamType2)
	int baseType = base_type(type);
	// If we have a tab index, make sure that our type was a TAB type.
	if (tabIndex != 0) FUAssert(baseType == type, tabIndex = 0);
	switch (baseType)
	{
	case TYPE_RGBA: {
		Color c = parameters->GetColor(parameterId, TIME_EXPORT_START, tabIndex);
		if (conversion != NULL)
		{
			c.r = (*conversion)(c.r);
			c.g = (*conversion)(c.g);
			c.b = (*conversion)(c.b);
		}
		colladaValue = ToFMVector4(c);
		break; }

	case TYPE_FRGBA: 
	case TYPE_POINT4: {
		Point4 c = parameters->GetPoint4(parameterId, TIME_EXPORT_START, tabIndex);
		if (conversion != NULL)
		{
			c.x = (*conversion)(c.x);
			c.y = (*conversion)(c.y);
			c.z = (*conversion)(c.z);
			c.w = (*conversion)(c.w);
		}
		colladaValue = ToFMVector4(c);
		break; }

	case TYPE_POINT3: {
		Point3 p = parameters->GetPoint3(parameterId, TIME_EXPORT_START, tabIndex);
		if (conversion != NULL)
		{
			p.x = (*conversion)(p.x);
			p.y = (*conversion)(p.y);
			p.z = (*conversion)(p.z);
		}
		colladaValue = ToFMVector4(p);
		break; }
	default:
#ifdef _DEBUG
		ParamDef& pd = parameters->GetParamDef(parameterId);
		pd = pd; // get rid of warning message
#endif
		FUFail(return);
	}

	if (OPTS->ExportAnimations())
	{
		
		Control* controller = parameters->GetController((ParamID)parameterId);
		if (baseType == TYPE_POINT3 || baseType == TYPE_RGBA)
			ExportAnimatedPoint3(id, controller, (FCDParameterAnimatableVector3&) colladaValue, -1, conversion);
		else
			ExportAnimatedPoint4(id, controller, colladaValue, -1, conversion);
	}
}
