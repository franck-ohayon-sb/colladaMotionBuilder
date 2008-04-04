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

// Animation Importer Class
// Creates controllers and curves for given COLLADA animations

#include "StdAfx.h"
#include "AnimationImporter.h"
#include "DocumentImporter.h"
#include "Common/ConversionFunctions.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDAnimationCurve.h"
#include "FCDocument/FCDAnimationCurveTools.h"
#include "FCDocument/FCDAnimationMultiCurve.h"
#include "FCDocument/FCDAnimationKey.h"
#include "FCDocument/FCDTransform.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDSceneNodeTools.h"
#include "FUtils/FUDaeEnum.h"
#include "AnimationImporter.hpp"
#include "decomp.h"

//
// AnimationImporter
//

AnimationImporter::AnimationImporter(DocumentImporter* _document)
{
	document = _document;
}

AnimationImporter::~AnimationImporter()
{
	document = NULL;
}

// Search for a property, given by name, on a given Animatable entity
IParamBlock2* AnimationImporter::FindProperty(Animatable* entity, const fm::string& propertyName, ParamID& pid)
{
	// Note that this code is adapted in the C++ exporter: ColladaExporter::FindParameter

	// Look in this Animatable object for the wanted property
	int blockCount = entity->NumParamBlocks();
	for (int i = 0; i < blockCount; ++i)
	{
		IParamBlock2* block = entity->GetParamBlock(i);
		if (block == NULL) continue;

		// Process each parameter, comparing their internal names with given property name
		int paramCount = block->NumParams();
		for (int j = 0; j < paramCount; ++j)
		{
			pid = block->IndextoID(j);
			ParamDef& d = block->GetParamDef(pid);
			if (propertyName == d.int_name) return block;
		}
	}
	return NULL;
}

// Search for a sub-animatable object, given by name, on a given Animatable entity
Animatable* AnimationImporter::FindAnimatable(Animatable* entity, const char* animName, int& subAnimIndex)
{
	fm::pvector<Animatable> queue; queue.reserve(6);
	queue.push_back(entity);
	while (!queue.empty())
	{
		Animatable* a = queue.back();
		queue.pop_back();

		// Look for the animatable object in the sub-anims
		int subAnimatableCount = a->NumSubs();
		for (subAnimIndex = 0; subAnimIndex < subAnimatableCount; ++subAnimIndex)
		{
			Animatable* b = a->SubAnim(subAnimIndex);
			if (b == NULL) continue;

			TSTR name = a->SubAnimName(subAnimIndex);
			if (IsEquivalent(animName, name.data())) return a;

			// Queue up the sub-animatable objects
			queue.push_back(b);
		}
	}
	return NULL;
}

// Returns whether a curve should be processed as a bezier curve
template <>
bool AnimationImporter::IsBezierCurve<FCDAnimationCurve>(const FCDAnimationCurve* colladaCurve)
{
	const FCDAnimationKey** keys = colladaCurve->GetKeys();
	size_t keyCount = colladaCurve->GetKeyCount();

	// The "step" interpolation is the default, but linear cannot approximate it well.
	// Only create linear controllers when all the keys have linear interpolation.
	bool isBezier = keyCount == 0;
	for (size_t i = 0; i < keyCount && !isBezier; ++i)
	{
		isBezier = keys[i]->interpolation != FUDaeInterpolation::LINEAR;
	}
	return isBezier;
}

template <>
bool AnimationImporter::IsBezierCurve<FCDAnimationMultiCurve>(const FCDAnimationMultiCurve* colladaCurve)
{
	const FCDAnimationMKey** keys = colladaCurve->GetKeys();
	size_t keyCount = colladaCurve->GetKeyCount();

	// The "step" interpolation is the default, but linear cannot approximate it well.
	// Only create linear controllers when all the keys have linear interpolation.
	bool isBezier = keyCount == 0;
	for (size_t i = 0; i < keyCount && !isBezier; ++i)
	{
		isBezier = keys[i]->interpolation != FUDaeInterpolation::LINEAR;
	}
	return isBezier;
}

template <class FCDAnimationXCurve>
IKeyControl* AnimationImporter::CreateKeyController(Animatable* entity, const char* propertyName, SClass_ID sclassID, const Class_ID& classID, FCDAnimationXCurve* curve)
{
	if (propertyName == NULL) return NULL;

	// Search for the corresponding Max animatable object
	int pid;
	entity = FindAnimatable(entity, propertyName, pid);
	if (entity == NULL) return NULL; // Programming error!

	return CreateKeyController(entity, pid, sclassID, classID, curve);
}

template <class FCDAnimationXCurve>
IKeyControl* AnimationImporter::CreateKeyController(Animatable* entity, uint32 subAnimNum, SClass_ID sclassID, const Class_ID& classID, FCDAnimationXCurve* curve)
{
	if (entity->NumSubs() > (int) subAnimNum && entity->SubAnim((int) subAnimNum) != NULL)
	{
		SClass_ID id = entity->SubAnim(subAnimNum)->SuperClassID();
		Class_ID cid = entity->SubAnim(subAnimNum)->ClassID();
		BOOL canOverwrite = (cid == Class_ID(DUMMYCHANNEL_CLASS_ID, 0) &&
							 id == CTRL_USERTYPE_CLASS_ID) || id == sclassID;
		FUAssert(canOverwrite, return NULL);
	}
	// Create the controller
	Control* controller = (Control*) document->MaxCreate(sclassID, classID);
	if (controller == NULL) return NULL;
	IKeyControl* keyController = GetKeyControlInterface(controller);
	if (keyController == NULL) return NULL; // Programming error!

	// Assign the controller to the animatable entity.
	if (entity->AssignController(controller, subAnimNum) != TRUE) return NULL;

	// Set the out-of-range types.
	controller->SetORT(ToORT(curve->GetPreInfinity()), ORT_BEFORE);
	controller->SetORT(ToORT(curve->GetPostInfinity()), ORT_AFTER);

	return keyController;
}

// Imports into Max a given COLLADA animation curve for a simple float property
void AnimationImporter::ImportAnimatedFloat(Animatable* entity, const char* propertyName, const FCDParameterAnimatableFloat& colladaValue, FCDConversionFunctor* conversion, bool collapse)
{
	if (propertyName == NULL) return;
	if (!colladaValue.IsAnimated()) return;

	// Search for the corresponding Max animatable object
	int pid;
	entity = FindAnimatable(entity, propertyName, pid);
	if (entity != NULL)
	{
		FCDAnimated* colladaAnimated = const_cast<FCDAnimated*>(colladaValue.GetAnimated());
		ImportAnimatedFloat(entity, pid, colladaAnimated, conversion, collapse);
	}
}

void AnimationImporter::ImportAnimatedFloat(Animatable* entity, const char* propertyName, const FCDAnimated* colladaAnimated, FCDConversionFunctor* conversion, bool collapse)
{
	if (propertyName == NULL) return;
	if (!colladaAnimated->HasCurve()) return;

	// Search for the corresponding Max animatable object
	int pid;
	entity = FindAnimatable(entity, propertyName, pid);
	if (entity != NULL)
	{
		ImportAnimatedFloat(entity, pid, const_cast<FCDAnimated*>(colladaAnimated), conversion, collapse);
	}
}

void AnimationImporter::ImportAnimatedFloat(Animatable* entity, const char* propertyName, const FCDAnimationCurve* colladaCurve, FCDConversionFunctor* conversion)
{
	if (propertyName == NULL || colladaCurve == NULL) return;

	// Search for the corresponding Max animatable object
	int pid;
	entity = FindAnimatable(entity, propertyName, pid);
	if (entity != NULL)
	{
		ImportAnimatedFloat(entity, pid, const_cast<FCDAnimationCurve*>(colladaCurve), conversion);
	}
}

void AnimationImporter::ImportAnimatedFloat(IParamBlock2* entity, uint32 pid, const FCDParameterAnimatableFloat& colladaValue, FCDConversionFunctor* conversion, bool collapse)
{
	if (!colladaValue.IsAnimated()) return;
	int subAnimNum = entity->GetAnimNum(pid);
	FCDAnimated* colladaAnimated = const_cast<FCDAnimated*>(colladaValue.GetAnimated());
	ImportAnimatedFloat((Animatable*)entity, subAnimNum, colladaAnimated, conversion, collapse);
}

void AnimationImporter::ImportAnimatedFloat(IParamBlock2* entity, uint32 pid, FCDAnimated* colladaAnimated, FCDConversionFunctor* conversion, bool collapse)
{
	int subAnimNum = entity->GetAnimNum(pid);
	ImportAnimatedFloat((Animatable*)entity, subAnimNum, colladaAnimated, conversion, collapse);
}

void AnimationImporter::ImportAnimatedFloat(Animatable* entity, uint32 subAnimId, FCDAnimated* colladaAnimated, FCDConversionFunctor* conversion, bool collapse)
{
	FCDAnimationCurve* curve = NULL;
	FUObjectRef<FCDAnimationCurve> ownedCurve;
	if (!collapse || colladaAnimated->GetValueCount() < 2)
	{
		// Look for a valid linked COLLADA curves
		// There should only be zero or one, or another import function should have been used
		curve = colladaAnimated->GetCurve(0);
		if (curve == NULL || curve->GetKeyCount() < 2) return; // Early exit for unanimated targets
	}
	else
	{
		// Collapse the multi-dimensional curve for this COLLADA animated value into a one-dimensional curve
		FUObjectRef<FCDAnimationMultiCurve> multiCurve = colladaAnimated->CreateMultiCurve();
		if (multiCurve != NULL)
		{
			ownedCurve = FCDAnimationCurveTools::Collapse(multiCurve);
			curve = ownedCurve;
		}
	}

	ImportAnimatedFloat(entity, subAnimId, curve, conversion);
}

void AnimationImporter::ImportAnimatedFloat(Animatable* entity, uint32 subAnimId, FCDAnimationCurve* colladaCurve, FCDConversionFunctor* conversion)
{
	if (colladaCurve == NULL) return;

	// Convert the values
	if (conversion != NULL)
	{
		colladaCurve->ConvertValues(conversion, conversion);
	}

	// Create the correct controller type: bezier or linear
	bool isBezier = IsBezierCurve(colladaCurve);
	Class_ID controllerClassID(isBezier ? HYBRIDINTERP_FLOAT_CLASS_ID : LININTERP_FLOAT_CLASS_ID, 0);
	IKeyControl* controller = CreateKeyController(entity, subAnimId, CTRL_FLOAT_CLASS_ID, controllerClassID, colladaCurve);
	if (controller == NULL) return;

	FillController<FCDAnimationCurve, 1>(controller, colladaCurve, isBezier);
}

void AnimationImporter::FillFloatController(Control* controller, const FCDParameterAnimatableFloat& colladaValue, bool isBezier)
{
	if (colladaValue.IsAnimated())
	{
		// Retrieve the key controller.
		IKeyControl* keyController = GetKeyControlInterface(controller);
		if (keyController == NULL) return;

		// Retrieve the COLLADA curve.
		const FCDAnimated* animatedValue = colladaValue.GetAnimated();
		if (animatedValue->GetValueCount() < 1) return;
		const FCDAnimationCurve* curve = animatedValue->GetCurve(0);
		
		FillController<FCDAnimationCurve, 1>(keyController, curve, isBezier);
	}
}

// Imports into Max the 3d COLLADA animation curve for a color/point3 value
void AnimationImporter::ImportAnimatedPoint4(Animatable* entity, const char* propertyName, const FCDParameterAnimatableVector4& colladaValue, FCDConversionFunctor* conversion)
{
	if (propertyName == NULL) return;
	if (!colladaValue.IsAnimated()) return;

	// Search for the corresponding Max animatable object
	int pid;
	entity = FindAnimatable(entity, propertyName, pid);
	if (entity != NULL)
	{
		ImportAnimatedPoint4(entity, pid, colladaValue, conversion);
	}
}

void AnimationImporter::ImportAnimatedPoint4(Animatable* entity, uint32 pid, const FCDParameterAnimatableVector4& colladaValue, FCDConversionFunctor* conversion)
{
	if (!colladaValue.IsAnimated()) return;

    // Search for a COLLADA animated value for the given COLLADA value
	const FCDAnimated* colladaAnimated = colladaValue.GetAnimated();
	if (colladaAnimated == NULL || !colladaAnimated->HasCurve() || colladaAnimated->GetValueCount() < 3) return;
	if (colladaAnimated->GetValueCount() == 3)
	{
		ImportAnimatedPoint3(entity, pid, (const FCDParameterAnimatableVector3&) colladaValue, conversion);
		return;
	}

	// Create the 3d COLLADA multi-curve.
	FUObjectRef<FCDAnimationMultiCurve> curve = colladaAnimated->CreateMultiCurve();
	if (curve == NULL) return;

	// Create the controller: Point4 doesn't have a linear controller.
	Class_ID controllerClassID(HYBRIDINTERP_POINT4_CLASS_ID, 0);
	IKeyControl* controller = CreateKeyController(entity, pid, CTRL_POINT4_CLASS_ID, controllerClassID, (FCDAnimationMultiCurve*) curve);
	if (controller == NULL) return;

	// Fill in the controller with the curve's data.
	FillController<FCDAnimationMultiCurve, 4>(controller, curve, true);
}

void AnimationImporter::ImportAnimatedFRGBA(Animatable* entity, uint32 pid, const FCDParameterAnimatableColor4& colladaValue, FCDConversionFunctor* conversion)
{
	if (!colladaValue.IsAnimated()) return;

    // Search for a COLLADA animated value for the given COLLADA value.
	const FCDAnimated* colladaAnimated = colladaValue.GetAnimated();
	if (colladaAnimated->GetValueCount() < 3) return;

	ImportAnimatedFRGBA(entity, pid, colladaAnimated, conversion);
}

void AnimationImporter::ImportAnimatedFRGBA(IParamBlock2* entity, uint32 pid, const FCDAnimated* colladaAnimated, FCDConversionFunctor* conversion)
{
	int subAnimNum = entity->GetAnimNum(pid);
	ImportAnimatedFRGBA((Animatable*) entity, subAnimNum, colladaAnimated, conversion);
}

void AnimationImporter::ImportAnimatedFRGBA(Animatable* entity, uint32 pid, const FCDAnimated* colladaAnimated, FCDConversionFunctor* UNUSED(conversion))
{
	// Create the 3d COLLADA multi-curve.
	FUObjectRef<FCDAnimationMultiCurve> curve = colladaAnimated->CreateMultiCurve();
	if (curve == NULL) return;

	// Create the controller: Point4 doesn't have a linear controller.
	Class_ID controllerClassID(HYBRIDINTERP_COLOR_CLASS_ID, 0);
	IKeyControl* controller = CreateKeyController(entity, pid, CTRL_POINT3_CLASS_ID, controllerClassID, (FCDAnimationMultiCurve*) curve);
	if (controller == NULL) return;

	// Fill in the controller with the curve's data
	FillController<FCDAnimationMultiCurve, 3>(controller, curve, true);
}

void AnimationImporter::ImportAnimatedPoint3(Animatable* entity, const char* propertyName, const FCDParameterAnimatableVector3& colladaValue, FCDConversionFunctor* conversion)
{
	if (propertyName == NULL) return;
	if (!colladaValue.IsAnimated()) return;

	// Search for the corresponding Max animatable object
	int pid;
	entity = FindAnimatable(entity, propertyName, pid);
	if (entity != NULL)
	{
		ImportAnimatedPoint3(entity, pid, colladaValue, conversion);
	}
}

void AnimationImporter::ImportAnimatedPoint3(Animatable* entity, uint32 pid, const FCDParameterAnimatableVector3& colladaValue, FCDConversionFunctor* UNUSED(conversion))
{
	if (!colladaValue.IsAnimated()) return;

	// Search for a COLLADA animated value for the given COLLADA value
	const FCDAnimated* colladaAnimated = colladaValue.GetAnimated();
	if (colladaAnimated == NULL || !colladaAnimated->HasCurve() || colladaAnimated->GetValueCount() < 3) return;

	// Create the 3d COLLADA multi-curve.
	FUObjectRef<FCDAnimationMultiCurve> curve = colladaAnimated->CreateMultiCurve();
	if (curve == NULL) return;

	// Create the controller: Point3 doesn't have a linear controller
	Class_ID controllerClassID(HYBRIDINTERP_POINT3_CLASS_ID, 0);
	IKeyControl* controller = CreateKeyController(entity, pid, CTRL_POINT3_CLASS_ID, controllerClassID, (FCDAnimationMultiCurve*) curve);
	if (controller == NULL) return;

	// Fill in the controller with the curve's data
	FillController<FCDAnimationMultiCurve, 3>(controller, curve, true);
}

void AnimationImporter::ImportAnimatedPoint3(MasterPointControl* masterController, int subControllerIndex, const FCDAnimationMultiCurve* curve, FCDConversionFunctor* UNUSED(conversion))
{
	if (curve == NULL || masterController == NULL || masterController->GetNumSubControllers() <= subControllerIndex) return;

	// Create the controller: Point3 doesn't have a linear controller
	Class_ID controllerClassID(HYBRIDINTERP_POINT3_CLASS_ID, 0);
	Control* reference = (Control*) document->MaxCreate(CTRL_POINT3_CLASS_ID, controllerClassID);
	if (reference == NULL) return;
	IKeyControl* controller = GetKeyControlInterface(reference);
	if (controller == NULL) return; // Programming error!
	masterController->SetSubController(subControllerIndex, reference);

	FillController<FCDAnimationMultiCurve, 3>(controller, curve, true);
}

// Imports into Max the 3-dimensional translation controller from the COLLADA animation curves
void AnimationImporter::ImportAnimatedTranslation(Control* tmController, const FCDTTranslation* translation)
{
	if (translation == NULL || tmController == NULL || !translation->IsAnimated()) return;

	// Generate the 3D multi-curve by merging the translation's COLLADA curves directly
	const FCDAnimated* animated = translation->GetAnimated();
	if (animated->GetValueCount() < 3) return;

	// Retrieve the position controller.
	Control* positionController = tmController->GetPositionController();
	FUAssert(positionController != NULL, return);

	// Process the components individually to avoid generating unnecessary keys.
	for (size_t i = 0; i < 3; ++i)
	{
		const FCDAnimationCurve* curve = animated->GetCurve(i);
		if (curve == NULL) continue;

		// Create the correct controller type: bezier or linear
		bool isBezier = IsBezierCurve((FCDAnimationMultiCurve*) curve);
		Class_ID controllerClassID(isBezier ? HYBRIDINTERP_FLOAT_CLASS_ID : LININTERP_FLOAT_CLASS_ID, 0);
		IKeyControl* controller = CreateKeyController(positionController, (uint32) i, CTRL_FLOAT_CLASS_ID, controllerClassID, curve);
		if (controller == NULL) return;

		// Fill in the controller with the curve's data
		FillController<FCDAnimationCurve, 1>(controller, curve, isBezier);
	}
}

void AnimationImporter::ImportAnimatedAxisRotation(Animatable* entity, const fm::string& propertyName, const FCDTRotation* angleAxisRotation)
{
	if (entity == NULL) return;
	if (!angleAxisRotation->IsAnimated()) return;

	// Generate the 4D multi-curve directly from the COLLADA animated rotation curves
	const FCDAnimated* animated = angleAxisRotation->GetAnimated();
	if (animated == NULL || !animated->HasCurve() || animated->GetValueCount() < 4) return;
	FUObjectRef<FCDAnimationMultiCurve> curve = animated->CreateMultiCurve();

	// Create the correct controller type: bezier or linear
	bool isBezier = IsBezierCurve((FCDAnimationMultiCurve*) curve);
	Class_ID controllerClassID(isBezier ? HYBRIDINTERP_ROTATION_CLASS_ID : LININTERP_ROTATION_CLASS_ID, 0);
	IKeyControl* controller = CreateKeyController(entity, propertyName.c_str(), CTRL_ROTATION_CLASS_ID, controllerClassID, (FCDAnimationMultiCurve*) curve);
	if (controller == NULL) return;

	// Fill in the keys
	size_t keyCount = curve->GetKeyCount();
	FCDAnimationMKey** keys = curve->GetKeys();
	controller->SetNumKeys((int) keyCount);
	float timeFactor = GetTimeFactor();

	if (isBezier)
	{
		// Because Max does not store the instance we create, we can use same
		// created key for every SetKey call
		Quat lastQuat;
		IBezQuatKey key;

		// Initialize our variables, as these do not change
		SetInTanType(key.flags, BEZKEY_SMOOTH);
		SetOutTanType(key.flags, BEZKEY_SMOOTH);

		for (size_t i = 0; i < keyCount; ++i)
		{
			// Create each bezier key independently.
			key.time = keys[i]->input * timeFactor;
			Matrix3 mx(TRUE); mx.SetAngleAxis(Point3(keys[i]->output[0], keys[i]->output[1], keys[i]->output[2]), FMath::DegToRad(keys[i]->output[3]));
			key.val.Set(mx);
			if (i > 0)
			{
				key.val.MakeClosest(lastQuat);
			}

			// No tangents for this type of key
			controller->SetKey((int) i, &key);
			lastQuat.Set(key.val);
		}
	}
	else
	{
		for (uint32 i = 0; i < keyCount; ++i)
		{
			// Create each linear key independently.
			ILinRotKey key;
			key.time = keys[i]->input * timeFactor;
			key.val.Set(AngAxis(keys[i]->output[0], keys[i]->output[1], keys[i]->output[2], FMath::DegToRad(keys[i]->output[3])));
			controller->SetKey((int) i, &key);
		}
	}
	controller->SortKeys();
}

void AnimationImporter::ImportAnimatedEulerRotation(Animatable* entity, const fm::string& propertyName, const FCDTRotation* xRotation, const FCDTRotation* yRotation, const FCDTRotation* zRotation)
{
	if (entity == NULL) return;

	// Find the animatable property
	int subAnimIndex;
	Animatable* subEntity = FindAnimatable(entity, propertyName.c_str(), subAnimIndex);
	Animatable* rotationEntity = subEntity->SubAnim(subAnimIndex);

	// Import each of the three euler angles independently
	if (xRotation != NULL && xRotation->GetAngleAxis().IsAnimated()) ImportAnimatedFloat(rotationEntity, "X Rotation", xRotation->GetAngleAxis().GetAnimated()->GetCurve(3), &FSConvert::DegToRad);
	if (yRotation != NULL && yRotation->GetAngleAxis().IsAnimated()) ImportAnimatedFloat(rotationEntity, "Y Rotation", yRotation->GetAngleAxis().GetAnimated()->GetCurve(3), &FSConvert::DegToRad);
	if (zRotation != NULL && zRotation->GetAngleAxis().IsAnimated()) ImportAnimatedFloat(rotationEntity, "Z Rotation", zRotation->GetAngleAxis().GetAnimated()->GetCurve(3), &FSConvert::DegToRad);
}

// Imports a scale factor with axis-rotation animation into Max
void AnimationImporter::ImportAnimatedScale(Animatable* entity, const fm::string& propertyName, const FCDTScale* scale, const FCDTRotation* scaleAxisRotation)
{
	if (entity == NULL || scale == NULL) return;

	// Gather up the COLLADA animation curves and their default values, they are necessary for the merge
	// Important: the default value for the scale rotation axis must be a normalized vector,
	// so set the default value for the x-component to 1.0f
	FloatList defaultValues(7, 0.0f);
	defaultValues[4] = 1.0f;
	FCDAnimationCurveConstList toMerge(7);
	bool isAnimated = false;
	if (scale->IsAnimated())
	{
		const FCDAnimated* animated = scale->GetAnimated();
		for (uint32 i = 0; i < 3; ++i)
		{
			toMerge[i] = animated->GetCurve(i);
			isAnimated |= toMerge[i] != NULL;
			defaultValues[i] = (*scale->GetScale())[i];
		}
	}
	if (scaleAxisRotation != NULL && scaleAxisRotation->IsAnimated())
	{
		const FCDAnimated* animated = scaleAxisRotation->GetAnimated();
		for (uint32 i = 0; i < 4; ++i)
		{
			toMerge[i] = animated->GetCurve(i);
			isAnimated |= toMerge[i] != NULL;
			defaultValues[i] = (i != 3) ? scaleAxisRotation->GetAxis()[i] : scaleAxisRotation->GetAngle();
		}
	}
	if (!isAnimated) return;

	// Create a 7D multi-curve by merging the individual COLLADA animation curves
	// This is done to ensure that all the individual curves have the same time input.
	FUObjectRef<FCDAnimationMultiCurve> curve = FCDAnimationCurveTools::MergeCurves(toMerge, defaultValues);
	if (curve == NULL || curve->GetDimension() < 7) return;

	// Create the keyframe controller for the scale animation
	bool isBezier = IsBezierCurve((FCDAnimationMultiCurve*) curve);
	Class_ID controllerClassID(isBezier ? HYBRIDINTERP_SCALE_CLASS_ID : LININTERP_SCALE_CLASS_ID, 0);
	IKeyControl* controller = CreateKeyController(entity, propertyName.c_str(), CTRL_SCALE_CLASS_ID, controllerClassID, (FCDAnimationMultiCurve*) curve);
	if (controller == NULL) return;

	// Fill in the keys
	size_t keyCount = curve->GetKeyCount();
	FCDAnimationMKey** keys = curve->GetKeys();
	controller->SetNumKeys((int) keyCount);
	float timeFactor = GetTimeFactor();
	if (isBezier)
	{
		for (size_t i = 0; i < keyCount; ++i)
		{
			IBezScaleKey key;
			key.time = keys[i]->input * timeFactor;
			key.val.s = Point3(keys[i]->output[0], keys[i]->output[1], keys[i]->output[2]);
			AngAxis aa(keys[i]->output[3], keys[i]->output[4], keys[i]->output[5], -1.0f * FMath::DegToRad(keys[i]->output[6]));
			key.val.q.Set(aa);
			SetInTanType(key.flags, ConvertInterpolation(keys[i > 0 ? i-1 : i]->interpolation));
			SetOutTanType(key.flags, ConvertInterpolation(keys[i]->interpolation));

			if (keys[i]->interpolation == FUDaeInterpolation::BEZIER)
			{
				FCDAnimationMKeyBezier* bkey = (FCDAnimationMKeyBezier*) keys[i];
				float previousSpan = bkey->input - (i > 0 ? keys[i-1]->input : (bkey->input - FLT_TOLERANCE));
				float nextSpan = (i < keyCount - 1 ? keys[i+1]->input : (bkey->input + FLT_TOLERANCE)) - bkey->input;

				// Calculate the in-tangent
				key.inLength = Point3((bkey->input - bkey->inTangent[0].x) / previousSpan, (bkey->input - bkey->inTangent[1].x) / previousSpan, (bkey->input - bkey->inTangent[2].x) / previousSpan);
				if (key.inLength.x < FLT_TOLERANCE && key.inLength.x > -FLT_TOLERANCE) key.inLength.x = FMath::Sign(key.inLength.x) * FLT_TOLERANCE;
				if (key.inLength.y < FLT_TOLERANCE && key.inLength.y > -FLT_TOLERANCE) key.inLength.y = FMath::Sign(key.inLength.y) * FLT_TOLERANCE;
				if (key.inLength.z < FLT_TOLERANCE && key.inLength.z > -FLT_TOLERANCE) key.inLength.z = FMath::Sign(key.inLength.z) * FLT_TOLERANCE;
				key.intan = Point3((bkey->inTangent[0].y - bkey->output[0]) / (key.inLength.x * previousSpan * timeFactor), (bkey->inTangent[1].y - bkey->output[1]) / (key.inLength.y * previousSpan * timeFactor), (bkey->inTangent[2].y - bkey->output[2]) / (key.inLength.z * previousSpan * timeFactor));

				// Calculate the out-tangent
				key.outLength = Point3((bkey->outTangent[0].x - bkey->input) / nextSpan, (bkey->outTangent[1].x - bkey->input) / nextSpan, (bkey->outTangent[2].x - bkey->input) / nextSpan);
				if (key.outLength.x < FLT_TOLERANCE && key.outLength.x > -FLT_TOLERANCE) key.outLength.x = FMath::Sign(key.outLength.x) * FLT_TOLERANCE;
				if (key.outLength.y < FLT_TOLERANCE && key.outLength.y > -FLT_TOLERANCE) key.outLength.y = FMath::Sign(key.outLength.y) * FLT_TOLERANCE;
				if (key.outLength.z < FLT_TOLERANCE && key.outLength.z > -FLT_TOLERANCE) key.outLength.z = FMath::Sign(key.outLength.z) * FLT_TOLERANCE;
				key.outtan = Point3((bkey->outTangent[0].y - bkey->output[0]) / (key.outLength.x * nextSpan * timeFactor), (bkey->outTangent[1].y - bkey->output[1]) / (key.outLength.y * nextSpan * timeFactor), (bkey->outTangent[2].y - bkey->output[2]) / (key.outLength.z * nextSpan * timeFactor));
			}
			else
			{
				key.intan = key.outtan = Point3::Origin;
				key.outLength = key.inLength = Point3(0.333f, 0.333f, 0.333f);
			}

			// In some rare cases, only half the key is set by the first call, so call the SetKey function twice.
			controller->SetKey((int) i, &key);
			controller->SetKey((int) i, &key);
		}
	}
	else
	{
		for (uint32 i = 0; i < keyCount; ++i)
		{
			// Create the linear scale key
			ILinScaleKey key;
			key.time = keys[i]->input * timeFactor;
			key.val.s = Point3(keys[i]->output[0], keys[i]->output[1], keys[i]->output[2]);
			AngAxis aa(keys[i]->output[3], keys[i]->output[4], keys[i]->output[5], -1.0f * FMath::DegToRad(keys[i]->output[6]));
			key.val.q.Set(aa);
			controller->SetKey((int) i, &key);
		}
	}

	controller->SortKeys();
}

// Imports a Max scene node transform, using sampling of the COLLADA scene node's transforms
void AnimationImporter::ImportAnimatedSceneNode(Animatable* tmController, FCDSceneNode* colladaSceneNode)
{
	FCDAnimatedList animateds;

	FUAssert(tmController != NULL && tmController->SuperClassID() == CTRL_MATRIX3_CLASS_ID, return);
	Control *tmControl = (Control *)tmController;
	
	// Collect all the animation curves
	size_t transformCount = colladaSceneNode->GetTransformCount();
	for (size_t t = 0; t < transformCount; ++t)
	{
		FCDAnimated* animated = colladaSceneNode->GetTransform(t)->GetAnimated();
		if (animated != NULL && animated->HasCurve()) animateds.push_back(animated);
	}
	if (animateds.empty()) return;

	// Make a list of the ordered key times to sample
	FCDSceneNodeTools::GenerateSampledAnimation(colladaSceneNode);
	const FloatList& sampleKeys = FCDSceneNodeTools::GetSampledAnimationKeys();
	const FMMatrix44List& sampleValues = FCDSceneNodeTools::GetSampledAnimationMatrices();
	
	size_t sampleKeyCount = sampleKeys.size();
	if (sampleKeyCount == 0) return;

	// Pre-allocate the key arrays;
	size_t keyCount = sampleKeys.size();
	
	// Sample the scene node transform
	float timeFactor = GetTimeFactor();

	SuspendAnimate();
	AnimateOn();
	Matrix3 transform, lastTransform;
	for (size_t i = 0; i < keyCount; ++i)
	{
		TimeValue t = sampleKeys[i] * timeFactor;
		Matrix3 transform = ToMatrix3(sampleValues[i]);
		SetXFormPacket val(transform);

		// Let Max handle decoding the matrix into parts,
		// it actually does quite a better job than we do.
		tmControl->SetValue(t, &val, 1, CTRL_RELATIVE);
	}
	ResumeAnimate();
}

