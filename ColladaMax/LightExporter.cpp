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
#include "DocumentExporter.h"
#include "EntityExporter.h"
#include "LightExporter.h"
#include "MaterialExporter.h"
#include "NodeExporter.h"
#include "ColladaOptions.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDLight.h"
#include "FCDocument/FCDExtra.h"
#include "FCDocument/FCDImage.h"
#include "Common/Light.h"

// more unreferenced params warning than we want to see
#pragma warning (disable : 4100)
#include <shadgen.h>


LightExporter::LightExporter(DocumentExporter* _document)
:	BaseExporter(_document)
{
}

LightExporter::~LightExporter()
{
}

FCDEntity* LightExporter::ExportLight(INode* node, LightObject* object)
{
	if (node == NULL || object == NULL || object->SuperClassID() != LIGHT_CLASS_ID) return NULL;

	// Retrieve the target node, if we are not baking matrices.
	// Baked matrices must always sample the transform!
	ULONG ClassId = object->ClassID().PartA();
	bool isTargeted = !OPTS->BakeMatrices() && (ClassId == SPOT_LIGHT_CLASS_ID || ClassId == TDIR_LIGHT_CLASS_ID);
	INode* targetNode = (!isTargeted) ? NULL : node->GetTarget();

	// some lights are not supported at all
	switch (ClassId)
	{
	case FSPOT_LIGHT_CLASS_ID:
	case SPOT_LIGHT_CLASS_ID:
	case DIR_LIGHT_CLASS_ID: 
	case TDIR_LIGHT_CLASS_ID:
	case SKY_LIGHT_CLASS_ID_PART_A:
	case OMNI_LIGHT_CLASS_ID:
		break;
	default:
		return NULL;
	}

	// Add a new light
	FCDLight* colladaLight = CDOC->GetLightLibrary()->AddEntity();
	colladaLight->SetUserHandle(targetNode);

	// Determine the light's type
	bool isSpot = false, isDirectional = false, isPoint = false, isSky = false;
	switch (ClassId)
	{
	case FSPOT_LIGHT_CLASS_ID:
	case SPOT_LIGHT_CLASS_ID: colladaLight->SetLightType(FCDLight::SPOT); isSpot = true; break;
	case DIR_LIGHT_CLASS_ID: 
	case TDIR_LIGHT_CLASS_ID: colladaLight->SetLightType(FCDLight::DIRECTIONAL); isDirectional = true; break;
	case SKY_LIGHT_CLASS_ID_PART_A:
		colladaLight->SetLightType(FCDLight::POINT);
		isSky = true;
		break;
	case OMNI_LIGHT_CLASS_ID:
		colladaLight->SetLightType(FCDLight::POINT);
		isPoint = true;
		break;
	default:
		FUFail(return NULL);
	}

	// Retrieve the parameter block
	IParamBlock* parameters1 = NULL;
	IParamBlock2* parameters2 = NULL;

	if (isSky)
	{
		parameters2 = (IParamBlock2*) object->GetReference(MaxLight::PBLOCK_REF_SKY);
	}
	else
	{
		parameters1 = (IParamBlock*) object->GetReference(MaxLight::PBLOCK_REF);
	}

	// Export the basic color parameters: color / intensity
	if (parameters1)
	{
		ANIM->ExportProperty(colladaLight->GetDaeId() + "-color", parameters1, MaxLight::PB_COLOR, (FCDParameterAnimatableVector3&) colladaLight->GetColor());
		ANIM->ExportProperty(colladaLight->GetDaeId() + "-intensity", parameters1, MaxLight::PB_INTENSITY, colladaLight->GetIntensity());
	}
	else if (parameters2 && isSky)
	{
		ANIM->ExportProperty(colladaLight->GetDaeId() + "-color", parameters2, MaxLight::PB_SKY_COLOR, 0, (FCDParameterAnimatableVector3&) colladaLight->GetColor());
		ANIM->ExportProperty(colladaLight->GetDaeId() + "-intensity", parameters2, MaxLight::PB_SKY_INTENSITY, 0, colladaLight->GetIntensity());
	}

	if (!parameters1 && !parameters2)
		FUFail(return colladaLight);

	// Decay is not animated
	colladaLight->SetLinearAttenuationFactor(0.0f);
	colladaLight->SetQuadraticAttenuationFactor(0.0f);
	colladaLight->SetConstantAttenuationFactor(0.0f);

	if (isSpot || isDirectional || isPoint)
	{
		int decayFunction = parameters1->GetInt(isPoint ? MaxLight::PB_DECAY : MaxLight::PB_OMNIDECAY, TIME_EXPORT_START);
		switch (decayFunction)
		{
		case 1:
			colladaLight->SetLinearAttenuationFactor(1.0f);
			break;
		case 2:
			colladaLight->SetQuadraticAttenuationFactor(1.0f);
			break;
		case 0:
		default:
			colladaLight->SetConstantAttenuationFactor(1.0f);
			break;
		}
	}
	else if (isSky)
	{
		colladaLight->SetConstantAttenuationFactor(1.0f);
	}

	// Export the directional and spot light angles/radii parameters
	if (isSpot || isDirectional)
	{
		ANIM->ExportProperty(colladaLight->GetDaeId() + "-outer_angle", parameters1, MaxLight::PB_FALLSIZE, colladaLight->GetOuterAngle());
		ANIM->ExportProperty(colladaLight->GetDaeId() + "-inner_angle", parameters1, MaxLight::PB_HOTSIZE, colladaLight->GetFallOffAngle());

		float aspectRatio = parameters1->GetFloat(MaxLight::PB_ASPECT, TIME_EXPORT_START);
		FCDETechnique *extra = colladaLight->GetExtra()->GetDefaultType()->AddTechnique(DAEMAX_MAX_PROFILE);
		FCDENode* aspectRatioNode = extra->AddParameter(DAEMAX_ASPECTRATIO_LIGHT_PARAMETER, aspectRatio);
		ANIM->ExportProperty(colladaLight->GetDaeId() + "-aspect_ratio", parameters1, MaxLight::PB_ASPECT, aspectRatioNode->GetAnimated());
	}

	if (isSpot || isDirectional || isPoint)
	{
		GenLight* light = (GenLight*)(object);

		// add some extra information
		// missing animation, but that's better than nothing
		FCDETechnique* extra = colladaLight->GetExtra()->GetDefaultType()->AddTechnique(DAEMAX_MAX_PROFILE);

		// Export the overshoot flag for directional lights
		if (isDirectional)
		{
			bool overshoots = light->GetOvershoot() != FALSE;
			extra->AddParameter(DAEMAX_OVERSHOOT_LIGHT_PARAMETER, overshoots);
		}
		else if (isSpot)
		{
			// For spot lights, export the COLLADA fall-off exponent
			colladaLight->SetFallOffExponent(0.0f);
		}

		float targetDistance = light->GetTDist(TIME_EXPORT_START);
		extra->AddParameter(DAEMAX_DEFAULT_TARGET_DIST_LIGHT_PARAMETER, targetDistance);

		int decayType = (int)light->GetDecayType();
		extra->AddParameter(DAEMAX_DECAY_TYPE_PARAMETER, decayType);
		
		float decayStart = light->GetDecayRadius(TIME_EXPORT_START);
		extra->AddParameter(DAEMAX_DECAY_START_PARAMETER, decayStart);

		bool useNearAtt = (light->GetUseAttenNear() == TRUE);
		extra->AddParameter(DAEMAX_USE_NEAR_ATTEN_PARAMETER, useNearAtt);

		float nearAttStart = light->GetAtten(TIME_EXPORT_START, ATTEN1_START);
		extra->AddParameter(DAEMAX_NEAR_ATTEN_START_PARAMETER, nearAttStart);

		float nearAttEnd = light->GetAtten(TIME_EXPORT_START, ATTEN1_END);
		extra->AddParameter(DAEMAX_NEAR_ATTEN_END_PARAMETER, nearAttEnd);

		bool useFarAtt = (light->GetUseAtten() == TRUE);
		extra->AddParameter(DAEMAX_USE_FAR_ATTEN_PARAMETER, useFarAtt);

		float farAttStart = light->GetAtten(TIME_EXPORT_START, ATTEN_START);
		extra->AddParameter(DAEMAX_FAR_ATTEN_START_PARAMETER, farAttStart);

		float farAttEnd = light->GetAtten(TIME_EXPORT_START, ATTEN_END);
		extra->AddParameter(DAEMAX_FAR_ATTEN_END_PARAMETER, farAttEnd);

		ExportShadowParams(light, extra);
		ExportLightMap(light, extra);

		ENTE->ExportEntity(light, colladaLight, (fm::string(node->GetName()) + "-light").c_str());
	}
	else // isSky
	{
		// add some extra information
		// missing animation, but that's better than nothing
		FCDETechnique *extra = colladaLight->GetExtra()->GetDefaultType()->AddTechnique(DAEMAX_MAX_PROFILE);
		FCDENode *skyNode = extra->AddChildNode(DAEMAX_SKY_LIGHT);

		bool intensityOn = parameters2->GetInt(MaxLight::PB_SKY_MULTIPLIER_ON, TIME_EXPORT_START) == TRUE;
		skyNode->AddParameter(DAEMAX_SKY_INTENSITY_ON, intensityOn);

		bool skyMode = parameters2->GetInt(MaxLight::PB_SKY_MODE, TIME_EXPORT_START) == TRUE;
		skyNode->AddParameter(DAEMAX_SKY_SKYMODE, skyMode);

		int raysPerSample = parameters2->GetInt(MaxLight::PB_SKY_RAYS_PER_SAMPLE, TIME_EXPORT_START);
		skyNode->AddParameter(DAEMAX_SKY_RAYS_PER_SAMPLE_PARAMETER, raysPerSample);

		float rayBias = parameters2->GetFloat(MaxLight::PB_SKY_RAY_BIAS, TIME_EXPORT_START);
		skyNode->AddParameter(DAEMAX_SKY_RAY_BIAS_PARAMETER, rayBias);

		bool castShadows = parameters2->GetInt(MaxLight::PB_SKY_CAST_SHADOWS, TIME_EXPORT_START) == TRUE;
		skyNode->AddParameter(DAEMAX_SKY_CAST_SHADOWS_PARAMETER, castShadows);

		Texmap *colorMap = parameters2->GetTexmap(MaxLight::PB_SKY_COLOR_MAP, TIME_EXPORT_START);
		if (colorMap != NULL)
		{
			FCDImage *colladaImage = document->GetMaterialExporter()->ExportImage(colorMap);
			if (colladaImage != NULL)
			{
				skyNode->AddParameter(DAEMAX_SKY_COLOR_MAP, colladaImage->GetDaeId());
			}
		}

		bool skyColorMapOn = parameters2->GetInt(MaxLight::PB_SKY_SKY_COLOR_MAP_ON, TIME_EXPORT_START) == TRUE;
		skyNode->AddParameter(DAEMAX_SKY_COLOR_MAP_ON_PARAMETER, skyColorMapOn);

		float skyColorMapAmount = parameters2->GetFloat(MaxLight::PB_SKY_SKY_COLOR_MAP_AMOUNT, TIME_EXPORT_START);
		skyNode->AddParameter(DAEMAX_SKY_COLOR_MAP_AMOUNT_PARAMETER, skyColorMapAmount);

		// Export the custom attributes for the mesh
		ENTE->ExportEntity(object, colladaLight, (fm::string(node->GetName()) + "-light").c_str());
	}

	return colladaLight;
}

void LightExporter::LinkLights()
{
	// Once the scene graph is fully exported: connect the targeted lights with their targets
	size_t lightCount = CDOC->GetLightLibrary()->GetEntityCount();
	for (size_t index = 0; index < lightCount; ++index)
	{
		FCDLight* colladaLight = CDOC->GetLightLibrary()->GetEntity(index);
		INode* targetNode = (INode*) colladaLight->GetUserHandle();
		if (targetNode == NULL) continue;
		FCDSceneNode* colladaTarget = document->GetNodeExporter()->FindExportedNode(targetNode);
		FUAssert(colladaTarget != NULL, continue);
		colladaLight->SetTargetNode(colladaTarget);
	}
}

FCDEntity* LightExporter::ExportAmbientLight(const Point3& ambient)
{
	FCDLight* light = CDOC->GetLightLibrary()->AddEntity();
	light->SetName(FC("Ambient"));
	light->SetDaeId("ambient-environment-light");
	light->SetLightType(FCDLight::AMBIENT);
	light->SetColor(ToFMVector3(ambient));
	return light;
}

void LightExporter::CalculateForcedNodes(INode* node, LightObject* object, NodeList& forcedNodes)
{
	if (node == NULL || object == NULL || object->SuperClassID() != LIGHT_CLASS_ID) return;

	bool isTargeted = object->ClassID().PartA() == SPOT_LIGHT_CLASS_ID || object->ClassID().PartA() == TDIR_LIGHT_CLASS_ID;
	if (isTargeted)
	{
		INode* targetNode = node->GetTarget();
		if (targetNode != NULL) forcedNodes.push_back(targetNode);
	}
}


void LightExporter::ExportShadowParams(GenLight* light, FCDENode* extra)
{
	// Export light shadow attributes
	if (light->GetShadow())
	{
		// Light casts shadows
		FCDENode* shadow = extra->AddParameter(DAEMAX_SHADOW_ATTRIBS, "");
		
		int shadType = light->GetShadowType();
		if (shadType == 0) shadow->AddParameter(DAEMAX_SHADOW_TYPE, DAEMAX_SHADOW_TYPE_MAP);
		else if (shadType == 1) shadow->AddParameter(DAEMAX_SHADOW_TYPE, DAEMAX_SHADOW_TYPE_RAYTRACE);
		/*ShadowType* shadowType = light->GetShadowGenerator();
		if (shadowType != NULL)
		{
			TSTR typeName;
			shadowType->GetClassName(typeName);
			shadow->AddParameter(DAEMAX_SHADOW_TYPE, typeName.data());
		} */

		// GetExclusion list
		ExclList& exclusions = light->GetExclusionList();
		int size = exclusions.Count();
		//NodeExporter* nodeExporter = document->GetNodeExporter();
		if (size > 0)
		{
			FCDENode* excludeNode = shadow->AddParameter(DAEMAX_SHADOW_AFFECTS, "");
			fm::string nodeIds;
			for (int i = 0; i < size; i++)
			{
				if (i > 0) nodeIds += " ";
				INode* affected = exclusions[i];
				if (affected == NULL) continue;
				//FCDSceneNode* colladaNode = nodeExporter->FindExportedNode
				TSTR name = affected->GetName();
				// Pretend we are a DaeID, this will work for the moment
				fm::string fsname = FCDObjectWithId::CleanId(name) + "-node";
				nodeIds += fsname;
			}
			excludeNode->AddParameter(DAEMAX_SHADOW_LIST_NODES, nodeIds.c_str());

			if (!exclusions.TestFlag(NT_INCLUDE))
			{
				excludeNode->AddParameter(DAEMAX_SHADOW_LIST_EXCLUDES, 1);
			}
			if (exclusions.TestFlag(NT_AFFECT_ILLUM))
			{
				excludeNode->AddParameter(DAEMAX_SHADOW_LIST_ILLUMINATES, 1);
			}
			if (exclusions.TestFlag(NT_AFFECT_SHADOWCAST))
			{
				excludeNode->AddParameter(DAEMAX_SHADOW_LIST_CASTS, 1);
			}
		}
		
		// Export our shadow projection map
		Texmap* shadowProj = light->GetShadowProjMap();
		if (shadowProj != NULL)
		{
			FCDENode* shadowMap = shadow->AddParameter(DAEMAX_SHADOW_MAP, "");

			IParamBlock* pblock = (IParamBlock*)light->GetReference(MaxLight::PBLOCK_REF);
			int shadowProjOn;
			pblock->GetValue(MaxLight::PB_OMNISHAD_COLMAP, 0, shadowProjOn, Interval::FOREVER);
			if (shadowProjOn)
			{
				FCDImage* fcdShadowMap = document->GetMaterialExporter()->ExportImage(shadowProj);
				FUAssert(fcdShadowMap != NULL, return);
				shadowMap->AddParameter(DAEMAX_PROJ_IMAGE, fcdShadowMap->GetDaeId());				
			}
			else
			{
				// Non-animatable is all they get
				Point3 sc = light->GetShadColor(TIME_EXPORT_START);
				FMVector3 shadowColor(sc.x, sc.y, sc.z);
				shadowMap->AddParameter(DAEMAX_SHADOW_PROJ_COLOR, shadowColor);
				shadowMap->AddParameter(DAEMAX_SHADOW_PROJ_COLOR_MULT, light->GetShadMult(0));
			}
			shadowMap->AddParameter(DAEMAX_LIGHT_AFFECTS_SHADOW, light->GetLightAffectsShadow());
		}
	}
}

void LightExporter::ExportLightMap(GenLight* light, FCDENode* extra)
{
	FUAssert(light != NULL, return);

	if (light->GetProjector())
	{
		Texmap* projMap = light->GetProjMap();
		if (projMap == NULL) return;

		FCDENode* lightMap = extra->AddChildNode(DAEMAX_LIGHT_MAP);
		
		FCDImage* fcdLightMap = document->GetMaterialExporter()->ExportImage(projMap);
		FUAssert(fcdLightMap != NULL, return);
		lightMap->AddParameter(DAEMAX_PROJ_IMAGE, fcdLightMap->GetDaeId());				
	}
}