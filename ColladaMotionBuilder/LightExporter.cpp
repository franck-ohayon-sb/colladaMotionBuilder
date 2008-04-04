/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "AnimationExporter.h"
#include "ColladaExporter.h"
#include "LightExporter.h"
#include "NodeExporter.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDAnimationCurve.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDLight.h"
#include "FCDocument/FCDSceneNode.h"

//
// LightExporter
//

LightExporter::LightExporter(ColladaExporter* base)
:	EntityExporter(base)
,	ambientLight(NULL)
{
}

LightExporter::~LightExporter()
{
	// We keep this function set around, in order to support sampling the ambient light color animation.
	SAFE_DELETE(ambientLight);
}

FCDEntity* LightExporter::ExportLight(FBLight* light)
{
	FCDLight* colladaLight = CDOC->GetLightLibrary()->AddEntity();
	ExportEntity(colladaLight, light);

	// Figure out the correct type for the light.
	FCDLight::LightType lightType;
	switch (light->LightType)
	{
	case kFBLightTypeInfinite: lightType = FCDLight::DIRECTIONAL; break;
	case kFBLightTypePoint: lightType = FCDLight::POINT; break;
	case kFBLightTypeSpot: lightType = FCDLight::SPOT; break;
	default: FUFail(return NULL);
	}
	colladaLight->SetLightType(lightType);

	// Write out the common light parameters.
	colladaLight->SetColor(ToFMVector3(light->DiffuseColor));
	ANIMATABLE(&light->DiffuseColor, colladaLight, colladaLight->GetColor(), -1, NULL, false);
	colladaLight->SetIntensity((float) light->Intensity / 100.0f);
	FCDConversionScaleFunctor functor(0.01f);
	ANIMATABLE(&light->Intensity, colladaLight, colladaLight->GetIntensity(), -1, &functor, false);

	// Only other parameter supported by Motion Builder is the cone angle.
	if (lightType == FCDLight::SPOT)
	{
		colladaLight->SetFallOffAngle((float) light->ConeAngle);
		ANIMATABLE(&light->ConeAngle, colladaLight, colladaLight->GetFallOffAngle(), -1, NULL, false);
	}

	return colladaLight;
}

FCDEntity* LightExporter::ExportAmbientLight(FCDSceneNode* colladaScene)
{
	if (ambientLight == NULL) ambientLight = new FBGlobalLight(); // This class is a function set.

	FMVector3 ambientColor = ToFMVector3(ambientLight->AmbientColor);
	if (IsEquivalent(ambientColor, FMVector3::Zero) && !ANIM->IsAnimated(&ambientLight->AmbientColor))
	{
		return NULL;
	}

	// Create the FCollada light structure for this ambient light.
	FCDLight* colladaLight = CDOC->GetLightLibrary()->AddEntity();
	colladaLight->SetDaeId("global-light");
	colladaLight->SetName(FC("global-light"));
	colladaLight->SetLightType(FCDLight::AMBIENT);
	colladaLight->SetColor(ambientColor);
	ANIMATABLE(&ambientLight->AmbientColor, colladaLight, colladaLight->GetColor(), -1, NULL, false);

	// Create the FCollada scene node to contain the light.
	FCDSceneNode* globalNode = colladaScene->AddChildNode();
	globalNode->SetDaeId("global-node");
	globalNode->SetName(FC("global-node"));
	globalNode->AddInstance(colladaLight);
	return NULL;
}
