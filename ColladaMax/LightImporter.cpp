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
#include "AnimationImporter.h"
#include "DocumentImporter.h"
#include "LightImporter.h"
#include "NodeImporter.h"
#include "TargetImporter.h"
#include "FUtils/FUDaeEnum.h"
#include "FUtils/FUDaeSyntax.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDLight.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDImage.h"
#include "FCDocument/FCDExtra.h"
#include "Common/Light.h"
#include "Common/ConversionFunctions.h"

//
// LightImporter
//

LightImporter::LightImporter(DocumentImporter* _document, NodeImporter* parent)
:	EntityImporter(_document, parent)
{
	colladaLight = NULL;
}

LightImporter::~LightImporter()
{
	colladaLight = NULL;
}

LightObject* LightImporter::ImportLight(FCDLight* _colladaLight)
{
	colladaLight = _colladaLight;
	bool isTargeted = colladaLight->HasTarget();

	// check for the extra node
	FCDETechnique *maxTech = colladaLight->GetExtra()->GetDefaultType()->FindTechnique(DAEMAX_MAX_PROFILE);
	FCDENode *skyLightNode = NULL;
	if (maxTech != NULL)
	{
		skyLightNode = maxTech->FindChildNode(DAEMAX_SKY_LIGHT);
	}
	bool isSky = (skyLightNode != NULL);

	if (isSky)
	{
		LightObject *light = (LightObject *)GetDocument()->MaxCreate(LIGHT_CLASS_ID, SKY_LIGHT_CLASS_ID);
		if (light == NULL) return NULL;

		// Common light parameters
		Point3 color = ToPoint3(colladaLight->GetColor());
		light->SetRGBColor(0, color);
		ANIM->ImportAnimatedPoint3(light, "Color", (const FCDParameterAnimatableVector3&) colladaLight->GetColor());
		light->SetIntensity(0, colladaLight->GetIntensity());
		ANIM->ImportAnimatedFloat(light, "Multiplier", colladaLight->GetIntensity());

		IParamBlock2* params = ((Animatable*)light)->GetParamBlock(MaxLight::PBLOCK_REF_SKY);
		if (params == NULL) return NULL;

		FCDENode *child = NULL;

		child = skyLightNode->FindChildNode(DAEMAX_SKY_CAST_SHADOWS_PARAMETER);
		if (child != NULL)
			params->SetValue(MaxLight::PB_SKY_CAST_SHADOWS, TIME_INITIAL_POSE, (BOOL)FUStringConversion::ToBoolean(child->GetContent()));

		child = skyLightNode->FindChildNode(DAEMAX_SKY_COLOR_MAP);
		if (child != NULL)
		{
			FCDImage *colladaImage = CDOC->GetImageLibrary()->FindDaeId(child->GetContent());
			if (colladaImage != NULL)
			{
				BitmapTex* map = (BitmapTex*)GetDocument()->MaxCreate(TEXMAP_CLASS_ID, Class_ID(BMTEX_CLASS_ID,0));
				if (map != NULL)
				{
					map->SetMapName(const_cast<char*>(colladaImage->GetFilename().c_str()));
					map->LoadMapFiles(TIME_INITIAL_POSE);

					params->SetValue(MaxLight::PB_SKY_COLOR_MAP, TIME_INITIAL_POSE, map);
				}
			}
		}

		child = skyLightNode->FindChildNode(DAEMAX_SKY_COLOR_MAP_AMOUNT_PARAMETER);
		if (child != NULL)
			params->SetValue(MaxLight::PB_SKY_SKY_COLOR_MAP_AMOUNT, TIME_INITIAL_POSE, FUStringConversion::ToFloat(child->GetContent()));

		child = skyLightNode->FindChildNode(DAEMAX_SKY_COLOR_MAP_ON_PARAMETER);
		if (child != NULL)
			params->SetValue(MaxLight::PB_SKY_SKY_COLOR_MAP_ON, TIME_INITIAL_POSE, (BOOL)FUStringConversion::ToBoolean(child->GetContent()));
 
		child = skyLightNode->FindChildNode(DAEMAX_SKY_INTENSITY_ON);
		if (child != NULL)
			params->SetValue(MaxLight::PB_SKY_MULTIPLIER_ON, TIME_INITIAL_POSE, (BOOL)FUStringConversion::ToBoolean(child->GetContent()));

		child = skyLightNode->FindChildNode(DAEMAX_SKY_RAY_BIAS_PARAMETER);
		if (child != NULL)
			params->SetValue(MaxLight::PB_SKY_RAY_BIAS, TIME_INITIAL_POSE, FUStringConversion::ToFloat(child->GetContent()));

		child = skyLightNode->FindChildNode(DAEMAX_SKY_RAYS_PER_SAMPLE_PARAMETER);
		if (child != NULL)
			params->SetValue(MaxLight::PB_SKY_RAYS_PER_SAMPLE, TIME_INITIAL_POSE, FUStringConversion::ToInt32(child->GetContent()));

		child = skyLightNode->FindChildNode(DAEMAX_SKY_SKYMODE);
		if (child != NULL)
			params->SetValue(MaxLight::PB_SKY_MODE, TIME_INITIAL_POSE, (BOOL)FUStringConversion::ToBoolean(child->GetContent()));

		lightObject = light;
	}
	else
	{
		// ambient, spot, directional, point

		// Create the correctly-typed 3dsMax light object
		int maxType;
		FCDLight::LightType colladaType = colladaLight->GetLightType();
		switch (colladaType)
		{
		case FCDLight::AMBIENT:
			// Ambient lighting is global and additive, in 3dsMax,
			// let the DocumentImporter object handle them
			GetDocument()->AddAmbientLight(colladaLight);
			GetParent()->SetAddNode(false);
			return NULL;

		case FCDLight::POINT: maxType = OMNI_LIGHT; break;
		case FCDLight::DIRECTIONAL: maxType = (isTargeted) ? TDIR_LIGHT : DIR_LIGHT; break;
		case FCDLight::SPOT: maxType = (isTargeted) ? TSPOT_LIGHT : FSPOT_LIGHT; break;
		default: return NULL;
		}
		GenLight* light = GetDocument()->GetImportInterface()->CreateLightObject(maxType);

		// Create the light's target
		TargetImporter targetImporter(GetDocument());
		float targetDistance = 0.0f;
		if (isTargeted) 
		{
			targetImporter.ImportTarget(GetParent(), colladaLight->GetTargetNode());
			targetDistance = targetImporter.GetTargetDistance();
		}
		else
		{
			targetDistance = 120.0f; // may change later on if the MAX profile value was found.
		}
		light->SetTDist(0, targetDistance);
		if (colladaType == FCDLight::DIRECTIONAL)
		{
			light->SetOvershoot(TRUE); // may change later on if the MAX profile value was found.
		}

		// Decay rates are available, in Max, on all light types
		const FCDParameterAnimatableFloat& constDecayFactor = colladaLight->GetConstantAttenuationFactor();
		const FCDParameterAnimatableFloat& linDecayFactor = colladaLight->GetLinearAttenuationFactor();
		//const float* quadDecayFactor = &colladaLight->GetQuadraticAttenuationFactor();

		// Figure out the decay type, since Max only allows for one attenuation factor
		// forget about the decay being animated, COLLADA doesn't support that.
		int decayType;
		if (*constDecayFactor > 0.0f) decayType = 0;
		else if (*linDecayFactor > 0.0f) decayType = 1;
		else decayType = 2;

		light->SetDecayType(decayType);

		if (colladaType == FCDLight::SPOT || colladaType == FCDLight::DIRECTIONAL)
		{
			// Force the spot to be a circle. Some of the light values seem to be
			// missing default assignation on construction and instead inherit
			// unknown values from previous, deleted scenes.
			light->SetSpotShape(CIRCLE_LIGHT);

			// Angles are shared between spot and directional lights.
			const FCDParameterAnimatableFloat& falloffValue = colladaLight->GetOuterAngle();
			const FCDParameterAnimatableFloat& hotspotValue = colladaLight->GetFallOffAngle();
			light->SetHotspot(0, *hotspotValue);

			// Max crashes when the fallsize is less or equal to the hotspot
			float outerAngle = *falloffValue;
			if (outerAngle <= *hotspotValue) outerAngle = *hotspotValue + 2.0f;
			light->SetFallsize(0, outerAngle);

			// Because of that crash, take special care, when loading the animation
			// If both the hotspot and the falloff are animated, load them without checks
			// If only hotspot is animated, load that curve into the hotspot and the falloff parameters
			// If only falloff is animated, don't load anything.
			if (hotspotValue.IsAnimated())
			{
				ANIM->ImportAnimatedFloat(light, "Hotspot", hotspotValue);
				bool isFalloffAnimated = falloffValue.IsAnimated();
				ANIM->ImportAnimatedFloat(light, "Falloff", isFalloffAnimated ? falloffValue : hotspotValue, isFalloffAnimated ? NULL : &FSConvert::AddTwo);
			}
		}

		// Common light parameters
		Point3 color = ToPoint3(colladaLight->GetColor());
		light->SetRGBColor(0, color);
		ANIM->ImportAnimatedPoint3(light, "Color", (const FCDParameterAnimatableVector3&) colladaLight->GetColor());
		light->SetIntensity(0, colladaLight->GetIntensity());
		ANIM->ImportAnimatedFloat(light, "Multiplier", colladaLight->GetIntensity());

		// import extra information
		const char* profiles[2] = { DAEMAX_MAX_PROFILE, DAE_FCOLLADA_PROFILE };
		for (size_t i = 0; i < 2; ++i)
		{
			FCDETechnique* extra = colladaLight->GetExtra()->GetDefaultType()->FindTechnique(profiles[i]);
			if (extra == NULL) continue;

			FCDENodeList nodes; StringList names;
			extra->FindParameters(nodes, names);
			size_t parameterCount = nodes.size();
			for (size_t j = 0; j < parameterCount; ++j)
			{
				fm::string& name = names[j];
				const fchar* content = nodes[j]->GetContent();

				if (IsEquivalent(name, DAEMAX_DECAY_TYPE_PARAMETER)) light->SetDecayType(FUStringConversion::ToInt32(content));
				else if (IsEquivalent(name, DAEMAX_USE_NEAR_ATTEN_PARAMETER)) light->SetUseAttenNear(FUStringConversion::ToInt32(content));
				else if (IsEquivalent(name, DAEMAX_DECAY_START_PARAMETER)) light->SetDecayRadius(0, FUStringConversion::ToFloat(content));
				else if (IsEquivalent(name, DAEMAX_NEAR_ATTEN_START_PARAMETER)) light->SetAtten(0, ATTEN1_START, FUStringConversion::ToFloat(content));
				else if (IsEquivalent(name, DAEMAX_NEAR_ATTEN_END_PARAMETER)) light->SetAtten(0, ATTEN1_END, FUStringConversion::ToFloat(content));
				else if (IsEquivalent(name, DAEMAX_USE_FAR_ATTEN_PARAMETER)) light->SetUseAtten(FUStringConversion::ToInt32(content));
				else if (IsEquivalent(name, DAEMAX_FAR_ATTEN_START_PARAMETER)) light->SetAtten(0, ATTEN_START, FUStringConversion::ToFloat(content));
				else if (IsEquivalent(name, DAEMAX_FAR_ATTEN_END_PARAMETER)) light->SetAtten(0, ATTEN_END, FUStringConversion::ToFloat(content));
				else if (IsEquivalent(name, DAEMAX_OVERSHOOT_LIGHT_PARAMETER)) light->SetOvershoot(FUStringConversion::ToBoolean(content));
				else if (IsEquivalent(name, DAEMAX_ASPECTRATIO_LIGHT_PARAMETER))
				{
					light->SetAspect(0, FUStringConversion::ToFloat(content));
					ANIM->ImportAnimatedFloat(light->GetReference(MaxLight::PBLOCK_REF), MaxLight::PB_ASPECT, nodes[j]->GetAnimated());
				}
				else if (IsEquivalent(name, DAEMAX_DEFAULT_TARGET_DIST_LIGHT_PARAMETER)) light->SetTDist(0, FUStringConversion::ToFloat(content));
			}

			ImportShadowParams(light, extra);
			ImportLightMap(extra, light);
		}

		// Enable to light
		light->Enable(TRUE);
		light->SetUseLight(TRUE);
		lightObject = light;
	}

	return lightObject;
}

void LightImporter::ImportShadowParams(GenLight* light, FCDENode* extra)
{
	FCDENode* shadowParams = extra->FindChildNode(DAEMAX_SHADOW_ATTRIBS);
	// Having this node means we cast shadows
	if (shadowParams != NULL)
	{
		light->SetShadow(1);

		FCDENode* shadowType = shadowParams->FindChildNode(DAEMAX_SHADOW_TYPE);
		if (shadowType != NULL)
		{
			if (IsEquivalent(shadowType->GetContent(), DAEMAX_SHADOW_TYPE_MAP))
			{
				light->SetShadowType(0);
			}
			else light->SetShadowType(1);
		}
		//	int shadType = light->GetShadowType();
		//if (shadType == 0) shadow->AddParameter(DAEMAX_SHADOW_TYPE, DAEMAX_SHADOW_TYPE_MAP);
		//else if (shadType == 1) shadow->AddParameter(DAEMAX_SHADOW_TYPE, DAEMAX_SHADOW_TYPE_RAYTRACE);

		ImportExclusionList(shadowParams, light);
		ImportShadowMap(shadowParams, light);
	}
	else
	{
		light->SetShadow(0);
	}
}

void LightImporter::ImportExclusionList(FCDENode *shadowParams, GenLight* light)
{
	// GetExclusion list
	FCDENode* shadowLists = shadowParams->FindChildNode(DAEMAX_SHADOW_AFFECTS);
	if (shadowLists)
	{
		ExclList& exclusions = light->GetExclusionList();
		exclusions.SetSize(0);

		FCDENode* nodeList = shadowLists->FindChildNode(DAEMAX_SHADOW_LIST_NODES);
		FUAssert(nodeList != NULL, return);
		//const fchar* nodeNames = node->GetContent();
		StringList nodeNames;
		FUStringConversion::ToStringList(nodeList->GetContent(), nodeNames);
		size_t numNodes = nodeNames.size();
		FUAssert(numNodes > 0, return);
		
		for (size_t i = 0; i < numNodes; ++i)
		{
			NodeImporter* nodeImporter = (NodeImporter*)GetDocument()->FindInstance(nodeNames[i]);
			if (nodeImporter == NULL) continue;

			exclusions.AddNode(nodeImporter->GetImportNode()->GetINode());
		}

		// Set up this list as exclusive or inclusive
		FCDENode* isExclusive = shadowLists->FindChildNode(DAEMAX_SHADOW_LIST_EXCLUDES);
		if (isExclusive != NULL && IsEquivalent(isExclusive->GetContent(), "1"))
		{
			exclusions.SetFlag(NT_INCLUDE, 0);
		}
		else
		{
			exclusions.SetFlag(NT_INCLUDE, 1);
		}

		FCDENode* illuminates = shadowLists->FindChildNode(DAEMAX_SHADOW_LIST_ILLUMINATES);
		if (illuminates != NULL && IsEquivalent(illuminates->GetContent(), "1"))
		{
			exclusions.SetFlag(NT_AFFECT_ILLUM, 1);
		}
		else
		{
			exclusions.SetFlag(NT_AFFECT_ILLUM, 0);
		}

		FCDENode* castShadows = shadowLists->FindChildNode(DAEMAX_SHADOW_LIST_CASTS);
		if (castShadows != NULL && IsEquivalent(castShadows->GetContent(), "1"))
		{
			exclusions.SetFlag(NT_AFFECT_SHADOWCAST, 1);
		}
		else
		{
			exclusions.SetFlag(NT_AFFECT_SHADOWCAST, 0);
		}
	}
}

void LightImporter::ImportShadowMap(FCDENode *shadowParams, GenLight* light)
{
	FCDENode* shadowMapParams = shadowParams->FindChildNode(DAEMAX_SHADOW_MAP);
	if (shadowMapParams)
	{
		// Turn our shadow mapping parameter on
		IParamBlock* pblock = (IParamBlock*)light->GetReference(MaxLight::PBLOCK_REF);
		pblock->SetValue(MaxLight::PB_OMNISHAD_COLMAP, 0, 1);

		FCDENode* lightAffects = shadowMapParams->FindChildNode(DAEMAX_LIGHT_AFFECTS_SHADOW);
		if (lightAffects != NULL)
		{
			bool doesAffect = FUStringConversion::ToBoolean(lightAffects->GetContent());
			light->SetLightAffectsShadow(doesAffect);
		}

		FCDENode* shadowMapId = shadowMapParams->FindChildNode(DAEMAX_PROJ_IMAGE);
		if (shadowMapId != NULL)
		{
			FCDImage* fcdImg = GetDocument()->GetCOLLADADocument()->FindImage(shadowMapId->GetContent());
			if (fcdImg != NULL)
			{
				// Create the BitmapTex object
				BitmapTex* bitmapTextureMap = NewDefaultBitmapTex();
				bitmapTextureMap->SetMapName(const_cast<char*>(fcdImg->GetFilename().c_str()));
				bitmapTextureMap->LoadMapFiles(0);
				bitmapTextureMap->InitSlotType(MAPSLOT_ENVIRON);
				light->SetShadowProjMap(bitmapTextureMap);
			}
		}

		FCDENode* shadowColor = shadowMapParams->FindChildNode(DAEMAX_SHADOW_PROJ_COLOR);
		if (shadowColor != NULL)
		{
			Point3 value = ToPoint3(FUStringConversion::ToVector3(shadowColor->GetContent()));
			light->SetShadColor(0, value);
		}

		FCDENode* shadowMult = shadowMapParams->FindChildNode(DAEMAX_SHADOW_PROJ_COLOR_MULT);
		if (shadowColor != NULL)
		{
			float value = FUStringConversion::ToFloat(shadowMult->GetContent());
			light->SetShadMult(0, value);
		}

		FCDENode* shadowAffected = shadowMapParams->FindChildNode(DAEMAX_LIGHT_AFFECTS_SHADOW);
		if (shadowAffected != NULL)
		{
			BOOL value = FUStringConversion::ToBoolean(shadowAffected->GetContent());
			light->SetLightAffectsShadow(value);
		}
	}
}

void LightImporter::ImportLightMap(FCDENode *extraParams, GenLight* light)
{
	FUAssert(extraParams != NULL && light != NULL, return);

	FCDENode* lightMapParams = extraParams->FindChildNode(DAEMAX_LIGHT_MAP);
	if (lightMapParams != NULL)
	{
		FCDENode* lightMapId = lightMapParams->FindChildNode(DAEMAX_PROJ_IMAGE);
		if (lightMapId != NULL)
		{
			FCDImage* fcdImg = GetDocument()->GetCOLLADADocument()->FindImage(lightMapId->GetContent());
			if (fcdImg != NULL)
			{
				// Create the BitmapTex object
				BitmapTex* bitmapTextureMap = NewDefaultBitmapTex();
				bitmapTextureMap->SetMapName(const_cast<char*>(fcdImg->GetFilename().c_str()));
				bitmapTextureMap->LoadMapFiles(0);
				bitmapTextureMap->InitSlotType(MAPSLOT_ENVIRON);
				light->SetProjMap(bitmapTextureMap);
				light->SetProjector(1);
			}
		}
	}
}
