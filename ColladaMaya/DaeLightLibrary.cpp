/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/*
* COLLADA Light Library
*/

#include "StdAfx.h"
#include <maya/MFnAmbientLight.h>
#include <maya/MFnSpotLight.h>
#include <maya/MFnSet.h>
#include <maya/MSelectionList.h>
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDExtra.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDLight.h"
#include "FCDocument/FCDLightTools.h"
#include "FCDocument/FCDPlaceHolder.h"
#include "FCDocument/FCDEntityReference.h"
#include "FCDocument/FCDAnimationCurve.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "DaeDocNode.h"
#include "CReferenceManager.h"

DaeLightLibrary::DaeLightLibrary(DaeDoc* doc)
:	DaeBaseLibrary(doc)
{
}


DaeLightLibrary::~DaeLightLibrary()
{
}

// Add a light to this library and return the export Id
DaeEntity* DaeLightLibrary::ExportLight(const MObject& lightNode)
{
	// Retrieve the Maya light object
	MStatus status;
	MFnLight lightFn(lightNode, &status); CHECK_MSTATUS(status);
	if (status != MStatus::kSuccess) return NULL;

	// Create the COLLADA light and set its basic information
	FCDLight* colladaLight = CDOC->GetLightLibrary()->AddEntity();
	DaeEntity* element = new DaeEntity(doc, colladaLight, lightNode);
	if (element->isOriginal)
	{
		// Generate a COLLADA id for the new light object
		MString libId = lightFn.name() + "-lib";
		colladaLight->SetDaeId(libId.asChar());
	}
	colladaLight->SetName(MConvert::ToFChar(lightFn.name()));

	// Figure out the type of light
	MFn::Type type = lightNode.apiType();
	switch (type)
	{
	case MFn::kAmbientLight: colladaLight->SetLightType(FCDLight::AMBIENT); break; 
	case MFn::kDirectionalLight: colladaLight->SetLightType(FCDLight::DIRECTIONAL); break;
	case MFn::kSpotLight: colladaLight->SetLightType(FCDLight::SPOT); break;
	case MFn::kPointLight: // Intentional pass-through
	default: colladaLight->SetLightType(FCDLight::POINT); break;
	}

	// Color/Intensity are the only common attributes of all lights
	colladaLight->SetColor(MConvert::ToFMVector(lightFn.color(&status))); CHECK_MSTATUS(status);
	ANIM->AddPlugAnimation(lightNode, "color", colladaLight->GetColor(), kColour);
	colladaLight->SetIntensity(lightFn.intensity(&status)); CHECK_MSTATUS(status);
	ANIM->AddPlugAnimation(lightNode, "intensity", colladaLight->GetIntensity(), kSingle);

	// Add the type specific attributes
	if (lightNode.hasFn(MFn::kNonAmbientLight))
	{
		// Needed Point and Spot light types parameters: Attenuation and Attenuation_Scale
		// Attenuation in COLLADA is equal to Decay in Maya.
		// 
		MFnNonAmbientLight naLightFn(lightNode);
		int decayRate = naLightFn.decayRate(&status); CHECK_MSTATUS(status);
		decayRate = min(decayRate, 2); decayRate = max(decayRate, 0);

		colladaLight->SetConstantAttenuationFactor((decayRate == 0) ? 1.0f : 0.0f);
		colladaLight->SetLinearAttenuationFactor((decayRate == 1) ? 1.0f : 0.0f);
		colladaLight->SetQuadraticAttenuationFactor((decayRate == 2) ? 1.0f : 0.0f);
	}
	else if (lightNode.hasFn(MFn::kAmbientLight))
	{
		MFnAmbientLight ambientLightFn(lightNode);
		FCDETechnique* mayaTechnique = colladaLight->GetExtra()->GetDefaultType()->AddTechnique(DAEMAYA_MAYA_PROFILE);
		FCDENode* ambientShadeParameter = mayaTechnique->AddParameter(DAEMAYA_AMBIENTSHADE_LIGHT_PARAMETER, ambientLightFn.ambientShade());
		ANIM->AddPlugAnimation(lightNode, "ambientShade", ambientShadeParameter->GetAnimated(), kSingle);
	}

	if (lightNode.hasFn(MFn::kSpotLight))
	{
		// Put in the needed spot light type attributes : Falloff, Falloff_Scale and Angle
		MFnSpotLight spotFn(lightNode);

		float fallOffAngle = FMath::RadToDeg((float)spotFn.coneAngle(&status)); CHECK_MSTATUS(status);
		colladaLight->SetFallOffAngle(fallOffAngle);
		ANIM->AddPlugAnimation(lightNode, "coneAngle", colladaLight->GetFallOffAngle(), kSingle | kAngle);
		colladaLight->SetFallOffExponent(1.0f);

		float penumbraValue = FMath::RadToDeg((float)spotFn.penumbraAngle(&status)); CHECK_MSTATUS(status);
		ANIM->AddPlugAnimation(lightNode, "penumbraAngle", colladaLight->GetOuterAngle(), kSingle | kAngle);
		FCDLightTools::LoadPenumbra(colladaLight, penumbraValue, colladaLight->GetOuterAngle().GetAnimated());

		colladaLight->SetDropoff((float) spotFn.dropOff(&status)); CHECK_MSTATUS(status);
		ANIM->AddPlugAnimation(lightNode, "dropoff", colladaLight->GetDropoff(), kSingle);
	}

	doc->GetEntityManager()->ExportEntity(lightNode, colladaLight);
	return element;
}

void DaeLightLibrary::ExportLight(FCDSceneNode* sceneNode, const MDagPath& dagPath, bool isLocal)
{
	FUAssert(sceneNode != NULL, return);

	// Add this light to our library
	if (isLocal)
	{
		DaeEntity* entity = ExportLight(dagPath.node());
		if (entity != NULL)
		{
			doc->GetSceneGraph()->ExportInstance(sceneNode, entity->entity);
			doc->GetSceneGraph()->AddElement(entity);
		}
	}
	else
	{
		// Check for an entity linkage.
		DaeEntity* entity = NODE->FindEntity(dagPath.node());
		if (entity != NULL) doc->GetSceneGraph()->ExportInstance(sceneNode, entity->entity);
		else
		{
			// Generate the XRef URI.
			FCDPlaceHolder* placeHolder = CReferenceManager::GetPlaceHolder(CDOC, dagPath);
			MString name = doc->MayaNameToColladaName(MFnDagNode(dagPath).name(), true);
			FUUri uri(placeHolder->GetFileUrl());
			uri.SetFragment(MConvert::ToFChar(name));

			FCDEntityInstance* colladaInstance = sceneNode->AddInstance(FCDEntity::LIGHT);
			FCDEntityReference* xref = colladaInstance->GetEntityReference();
			xref->SetUri(uri);
		}
	}
}

void DaeLightLibrary::Import()
{
	FCDLightLibrary* library = CDOC->GetLightLibrary();
	for (size_t i = 0; i < library->GetEntityCount(); ++i)
	{
		FCDLight* light = library->GetEntity(i);
		if (light->GetUserHandle() == NULL) ImportLight(light);
	}
}

// Create a Maya light from the COLLADA light entity
DaeEntity* DaeLightLibrary::ImportLight(FCDLight* colladaLight)
{
	MStatus status;

	// Verify that we haven't imported this light already
	if (colladaLight->GetUserHandle() != NULL)
	{
		return (DaeEntity*) colladaLight->GetUserHandle();
	}

	// Create the connection object for this light
	DaeEntity* element = new DaeEntity(doc, colladaLight, MObject::kNullObj);

	// Figure out the maya type for this light
	const char* mayaType;
	switch (colladaLight->GetLightType())
	{
	case FCDLight::AMBIENT: mayaType = "ambientLight"; break;
	case FCDLight::DIRECTIONAL: mayaType = "directionalLight"; break;
	case FCDLight::POINT: mayaType = "pointLight"; break;
	case FCDLight::SPOT: mayaType = "spotLight"; break;
	default: MGlobal::displayWarning(MString("Unknown type for light: ") + colladaLight->GetDaeId());
	}

	// Create a new Maya light object
	// Strangely: Maya returns and uses the transform object, rather than the light object.
	MFnDagNode dagFn;
	MObject light = dagFn.create(mayaType, colladaLight->GetName().c_str(), MObject::kNullObj, &status); CHECK_MSTATUS(status);
	element->SetNode(light = dagFn.child(0, &status)); CHECK_MSTATUS(status);
	CDagHelper::SetPlugValue(light, "visibility", false);

	MFnLight lightFn(light, &status); CHECK_MSTATUS(status);
	MFn::Type lightType = light.apiType();

	// Set the standard light parameters
	CHECK_MSTATUS(lightFn.setColor(MConvert::ToMColor(*colladaLight->GetColor())));
	ANIM->AddPlugAnimation(lightFn.object(), "color", colladaLight->GetColor(), kColour);
	CHECK_MSTATUS(lightFn.setIntensity(colladaLight->GetIntensity()));
	ANIM->AddPlugAnimation(lightFn.object(), "intensity", colladaLight->GetIntensity(), kSingle);

    if (lightType == MFn::kPointLight || lightType == MFn::kSpotLight)
	{
		// Set the decay rate. Maya doesn't support animated decay or scaled decay,
		// so grab either the largest animated decay or the largest valued decay.
		FCDParameterAnimatableFloat& quadDecayFactor = colladaLight->GetQuadraticAttenuationFactor();
		FCDParameterAnimatableFloat& linDecayFactor = colladaLight->GetLinearAttenuationFactor();
		FCDParameterAnimatableFloat& constDecayFactor = colladaLight->GetConstantAttenuationFactor();

		MFnNonAmbientLight nalightFn(light, &status); CHECK_MSTATUS(status);
		short decayRate;
		if (quadDecayFactor.IsAnimated()) decayRate = 2;
		else if (linDecayFactor.IsAnimated()) decayRate = 1;
		else if (constDecayFactor.IsAnimated()) decayRate = 0;
		else if (*quadDecayFactor > *linDecayFactor && *quadDecayFactor > *constDecayFactor) decayRate = 2;
		else if (*linDecayFactor > *constDecayFactor) decayRate = 1;
		else decayRate = 0;
		CHECK_MSTATUS(nalightFn.setDecayRate(decayRate));
	}

	if (lightType == MFn::kSpotLight)
	{
		// Set the Maya spot light parameters
		MFnSpotLight spotLightFn(light, &status); CHECK_MSTATUS(status);

		// Drop-off
		float dropOff;
		if (IsEquivalent(colladaLight->GetFallOffExponent(), 0.0f)) dropOff = 0.0f;
		else dropOff = 1.0f; // COLLADA 1.4 took out the fallOffScale, for some reason.
		CHECK_MSTATUS(spotLightFn.setDropOff(dropOff));

		// convert the outer angle to penumbra.. no one should be using outer angle after this, so it is ok
		// this must also be called before adding the falloff animation as it will convert its values too
		ANIM->OffsetAnimationWithAnimation(colladaLight->GetOuterAngle(), colladaLight->GetFallOffAngle());

		// Cone and Penumbra Angles
		FCDParameterAnimatableFloat& fallOffAngle = colladaLight->GetFallOffAngle();
		CHECK_MSTATUS(spotLightFn.setConeAngle(FMath::DegToRad(*fallOffAngle)));
		ANIM->AddPlugAnimation(lightFn.object(), "coneAngle", fallOffAngle, kSingle | kAngle);

		CHECK_MSTATUS(spotLightFn.setPenumbraAngle(FMath::DegToRad((colladaLight->GetOuterAngle() - colladaLight->GetFallOffAngle()) / 2.0f)));
		FCDConversionScaleFunctor* functor = new FCDConversionScaleFunctor(FMath::DegToRad(1.0f) / 2); // penumbra is half angle
		ANIM->AddPlugAnimation(lightFn.object(), "penumbraAngle", colladaLight->GetOuterAngle(), kSingle | kAngle, functor);

		CHECK_MSTATUS(spotLightFn.setDropOff(colladaLight->GetDropoff()));
		ANIM->AddPlugAnimation(lightFn.object(), "dropoff", colladaLight->GetDropoff(), kSingle);
	}

	if (lightType == MFn::kAmbientLight)
	{
		// Import the ambient shade parameter
		MFnAmbientLight ambientLightFn(light, &status); CHECK_MSTATUS(status);
		float ambientShade = 0.0f;
		FCDETechnique* mayaTechnique = colladaLight->GetExtra()->GetDefaultType()->FindTechnique(DAEMAYA_MAYA_PROFILE);
		if (mayaTechnique != NULL)
		{
			FCDENode* ambientShadeParameter = mayaTechnique->FindParameter(DAEMAYA_AMBIENTSHADE_LIGHT_PARAMETER);
			if (ambientShadeParameter != NULL)
			{
				ambientShade = FUStringConversion::ToFloat(ambientShadeParameter->GetContent());
				ANIM->AddPlugAnimation(lightFn.object(), "ambientShade", ambientShadeParameter->GetAnimated(), kSingle);
			}
		}
		CHECK_MSTATUS(ambientLightFn.setAmbientShade(ambientShade));
	}

	MObject elementNode = element->GetNode();
	doc->GetEntityManager()->ImportEntity(elementNode, element);
	return element;
}

void DaeLightLibrary::InstantiateLight(DaeEntity* sceneNode, FCDLight* colladaLight)
{
	MStatus status;

	DaeEntity* light = ImportLight(colladaLight);
	if (light == NULL) return;

	// Parent the light to this scene node
	light->Instantiate(sceneNode);

	// Add this light to the default light set
	MFnSet defaultLightSet(CDagHelper::GetNode("defaultLightSet"), &status); CHECK_MSTATUS(status);
	CHECK_MSTATUS(defaultLightSet.addMember(sceneNode->GetNode()));

	// Add this light to the default light list
	MObject lightListNode(CDagHelper::GetNode("lightList1"));
	CDagHelper::ConnectToList(light->GetNode(), "lightData", lightListNode, "lights");
}
