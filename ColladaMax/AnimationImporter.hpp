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

//
// Static Macros
//

inline int ConvertInterpolation(uint32 interpolation)
{
	switch (interpolation)
	{
	case FUDaeInterpolation::STEP: return BEZKEY_STEP;
	case FUDaeInterpolation::LINEAR: return BEZKEY_LINEAR;
	default: return BEZKEY_USER;
	}
}

static int ToORT(FUDaeInfinity::Infinity infinity)
{
	switch (infinity)
	{
	case FUDaeInfinity::CONSTANT: return ORT_CONSTANT;
	case FUDaeInfinity::CYCLE: return ORT_CYCLE;
	case FUDaeInfinity::CYCLE_RELATIVE: return ORT_RELATIVE_REPEAT;
	case FUDaeInfinity::OSCILLATE: return ORT_OSCILLATE;
	case FUDaeInfinity::LINEAR: return ORT_LINEAR;
	default: return ORT_CONSTANT;
	}
}

//
// AnimationImporter
//

//template <class FCDAnimationXCurve, int Dimensions>
//void AnimationImporter::FillController(IKeyControl* controller, const FCDAnimationXCurve* curve, bool isBezier)
//{
//	FUBreak;
//}
//
template <>
void AnimationImporter::FillController<FCDAnimationCurve, 1>(IKeyControl* controller, const FCDAnimationCurve* curve, bool isBezier)
{
	if (curve == NULL || controller == NULL) return;

	// Fill in the controller with the animation data from the curve
	float timeFactor = GetTimeFactor();
	size_t keyCount = curve->GetKeyCount();
	controller->SetNumKeys((int) keyCount);

	const FCDAnimationKey** keys = curve->GetKeys();
	if (isBezier)
	{
		for (size_t i = 0; i < keyCount; ++i)
		{
			// Create each bezier key independently.
			IBezFloatKey key;
			key.time = keys[i]->input * timeFactor;
			key.val = keys[i]->output;
			SetInTanType(key.flags, ConvertInterpolation(keys[i > 0 ? i-1 : i]->interpolation));
			SetOutTanType(key.flags, ConvertInterpolation(keys[i]->interpolation));

			if (keys[i]->interpolation == FUDaeInterpolation::BEZIER)
			{
				FCDAnimationKeyBezier* bkey = (FCDAnimationKeyBezier*) keys[i];
				float previousSpan = keys[i]->input - (i > 0 ? keys[i-1]->input : (keys[i]->input - FLT_TOLERANCE));
				float nextSpan = (i < keyCount - 1 ? keys[i+1]->input : (keys[i]->input + FLT_TOLERANCE)) - keys[i]->input;

				// Calculate the in-tangent
				key.inLength = (bkey->input - bkey->inTangent.x) / previousSpan;
				if (key.inLength < FLT_TOLERANCE && key.inLength > -FLT_TOLERANCE) key.inLength = FMath::Sign(key.inLength) * FLT_TOLERANCE;
				key.intan = (bkey->inTangent.y - key.val) / (key.inLength * previousSpan * timeFactor);

				// Calculate the out-tangent
				key.outLength = (bkey->outTangent.x - bkey->input) / nextSpan;
				if (key.outLength < FLT_TOLERANCE && key.outLength > -FLT_TOLERANCE) key.outLength = FMath::Sign(key.outLength) * FLT_TOLERANCE;
				key.outtan = (bkey->outTangent.y - key.val) / (key.outLength * nextSpan * timeFactor);
			}
			else
			{
				key.intan = key.outtan = 0.0f;
				key.outLength = key.inLength = 0.333f;
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
			// Create the linear keys
			ILinFloatKey key;
			key.time = keys[i]->input * timeFactor;
			key.val = keys[i]->output;

			controller->SetKey((int) i, &key);
		}
	}
	controller->SortKeys();
}

// Fills in a Point3 controller with the key data from a given COLLADA animation curve
template <>
void AnimationImporter::FillController<FCDAnimationMultiCurve, 3>(IKeyControl* controller, const FCDAnimationMultiCurve* colladaCurve, bool isBezier)
{
	// Pre-calculate some import conversion factors
	float timeFactor = GetTimeFactor();

	// Fill in the keys
	size_t keyCount = colladaCurve->GetKeyCount();
	const FCDAnimationMKey** keys = colladaCurve->GetKeys();
	controller->SetNumKeys((int) keyCount);
	if (isBezier)
	{
		for (size_t i = 0; i < keyCount; ++i)
		{
			IBezPoint3Key key;
			key.time = keys[i]->input * timeFactor;
			key.val = Point3(keys[i]->output[0], keys[i]->output[1], keys[i]->output[2]);
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
			// Create each linear key independently.
			ILinPoint3Key key;
			key.time = keys[i]->input * timeFactor;
			key.val = Point3(keys[i]->output[0], keys[i]->output[1], keys[i]->output[2]);
			controller->SetKey((int) i, &key);
		}
	}
	controller->SortKeys();
}

template <>
void AnimationImporter::FillController<FCDAnimationMultiCurve, 4>(IKeyControl* controller, const FCDAnimationMultiCurve* colladaCurve, bool isBezier)
{
	// Pre-calculate some import conversion factors
	float timeFactor = GetTimeFactor();

	// Fill in the keys
	size_t keyCount = colladaCurve->GetKeyCount();
	const FCDAnimationMKey** keys = colladaCurve->GetKeys();
	controller->SetNumKeys((int) keyCount);

	if (isBezier)
	{
		for (size_t i = 0; i < keyCount; ++i)
		{
			IBezPoint4Key key;
			key.time = keys[i]->input * timeFactor;
			key.val = Point4(keys[i]->output[0], keys[i]->output[1], keys[i]->output[2], keys[i]->output[3]);
			SetInTanType(key.flags, ConvertInterpolation(keys[i > 0 ? i-1 : i]->interpolation));
			SetOutTanType(key.flags, ConvertInterpolation(keys[i]->interpolation));

			if (keys[i]->interpolation == FUDaeInterpolation::BEZIER)
			{
				const FCDAnimationMKeyBezier* bkey = (const FCDAnimationMKeyBezier*) keys[i];
				float previousSpan = bkey->input - (i > 0 ? keys[i-1]->input : (bkey->input - FLT_TOLERANCE));
				float nextSpan = (i < keyCount - 1 ? keys[i+1]->input : (bkey->input + FLT_TOLERANCE)) - bkey->input;

				// Calculate the in-tangent
				key.inLength = Point4((bkey->input - bkey->inTangent[0].x) / previousSpan, (bkey->input - bkey->inTangent[1].x) / previousSpan, (bkey->input - bkey->inTangent[2].x) / previousSpan, (bkey->input - bkey->inTangent[3].x) / previousSpan);
				if (key.inLength.x < FLT_TOLERANCE && key.inLength.x > -FLT_TOLERANCE) key.inLength.x = FMath::Sign(key.inLength.x) * FLT_TOLERANCE;
				if (key.inLength.y < FLT_TOLERANCE && key.inLength.y > -FLT_TOLERANCE) key.inLength.y = FMath::Sign(key.inLength.y) * FLT_TOLERANCE;
				if (key.inLength.z < FLT_TOLERANCE && key.inLength.z > -FLT_TOLERANCE) key.inLength.z = FMath::Sign(key.inLength.z) * FLT_TOLERANCE;
				if (key.inLength.w < FLT_TOLERANCE && key.inLength.w > -FLT_TOLERANCE) key.inLength.w = FMath::Sign(key.inLength.w) * FLT_TOLERANCE;
				key.intan = Point4((bkey->inTangent[0].y - bkey->output[0]) / (key.inLength.x * previousSpan * timeFactor), (bkey->inTangent[1].y - bkey->output[1]) / (key.inLength.y * previousSpan * timeFactor), (bkey->inTangent[2].y - bkey->output[2]) / (key.inLength.z * previousSpan * timeFactor), (bkey->inTangent[3].y - bkey->output[3]) / (key.inLength.z * previousSpan * timeFactor));

				// Calculate the out-tangent
				key.outLength = Point4((bkey->outTangent[0].x - bkey->input) / nextSpan, (bkey->outTangent[1].x - bkey->input) / nextSpan, (bkey->outTangent[2].x - bkey->input) / nextSpan, (bkey->outTangent[3].x - bkey->input) / nextSpan);
				if (key.outLength.x < FLT_TOLERANCE && key.outLength.x > -FLT_TOLERANCE) key.outLength.x = FMath::Sign(key.outLength.x) * FLT_TOLERANCE;
				if (key.outLength.y < FLT_TOLERANCE && key.outLength.y > -FLT_TOLERANCE) key.outLength.y = FMath::Sign(key.outLength.y) * FLT_TOLERANCE;
				if (key.outLength.z < FLT_TOLERANCE && key.outLength.z > -FLT_TOLERANCE) key.outLength.z = FMath::Sign(key.outLength.z) * FLT_TOLERANCE;
				if (key.outLength.w < FLT_TOLERANCE && key.outLength.w > -FLT_TOLERANCE) key.outLength.w = FMath::Sign(key.outLength.w) * FLT_TOLERANCE;
				key.outtan = Point4((bkey->outTangent[0].y - bkey->output[0]) / (key.outLength.x * nextSpan * timeFactor), (bkey->outTangent[1].y - bkey->output[1]) / (key.outLength.y * nextSpan * timeFactor), (bkey->outTangent[2].y - bkey->output[2]) / (key.outLength.z * nextSpan * timeFactor), (bkey->outTangent[3].y - bkey->output[3]) / (key.outLength.z * nextSpan * timeFactor));
			}
			else
			{
				key.intan = key.outtan = Point4::Origin;
				key.outLength = key.inLength = Point4(0.333f, 0.333f, 0.333f, 0.333f);
			}

			// In some rare cases, only half the key is set by the first call, so call the SetKey function twice.
			controller->SetKey((int) i, &key);
			controller->SetKey((int) i, &key);
		}
	}
	else
	{
		// No support for linear Point4 controllers in 3dsMax.
	}
	controller->SortKeys();
}
