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
#include "ConversionFunctions.h"

void AngleApproach(float pval, float& val)
{
	while (val - pval > FMath::Pi) val -= FMath::Pi * 2.0f;
	while (val - pval < -FMath::Pi) val += FMath::Pi * 2.0f;
}

void FSConvert::PatchEuler(float* pval, float* val)
{
	// Approach these Eulers to the previous value.
	for (int i = 0; i < 3; ++i) AngleApproach(pval[i], val[i]);
	float distanceSq = 0.0f; for (int i = 0; i < 3; ++i) distanceSq += (val[i] - pval[i]) * (val[i] - pval[i]);

	// All quaternions can be expressed two ways. Check if the second way is better.
	float alternative[3] = { val[0] + FMath::Pi, FMath::Pi - val[1], val[2] + FMath::Pi };
	for (int i = 0; i < 3; ++i) AngleApproach(pval[i], alternative[i]);
	float alternateDistanceSq = 0.0f; for (int i = 0; i < 3; ++i) alternateDistanceSq += (alternative[i] - pval[i]) * (alternative[i] - pval[i]);

	if (alternateDistanceSq < distanceSq)
	{
		// Pick the alternative
		for (int i = 0; i < 3; ++i) val[i] = alternative[i];
	}
}