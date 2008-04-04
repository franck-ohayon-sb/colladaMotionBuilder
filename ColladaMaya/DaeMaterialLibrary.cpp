/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include <maya/MFnLambertShader.h>
#include <maya/MFnPhongShader.h>
#include <maya/MFnBlinnShader.h>
#include <maya/MDGModifier.h>
#include <maya/MFnSet.h>
#include <maya/MFnStringArrayData.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MItDependencyNodes.h>
#include "TranslatorHelpers/CShaderHelper.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "CExportOptions.h"
#include "CMeshExporter.h"
#include "CReferenceManager.h"
#include "DaeDocNode.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDAnimationCurve.h"
#include "FCDocument/FCDEffect.h"
#include "FCDocument/FCDEffectTools.h"
#include "FCDocument/FCDEffectCode.h"
#include "FCDocument/FCDEffectParameter.h"
#include "FCDocument/FCDEffectParameterSampler.h"
#include "FCDocument/FCDEffectParameterSurface.h"
#include "FCDocument/FCDEffectPass.h"
#include "FCDocument/FCDEffectPassShader.h"
#include "FCDocument/FCDEffectPassState.h"
#include "FCDocument/FCDEffectProfileFX.h"
#include "FCDocument/FCDEffectStandard.h"
#include "FCDocument/FCDEffectTechnique.h"
#include "FCDocument/FCDEntityReference.h"
#include "FCDocument/FCDExtra.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDGeometryInstance.h"
#include "FCDocument/FCDGeometryMesh.h"
#include "FCDocument/FCDGeometryPolygons.h"
#include "FCDocument/FCDGeometryPolygonsInput.h"
#include "FCDocument/FCDGeometrySource.h"
#include "FCDocument/FCDImage.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDMaterial.h"
#include "FCDocument/FCDMaterialInstance.h"
#include "FCDocument/FCDTexture.h"
#include "FUtils/FUFileManager.h"
//#include "FUtils/FUDaeParser.h"
#include "FUtils/FUDaeEnum.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "ColladaFX/CFXShaderNode.h"
#include "ColladaFX/CFXPasses.h"
#include "ColladaFX/CFXParameter.h"
#include "ColladaFX/CFXRenderState.h"
//using namespace FUDaeParser;

#ifdef BLINN_EXPONENT_MODEL
class BlinnEccentricityToShininess : public FCDConversionFunctor
{
public:
	virtual float operator() (float v) { return 100.0f - 99.0f * sqrtf(v); }
};

class BlinnShininessToEccentricity : public FCDConversionFunctor
{
public:
	virtual float operator() (float v) { float out = (100.0f - v) / 99.0f; return out * out; }
};
#endif // BLINN_EXPONENT_MODEL

//
// DaeMaterial
//

// [GLaforte 05-01-2007] This nasty copy-paste is my bad!
MObject DaeMaterial::GetShaderGroup() const
{
	if (shaderGroup.isNull())
	{
		// Retrieve a MObject for this shaderGroup using the name/path.
		DaeMaterial* _this = const_cast<DaeMaterial*>(this);
		if (shaderGroupName.length() > 0)
		{
			_this->shaderGroup = CDagHelper::GetNode(shaderGroupName);
			_this->shaderGroupName = "";
		}
	}
	return shaderGroup;
}

void DaeMaterial::SetShaderGroup(const MObject& _shaderGroup)
{
	shaderGroup = _shaderGroup;
	shaderGroupName = "";
}

void DaeMaterial::ReleaseNode()
{
	// Call the parent to release the MObject for "node".
	DaeEntity::ReleaseNode();

	if (shaderGroup.isNull()) return;
	
	// For DG nodes, simply use the node's name, which should be unique.
	shaderGroupName = MFnDependencyNode(shaderGroup).name();
	shaderGroup = MObject::kNullObj;
}

//
// DaeMaterialLibrary
//

DaeMaterialLibrary::DaeMaterialLibrary(DaeDoc* _doc)
:	DaeBaseLibrary(_doc)
{
}

DaeMaterialLibrary::~DaeMaterialLibrary()
{
	CLEAR_POINTER_STD_PAIR_CONT(FCDocumentFilenameMap, externalLibraries);
}

void DaeMaterialLibrary::Export()
{
	MObject defaultShaderList = CDagHelper::GetNode("defaultShaderList1");
	MPlug defaultShadersPlug = MFnDependencyNode(defaultShaderList).findPlug("shaders");
	uint shaderCount = defaultShadersPlug.evaluateNumElements();
	for (uint i = 0; i < shaderCount; ++i)
	{
		MObject shader = CDagHelper::GetNodeConnectedTo(defaultShadersPlug.elementByPhysicalIndex(i));
		MFnDependencyNode shadingEngineFn(shader);
		if (!shadingEngineFn.isFromReferencedFile() && !shadingEngineFn.isDefaultNode())
		{
			MObject shadingEngine = CShaderHelper::GetShadingEngine(shader);
			ExportMaterial(shadingEngine);
		}
	}
}

//
// Add a shading network to this library and return the export Id
//
DaeMaterial* DaeMaterialLibrary::ExportMaterial(MObject shadingEngine)
{
	DaeMaterial* material = NULL;
	MObject shader = CDagHelper::GetSourceNodeConnectedTo(shadingEngine, "surfaceShader");

	// Check for XRef.
	MFnDependencyNode shadingEngineFn(shadingEngine);
	bool isReference = shadingEngineFn.isFromReferencedFile();
	if (isReference && !CExportOptions::ExportXRefs()) return NULL;
	else if (isReference && CExportOptions::ExportXRefs() && !CExportOptions::DereferenceXRefs())
	{
		// Overwrite the id of the material with the id saved by the DaeDocNode.
		DaeDoc* xrefDocument = NULL;
		DaeEntity* xrefMaterial = doc->GetCOLLADANode()->FindEntity(shader,  &xrefDocument);
		if (xrefDocument != NULL && xrefMaterial != NULL && xrefMaterial->entity->GetObjectType().Includes(FCDMaterial::GetClassType()))
		{
			material = (DaeMaterial*) xrefMaterial;
		}
		else
		{
			MString referenceFilename = CReferenceManager::GetReferenceFilename(shader);
			FCDocumentFilenameMap::iterator it = externalLibraries.find(referenceFilename);
			if (it == externalLibraries.end())
			{
				// Create a document to hold this document's information.
				FCDocument* d = FCollada::NewTopDocument();
				d->SetFileUrl(MConvert::ToFChar(referenceFilename));
				it = externalLibraries.insert(referenceFilename, d);
			}
			material = ExportMaterial(shader, it->second);
		}
	}
	else
	{
		material = ExportMaterial(shader, CDOC);
	}

	return material;
}

DaeMaterial* DaeMaterialLibrary::ExportMaterial(MObject shadingEngine, FCDocument* colladaDocument)
{
	// Find the actual shader node, since this function received shading sets as input
	MStatus status;
	MFnDependencyNode shaderNode(shadingEngine, &status);
	if (status != MStatus::kSuccess) return NULL;

	// Have we seen this shader before?
	ShaderMaterialMap::iterator itId = exportedShaderMap.find(shadingEngine);
	if (itId != exportedShaderMap.end()) return (*itId).second;

	// This is a new shading engine
	FCDMaterial* colladaMaterial = colladaDocument->GetMaterialLibrary()->AddEntity();
	DaeMaterial* material = new DaeMaterial(doc, colladaMaterial, shadingEngine);
	exportedShaderMap[shadingEngine] = material;
	// ... don't keep the cloned copy of parameters.
	while (colladaMaterial->GetEffectParameterCount() != 0) colladaMaterial->GetEffectParameter(colladaMaterial->GetEffectParameterCount() - 1)->Release();

	fstring colladaName = MConvert::ToFChar(doc->MayaNameToColladaName(shaderNode.name(), true));
	colladaMaterial->SetName(colladaName);
	if (material->isOriginal) colladaMaterial->SetDaeId(TO_STRING(colladaName));

	// Add the correct effect for the material
	FCDEffect* effect;
	if (shadingEngine.hasFn(MFn::kLambert))
	{
		effect = ExportStandardShader(colladaDocument, shadingEngine);
	}
	else if (shadingEngine.hasFn(MFn::kPluginHwShaderNode) && shaderNode.typeName() == "colladafxShader")
	{
		effect = ExportColladaFXShader(colladaDocument, shadingEngine, colladaMaterial);
	}
	else if (shadingEngine.hasFn(MFn::kPluginHwShaderNode) && shaderNode.typeName() == "colladafxPasses")
	{
		effect = ExportColladaFXPasses(colladaDocument, shadingEngine, colladaMaterial);
	}
	else
	{
		// For the constant shader, you should use the "surface shader" node in Maya
		// But always export some material parameters, even if we don't know this material.
		effect = ExportConstantShader(colladaDocument, shadingEngine);
	}
	effect->SetDaeId(colladaMaterial->GetDaeId() + "-fx");
	colladaMaterial->SetEffect(effect);

	doc->GetEntityManager()->ExportEntity(shadingEngine, colladaMaterial);
	return material;
}

// export ColladaFX passes
FCDEffect* DaeMaterialLibrary::ExportColladaFXPasses(FCDocument* colladaDocument, MObject shadingNetwork, FCDMaterial* instance)
{
	// Create an effect with the CG profile.
	FCDEffect* effect = colladaDocument->GetEffectLibrary()->AddEntity();
	FCDEffectProfileFX* profile = (FCDEffectProfileFX*) effect->AddProfile(FUDaeProfileType::CG);
	
	// at this point we're sure that this node is of type colladafxPasses
	MFnDependencyNode nodeFn(shadingNetwork);
	CFXPasses* colladaFXPasses = (CFXPasses*)nodeFn.userNode();

	// for each technique
	MStringArray aTechNames;
	colladaFXPasses->getTechniqueNames(aTechNames);
	for (uint i = 0; i < aTechNames.length(); ++i)
	{
		MString techNameDecorated = aTechNames[i];
		MString techName = MString(techNameDecorated.asChar() + 3); // removes the "cfx" prefix
		FCDEffectTechnique* technique = profile->AddTechnique();
		technique->SetName(MConvert::ToFChar(techName));

		CFXShaderNodeList passes;
		colladaFXPasses->getTechniquePasses(techName,passes);
		
		// for each pass
		for (uint j = 0; j < passes.size(); ++j)
		{
			// export effect parameters
			ExportNewParameters(profile, passes[j]->thisMObject(), instance);
			// export pass information
			FCDEffectPass* newPass = technique->AddPass();
			ExportPass(profile, newPass, passes[j]->thisMObject(), instance);
			MString passName = colladaFXPasses->getPassName(techName,j);
			if (passName.length() > techNameDecorated.length() + 1)
			{
				newPass->SetPassName(MConvert::ToFChar(passName) + techNameDecorated.length() + 1); // removes cfx[tech]_
			}
		}
	}

	// Export a <technique_hint>.
	FCDMaterialTechniqueHint hint;
	hint.platform = FC("PC-OGL");
	MPlug chosenTechPlug(colladaFXPasses->thisMObject(), CFXPasses::aChosenTechniqueName);
	MString chosenTechName; chosenTechPlug.getValue(chosenTechName);
	if (chosenTechName.length() > 0)
	{
		hint.technique = (chosenTechName.asChar() + 3); // removes the "cfx" prefix
		instance->GetTechniqueHints().push_back(hint);
	}

	return effect;
}

// Export ColladaFX shader
FCDEffect* DaeMaterialLibrary::ExportColladaFXShader(FCDocument* colladaDocument, MObject shadingNetwork, FCDMaterial* instance)
{
	FCDEffect* effect = colladaDocument->GetEffectLibrary()->AddEntity();
	MFnDependencyNode nodeFn(shadingNetwork);
	CFXShaderNode* fxShader = (CFXShaderNode*) nodeFn.userNode();
	
	// Retrieve the ColladaFX shader node and force its loading.
	CFXShaderNode* colladaFxNode = (CFXShaderNode*) nodeFn.userNode();
	if (colladaFxNode == NULL) return effect;
	if (!colladaFxNode->IsLoaded()) colladaFxNode->forceLoad();

	FCDEffectProfileFX* profile = (FCDEffectProfileFX*) effect->AddProfile(FUDaeProfileType::CG);
	
	// export effect parameters
	ExportNewParameters(profile, shadingNetwork, instance);

	// add effect technique node
	FCDEffectTechnique* technique = profile->AddTechnique();
	technique->SetName(fxShader->getTechniqueName());
	FCDEffectPass* pass = technique->AddPass();
	pass->SetPassName(fxShader->getPassName());
	ExportPass(profile, pass, shadingNetwork, instance);

	// Export a <technique_hint>.
	FCDMaterialTechniqueHint hint;
	hint.platform = FC("PC-OGL");
	hint.technique = TO_STRING(fxShader->getTechniqueName());
	instance->GetTechniqueHints().push_back(hint);

	return effect;
}

void DaeMaterialLibrary::ExportPass(FCDEffectProfileFX* profile, FCDEffectPass* pass, MObject shaderObj, FCDMaterial* instance)
{
	profile->SetPlatform(FC("PC-OGL"));

	// Retrieve the ColladaFX shader node and make sure it is loaded.
	MFnDependencyNode shaderFn(shaderObj);
	CFXShaderNode* pNode = (CFXShaderNode*)shaderFn.userNode();
	if (pNode == NULL) return;
	if (!pNode->IsLoaded()) pNode->forceLoad();

	pass->SetPassName(MConvert::ToFChar(doc->MayaNameToColladaName(shaderFn.name())));
	FCDEffectPassShader* vertexShader = pass->AddVertexShader();
	FCDEffectPassShader* fragmentShader = pass->AddFragmentShader();
	
	vertexShader->SetName(pNode->getVertexEntry());
	fragmentShader->SetName(pNode->getFragmentEntry());

	MString f_strM;
	CDagHelper::GetPlugValue(shaderObj, "vertexProgram", f_strM);
	FCDEffectCode* vCode = profile->AddCode();
	vCode->SetFilename(MConvert::ToFChar(f_strM));
	vertexShader->SetCode(vCode);

	CDagHelper::GetPlugValue(shaderObj, "fragmentProgram", f_strM);
	vCode = profile->AddCode();
	vCode->SetFilename(MConvert::ToFChar(f_strM));
	fragmentShader->SetCode(vCode);

	ExportProgramBinding(vertexShader, MString("vertParam"), shaderFn);
	ExportProgramBinding(fragmentShader, MString("fragParam"), shaderFn);

	// Export the pass render states.
	for (CFXRenderStateList::iterator it = pNode->GetRenderStates().begin(); it != pNode->GetRenderStates().end(); ++it)
	{
		const FCDEffectPassState* fxState = (*it)->GetData();
		FCDEffectPassState* colladaState = pass->AddRenderState(fxState->GetType()); 
		fxState->Clone(colladaState);
	}
}

void DaeMaterialLibrary::ExportNewParameters(FCDEffectProfileFX* profile, const MObject& shader, FCDMaterial* instance)
{
	MFnDependencyNode shaderFn(shader);
	CFXShaderNode* pNode = (CFXShaderNode*)shaderFn.userNode();
	CFXParameterList& parameters = pNode->GetParameters();
	size_t parameterCount = parameters.size();
	for (size_t i = 0; i < parameterCount; ++i)
	{
		CFXParameter* attrib = parameters[i];
		fm::string reference = (shaderFn.name() + attrib->getName()).asChar();

		switch (attrib->getType())
		{
		case CFXParameter::kBool: {
			bool bval;
			CDagHelper::GetPlugValue(attrib->getPlug(), bval);

			FCDEffectParameterBool* parameter = (FCDEffectParameterBool*) profile->AddEffectParameter(FCDEffectParameter::BOOLEAN);
			parameter->SetGenerator();
			parameter->SetSemantic(attrib->getSemanticStringForFXC());
			parameter->SetValue(bval);
			parameter->SetReference(reference);
			parameter->AddAnnotation(TO_FSTRING(CFX_ANNOTATION_UINAME), FCDEffectParameter::STRING, attrib->getName());

			parameter = (FCDEffectParameterBool*) instance->AddEffectParameter(FCDEffectParameter::BOOLEAN);
			parameter->SetModifier();
			parameter->SetReference(reference);
			parameter->SetValue(bval);
			break; }

		case CFXParameter::kHalf:
		case CFXParameter::kFloat: {
			FCDEffectParameterFloat* parameter = (FCDEffectParameterFloat*) profile->AddEffectParameter(FCDEffectParameter::FLOAT);
			parameter->SetGenerator();
			parameter->SetSemantic(attrib->getSemanticStringForFXC());
			parameter->SetValue(attrib->getFloatValue());
			parameter->SetReference(reference);
			parameter->SetFloatType(attrib->getType() == CFXParameter::kFloat ? FCDEffectParameterFloat::FLOAT : FCDEffectParameterFloat::HALF);

			MFnNumericAttribute fnum(attrib->getAttribute());
			double fmin, fmax;
			fnum.getMin(fmin);
			fnum.getMax(fmax);

			parameter->AddAnnotation(TO_FSTRING(CFX_ANNOTATION_UINAME), FCDEffectParameter::STRING, attrib->getName());
			parameter->AddAnnotation(TO_FSTRING(CFX_ANNOTATION_UIMIN), FCDEffectParameter::FLOAT, (float)fmin);
			parameter->AddAnnotation(TO_FSTRING(CFX_ANNOTATION_UIMAX), FCDEffectParameter::FLOAT, (float)fmax);
			
			if (attrib->getSemantic() != CFXParameter::kTIME)
			{
				FCDAnimated* animated = parameter->GetValue().GetAnimated();
				ANIM->AddPlugAnimation(shader, attrib->getAttributeName(), animated, kSingle);
			}

			parameter = (FCDEffectParameterFloat*) instance->AddEffectParameter(FCDEffectParameter::FLOAT);
			parameter->SetModifier();
			parameter->SetValue(attrib->getFloatValue());
			parameter->SetReference(reference);
			parameter->SetFloatType(attrib->getType() == CFXParameter::kFloat ? FCDEffectParameterFloat::FLOAT : FCDEffectParameterFloat::HALF);
			break; }

		case CFXParameter::kFloat2:
		case CFXParameter::kHalf2: {
			FCDEffectParameterFloat2* parameter = (FCDEffectParameterFloat2*) profile->AddEffectParameter(FCDEffectParameter::FLOAT2);
			parameter->SetGenerator();
			parameter->SetSemantic(attrib->getSemanticStringForFXC());
			parameter->SetValue(FMVector2(attrib->getFloatValueX(), attrib->getFloatValueY()));
			parameter->SetReference(reference);
			parameter->SetFloatType(attrib->getType() == CFXParameter::kFloat2 ? FCDEffectParameterFloat2::FLOAT : FCDEffectParameterFloat2::HALF);

			parameter->AddAnnotation(TO_FSTRING(CFX_ANNOTATION_UINAME), FCDEffectParameter::STRING, attrib->getName());

			parameter = (FCDEffectParameterFloat2*) instance->AddEffectParameter(FCDEffectParameter::FLOAT2);
			parameter->SetModifier();
			parameter->SetValue(FMVector2(attrib->getFloatValueX(), attrib->getFloatValueY()));
			parameter->SetReference(reference);
			parameter->SetFloatType(attrib->getType() == CFXParameter::kFloat2 ? FCDEffectParameterFloat2::FLOAT : FCDEffectParameterFloat2::HALF);
			break; }

		case CFXParameter::kHalf3:
		case CFXParameter::kFloat3: {
			FCDEffectParameterFloat3* parameter = (FCDEffectParameterFloat3*) profile->AddEffectParameter(FCDEffectParameter::FLOAT3);
			parameter->SetGenerator();
			parameter->SetSemantic(attrib->getSemanticStringForFXC());
			parameter->SetValue(FMVector3(attrib->getFloatValueX(), attrib->getFloatValueY(), attrib->getFloatValueZ()));
			parameter->SetReference(reference);

			parameter->AddAnnotation(TO_FSTRING(CFX_ANNOTATION_UINAME), FCDEffectParameter::STRING, attrib->getName());
			fstring s = TO_FSTRING(attrib->getUIType().asChar());
			parameter->AddAnnotation(TO_FSTRING(CFX_ANNOTATION_UITYPE), FCDEffectParameter::STRING, s.c_str());

			parameter->SetFloatType(attrib->getType() == CFXParameter::kFloat3 ? FCDEffectParameterFloat3::FLOAT : FCDEffectParameterFloat3::HALF);

			ANIM->AddPlugAnimation(shader, attrib->getAttributeName(), parameter->GetValue().GetAnimated(), kColour);

			parameter = (FCDEffectParameterFloat3*) instance->AddEffectParameter(FCDEffectParameter::FLOAT3);
			parameter->SetModifier();
			parameter->SetValue(FMVector3(attrib->getFloatValueX(), attrib->getFloatValueY(), attrib->getFloatValueZ()));
			parameter->SetReference(reference);
			parameter->SetFloatType(attrib->getType() == CFXParameter::kFloat3 ? FCDEffectParameterFloat3::FLOAT : FCDEffectParameterFloat3::HALF);
			break; }

		case CFXParameter::kFloat4:
		case CFXParameter::kHalf4: {	
			FCDEffectParameterVector* parameter = (FCDEffectParameterVector*) profile->AddEffectParameter(FCDEffectParameter::VECTOR);
			parameter->SetGenerator();
			parameter->SetSemantic(attrib->getSemanticStringForFXC());
			parameter->SetValue(FMVector4(attrib->getFloatValueX(), attrib->getFloatValueY(), attrib->getFloatValueZ(), 0.0f));
			parameter->SetReference(reference);
			
			parameter->AddAnnotation(TO_FSTRING(CFX_ANNOTATION_UINAME), FCDEffectParameter::STRING, attrib->getName());
			fstring s = TO_FSTRING(attrib->getUIType().asChar());
			parameter->AddAnnotation(TO_FSTRING(CFX_ANNOTATION_UITYPE), FCDEffectParameter::STRING, s.c_str());
			
			parameter->SetFloatType(attrib->getType() == CFXParameter::kFloat4 ? FCDEffectParameterVector::FLOAT : FCDEffectParameterVector::HALF);

			ANIM->AddPlugAnimation(shader, attrib->getAttributeName(), parameter->GetValue().GetAnimated(), kColour);

			parameter = (FCDEffectParameterVector*) instance->AddEffectParameter(FCDEffectParameter::VECTOR);
			parameter->SetModifier();
			parameter->SetValue(FMVector4(attrib->getFloatValueX(), attrib->getFloatValueY(), attrib->getFloatValueZ(), 0.0f));
			parameter->SetReference(reference);
			parameter->SetFloatType(attrib->getType() == CFXParameter::kFloat4 ? FCDEffectParameterVector::FLOAT : FCDEffectParameterVector::HALF);
			break; }

		case CFXParameter::kHalf4x4:
		case CFXParameter::kFloat4x4: {
			FCDEffectParameterMatrix* parameter = (FCDEffectParameterMatrix*) profile->AddEffectParameter(FCDEffectParameter::MATRIX);
			parameter->SetGenerator();
			parameter->SetSemantic(attrib->getSemanticStringForFXC());
			parameter->SetValue(FMMatrix44::Identity);
			parameter->SetReference(reference);
			parameter->SetFloatType(attrib->getType() == CFXParameter::kFloat4x4 ? FCDEffectParameterMatrix::FLOAT : FCDEffectParameterMatrix::HALF);

			parameter = (FCDEffectParameterMatrix*) instance->AddEffectParameter(FCDEffectParameter::MATRIX);
			parameter->SetModifier();
			parameter->SetValue(FMMatrix44::Identity);
			parameter->SetReference(reference);
			parameter->SetFloatType(attrib->getType() == CFXParameter::kFloat4x4 ? FCDEffectParameterMatrix::FLOAT : FCDEffectParameterMatrix::HALF);
			break; }

		case CFXParameter::kSampler2D:
		case CFXParameter::kSamplerCUBE: 
			{
				MPlug tex_plug;
				if (CDagHelper::GetPlugConnectedTo(attrib->getPlug(), tex_plug))
				{
					FCDEffectParameterSampler* parameter1 = (FCDEffectParameterSampler*) profile->AddEffectParameter(FCDEffectParameter::SAMPLER);
					parameter1->SetGenerator();
					parameter1->SetSemantic(attrib->getSemanticStringForFXC());
					parameter1->SetReference(reference);
					parameter1->AddAnnotation(TO_FSTRING(CFX_ANNOTATION_UINAME), FCDEffectParameter::STRING, attrib->getName());
					parameter1->SetSamplerType(attrib->getType() == CFXParameter::kSampler2D ? FCDEffectParameterSampler::SAMPLER2D : FCDEffectParameterSampler::SAMPLERCUBE);
		
					FCDEffectParameterSampler* parameter2 = (FCDEffectParameterSampler*) instance->AddEffectParameter(FCDEffectParameter::SAMPLER);
					parameter2->SetModifier();
					parameter2->SetReference(reference);
					parameter2->SetSamplerType(attrib->getType() == CFXParameter::kSampler2D ? FCDEffectParameterSampler::SAMPLER2D : FCDEffectParameterSampler::SAMPLERCUBE);
		
					const char* type = attrib->getType() == CFXParameter::kSampler2D ? "2D" : "CUBE";
				
					// Export the surface.
					FCDEffectParameterSurface* surfaceParameter = writeSurfaceTexture(profile, tex_plug.node(), instance, type);
					parameter1->SetSurface(surfaceParameter);
					parameter2->SetSurface(surfaceParameter);
				}
				break;
			}
		case CFXParameter::kSurface:
		case CFXParameter::kStruct:
		case CFXParameter::kUnknown:
		default:
			// FUFail?
			break;
		}
	}
}

FCDEffect* DaeMaterialLibrary::ExportStandardShader(FCDocument* colladaDocument, MObject shadingNetwork)
{
	FCDEffect* effect = colladaDocument->GetEffectLibrary()->AddEntity();
	FCDEffectStandard* standardFX = (FCDEffectStandard*) effect->AddProfile(FUDaeProfileType::COMMON);

	MFnDependencyNode dgFn(shadingNetwork);
	MFnLambertShader matFn(shadingNetwork);

	int nextTextureIndex = 0;

	// Add the shader element: <constant> and the <extra><technique profile="MAYA"> elements
	if (shadingNetwork.hasFn(MFn::kPhong)) standardFX->SetLightingType(FCDEffectStandard::PHONG);
	else if (shadingNetwork.hasFn(MFn::kBlinn)) standardFX->SetLightingType(FCDEffectStandard::BLINN);
	else standardFX->SetLightingType(FCDEffectStandard::LAMBERT);

	// Emission color / Incandescence
	standardFX->SetEmissionColor(MConvert::ToFMVector4(matFn.incandescence()));
	ExportTexturedParameter(shadingNetwork, "incandescence", standardFX, FUDaeTextureChannel::EMISSION, nextTextureIndex);
	ANIM->AddPlugAnimation(shadingNetwork, "incandescence", standardFX->GetEmissionColorParam()->GetValue(), kColour);

	// Ambient color
	standardFX->SetAmbientColor(MConvert::ToFMVector4(matFn.ambientColor()));
	ExportTexturedParameter(shadingNetwork, "ambientColor", standardFX, FUDaeTextureChannel::AMBIENT, nextTextureIndex);
	ANIM->AddPlugAnimation(shadingNetwork, "ambientColor", standardFX->GetAmbientColorParam()->GetValue(), kColour);

	// Diffuse color
	standardFX->SetDiffuseColor(MConvert::ToFMVector4(matFn.color()) * matFn.diffuseCoeff());
	ExportTexturedParameter(shadingNetwork, "color", standardFX, FUDaeTextureChannel::DIFFUSE, nextTextureIndex);
	ANIM->AddPlugAnimation(shadingNetwork, "color", standardFX->GetDiffuseColorParam()->GetValue(), kColour, new FCDConversionScaleFunctor(matFn.diffuseCoeff()));

	// Transparent color
	ExportTransparency(colladaDocument, shadingNetwork, matFn.transparency(), standardFX, "transparency", nextTextureIndex);

	// Bump textures
	ExportTexturedParameter(shadingNetwork, "normalCamera", standardFX, FUDaeTextureChannel::BUMP, nextTextureIndex);

	if (shadingNetwork.hasFn(MFn::kReflect)) // includes Phong and Blinn
	{
		// Specular color
		MFnReflectShader reflectFn(shadingNetwork);
		standardFX->SetSpecularColor(MConvert::ToFMVector4(reflectFn.specularColor()));
		ExportTexturedParameter(shadingNetwork, "specularColor", standardFX, FUDaeTextureChannel::SPECULAR, nextTextureIndex);
		ANIM->AddPlugAnimation(shadingNetwork, "specularColor", standardFX->GetSpecularColorParam()->GetValue(), kColour);

		// Reflected color
		standardFX->SetReflectivityColor(MConvert::ToFMVector4(reflectFn.reflectedColor()));
		ExportTexturedParameter(shadingNetwork, "reflectedColor", standardFX, FUDaeTextureChannel::REFLECTION, nextTextureIndex);
		ANIM->AddPlugAnimation(shadingNetwork, "reflectedColor", standardFX->GetReflectivityColorParam()->GetValue(), kColour);

		// Reflectivity factor
		standardFX->SetReflectivityFactor(reflectFn.reflectivity());
		ANIM->AddPlugAnimation(shadingNetwork, "reflectivity", standardFX->GetReflectivityFactorParam()->GetValue(), kSingle);
	}
	else
	{
		standardFX->SetReflective(false);
	}

	// index of refraction
	bool refractive;
	CDagHelper::GetPlugValue(shadingNetwork, "refractions", refractive);
	if (refractive)
	{
		standardFX->SetIndexOfRefraction(matFn.refractiveIndex());
		ANIM->AddPlugAnimation(shadingNetwork, "refractiveIndex", standardFX->GetIndexOfRefractionParam()->GetValue(), kSingle);
		standardFX->SetRefractive(true);
	}
	else
	{
		standardFX->SetRefractive(false);
	}

	// Phong and Blinn's specular factor
	if (shadingNetwork.hasFn(MFn::kPhong))
	{
		MFnPhongShader phongFn(shadingNetwork);
		standardFX->SetShininess(phongFn.cosPower());
		ANIM->AddPlugAnimation(shadingNetwork, "cosinePower", standardFX->GetShininessParam()->GetValue(), kSingle);
	}
	else if (shadingNetwork.hasFn(MFn::kBlinn))
	{
#ifdef BLINN_EXPONENT_MODEL
		MFnBlinnShader blinnFn(shadingNetwork);
		BlinnEccentricityToShininess* converter = new BlinnEccentricityToShininess();
		standardFX->SetShininess((*converter)(blinnFn.eccentricity()));
		ANIM->AddPlugAnimation(shadingNetwork, "eccentricity", standardFX->GetShininessParam()->GetValue(), kSingle, converter);
#else
		MFnBlinnShader blinnFn(shadingNetwork);
		standardFX->SetShininess(blinnFn.eccentricity());
		ANIM->AddPlugAnimation(shadingNetwork, "eccentricity", standardFX->GetShininessParam()->GetValue(), kSingle);
#endif // BLINN_EXPONENT_MODEL		
	}

	return effect;
}

void DaeMaterialLibrary::ExportTransparency(FCDocument* colladaDocument, MObject shadingNetwork, const MColor& transparentColor, FCDEffectStandard* fx, const char* transparencyPlugName, int& nextTextureIndex)
{
	fx->SetTranslucencyColor(MConvert::ToFMVector4(transparentColor));
	MObject transparentTextureNode = ExportTexturedParameter(shadingNetwork, transparencyPlugName, fx, FUDaeTextureChannel::TRANSPARENT, nextTextureIndex);
	ANIM->AddPlugAnimation(shadingNetwork, transparencyPlugName, fx->GetTranslucencyColorParam()->GetValue(), kColour);

	// For the 'opaque' attribute, check the plug's name, that's connected to the shader's 'transparency' plug.
	MPlug connectedPlug;
	if (!transparentTextureNode.isNull())
	{
		// DO NOTE: We're missing 2 transparency mode that were wrongly deemed useless in COLLADA 1.4.1.
		// Until then, we cannot use the 'invert' attribute or correctly export the transparency mode.
		MFnDependencyNode texture2DFn(transparentTextureNode);
		bool isInverted = false;
		texture2DFn.findPlug("invert").getValue(isInverted);

		CDagHelper::GetPlugConnectedTo(shadingNetwork, transparencyPlugName, connectedPlug);
		if (connectedPlug.partialName() == "oc") fx->SetTransparencyMode(FCDEffectStandard::RGB_ZERO); // should be RGB_ONE.
		else if (connectedPlug.partialName() == "ot") fx->SetTransparencyMode(FCDEffectStandard::A_ONE);
		else if (connectedPlug.partialName() == "oa") fx->SetTransparencyMode(FCDEffectStandard::A_ONE); // valid?

		if (fx->GetTransparencyMode() == FCDEffectStandard::A_ONE)
		{
			// Export any animation on the alpha gain/alpha offset
			if (CAnimationHelper::IsAnimated(doc->GetAnimationCache(), transparentTextureNode, "alphaGain"))
			{
				ANIM->AddPlugAnimation(transparentTextureNode, "alphaGain", fx->GetTranslucencyFactorParam()->GetValue(), kSingle, NULL);
			}
			else
			{
				ANIM->AddPlugAnimation(transparentTextureNode, "alphaOffset", fx->GetTranslucencyFactorParam()->GetValue(), kSingle, new FCDConversionOffsetFunctor(1.0f));
			}
		}
	}
	else
	{
		fx->SetTransparencyMode(FCDEffectStandard::RGB_ZERO); // correctly RGB_zero.
	}
}

FCDEffect* DaeMaterialLibrary::ExportConstantShader(FCDocument* colladaDocument, MObject shadingNetwork)
{
	// Create the constant effect
	FCDEffect* effect = colladaDocument->GetEffectLibrary()->AddEntity();
	FCDEffectStandard* constantFX = (FCDEffectStandard*) effect->AddProfile(FUDaeProfileType::COMMON);
	constantFX->SetLightingType(FCDEffectStandard::CONSTANT);

	// Set the constant color/texture
	MColor outColor;
	CDagHelper::GetPlugValue(shadingNetwork, "outColor", outColor);
	constantFX->SetEmissionColor(MConvert::ToFMVector4(outColor));
	int nextTextureIndex = 0;
	ExportTexturedParameter(shadingNetwork, "outColor", constantFX, FUDaeTextureChannel::EMISSION, nextTextureIndex);
	ANIM->AddPlugAnimation(shadingNetwork, "outColor", constantFX->GetEmissionColorParam()->GetValue(), kColour);

	// Transparent color
	MColor transparentColor;
	CDagHelper::GetPlugValue(shadingNetwork, "outTransparency", transparentColor);
	ExportTransparency(colladaDocument, shadingNetwork, transparentColor, constantFX, "outTransparency", nextTextureIndex);

	return effect;
}

// Retrieve any texture (file or layered) associated with a material attribute
void DaeMaterialLibrary::GetShaderTextures(const MObject& shader, const char* attribute, MObjectArray& textures, MIntArray& blendModes)
{
	MObject texture = CDagHelper::GetSourceNodeConnectedTo(shader, attribute);
	while (texture != MObject::kNullObj && !texture.hasFn(MFn::kLayeredTexture) && !texture.hasFn(MFn::kFileTexture))
	{
		// Bypass the bump and projection nodes
		if (texture.hasFn(MFn::kBump) || texture.hasFn(MFn::kBump3d))
		{
			texture = CDagHelper::GetSourceNodeConnectedTo(texture, "bumpValue");
		}
		else if (texture.hasFn(MFn::kProjection))
		{
			texture = CDagHelper::GetSourceNodeConnectedTo(texture, "image");
		}
		else break;
	}

	// Verify that we have a supported texture type: file or layered.
	bool isFileTexture = texture.hasFn(MFn::kFileTexture);
	bool isLayeredTexture = texture.hasFn(MFn::kLayeredTexture);

	if (!isFileTexture && !isLayeredTexture) return;
	
	// Return the textures and blend modes
	if (isFileTexture)
	{
		textures.append(texture);
		blendModes.append(0); // 0 -> No blending
	}
	else if (isLayeredTexture)
	{
		CShaderHelper::GetLayeredTextures(texture, textures, blendModes);
	}
}


// Find any textures connected to a material attribute and create the
// associated texture elements.
MObject DaeMaterialLibrary::ExportTexturedParameter(const MObject& node, const char* attr, FCDEffectStandard* effect, FUDaeTextureChannel::Channel channel, int& nextTextureIndex)
{
	// Retrieve all the file textures
	MObjectArray fileTextures;
	MIntArray blendModes;
	GetShaderTextures(node, attr, fileTextures, blendModes);
	uint fileTextureCount = fileTextures.length();
	for (uint i = 0; i < fileTextureCount; ++i)
	{
		// Verify that the texture is linked to a filename: COLLADA doesn't like empty file texture nodes.
		MFnDependencyNode nodeFn(fileTextures[i]);
		MPlug filenamePlug = nodeFn.findPlug("fileTextureName");
		MString filename; filenamePlug.getValue(filename);
		if (filename.length() == 0) continue;

		// Create the FCollada texture linking object.
		FCDTexture* colladaTexture = effect->AddTexture(channel);
		doc->GetTextureLibrary()->ExportTexture(colladaTexture, fileTextures[i], blendModes[i], nextTextureIndex++);

		// Special case for bump maps: export the bump height in the "amount" texture parameter.
		// Exists currently within the ColladaMax profile.
		if (channel == FUDaeTextureChannel::BUMP)
		{
			MObject bumpNode = CDagHelper::GetNodeConnectedTo(node, attr);
			if (!bumpNode.isNull() && (bumpNode.hasFn(MFn::kBump) || bumpNode.hasFn(MFn::kBump3d)))
			{
				FCDETechnique* colladaMaxTechnique = 
						colladaTexture->GetExtra()->GetDefaultType()->AddTechnique(DAEMAX_MAX_PROFILE);

				float amount = 1.0f;
				MFnDependencyNode(bumpNode).findPlug("bumpDepth").getValue(amount);
				FCDENode* amountNode = colladaMaxTechnique->AddParameter(DAEMAX_AMOUNT_TEXTURE_PARAMETER, amount);
				ANIM->AddPlugAnimation(bumpNode, "bumpDepth", amountNode->GetAnimated(), kSingle);

				int interp = 0;
				MFnDependencyNode(bumpNode).findPlug("bumpInterp").getValue(interp);
				FCDENode* interpNode = colladaMaxTechnique->AddParameter(DAEMAX_BUMP_INTERP_TEXTURE_PARAMETER, 
						interp);
			}
		}
	}


	return (fileTextures.length() > 0) ? fileTextures[0] : MObject::kNullObj;
}

// <bind_material> export
void DaeMaterialLibrary::ExportTextureBinding(FCDGeometry* geometry, FCDMaterialInstance* instance, const MObject& shaderNode)
{
	MStatus status;
	
	if (!geometry->IsMesh()) return;
	FCDGeometryMesh* mesh = geometry->GetMesh();
	DaeEntity* geometryEntity = (DaeEntity*) geometry->GetUserHandle();

	// Retrieve the file textures for all the known channels
	// Keep the order consistent!!
	MObjectArray fileTextures;
	MIntArray blendModes;
	GetShaderTextures(shaderNode, "outColor", fileTextures, blendModes); // Diffuse on Constant
	GetShaderTextures(shaderNode, "incandescence", fileTextures, blendModes); // Emission channel on Phong/Lambert
	GetShaderTextures(shaderNode, "ambientColor", fileTextures, blendModes); // Ambient channel on Phong/Lambert
	GetShaderTextures(shaderNode, "color", fileTextures, blendModes); // Diffuse channel on Phong/Lambert
	GetShaderTextures(shaderNode, "transparency", fileTextures, blendModes); // Transparency channel on Phong/Lambert
	GetShaderTextures(shaderNode, "normalCamera", fileTextures, blendModes); // Bump channel
	GetShaderTextures(shaderNode, "specularColor", fileTextures, blendModes); // Specular channel on Phong
	GetShaderTextures(shaderNode, "reflectedColor", fileTextures, blendModes); // Reflective channel on Phong
	uint fileTextureCount = fileTextures.length();
	if (fileTextureCount == 0) return;

	// Find the polygon set associated with this material instance.
	FCDObject* polygonsObj = instance->GetGeometryTarget();
	if (polygonsObj == NULL) return;

	FUAssert(polygonsObj->HasType(FCDGeometryPolygons::GetClassType()), return);
	FCDGeometryPolygons* polygons = static_cast<FCDGeometryPolygons*>(polygonsObj);

	// Process the file textures to find the unique UV set indices
	MPlug uvSetMainPlug = MFnDependencyNode(geometryEntity->GetNode()).findPlug("uvSet");
	for (uint i = 0; i < fileTextureCount; ++i)
	{
		const MObject& fileTexture = fileTextures[i];
		FUSStringBuilder texCoordName("TEX"); texCoordName.append(i);

		// Retrieve the UV set index for this texture
		uint uvSetIndex = CShaderHelper::GetAssociatedUVSet(geometryEntity->GetNode(), fileTexture);

		// Retrieve the name for this UV set and match it with a DAE source.
		MPlug uvSetPlug = uvSetMainPlug.elementByPhysicalIndex(uvSetIndex, &status);
		if (status != MStatus::kSuccess) continue;
		MPlug uvSetNamePlug = CDagHelper::GetChildPlug(uvSetPlug, "uvsn"); // "uvSetName"
		MString uvSetName; uvSetNamePlug.getValue(uvSetName);
		if (uvSetName.length() == 0) continue;
		FCDGeometrySource* source = mesh->FindSourceByName(MConvert::ToFChar(uvSetName));
		if (source == NULL) continue;

		// Retrieve the input information for this DAE source.
		FCDGeometryPolygonsInput* input = polygons->FindInput(source);
		if (input == NULL) continue;
		
		instance->AddVertexInputBinding(texCoordName.ToCharPtr(), input->GetSemantic(), input->GetSet());
	}
}

void DaeMaterialLibrary::Import()
{
	size_t materialCount = CDOC->GetMaterialLibrary()->GetEntityCount();
	for (size_t i = 0; i < materialCount; ++i)
	{
		FCDMaterial* colladaMaterial = CDOC->GetMaterialLibrary()->GetEntity(i);
		ImportMaterial(colladaMaterial);
	}
}

DaeMaterial* DaeMaterialLibrary::ImportMaterial(FCDMaterial* colladaMaterial)
{
	if (colladaMaterial == NULL || colladaMaterial->GetEffect() == NULL) return NULL;

	// Retrieve the material, if it has already been created.
	DaeMaterial* material = (DaeMaterial*) colladaMaterial->GetUserHandle();
	if (material != NULL) return material;

	material = CreateMaterial(NULL, NULL, colladaMaterial);

	return material;
}

DaeMaterial* DaeMaterialLibrary::CreateMaterial(FCDGeometryInstance* colladaInstance, FCDMaterialInstance* colladaMaterialInstance, FCDMaterial* colladaMaterial)
{
	// Create a placeholder for this material
	DaeMaterial* material = new DaeMaterial(doc, colladaMaterial, MObject::kNullObj);

	// Retrieve and import the COLLADA effect
	FCDEffect* effect = colladaMaterial->GetEffect();
	
	// Use the CG profile on PC-OGL platform, if available.
	material->profile = effect->FindProfileByTypeAndPlatform(FUDaeProfileType::CG, "PC-OGL");
	if (material->profile == NULL) material->profile = effect->FindProfile(FUDaeProfileType::CG);
	if (material->profile != NULL)
	{
		material->SetShaderGroup(ImportColladaFxShader((FCDEffectProfileFX*) material->profile, effect, colladaMaterial));
		material->isColladaFX = true;
	}
	else
	{
		// Otherwise, use the standard profile.
		material->profile = effect->FindProfile(FUDaeProfileType::COMMON);
		if (material->profile != NULL)
		{
			material->SetShaderGroup(ImportStandardShader(colladaInstance, colladaMaterialInstance, (FCDEffectStandard*) material->profile, colladaMaterial));
			material->isStandard = true;
		}
	}

	if (material->GetShaderGroup().isNull())
	{
		material->SetShaderGroup(CDagHelper::GetNode("initialShadingGroup"));
	}
	material->SetNode(CDagHelper::GetNodeConnectedTo(material->GetShaderGroup(), "surfaceShader"));

	MObject entityNode = material->GetNode();
	doc->GetEntityManager()->ImportEntity(entityNode, material);
	return material;
}

DaeMaterial* DaeMaterialLibrary::InstantiateMaterial(FCDGeometryInstance* colladaInstance, FCDMaterialInstance* colladaMaterialInstance)
{
	if (colladaMaterialInstance == NULL) return NULL;

	// Check for external references
	if (colladaMaterialInstance->IsExternalReference())
	{
		FCDEntityReference* reference = colladaMaterialInstance->GetEntityReference();
		FUUri uri(reference->GetUri().GetAbsoluteUri());
		GetReferenceManager()->CreateReference(uri);
	}

	// Retrieve the COLLADA entity information for this material
	DaeMaterial* material;
	if (colladaInstance->GetEffectParameterCount() == 0)
	{
		FCDMaterial* colladaMaterial = colladaMaterialInstance->GetMaterial();
		material = ImportMaterial(colladaMaterial);
		if (material == NULL) return NULL;
	}
	else
	{
		// Need duplication..
		material = CreateMaterial(colladaInstance, colladaMaterialInstance, colladaMaterialInstance->GetMaterial());
	}

	// Instantiate the Maya material
	if (material->isColladaFX) InstantiateColladaFxShader(material, colladaMaterialInstance);
	else if (material->isStandard) InstantiateStandardShader(material, colladaMaterialInstance);
	material->instanceCount++;

	return material;
}

// Import all the textures needed for this effect
MObjectArray DaeMaterialLibrary::ImportTextures(FCDEffect* cgEffect, FCDEffectProfileFX* cgEffectProfile, FCDMaterial* colladaMaterial)
{
	// no parameters in technique
	MObjectArray texlist;
	FCDEffectParameterList surfaces;

	// Retrieve all the surface parameters.
	size_t parameterCount = colladaMaterial->GetEffectParameterCount();
	for (size_t p = 0; p < parameterCount; ++p)
	{
		FCDEffectParameter* parameter = colladaMaterial->GetEffectParameter(p);
		if (parameter->IsType(FCDEffectParameterSurface::GetClassType())) surfaces.push_back(parameter);
	}
	parameterCount = cgEffect->GetEffectParameterCount();
	for (size_t p = 0; p < parameterCount; ++p)
	{
		FCDEffectParameter* parameter = cgEffect->GetEffectParameter(p);
		if (parameter->IsType(FCDEffectParameterSurface::GetClassType())) surfaces.push_back(parameter);
	}

	// Load the images for these surface parameters
	for (FCDEffectParameter** itR = surfaces.begin(); itR != surfaces.end(); ++itR)
	{
		FCDImage* image = ((FCDEffectParameterSurface*) (*itR))->GetImage();
		TextureMap::iterator itF = importedTextures.find(image);
		if (itF == importedTextures.end())
		{
			MObject texnode = doc->GetTextureLibrary()->ImportImage(image);
			texlist.append(texnode);
			importedTextures[image] = texnode;
		}
		else
		{
			texlist.append((*itF).second);
		}
	}
	return texlist;
}


// Create a ColladaFX shader and set its parameters
MObject	DaeMaterialLibrary::ImportColladaFxShader(FCDEffectProfileFX* cgEffect, FCDEffect* effect, FCDMaterial* colladaMaterial)
{	
	if (cgEffect == NULL || colladaMaterial == NULL || cgEffect == NULL) return MObject::kNullObj;
	
	MObject shaderGroup, shaderEngine;
	MString shaderName;
	size_t techniqueCount = cgEffect->GetTechniqueCount();
	if (techniqueCount == 0) return MObject::kNullObj;

	// import textures
	ImportTextures(effect, cgEffect, colladaMaterial);

	// Create the ColladaFX passes node
	MGlobal::executeCommand("shadingNode -asShader colladafxPasses;", shaderName);
	shaderEngine = CDagHelper::GetNode(shaderName);

	// Retrieve the created CFXPasses object
	MFnDependencyNode passesFn(shaderEngine);
	passesFn.setName(colladaMaterial->GetName().c_str());
	CFXPasses* passes = (CFXPasses*) passesFn.userNode();
	cgEffect->SetUserHandle(passes); // keep the CFXPasses pointer in the FCDEffect for later use

	// for each effect technique
	for (uint tech = 0; tech < techniqueCount; ++tech)
	{
		FCDEffectTechnique* cgTechnique = cgEffect->GetTechnique(tech);
		size_t passCount = cgTechnique->GetPassCount();
		
		// add an FX technique and make it current
		MString techName = cgTechnique->GetName().c_str();
		passes->addTechnique(techName);

		// for each technique pass
		for (uint i = 0; i < passCount; ++i)
		{
			FCDEffectPass* pass = cgTechnique->GetPass(i);
			if (pass == NULL) continue;
			MString nodeName = pass->GetPassName().c_str();

			// add a FX pass to the technique
			passes->addPass(nodeName);

			// create the shader
			MString shaderName;
			MGlobal::executeCommand("shadingNode -asShader \"colladafxShader\"", shaderName);

			MFnDependencyNode nodeFn(CDagHelper::GetNode(shaderName));
			CFXShaderNode* shader = (CFXShaderNode*) nodeFn.userNode();

			// this is an assumption: all the shaders in the node
			// must come from the same cgfx shader... as if the Load from CgFX button
			// has been used.
			// We should add extra tags in the collada file to take care of
			// the other cases.
			shader->setEffectTargets(tech,i);

			// make the connection (shader.outColor) -> (pass.outColor)
			MStatus status;
			MString inPlugName = "cfx" + techName + "_outColors";
			MPlug inPlug = passesFn.findPlug(inPlugName,&status);
			inPlug = inPlug.elementByLogicalIndex(i,&status);
			MPlug outPlug = nodeFn.findPlug("outColor",&status);
			status = CDagHelper::Connect(outPlug,inPlug) ? MS::kSuccess : MS::kFailure;

			// Load the vertex and fragment programs
			FCDEffectPassShader* vertexShader = pass->GetVertexShader();
			FCDEffectPassShader* fragmentShader = pass->GetFragmentShader();

			if (vertexShader && fragmentShader)
			{
				MString vertexFilename = vertexShader->GetCode() != NULL ? vertexShader->GetCode()->GetFilename().c_str() : emptyFString.c_str();
				MString fragmentFilename = fragmentShader->GetCode() != NULL ? fragmentShader->GetCode()->GetFilename().c_str() : emptyFString.c_str();
				
				fstring extension = FUFileManager::GetFileExtension(MConvert::ToFChar(vertexFilename));
				if (IsEquivalent(extension,FC("cgfx")))
				{
					shader->setEffectFilename(vertexFilename.asChar());
				}
				else
				{
					shader->setVertexProgramFilename(vertexFilename.asChar(),vertexShader->GetName().c_str());
					shader->setFragmentProgramFilename(fragmentFilename.asChar(),fragmentShader->GetName().c_str());
				}

				// Load the program parameters and their values
				AddDynAttribute(cgEffect, vertexShader, colladaMaterial, shader, shader->thisMObject());
				AddDynAttribute(cgEffect, fragmentShader, colladaMaterial, shader, shader->thisMObject());

				// Load the render states
				size_t renderStateCount = pass->GetRenderStateCount();
				for (size_t j = 0; j < renderStateCount; ++j)
				{
					FCDEffectPassState* passState = pass->GetRenderState(j);
					CFXRenderState* renderState = shader->FindRenderState(passState->GetType());
					if (renderState == NULL)
					{
						renderState = new CFXRenderState(shader);
						shader->GetRenderStates().push_back(renderState);
					}
					renderState->Initialize(passState);
				}
			}
		}
	}

	// Second step is to setup the shadingEngine and connect it up ...
	MString shadingGroupName;
	MString cmd = MString("sets -renderable true -noSurfaceShader true -empty -name ") + shaderName + "SG;";
	MGlobal::executeCommand(cmd, shadingGroupName);
	shaderGroup = CDagHelper::GetNode(shadingGroupName);
	CDagHelper::Connect(shaderEngine, "outColor", shaderGroup, "surfaceShader");

	// set the current technique if there's a technique hint
	FCDMaterialTechniqueHintList& hints = colladaMaterial->GetTechniqueHints();
	if (hints.size() > 0)
	{
		FCDMaterialTechniqueHint& hint = hints[0];
		if (hint.technique.size() > 0)
		{
			MPlug techPlug(passes->thisMObject(), CFXPasses::aChosenTechniqueName);
			MString temp = MString("cfx") + MString(hint.technique.c_str());
			techPlug.setValue(temp);
		}
	}

	return shaderGroup;
}

void DaeMaterialLibrary::InstantiateColladaFxShader(DaeMaterial* material, FCDMaterialInstance* colladaMaterialInstance)
{
	if (material == NULL || material->profile == NULL || colladaMaterialInstance == NULL) return;

	CFXPasses* passes = (CFXPasses*)material->profile->GetUserHandle();
	if (passes == NULL)
		return;

	FCDEffectProfileFX* fxProfile = (FCDEffectProfileFX*) material->profile;

	size_t techCount = fxProfile->GetTechniqueCount();
	for (size_t i = 0; i < techCount; ++i)
	{
		FCDEffectTechnique* colladaTechnique = fxProfile->GetTechnique(i);

		size_t passCount = colladaTechnique->GetPassCount();
		for (size_t j = 0; j < passCount; ++j)
		{
			FCDEffectPass* colladaPass = colladaTechnique->GetPass(j);

			CFXShaderNode* shader = passes->getPass((uint)i, (uint)j);
			if (shader == NULL) continue;

			// Load the parameter bindings
			for (size_t bindingIndex = 0; bindingIndex < colladaMaterialInstance->GetBindingCount(); ++bindingIndex)
			{
				FCDMaterialInstanceBind* binding = colladaMaterialInstance->GetBinding(bindingIndex);
				CFXParameter* attribute = shader->FindEffectParameter(*binding->semantic);
				if (attribute == NULL) attribute = shader->FindEffectParameterBySemantic(*binding->semantic);
				if (attribute != NULL)
				{
					// HACK: the target is bad here... It should really be a valid animation target!
					// You can use the driver target/plug animation look-up system here.
					// For now, look for the correct entity and pad in the qualifier.
					fm::string pointer, qualifier;
					FUStringConversion::SplitTarget(binding->target, pointer, qualifier);
					size_t p = pointer.find('/');
					if (p != fm::string::npos)
					{
						// backward compatibility: the qualifier will contain the actual plug name.
						// new version: grab the qualifier from the target's sid.
						qualifier = pointer.substr(p + 1);
						pointer = pointer.substr(0, p);
					}
					if (qualifier.size() > 0 && qualifier[0] != '.') qualifier.insert(0, ".");

					FCDEntity* entity = CDOC->FindEntity(pointer);
					if (entity != NULL)
					{
						DaeEntity* e = doc->Import(entity);
						if (e != NULL)
						{
							MPlug p = attribute->getPlug();
							MDagPath fullPath = MDagPath::getAPathTo(e->GetNode());
							MGlobal::executeCommand(MString("connectAttr -f ") + fullPath.fullPathName() + qualifier.c_str() + " " + p.info(), true, true);
						}
					}
				} // attribute exists
			} // for each binding
		} // for each pass
	} // for each technique
}

// Create a type-specific material
//
MObject	DaeMaterialLibrary::ImportStandardShader(FCDGeometryInstance* colladaInstance, FCDMaterialInstance* colladaMaterialInstance, FCDEffectStandard* effectStandard, FCDMaterial* colladaMaterial)
{
	if (effectStandard == NULL || colladaMaterial == NULL) return MObject::kNullObj;
	FCDEffect* effect = colladaMaterial->GetEffect();
	FCDEffectProfile* effectProfile = effect->FindProfile(FUDaeProfileType::COMMON);
	bool dummy = false;

	// Determine the Maya type for this standard material shader
	MFn::Type shaderType;
	switch (effectStandard->GetLightingType())
	{
	case FCDEffectStandard::CONSTANT: shaderType = MFn::kSurfaceShader; break;
	case FCDEffectStandard::LAMBERT: shaderType = MFn::kLambert; break;
	case FCDEffectStandard::PHONG: shaderType = MFn::kPhong; break;
	case FCDEffectStandard::BLINN: shaderType = MFn::kBlinn; break;
	default: shaderType = MFn::kLambert; break;
	}

	// Create the shader node
	MString shaderName = doc->ColladaNameToDagName(colladaMaterial->GetName().c_str());
	MObject shader = CShaderHelper::CreateShader(shaderType, shaderName);
	MFnDependencyNode shaderFn(shader);
	MFnLambertShader lambertFn(shader);
	MFnPhongShader phongFn(shader);
	MFnBlinnShader blinnFn(shader);

	//Synchronize the animators with the rest of the standard effect...
	if (colladaInstance != NULL && colladaMaterialInstance != NULL)
	{
		FCDEffectTools::SynchronizeAnimatedParams(colladaInstance, colladaMaterialInstance);
	}

	FCDParameterAnimatableColor4* translucencyColor = FCDEffectTools::GetAnimatedColor(colladaInstance, colladaMaterial, FCDEffectStandard::TranslucencyColorSemantic, &dummy);
	FCDParameterAnimatableFloat* translucencyFactor = FCDEffectTools::GetAnimatedFloat(colladaInstance, colladaMaterial, FCDEffectStandard::TranslucencyFactorSemantic);
	FCDParameterAnimatableColor4* emissionColor = FCDEffectTools::GetAnimatedColor(colladaInstance, colladaMaterial, FCDEffectStandard::EmissionColorSemantic, &dummy);
	FCDParameterAnimatableFloat* emissionFactor = FCDEffectTools::GetAnimatedFloat(colladaInstance, colladaMaterial, FCDEffectStandard::EmissionFactorSemantic);
	FCDParameterAnimatableColor4* diffuseColor = FCDEffectTools::GetAnimatedColor(colladaInstance, colladaMaterial, FCDEffectStandard::DiffuseColorSemantic, &dummy);
	FCDParameterAnimatableColor4* ambientColor = FCDEffectTools::GetAnimatedColor(colladaInstance, colladaMaterial, FCDEffectStandard::AmbientColorSemantic, &dummy);
	FCDParameterAnimatableColor4* specularColor = FCDEffectTools::GetAnimatedColor(colladaInstance, colladaMaterial, FCDEffectStandard::SpecularColorSemantic, &dummy);
	FCDParameterAnimatableFloat* specularFactor = FCDEffectTools::GetAnimatedFloat(colladaInstance, colladaMaterial, FCDEffectStandard::SpecularFactorSemantic);
	FCDParameterAnimatableColor4* reflectivityColor = FCDEffectTools::GetAnimatedColor(colladaInstance, colladaMaterial, FCDEffectStandard::ReflectivityColorSemantic, &dummy);
	FCDParameterAnimatableFloat* reflectivityFactor = FCDEffectTools::GetAnimatedFloat(colladaInstance, colladaMaterial, FCDEffectStandard::ReflectivityFactorSemantic);
	FCDParameterAnimatableFloat* shininess = FCDEffectTools::GetAnimatedFloat(colladaInstance, colladaMaterial, FCDEffectStandard::ShininessSemantic);
	FCDParameterAnimatableFloat* refraction = FCDEffectTools::GetAnimatedFloat(colladaInstance, colladaMaterial, FCDEffectStandard::IndexOfRefractionSemantic);

	// Used to setup any inputs (e.g. connected textures)
	// Keep track of the textures and the order as they are used to assign uv sets
	//
	DaeMaterial* material = (DaeMaterial*) colladaMaterial->GetUserHandle(); // slightly-round-about.
	DaeTextureList textures;

#define PROCESS_BUCKET(textureChannel) { \
		size_t colladaTextureCount = effectStandard->GetTextureCount(textureChannel); \
		for (size_t t = 0; t < colladaTextureCount; ++t) { \
			FCDTexture* colladaTexture = effectStandard->GetTexture(textureChannel, t); \
			DaeTexture texture(colladaTexture); \
			if (!doc->GetTextureLibrary()->ImportTexture(texture, colladaTexture)) continue; \
			textures.push_back(texture); \
			material->textures.append(texture.textureObject); \
			material->textureSetNames.push_back(colladaTexture->GetSet()->GetSemantic()); } }

#define CONNECT_TEXTURES(outPlugName, inPlugName) { \
		uint textureCount = (uint) textures.size(); \
		if (textureCount == 1) CDagHelper::Connect(textures.front().frontObject, outPlugName, shader, inPlugName); \
		else if (textureCount > 1) { \
			MObject layeredTexture = CShaderHelper::CreateLayeredTexture(textures); \
			CDagHelper::Connect(layeredTexture, outPlugName, shader, inPlugName); } \
		textures.clear(); } 

	// Read in the color/factor parameters, according to the material type.
	if (shader.hasFn(MFn::kSurfaceShader))
	{
		// CONSTANT material: has only one interesting color parameter, for emission.
		
		CDagHelper::SetPlugValue(shader, "outColor", MConvert::ToMColor(*emissionColor));
		ANIM->AddPlugAnimation(shader, "outColor", *emissionColor, kColour);

		const char* transparencyOutput;
		switch (effectStandard->GetTransparencyMode())
		{
		case FCDEffectStandard::RGB_ZERO:
			CDagHelper::SetPlugValue(shader, "outTransparency", 
					MConvert::ToMColor(*translucencyColor) * *translucencyFactor);
			ANIM->AddPlugAnimation(shader, "outTransparency", *translucencyColor, kColour);
			transparencyOutput = "outColor"; // For any subsequent textures.
			break;

		case FCDEffectStandard::A_ONE:
		default:			
			MColor transparencyColor;
			transparencyColor.r = transparencyColor.g = transparencyColor.b = 1.0f - (*translucencyColor)->w * *translucencyFactor;
			CDagHelper::SetPlugValue(shader, "outTransparency", transparencyColor);
			transparencyOutput = "outTransparency"; // For any subsequent textures.
			break;
		}

		PROCESS_BUCKET(FUDaeTextureChannel::EMISSION);
		CONNECT_TEXTURES("outColor", "outColor");

		PROCESS_BUCKET(FUDaeTextureChannel::TRANSPARENT);
		if (textures.size() == 1 && effectStandard->GetTransparencyMode() == FCDEffectStandard::A_ONE)
		{
			// Import any animation on the translucency factor
			MObject textureNode = textures.front().frontObject;
			ANIM->AddPlugAnimation(textureNode, "alphaGain", *translucencyFactor, kSingle, NULL);
		}
		CONNECT_TEXTURES(transparencyOutput, "outTransparency");
	}
	else
	{
		if (effectStandard->IsRefractive())
		{
			lambertFn.setRefractiveIndex(*refraction);
			ANIM->AddPlugAnimation(shader, "refractiveIndex", *refraction, kSingle);
			CDagHelper::SetPlugValue(shader, "refractions", true);
		}
		
		lambertFn.setColor(MConvert::ToMColor(*diffuseColor));
		ANIM->AddPlugAnimation(shader, "color", *diffuseColor, kColour);

		lambertFn.setDiffuseCoeff(1.0f);
		
		// Ambient/Emission/Transparent colors
		
		lambertFn.setAmbientColor(MConvert::ToMColor(*ambientColor));
		ANIM->AddPlugAnimation(shader, "ambientColor", *ambientColor, kColour);
		
		lambertFn.setIncandescence(MConvert::ToMColor(*emissionColor) * *emissionFactor);
		ANIM->AddPlugAnimation(shader, "incandescence", *emissionColor, kColour);

		// Connect the transparency textures to the correct outputs: according to the transparency mode.
		const char* transparencyOutput;
		switch (effectStandard->GetTransparencyMode())
		{
		case FCDEffectStandard::RGB_ZERO:

			lambertFn.setTransparency(MConvert::ToMColor(*translucencyColor) * *translucencyFactor);
			ANIM->AddPlugAnimation(shader, "transparency", *translucencyColor, kColour);
			transparencyOutput = "outColor"; // For any subsequent textures.
			break;

		case FCDEffectStandard::A_ONE:
		default:
			MColor transparencyColor;
			transparencyColor.r = transparencyColor.g = transparencyColor.b = 1.0f - (*translucencyColor)->w * *translucencyFactor;
			lambertFn.setTransparency(transparencyColor);
			transparencyOutput = "outTransparency"; // For any subsequent textures.
			break;
		}

		// Phong shader parameters
		if (shader.hasFn(MFn::kPhong))
		{
			
			phongFn.setSpecularColor(MConvert::ToMColor(*specularColor) * *specularFactor);
			ANIM->AddPlugAnimation(shader, "specularColor", *specularColor, kColour);
			
			phongFn.setReflectedColor(MConvert::ToMColor(*reflectivityColor));
			ANIM->AddPlugAnimation(shader, "reflectedColor", *reflectivityColor, kColour);
			CDagHelper::SetPlugValue(shader, "reflectivity", *reflectivityFactor);
			ANIM->AddPlugAnimation(shader, "reflectivity", *reflectivityFactor, kSingle);

			// Shininess should never be less than 2.0
			
			float cospower = *shininess;
			phongFn.setCosPower((cospower > 2.0f) ? cospower : 2.0f);
			ANIM->AddPlugAnimation(shader, "cosinePower", *shininess, kSingle);
		}
		
		// blinn shader parameters
		if (shader.hasFn(MFn::kBlinn))
		{
			blinnFn.setSpecularColor(MConvert::ToMColor(*specularColor) * *specularFactor);
			ANIM->AddPlugAnimation(shader, "specularColor", *specularColor, kColour);
			blinnFn.setReflectedColor(MConvert::ToMColor(*reflectivityColor));
			ANIM->AddPlugAnimation(shader, "reflectedColor", *reflectivityColor, kColour);
			CDagHelper::SetPlugValue(shader, "reflectivity", *reflectivityFactor);
			ANIM->AddPlugAnimation(shader, "reflectivity", *reflectivityFactor, kSingle);

#ifdef BLINN_EXPONENT_MODEL
			BlinnShininessToEccentricity* converter = new BlinnShininessToEccentricity();
			blinnFn.setEccentricity((*converter)(effectStandard->GetShininess()));
			ANIM->AddPlugAnimation(shader, "eccentricity", *shininess, kSingle, converter);
#else
			blinnFn.setEccentricity(effectStandard->GetShininess());
			ANIM->AddPlugAnimation(shader, "eccentricity", *shininess, kSingle);
#endif // BLINN_EXPONENT_MODEL
		}

		// Connect the textures in each of the buckets. If there is more than once
		// texture in a bucket, use a modulated layeredTexture.
		//
		PROCESS_BUCKET(FUDaeTextureChannel::DIFFUSE);
		CONNECT_TEXTURES("outColor", "color");
		PROCESS_BUCKET(FUDaeTextureChannel::AMBIENT);
		CONNECT_TEXTURES("outColor", "ambientColor");

		PROCESS_BUCKET(FUDaeTextureChannel::TRANSPARENT);
		if (textures.size() == 1 && effectStandard->GetTransparencyMode() == FCDEffectStandard::A_ONE)
		{
			// Import any animation on the translucency factor
			MObject textureNode = textures.front().frontObject;
			
			ANIM->AddPlugAnimation(textureNode, "alphaGain", *translucencyFactor, kSingle);
		}
		CONNECT_TEXTURES(transparencyOutput, "transparency");

		PROCESS_BUCKET(FUDaeTextureChannel::EMISSION);
		CONNECT_TEXTURES("outColor", "incandescence");
		PROCESS_BUCKET(FUDaeTextureChannel::SPECULAR);
		PROCESS_BUCKET(FUDaeTextureChannel::SPECULAR_LEVEL);
		PROCESS_BUCKET(FUDaeTextureChannel::SHININESS);
		CONNECT_TEXTURES("outColor", "specularColor");

		// Bump textures are a little special
		PROCESS_BUCKET(FUDaeTextureChannel::BUMP);
		MObject bumpTexture(MObject::kNullObj);
		uint bumpTextureCount = (uint) textures.size();
		if (bumpTextureCount == 1) bumpTexture = textures.front().frontObject;
		else if (bumpTextureCount > 1)
		{
			MObject layeredTexture = CShaderHelper::CreateLayeredTexture(textures);
			bumpTexture = layeredTexture;
		}
		if (bumpTexture != MObject::kNullObj)
		{
			MFnDependencyNode bumpTextureFn(bumpTexture);
			MPlug alphaInput = bumpTextureFn.findPlug("alphaIsLuminance");
			alphaInput.setValue(1);

			MObject bumpObject = CShaderHelper::CreateBumpNode(bumpTexture);
			CDagHelper::Connect(bumpObject, "outNormal", shader, "normalCamera");

			// Look for the bump-depth, within the ColladaMax technique.
			const FCDETechnique* colladaMaxTechnique = textures.front().colladaTexture->GetExtra()->GetDefaultType()->FindTechnique(DAEMAX_MAX_PROFILE);
			if (colladaMaxTechnique != NULL)
			{
				float amount = 1.0f;
				const FCDENode* amountNode = colladaMaxTechnique->FindParameter(DAEMAX_AMOUNT_TEXTURE_PARAMETER);
				if (amountNode != NULL)
				{
					ANIM->AddPlugAnimation(bumpObject, "bumpDepth", const_cast<FCDAnimatedCustom*>(amountNode->GetAnimated()), kSingle);
				}
				amount = FUStringConversion::ToFloat(amountNode->GetContent());
				CDagHelper::SetPlugValue(bumpObject, "bumpDepth", amount);

				const FCDENode* interpNode = colladaMaxTechnique->FindParameter(DAEMAX_BUMP_INTERP_TEXTURE_PARAMETER);
				if (interpNode != NULL)
				{
					int interp = FUStringConversion::ToInt32(interpNode->GetContent());
					CDagHelper::SetPlugValue(bumpObject, "bumpInterp", interp);
				}
			}
		}
	}

#undef PROCESS_BUCKET
#undef CONNECT_TEXTURES
	

	// Return the shadingGroup/shadingEngine name
	MObject shaderGroup = CShaderHelper::GetShadingEngine(shader);
	return shaderGroup;
}

void DaeMaterialLibrary::InstantiateStandardShader(DaeMaterial* material, FCDMaterialInstance* colladaMaterialInstance)
{
	// Need to overwrite the material's texture set linkages in order to follow this particular instance's information.
	Int32List& textureSets = material->textureSets;
	size_t textureCount = material->textureSetNames.size();
	if (textureSets.empty() && textureCount == 0) return;
	else if (textureSets.empty()) textureSets.resize(textureCount, 0);

	size_t linkageCount = colladaMaterialInstance->GetVertexInputBindingCount();
	for (size_t l = 0; l < linkageCount; ++l)
	{
		FCDMaterialInstanceBindVertexInput* linkage = colladaMaterialInstance->GetVertexInputBinding(l);
		if (linkage->inputSemantic != FUDaeGeometryInput::TEXCOORD) continue;
		for (size_t i = 0; i < textureCount; ++i)
		{
			fm::string& setName = material->textureSetNames[i];
			if (IsEquivalent(linkage->semantic, setName))
			{
				textureSets[i] = linkage->inputSet;
			}
		}
	}
}

void DaeMaterialLibrary::AddDynAttribute(FCDEffectProfileFX* cgEffect, FCDEffectPassShader* colladaShader, FCDMaterial* colladaMaterial, CFXShaderNode* shader, MObject shaderNode)
{
	size_t cgParamCount = cgEffect->GetEffectParameterCount();
	for (size_t p = 0; p < cgParamCount; ++p)
	{
		FCDEffectParameter* cgParam = cgEffect->GetEffectParameter(p);

		// Get attribute name, retrieve its bind name
		const fm::string& attrname = cgParam->GetReference();
		FCDEffectPassBind* binding = colladaShader->FindBindingReference(attrname);
		if (binding == NULL) continue; // This parameter is not used in this shader.
		CFXParameter* shaderAttribute = shader->FindEffectParameter(*binding->symbol);
		
		if (shaderAttribute == NULL) continue; // An error occured in the parsing of the Cg program.
		
		// add the dynamic attribute to shader for the parameter 
		MPlug plug = shaderAttribute->getPlug();

		const FCDEffectParameter* pset = FCDEffectTools::FindEffectParameterByReference(colladaMaterial, cgParam->GetReference());
		if (pset == NULL) pset = cgParam;

		switch (cgParam->GetType()) 
		{
		case FCDEffectParameter::STRING:
			CDagHelper::SetPlugValue(plug, ((FCDEffectParameterString*) pset)->GetValue());
			break;

		case FCDEffectParameter::SURFACE: {
			const FCDEffectParameterSurface* surface = (FCDEffectParameterSurface*) pset;
			const FCDImage* image = surface->GetImage();
			if (image != NULL)
			{
				CDagHelper::SetPlugValue(plug, image->GetFilename());
			}
			break; }

		case FCDEffectParameter::BOOLEAN:
			CDagHelper::SetPlugValue(plug, ((FCDEffectParameterBool*)pset)->GetValue());
			break;

		case FCDEffectParameter::FLOAT: {
			float min = 0.0f, max = 1.0f;
			for (size_t i = 0; i < cgParam->GetAnnotationCount(); ++i)
			{
				const FCDEffectParameterAnnotation* a = cgParam->GetAnnotation(i);
				if (IsEquivalent(a->name, TO_FSTRING(CFX_ANNOTATION_UIMIN))) min = FUStringConversion::ToFloat(*a->value);
				else if (IsEquivalent(a->name, TO_FSTRING(CFX_ANNOTATION_UIMAX))) max = FUStringConversion::ToFloat(*a->value);
			}
			float val = ((FCDEffectParameterFloat*)pset)->GetValue();
			plug.setValue(val);
			shaderAttribute->setDefaultValue(((FCDEffectParameterFloat*) cgParam)->GetValue());
			shaderAttribute->setBounds(min, max);
			ANIM->AddPlugAnimation(plug, ((FCDEffectParameterFloat*) cgParam)->GetValue(), kSingle);
			break; }

		case FCDEffectParameter::INTEGER: {
			int val = ((FCDEffectParameterInt*)pset)->GetValue();
			plug.setValue(val);
			break; }

		case FCDEffectParameter::FLOAT2: {
			float valx = ((FCDEffectParameterFloat2*) pset)->GetValue()->x;
			float valy = ((FCDEffectParameterFloat2*) pset)->GetValue()->y;
			CDagHelper::SetPlugValue(plug, valx, valy);
			break; }

		case FCDEffectParameter::FLOAT3: {
			float valx = ((FCDEffectParameterFloat3*)pset)->GetValue()->x;
			float valy = ((FCDEffectParameterFloat3*)pset)->GetValue()->y; 
			float valz = ((FCDEffectParameterFloat3*)pset)->GetValue()->z;
			CDagHelper::SetPlugValue(plug, MColor(valx, valy, valz));
			ANIM->AddPlugAnimation(plug, ((FCDEffectParameterFloat3*) cgParam)->GetValue(), kColour);
			
			for (size_t i = 0; i < cgParam->GetAnnotationCount(); ++i)
			{
				const FCDEffectParameterAnnotation* a = cgParam->GetAnnotation(i);
				if (a->name == TO_FSTRING(CFX_ANNOTATION_UITYPE))
				{
					if (IsEquivalentI(a->value, TO_FSTRING(CFX_UITYPE_NAVIGATION)))
					{
						shaderAttribute->setUIType(TO_FSTRING(CFX_UITYPE_NAVIGATION).c_str());
					}
					else if (IsEquivalentI(a->value, TO_FSTRING(CFX_UITYPE_COLOR)))
					{
						shaderAttribute->setUIType(TO_FSTRING(CFX_UITYPE_COLOR).c_str());
					}
					else
					{
						shaderAttribute->setUIType(TO_FSTRING(CFX_UITYPE_VECTOR).c_str());
					}
				}
			}			
			break; }

		case FCDEffectParameter::VECTOR: {
			MColor valc = MConvert::ToMColor(*((FCDEffectParameterVector*) pset)->GetValue());
			CDagHelper::SetPlugValue(plug, valc);
			ANIM->AddPlugAnimation(plug, ((FCDEffectParameterVector*) cgParam)->GetValue(), kColour);
			
			for (size_t i = 0; i < cgParam->GetAnnotationCount(); ++i)
			{
				const FCDEffectParameterAnnotation* a = cgParam->GetAnnotation(i);
				if (a->name == TO_FSTRING(CFX_ANNOTATION_UITYPE))
				{
					if (IsEquivalentI(a->value, TO_FSTRING(CFX_UITYPE_NAVIGATION)))
					{
						shaderAttribute->setUIType(TO_FSTRING(CFX_UITYPE_NAVIGATION).c_str());
					}
					else if (IsEquivalentI(a->value, TO_FSTRING(CFX_UITYPE_COLOR)))
					{
						shaderAttribute->setUIType(TO_FSTRING(CFX_UITYPE_COLOR).c_str());
					}
					else
					{
						shaderAttribute->setUIType(TO_FSTRING(CFX_UITYPE_VECTOR).c_str());
					}
				}
			}			
			break; }

		case FCDEffectParameter::SAMPLER: {
			// find source <surface> in material
			FCDEffectParameterSurface* baseSurface = ((FCDEffectParameterSampler*) cgParam)->GetSurface();
			if (baseSurface == NULL) break;
			FCDEffectParameter* psurface = FCDEffectTools::FindEffectParameterByReference(colladaMaterial, baseSurface->GetReference());
			if (psurface == NULL) psurface = FCDEffectTools::FindEffectParameterByReference(cgEffect, baseSurface->GetReference());
			if (psurface == NULL || psurface->GetType() != FCDEffectParameter::SURFACE) break;
			FCDEffectParameterSurface* colladaSurface = (FCDEffectParameterSurface*) psurface;
			FCDImage* colladaImage = colladaSurface->GetImage();
			if (colladaImage == NULL) break;
			
			TextureMap::iterator itF = importedTextures.find(colladaImage);
			if (itF == importedTextures.end()) break;
			CDagHelper::Connect((*itF).second, "outColor", plug);
			
			break; }

		case FCDEffectParameter::MATRIX: {
			FCDEffectParameterMatrix* matrix = (FCDEffectParameterMatrix*) pset;
			CDagHelper::SetPlugValue(plug, MConvert::ToMMatrix(matrix->GetValue()));
			break; }
		}

		// override the UIName annotation
		for (size_t i = 0; i < cgParam->GetAnnotationCount(); ++i)
		{
			const FCDEffectParameterAnnotation* a = cgParam->GetAnnotation(i);
			if (IsEquivalent(a->name, TO_FSTRING(CFX_ANNOTATION_UINAME)))
			{
				shaderAttribute->setUIName(TO_FSTRING(*a->value).c_str());
			}
		}
	}
}

FCDEffectParameterSurface* DaeMaterialLibrary::writeSurfaceTexture(FCDEffectProfileFX* profile, MObject textureNode, FCDMaterial* material, const fm::string& type)
{
	MFnDependencyNode textureFn(textureNode);

	FCDImage* image = doc->GetTextureLibrary()->ExportImage(profile->GetDocument(), textureNode);
	if (image != NULL)
	{
		fm::string surfaceId = image->GetDaeId() + "Surface";

		FCDEffectParameterSurfaceInit* initMethod = NULL;
		if (IsEquivalent(type,"2D"))
		{
			FCDEffectParameterSurfaceInitFrom* init2D = new FCDEffectParameterSurfaceInitFrom();
			init2D->mip.push_back("0");
			init2D->slice.push_back("0");
			initMethod = init2D;
		}
		else if (IsEquivalent(type,"CUBE"))
		{
			// DDS-like cube map
			FCDEffectParameterSurfaceInitCube* initCube = new FCDEffectParameterSurfaceInitCube();
			initCube->cubeType = FCDEffectParameterSurfaceInitCube::ALL;
			initMethod = initCube;
		}

		FCDEffectParameterSurface* parameter1 = (FCDEffectParameterSurface*) profile->AddEffectParameter(FCDEffectParameter::SURFACE);
		parameter1->SetGenerator();
		parameter1->SetReference(surfaceId);
		parameter1->AddAnnotation(TO_FSTRING(CFX_ANNOTATION_UINAME), FCDEffectParameter::STRING, MConvert::ToFChar(textureFn.name()));
		parameter1->AddImage(image);
		parameter1->SetSurfaceType(type);
		parameter1->SetInitMethod(initMethod);

		if (initMethod != NULL)
			initMethod = initMethod->Clone(NULL);

		FCDEffectParameterSurface* parameter2 = (FCDEffectParameterSurface*) material->AddEffectParameter(FCDEffectParameter::SURFACE);
		parameter2->SetModifier();
		parameter2->AddImage(image);
		parameter2->SetReference(surfaceId);
		parameter2->SetSurfaceType(type);
		parameter2->SetInitMethod(initMethod);
		
		return parameter1;
	}
	else
	{
		return NULL;
	}
}

void DaeMaterialLibrary::ExportProgramBinding(FCDEffectPassShader* shader, const MString& param, MFnDependencyNode& nodeFn)
{
	MStatus status;
	if (nodeFn.typeId() != CFXShaderNode::id) return;
	CFXShaderNode* pNode = (CFXShaderNode*) nodeFn.userNode();

	// Set the compiler target
	const char* entryPoint = shader->IsVertexShader() ? pNode->getVertexProfileName() : pNode->getFragmentProfileName();
	shader->SetCompilerTarget(TO_FSTRING(entryPoint));

	// Add the parameter bindings
	for (CFXParameterList::iterator itE = pNode->GetParameters().begin(); itE != pNode->GetParameters().end(); ++itE)
	{
		const CFXParameter* attrib = (*itE);
		if ((shader->IsVertexShader() && attrib->getBindTarget() == CFXParameter::kVERTEX) ||
			(shader->IsFragmentShader() && attrib->getBindTarget() == CFXParameter::kFRAGMENT) ||
			attrib->getBindTarget() == CFXParameter::kBOTH)
		{
			FCDEffectPassBind* binding = shader->AddBinding();
			binding->reference = (nodeFn.name() + attrib->getName()).asChar();
			binding->symbol = attrib->getProgramEntry().asChar();
		}
	}
}

void DaeMaterialLibrary::ImportVertexInputBindings(FCDMaterialInstance* materialInstance, FCDGeometryPolygons* polygons, MObject& shaderEngine)
{
	if (shaderEngine.isNull())
		return;
	MFnDependencyNode shaderFn(shaderEngine);
	
	// we always import a CFXPasses node
	if (shaderFn.typeId() != CFXPasses::id)
		return;

	CFXPasses* passes = (CFXPasses*) shaderFn.userNode();
	if (passes == NULL)
		return;

	uint techCount = passes->getTechniqueCount();
	for (uint i = 0; i < techCount; ++i)
	{
		uint passCount = passes->getPassCount(i);
		for (uint j = 0; j < passCount; ++j)
		{
			CFXShaderNode* shader = passes->getPass(i,j);
			if (shader == NULL) continue;

			// Process the vertex input bindings
			for (size_t bindingIndex = 0; bindingIndex < materialInstance->GetVertexInputBindingCount(); ++bindingIndex)
			{
				const FCDMaterialInstanceBindVertexInput* binding = materialInstance->GetVertexInputBinding(bindingIndex);

				int id = -1;
				if (binding->semantic->substr(0, 5) == "COLOR") id = 8; // only one color set is supported in the ColladaFX render node
				else if (binding->semantic->substr(0, 8) == "TEXCOORD") id = FUStringConversion::ToInt32(binding->semantic->substr(8));
				else continue;
				
				// Check for the special cases
				if (binding->inputSemantic == FUDaeGeometryInput::TEXTANGENT)
				{
					shader->setChannelName(id, FC("tangent"));
				}
				else if (binding->inputSemantic == FUDaeGeometryInput::TEXBINORMAL)
				{
					shader->setChannelName(id, FC("binormal"));
				}
				else
				{
					// Look for the name attached to the source with the wanted semantic/set
					for (size_t inputIndex = 0; inputIndex < polygons->GetInputCount(); ++inputIndex)
					{
						FCDGeometryPolygonsInput* input = polygons->GetInput(inputIndex);
						if (input->GetSemantic() == binding->inputSemantic && input->GetSet() == binding->inputSet)
						{
							shader->setChannelName(id, input->GetSource()->GetName().c_str());
							break;
						}
					}
				}
			} // for each vertex input binding
		} // for each pass
	} // for each technique
}

