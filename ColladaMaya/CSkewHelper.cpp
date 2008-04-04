/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include <maya/MStatus.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MFnTransform.h>
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDTransform.h"
#include "CSkewHelper.h"

//
// CSkewHelper
//

void CSkewHelper::ExportSkew(FCDSceneNode* parent, double* shear)
{
	// From my derivation, it is easier to split the shear into three.
	// This forces the hardcoded axises and the angle becomes simply:
	// skewAngle = arctan(shearValue);
	//
	if (!IsEquivalent(shear[0], 0.0))
	{
		FCDTSkew* skewT = (FCDTSkew*) parent->AddTransform(FCDTransform::SKEW);
		skewT->SetAngle(FMath::RadToDeg((float) atan(shear[0])));
		skewT->SetRotateAxis(FMVector3::XAxis);
		skewT->SetAroundAxis(FMVector3::YAxis);
		skewT->SetSubId("skewXY");
	}

	if (!IsEquivalent(shear[1], 0.0))
	{
		FCDTSkew* skewT = (FCDTSkew*) parent->AddTransform(FCDTransform::SKEW);
		skewT->SetAngle(FMath::RadToDeg((float) atan(shear[1])));
		skewT->SetRotateAxis(FMVector3::XAxis);
		skewT->SetAroundAxis(FMVector3::ZAxis);
		skewT->SetSubId("skewXZ");
	}

	if (!IsEquivalent(shear[2], 0.0))
	{
		FCDTSkew* skewT = (FCDTSkew*) parent->AddTransform(FCDTransform::SKEW);
		skewT->SetAngle(FMath::RadToDeg((float) atan(shear[2])));
		skewT->SetRotateAxis(FMVector3::YAxis);
		skewT->SetAroundAxis(FMVector3::ZAxis);
		skewT->SetSubId("skewYZ");
	}
}
