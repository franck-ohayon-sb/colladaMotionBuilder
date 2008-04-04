/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDTransform.h"
#include "CRotateHelper.h"

//
// CRotateHelper
//

CRotateHelper::CRotateHelper(DaeDoc* _doc, FCDTransformContainer* _container, MEulerRotation rotation)
:	doc(_doc), transformId(NULL)
,	rot(rotation), container(_container)
,	parentNode(NULL), eliminateEmptyRotations(false)
{
}

FCDTRotation* CRotateHelper::CreateRotation(const char* coordinate, double angle, const FMVector3& axis)
{
	FCDTRotation* colladaTransform = NULL;
	if (!eliminateEmptyRotations || !IsEquivalent(rot.x, 0.0))
	{
		if (parentNode != NULL)
		{
			colladaTransform = (FCDTRotation*) parentNode->AddTransform(FCDTransform::ROTATION);
		}
		else
		{
			colladaTransform = new FCDTRotation(CDOC, NULL);
			container->push_back(colladaTransform);
		}

		colladaTransform->SetAxis(axis);
		colladaTransform->SetAngle(FMath::RadToDeg((float)angle));

		if (transformId != NULL)
		{
			colladaTransform->SetSubId(fm::string(transformId) + coordinate);
		}
	}
	return colladaTransform;
}

void CRotateHelper::OutX() { colladaRotations[0] = CreateRotation("X", rot.x, FMVector3::XAxis); }
void CRotateHelper::OutY() { colladaRotations[1] = CreateRotation("Y", rot.y, FMVector3::YAxis); }
void CRotateHelper::OutZ() { colladaRotations[2] = CreateRotation("Z", rot.z, FMVector3::ZAxis); }

void CRotateHelper::Output()
{
	switch (rot.order)
	{
	case MEulerRotation::kXYZ: OutZ(); OutY(); OutX(); break;
	case MEulerRotation::kXZY: OutY(); OutZ(); OutX(); break;
	case MEulerRotation::kYXZ: OutZ(); OutX(); OutY(); break;
	case MEulerRotation::kYZX: OutX(); OutZ(); OutY(); break;
	case MEulerRotation::kZXY: OutY(); OutX(); OutZ(); break;
	case MEulerRotation::kZYX: OutX(); OutY(); OutZ(); break;
	default: OutZ(); OutY(); OutX(); break;
	}
}
