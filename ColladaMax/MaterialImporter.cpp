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

// Standard Material Importer Class

#include "StdAfx.h"
#include "ColladaAttrib.h"
#include "AnimationImporter.h"
#include "DocumentImporter.h"
#include "MaterialImporter.h"
#include "Common/MultiMtl.h"
#include "Common/ConversionFunctions.h"
#include "TextureOutput.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDImage.h"
#include "FCDocument/FCDMaterial.h"
#include "FCDocument/FCDMaterialInstance.h"
#include "FCDocument/FCDEffect.h"
#include "FCDocument/FCdEffectCode.h"
#include "FCDocument/FCDEffectParameter.h"
#include "FCDocument/FCDEffectParameterSampler.h"
#include "FCDocument/FCDEffectParameterSurface.h"
#include "FCDocument/FCDEffectPass.h"
#include "FCDocument/FCDEffectPassShader.h"
#include "FCDocument/FCDEffectProfileFX.h"
#include "FCDocument/FCDEffectStandard.h"
#include "FCDocument/FCDEffectTools.h"
#include "FCDocument/FCDTexture.h"
#include "FCDocument/FCDExtra.h"
#include "FCDocument/FCDEffectTechnique.h"
#include "FUtils/FUDaeEnum.h"
#include "FUtils/FUFileManager.h"
#include "FUtils/FUXmlWriter.h"
#include "FUtils/FUXmlParser.h"
#include "Common/StandardMtl.h"
#include "ColladaFX/ColladaEffect.h"
#include "ColladaFX/ColladaEffectTechnique.h"
#include "ColladaFX/ColladaEffectPass.h"
#include "ColladaFX/ColladaEffectShader.h"
#include "ColladaFX/ColladaEffectParameter.h"

// Texturing: maps the 3dsMax channels to the FCollada channels.
static const uint32 textureChannelMap[NTEXMAPS] = 
{
	FUDaeTextureChannel::AMBIENT, FUDaeTextureChannel::DIFFUSE, FUDaeTextureChannel::SPECULAR, FUDaeTextureChannel::SPECULAR_LEVEL,
	FUDaeTextureChannel::SHININESS, FUDaeTextureChannel::EMISSION, FUDaeTextureChannel::TRANSPARENT, FUDaeTextureChannel::FILTER,
	FUDaeTextureChannel::BUMP, FUDaeTextureChannel::REFLECTION, FUDaeTextureChannel::REFRACTION, FUDaeTextureChannel::DISPLACEMENT
};

//
// Conversion functions and helpers.
//

class ReverseShininessConversionFunctor : public FCDConversionFunctor
{
public:
	virtual float operator() (float v)
	{
		// According to my derivation analysis, using MS Excel: the
		// best way to appromixate this micro-facet factor into the Blinn-Phong
		// factor is using the formulas below. This only works in very specific
		// angles, otherwise the values are slightly off: but it is better than nothing!
		//
		// To avoid a C0 discontinuity: do a linear interpolation of the two
		// formulas when x is between 0.36f and 0.45f.
		//
		float out = 0.0f;
		v = FMath::Clamp(v, 0.0001f, 1.0f);
		if (v >= 0.36f) out = expf((v - 0.0226f) / -0.157f);
		if (v <= 0.45f)
		{
			float factor = max(0.0f, (v - 0.36f) / (0.45f - 0.36f));
			float f = logf(v / 0.64893f) / -3.3611f;
			out = out * factor + (1.0f - factor) * f;
		}
		return out;
	}
};

namespace DxMaterial
{
	enum{
		kParamBlockRef,
		kLoaderBlockRef,
		kRenderBlockRef,
		kTechniqueBlockRef,
		kWrapperBlockRef,
		kRenderMaterialRef,
	};
}

static inline Mtl* NewDxMaterial()
{
	return static_cast<Mtl*>(CreateInstance(MATERIAL_CLASS_ID,DXMATERIAL_CLASS_ID));
}

static Mtl* NewDxMaterial(const char* filename, Mtl* stub = NULL)
{
	Mtl* dxmtl = NewDxMaterial();
	IDxMaterial* dxm = static_cast<IDxMaterial*>(dxmtl->GetInterface(IDXMATERIAL_INTERFACE));
	dxm->SetEffectFilename(TSTR(filename));
	if (stub != NULL)
		dxmtl->ReplaceReference(DxMaterial::kRenderMaterialRef, stub);
	return dxmtl;
}

//
// MaterialImporter
//

MaterialImporter::MaterialImporter(DocumentImporter* document)
:	EntityImporter(document, NULL)
{
	colladaEffect = NULL;
	material = NULL;
}

MaterialImporter::~MaterialImporter()
{
	colladaEffect = NULL;
	material = NULL;
	for (int i = 0; i < NTEXMAPS; ++i) bitmapTextureMaps[i].clear();
}

// Create a 3dsMax material for the given COLLADA material
Mtl* MaterialImporter::ImportMaterial(const FCDMaterial* colladaMaterial, FCDMaterialInstance* materialInstance, FCDGeometryInstance* geometryInstance)
{
	FUAssert(colladaMaterial != NULL, return NULL);

	// A COLLADA material just points to a COLLADA effect.
	colladaEffect = colladaMaterial->GetEffect();
	if (colladaEffect == NULL) return NULL;

	// Check for nVidia's import mechanism
	const FCDExtra* extra = colladaEffect->GetExtra();
	if (extra != NULL)
	{
		const FCDETechnique* tech = extra->GetDefaultType()->FindTechnique(DAENV_NVIMPORT_PROFILE);
		if (tech != NULL)
		{
			return ImportFXMaterial(colladaMaterial, materialInstance, geometryInstance);
		}
	}
	
	// Check for ColladaEffect plugin
	FCDEffectProfileFX* fxProfile = (FCDEffectProfileFX*) 
		colladaEffect->FindProfileByTypeAndPlatform(FUDaeProfileType::CG, "PC-DX9");
	// Try to at least find a CG profile... This will not render but at least you'll be able to export it.
	if (fxProfile == NULL) 
		colladaEffect->FindProfile(FUDaeProfileType::CG);
	if (fxProfile != NULL)
	{
		return ImportColladaEffectMaterial(colladaMaterial, fxProfile);
	}

	// Import the standard effect
	if (colladaEffect->HasProfile(FUDaeProfileType::COMMON))
	{
		return ImportStdMaterial(colladaMaterial, materialInstance, geometryInstance);
	}
	
	StdMat2* baseMaterial = NewDefaultStdMat();

	baseMaterial->SetShading(SHADE_PHONG);
	baseMaterial->SetName(colladaMaterial->GetName().c_str());

	material = baseMaterial;
	return material;
}

// Create a 3dsMax material for the given COLLADA material
Mtl* MaterialImporter::ImportStdMaterial(const FCDMaterial* colladaMaterial, FCDMaterialInstance* materialInstance, FCDGeometryInstance* geometryInstance)
{
	// A COLLADA material just points to a COLLADA effect.
	colladaEffect = colladaMaterial->GetEffect();
	const FCDEffectStandard* colladaStdMtl = (const FCDEffectStandard*) colladaEffect->FindProfile(FUDaeProfileType::COMMON);
	return (colladaStdMtl != NULL) ? CreateStdMaterial(colladaStdMtl, materialInstance, geometryInstance) : NULL;
}

// Create a 3dsMax material for the given COLLADA material
Mtl* MaterialImporter::ImportFXMaterial(const FCDMaterial* colladaMaterial, FCDMaterialInstance* materialInstance, FCDGeometryInstance* geometryInstance)
{
	// A COLLADA material just points to a COLLADA effect.
	colladaEffect = colladaMaterial->GetEffect();

	// If there is a HLSL or CG profile effect, use that.
	// Otherwise, use the COMMON profile.
	Mtl *stub = NULL;
	const FCDEffectProfileFX* profileHLSL = (const FCDEffectProfileFX*) colladaEffect->FindProfile(FUDaeProfileType::HLSL);
	const FCDEffectProfileFX* profileCG = (const FCDEffectProfileFX*) colladaEffect->FindProfile(FUDaeProfileType::CG);
	const FCDEffectStandard* colladaStdMtl = (const FCDEffectStandard*) colladaEffect->FindProfile(FUDaeProfileType::COMMON);
	if (profileHLSL == NULL && profileCG == NULL)
	{
		// bug 295. On an empty <effect> node, colladaStdMtl
		// will be NULL
		stub = CreateStdMaterial(colladaStdMtl, materialInstance, geometryInstance);
	}
	else
	{
		StdMat2* baseMaterial = NewDefaultStdMat();

		baseMaterial->SetShading(SHADE_PHONG);
		baseMaterial->SetName(colladaMaterial->GetName().c_str());

		stub = baseMaterial;
	}

	if (stub != NULL && profileCG != NULL)
	{
#if CG_SUPPORTED
		const fm::string filestr = profileCG->GetIncludeFilename();
		const char* cstr = filestr.c_str();
		if (!filestr.empty())
		{
			material = NewDxMaterial(filestr.c_str(), stub);
			return material;
		}
#else
		/*
		// Add the data as a Custom Attribute
		const FCDEffectParameterList* plist = profileCG->GetParameters();
		for (FCDEffectParameterList::const_iterator itp = plist->begin() ; itp != plist->end() ; itp++)
		{
			if ((*itp)->GetType() == FCDEffectParameter::SURFACE)
			{
				const FCDEffectParameterSurface* surf = (const FCDEffectParameterSurface*) ((*itp));
				const FCDImage *img = surf->GetImage();
				if (img != NULL) img->WriteToXML(GetMatCustAttrImageNode(stub));
			}
		}
		profileCG->WriteToXML(GetMatCustAttrFXNode(stub));
		*/
#endif
	}

	if (stub != NULL && profileHLSL != NULL)
	{
		// this is NOT complete! It doesn't include the shader parameters
		// todo. Add profile parameters from <newparam> and <setparam> nodes in the material
		// not only the fx filename.
		if (profileHLSL->GetTechnique(0) != NULL && profileHLSL->GetTechnique(0)->GetCode(0) != NULL)
		{
			const fm::string filestr = profileHLSL->GetTechnique(0)->GetCode(0)->GetFilename();
			if (!filestr.empty())
			{
				material = NewDxMaterial(filestr.c_str(), stub);
				return material;
			}
		}
	}
	
	if (stub != NULL)
	{
		size_t effectParameterCount = colladaMaterial->GetEffectParameterCount();
		if (effectParameterCount == 0) return material;

		/*xmlNode* xnode = GetMatCustAttrInstNode(stub);
		for (FCDEffectParameterList::const_iterator it = plist->begin(); it != plist->end() ; it++)
		{
			if (parameter->GetType() == FCDEffectParameter::SURFACE)
			{
				const FCDEffectParameterSurface* surf = (const FCDEffectParameterSurface*) parameter;
				const FCDImage *img = surf->GetImage();
				if (img != NULL) img->WriteToXML(GetMatCustAttrImageNode(stub));
			}
			parameter->WriteToXML(xnode);
		}*/
	}
	
	if (stub != NULL && colladaEffect->GetExtra() != NULL) // Check for nVidia's <import> mechanism
	{
		const FCDExtra* extra = colladaEffect->GetExtra();
		const FCDETechnique* tech = extra->GetDefaultType()->FindTechnique(DAENV_NVIMPORT_PROFILE);
		if (tech != NULL)
		{
			const FCDENode* node = tech->FindChildNode("import");
			const FCDEAttribute* attr = node->FindAttribute("profile");
			if (attr != NULL && strcmp(attr->GetValue(), DAENV_HLSL_PROFILE) == 0)
			{
				attr = node->FindAttribute(DAENV_FILENAME_PROPERTY);
				if (attr != NULL)
				{
					// FCDAttributes do not do content parsing when loading, which
					// means the string contained is still XML compatable
					fstring filename = FUXmlParser::XmlToString(attr->GetValue());
					filename = FUUri(filename).GetAbsolutePath();
					if (::strstr(filename.c_str(), ".fx") != NULL)
					{
						material = CreateFXMaterial(colladaMaterial, filename, stub);
						return material;
					}
				}
			}
		}

		// Add the data as a Custom Attribute
		//xmlNode* xnode = GetMatCustAttrFXNode(stub);
		//extra->WriteToXML(xnode);
	}
	
	if ((material = stub) == NULL) // Create a default material
	{
		StdMat2* baseMaterial = NewDefaultStdMat();

		baseMaterial->SetShading(SHADE_PHONG);
		baseMaterial->SetName(colladaMaterial->GetName().c_str());

		material = baseMaterial;
	}
	material->SetName(colladaMaterial->GetName().c_str());

	return material;
}

Mtl* MaterialImporter::ImportColladaEffectMaterial(const FCDMaterial* colladaMaterial, FCDEffectProfileFX* fxProfile)
{
	if (colladaMaterial == NULL || fxProfile == NULL)
		return NULL;

	// create our new ColladaEffect
	ColladaEffect* cfx = static_cast<ColladaEffect*>(GetCOLLADAEffectDesc()->Create(FALSE));
	if (cfx == NULL)
		return NULL;

	cfx->SetName(TSTR(colladaMaterial->GetName().c_str()));

	// select a technique based on a hint
	const FCDMaterialTechniqueHintList& hints = colladaMaterial->GetTechniqueHints();
	fstring curTechName;
	int curTechIndex = 0;
	for (FCDMaterialTechniqueHintList::const_iterator it = hints.begin(); it != hints.end(); ++it)
	{
		const FCDMaterialTechniqueHint& hint = (*it);
		if (IsEquivalentI(hint.platform, "PC-DX9"))
		{
			curTechName = hint.technique;
		}
	}

	size_t techCount = fxProfile->GetTechniqueCount();
	int actualTechCount = 0;
	for (size_t i = 0; i < techCount; ++i)
	{
		FCDEffectTechnique* tech = fxProfile->GetTechnique(i);
		if (tech == NULL) continue;
		ColladaEffectTechnique* cfxTech = cfx->addTechnique(tech->GetName());
		if (cfxTech == NULL) continue;

		if (IsEquivalent(tech->GetName(), curTechName))
		{
			curTechIndex = actualTechCount;
		}
		
		size_t passCount = tech->GetPassCount();
		for (size_t j = 0; j < passCount; ++j)
		{
			FCDEffectPass* pass = tech->GetPass(j);
			if (pass == NULL) continue;

			ColladaEffectPass* cfxPass = cfxTech->addPass(pass->GetPassName());
			if (cfxPass == NULL) continue;

			if (j==0)
			{
				// edit this pass
				cfxTech->setEditedPass(0);
			}

			for (size_t k = 0; k < 2; ++k)
			{
				bool isVertex = (k==0);
				FCDEffectPassShader* shader = (isVertex) ? pass->GetVertexShader() : pass->GetFragmentShader();
				if (shader == NULL) continue;
				FCDEffectCode* code = shader->GetCode();
				if (code == NULL || code->GetType() != FCDEffectCode::INCLUDE) continue;
				fstring filename = code->GetFilename();
				// check for CgFX
				if (IsEquivalentI(FUFileManager::GetFileExtension(filename), "CGFX"))
				{
					// the same assumption as in ColladaMaya: the technique and pass
					// targets are the equivalent in both the Collada effect and the
					// CgFX file...
					if (cfxPass->loadEffect(filename, (uint32)i, (uint32)j))
					{
						ImportColladaEffectParameters(colladaMaterial, fxProfile, shader, cfxPass);
						// load the opposite shader parameters
						ImportColladaEffectParameters(colladaMaterial, fxProfile, isVertex ? pass->GetFragmentShader() : pass->GetVertexShader(), cfxPass);
					}
					break; // no need for other imports
				}
				// assume Cg
				if (cfxPass->loadShader(isVertex, filename, TO_FSTRING(shader->GetName())))
				{
					ImportColladaEffectParameters(colladaMaterial, fxProfile, shader, cfxPass);
				}
			} // for vertex|fragment shaders
		} // for each pass
		++ actualTechCount;
	} // for each technique

	// select the current technique (default with no hint = 0)
	cfx->setCurrentTechnique(curTechIndex);

	// set the effect's custom attribute for uniform parameters
	ColladaEffectTechnique* cfxTech = cfx->getCurrentTechnique();
	if (cfxTech != NULL)
	{
		if (cfxTech->getEditedPass() != NULL)
			cfx->reloadUniforms(cfxTech->getEditedPass());
	}

	material = cfx; // buffer created material in importer
	return material;
}

void MaterialImporter::ImportColladaEffectParameters(const FCDMaterial* colladaMaterial, FCDEffectProfileFX* cgEffect, FCDEffectPassShader* shader, ColladaEffectPass* cfxPass)
{
	if (colladaMaterial == NULL || cgEffect == NULL || shader == NULL || cfxPass == NULL) return;

	size_t cgParamCount = cgEffect->GetEffectParameterCount();
	for (size_t p = 0; p < cgParamCount; ++p)
	{
		FCDEffectParameter* parameter = cgEffect->GetEffectParameter(p);
		FCDEffectPassBind* binding = shader->FindBindingReference(parameter->GetReference());
		if (binding == NULL) continue; // This parameter is not used in this shader.

		ColladaEffectParameter* param = cfxPass->getParameterCollection()->findParameter(binding->symbol);
		if (param == NULL) continue; // we haven't loaded this parameter, maybe the source file changed.

		// fetch the modifier (reuse the same parameter if it doesn't exist)
		const FCDEffectParameter* pset = FCDEffectTools::FindEffectParameterByReference(colladaMaterial, parameter->GetReference());
		if (pset == NULL) pset = parameter;

		IParamBlock2* paramblock = param->getParamBlock();
		if (paramblock == NULL) continue; // programming error, should not happen
		ParamID paramid = param->getParamID();

		switch (parameter->GetType()) 
		{
		case FCDEffectParameter::STRING:
			// NOT SUPPORTED yet.
			break;

		case FCDEffectParameter::SURFACE: {
			// MANAGED BY SAMPLER PARAMETER
			break; }

		case FCDEffectParameter::BOOLEAN: {
			FUAssert(param->getType() == ColladaEffectParameter::kBool, break);
			// not animated
			int val = ((FCDEffectParameterBool*)pset)->GetValue() ? 1:0;
			paramblock->SetValue(paramid, TIME_INITIAL_POSE, val);
			param->synchronize();
			break; }

		case FCDEffectParameter::FLOAT: {
			float min = 0.0f, max = 1.0f;
			// parse annotations
			for (size_t i = 0; i < parameter->GetAnnotationCount(); ++i)
			{
				const FCDEffectParameterAnnotation* a = parameter->GetAnnotation(i);
				if (IsEquivalent(a->name, FC("UIMin"))) min = FUStringConversion::ToFloat(*a->value);
				else if (IsEquivalent(a->name, FC("UIMax"))) max = FUStringConversion::ToFloat(*a->value);
			}
			float val = ((FCDEffectParameterFloat*)pset)->GetValue();

			// animated
			paramblock->SetValue(paramid, TIME_INITIAL_POSE, val);
			int animNum = paramblock->GetAnimNum(paramid);
			ANIM->ImportAnimatedFloat(paramblock, animNum, ((FCDEffectParameterFloat*)parameter)->GetValue());
			
			// set the ranges
			FUAssert(max >= min, break);
			ParamDef& def = paramblock->GetParamDef(paramid);
			def.range_low.f = min;
			def.range_high.f = max;

			break; }

		case FCDEffectParameter::INTEGER: {
			// NOT SUPPORTED
			break; }

		case FCDEffectParameter::FLOAT2: {
			// NOT SUPPORTED
			break; }

		case FCDEffectParameter::FLOAT3: {
			// RGB COLOR ONLY
			FMVector3& v = ((FCDEffectParameterFloat3*)pset)->GetValue();

			// animated
			Point3 val(v.x, v.y, v.z);
			paramblock->SetValue(paramid, TIME_INITIAL_POSE, val);
			int animNum = paramblock->GetAnimNum(paramid);
			ANIM->ImportAnimatedPoint3(paramblock, animNum, ((FCDEffectParameterFloat3*) parameter)->GetValue());
			break; }

		case FCDEffectParameter::VECTOR: {
			// RGBA COLOR or NODE POSITION
			FMVector4& v = ((FCDEffectParameterVector*)pset)->GetValue();

			// Heuristic: check annotations to determine if its a color or a node position
			bool isNode = false;
			for (size_t i = 0; i < parameter->GetAnnotationCount(); ++i)
			{
				const FCDEffectParameterAnnotation* a = parameter->GetAnnotation(i);
				if (IsEquivalentI(a->name, FC("OBJECT")))
				{ 
					isNode = true;
					break;
				}
			}

			if (isNode)
			{
				// not animated
				paramblock->SetValue(paramid, TIME_INITIAL_POSE, (INode*)NULL);
				// register the parameter for post-import node retrieval
				ColladaEffectParameterNode* nodeParam = static_cast<ColladaEffectParameterNode*>(param);
				if (nodeParam != NULL)
				{
					PostImportListener::GetInstance().RegisterNodeParameter(nodeParam);
				}
			}
			else
			{
				// animated
				AColor val(v.x,v.y,v.z,v.w);
				paramblock->SetValue(paramid, TIME_INITIAL_POSE, val);
				int animNum = paramblock->GetAnimNum(paramid);
				ANIM->ImportAnimatedPoint4(paramblock, animNum, ((FCDEffectParameterVector*) parameter)->GetValue());
			}
			break; }

		case FCDEffectParameter::SAMPLER: {
			// find source <surface> in material
			FCDEffectParameterSurface* baseSurface = ((FCDEffectParameterSampler*) parameter)->GetSurface();
			if (baseSurface == NULL) break;
			const FCDEffectParameter* psurface = FCDEffectTools::FindEffectParameterByReference(colladaMaterial, baseSurface->GetReference());
			if (psurface == NULL) psurface = FCDEffectTools::FindEffectParameterByReference(cgEffect, baseSurface->GetReference());
			if (psurface == NULL || psurface->GetType() != FCDEffectParameter::SURFACE) break;
			FCDEffectParameterSurface* colladaSurface = (FCDEffectParameterSurface*) psurface;
			FCDImage* colladaImage = colladaSurface->GetImage();
			if (colladaImage == NULL) break;

			TSTR filename(colladaImage->GetFilename().c_str());
			PBBitmap bmp;
			bmp.bi.SetName(filename.data());
			paramblock->SetValue(paramid, TIME_INITIAL_POSE, &bmp);
			
			break; }

		case FCDEffectParameter::MATRIX: {
			FCDEffectParameterMatrix* matrix = (FCDEffectParameterMatrix*) pset;
			Matrix3 val = ToMatrix3(matrix->GetValue());
			// not animated
			paramblock->SetValue(paramid, TIME_INITIAL_POSE, val);
			break; }
		default:
			break;
		}
	}
}



// Create a 3dsMax material for the given COLLADA material
Mtl* MaterialImporter::CreateStdMaterial(const FCDEffectStandard* colladaStdMtl, FCDMaterialInstance* materialInstance, FCDGeometryInstance* geometryInstance)
{
	if (colladaStdMtl == NULL) return NULL;

	// Create a Max standard material
	StdMat2* baseMaterial = NewDefaultStdMat();
	FCDEffectStandard::LightingType materialType = colladaStdMtl->GetLightingType();

	//Synchronize animated params at the geometry instance level, if any.
	FCDEffectTools::SynchronizeAnimatedParams(geometryInstance, materialInstance);

	//Get all the params, including those that are animated.
	bool dummy;
	FCDParameterAnimatableColor4* diffuseColor = FCDEffectTools::GetAnimatedColor(geometryInstance, materialInstance->GetMaterial(), FCDEffectStandard::DiffuseColorSemantic, &dummy);
	FCDParameterAnimatableColor4* ambientColor = FCDEffectTools::GetAnimatedColor(geometryInstance, materialInstance->GetMaterial(), FCDEffectStandard::AmbientColorSemantic, &dummy); 
	FCDParameterAnimatableColor4* specularColor = FCDEffectTools::GetAnimatedColor(geometryInstance, materialInstance->GetMaterial(), FCDEffectStandard::SpecularColorSemantic, &dummy);
	FCDParameterAnimatableFloat* specularFactor = FCDEffectTools::GetAnimatedFloat(geometryInstance, materialInstance->GetMaterial(), FCDEffectStandard::SpecularFactorSemantic);
	FCDParameterAnimatableFloat* shininess = FCDEffectTools::GetAnimatedFloat(geometryInstance, materialInstance->GetMaterial(), FCDEffectStandard::ShininessSemantic);
	FCDParameterAnimatableColor4* translucencyColor = FCDEffectTools::GetAnimatedColor(geometryInstance, materialInstance->GetMaterial(), FCDEffectStandard::TranslucencyColorSemantic, &dummy);
	FCDParameterAnimatableFloat* emissionFactor = FCDEffectTools::GetAnimatedFloat(geometryInstance, materialInstance->GetMaterial(), FCDEffectStandard::EmissionFactorSemantic);
	FCDParameterAnimatableColor4* emissionColor = FCDEffectTools::GetAnimatedColor(geometryInstance, materialInstance->GetMaterial(), FCDEffectStandard::EmissionColorSemantic, &dummy);
	FCDParameterAnimatableFloat* translucencyFactor = FCDEffectTools::GetAnimatedFloat(geometryInstance, materialInstance->GetMaterial(), FCDEffectStandard::TranslucencyFactorSemantic);


	switch (materialType)
	{
	case FCDEffectStandard::CONSTANT: baseMaterial->SetFaceted(true); // BUG393: Max actually does not support a constant shader!
	case FCDEffectStandard::BLINN: baseMaterial->SwitchShader(Class_ID(FSStandardMaterial::STD2_BLINN_SHADER_CLASS_ID, 0)); break;
	case FCDEffectStandard::LAMBERT:
	case FCDEffectStandard::PHONG:
	case FCDEffectStandard::UNKNOWN:
	default: baseMaterial->SwitchShader(Class_ID(FSStandardMaterial::STD2_PHONG_CLASS_ID, 0)); break;
	}

	// Retrieve the shader parameter blocks
	Shader* materialShader = baseMaterial->GetShader();
	IParamBlock2* shaderParameters = (IParamBlock2*) materialShader->GetReference(0);
	IParamBlock2* extendedParameters = (IParamBlock2*) baseMaterial->GetReference(FSStandardMaterial::EXTENDED_PB_REF);

	// Common material parameters
	baseMaterial->SetName(colladaEffect->GetName().c_str());
	baseMaterial->SetDiffuse(ToColor(**diffuseColor), 0);
	ANIM->ImportAnimatedFRGBA(shaderParameters, FSStandardMaterial::shdr_diffuse, *diffuseColor);
	ANIM->ImportAnimatedFloat(shaderParameters, FSStandardMaterial::shdr_self_illum_amnt, *emissionFactor);

	// Self-Illumination
	// Can be either the emission color or a factor
	if (colladaStdMtl->IsEmissionFactor())
	{
		baseMaterial->SetSelfIllumColorOn(FALSE);
		baseMaterial->SetSelfIllum(**emissionFactor, 0);
		ANIM->ImportAnimatedFloat(shaderParameters, FSStandardMaterial::shdr_self_illum_amnt, *emissionFactor);
	}
	else
	{
		baseMaterial->SetSelfIllumColorOn(TRUE);
		baseMaterial->SetSelfIllumColor(ToColor(**emissionColor), 0);
		ANIM->ImportAnimatedFRGBA(shaderParameters, FSStandardMaterial::shdr_self_illum_color, *emissionColor);
	}

	// Opacity is more complex: its animation may be based on the factor or the color
	baseMaterial->SetOpacity(colladaStdMtl->GetOpacity(), 0);

	// [staylor] - All ColladaMax pre 3.03 materials exported with an opacity of 1 meaning a transparency of 1
	// On import, all these scenes are fully transparent.  We can test the version exported with, and fix the 
	// transparency, can we make this optional?
	FCDConversionFunctor* opacityConv = NULL;
	if (GetDocument()->IsDocumentColladaMaxExported() && GetDocument()->GetDocumentColladaMaxVersion() < 3.0299f && GetDocument()->GetDocumentColladaMaxVersion() > 1.0299f) // Account for float errors
	{
		opacityConv = &FSConvert::ToMaxOpacity;
	}

	bool useTranslucencyFactor = translucencyFactor->IsAnimated();
	FCDAnimated* translucencyAnimated = (useTranslucencyFactor) ? translucencyFactor->GetAnimated() : translucencyColor->GetAnimated();
	if (colladaStdMtl->GetTextureCount(FUDaeTextureChannel::TRANSPARENT) == 0)
	{
		ANIM->ImportAnimatedFloat(extendedParameters, FSStandardMaterial::std2_opacity, translucencyAnimated, opacityConv, !useTranslucencyFactor);
	}

	if (materialType != FCDEffectStandard::CONSTANT && materialType != FCDEffectStandard::UNKNOWN)
	{
		// Unlock the ambient and diffuse colors
		materialShader->SetLockAD(FALSE);
		materialShader->SetLockADTex(FALSE);
		baseMaterial->LockAmbDiffTex(FALSE);
		baseMaterial->SyncADTexLock(FALSE);

		// Lambert/Phong material parameters
		baseMaterial->SetAmbient(ToColor(*ambientColor), 0);
		ANIM->ImportAnimatedFRGBA(shaderParameters, FSStandardMaterial::shdr_ambient, *ambientColor);
	}
	else
	{
		// Approximate constant shader, specular is the same color
		baseMaterial->SetSpecular(ToColor(*diffuseColor), 0);
	}
	if (materialType == FCDEffectStandard::PHONG || materialType == FCDEffectStandard::BLINN)
	{
		// Phong and Blinn have a very different shininess factor meaning.
		ReverseShininessConversionFunctor blinnShininess;
		FCDConversionFunctor* shininessFunctor = &FSConvert::ToMaxGlossiness;
		if (materialType == FCDEffectStandard::BLINN)
		{
			shininessFunctor = &blinnShininess;

			// Backward compatibility: documents before 3.04 exported Blinn-Phong shininess
			if (GetDocument()->IsDocumentColladaMaxExported() && GetDocument()->GetDocumentColladaMaxVersion() < 3.0399f) // Account for float errors
			{
				shininessFunctor = &FSConvert::ToMaxGlossiness;
			}
		}

		// Phong material parameters
		baseMaterial->SetSpecular(ToColor(*specularColor), 0);
		ANIM->ImportAnimatedFRGBA(shaderParameters, FSStandardMaterial::shdr_specular, *specularColor);
		baseMaterial->SetShininess((*shininessFunctor)(*shininess), 0);
		ANIM->ImportAnimatedFloat(shaderParameters, FSStandardMaterial::shdr_glossiness, *shininess, shininessFunctor);
		baseMaterial->SetShinStr(*specularFactor, 0);
		ANIM->ImportAnimatedFloat(shaderParameters, FSStandardMaterial::shdr_spec_lvl, *specularFactor);
	}

	// Geometric modifier flags
	baseMaterial->SetFaceted(FUStringConversion::ToBoolean(colladaStdMtl->GetExtraAttribute(DAEMAX_MAX_PROFILE, DAEMAX_FACETED_MATERIAL_PARAMETER)));
	baseMaterial->SetWire(FUStringConversion::ToBoolean(colladaStdMtl->GetExtraAttribute(DAEMAX_MAX_PROFILE, DAEMAX_WIREFRAME_MATERIAL_PARAMETER)));
	baseMaterial->SetTwoSided(FUStringConversion::ToBoolean(colladaStdMtl->GetExtraAttribute(DAEMAX_MAX_PROFILE, DAESHD_DOUBLESIDED_PARAMETER)));
	baseMaterial->SetFaceMap(FUStringConversion::ToBoolean(colladaStdMtl->GetExtraAttribute(DAEMAX_MAX_PROFILE, DAEMAX_FACEMAP_MATERIAL_PARAMETER)));

	// The current StdMat object will do the trick, create/assign to it the textures
	material = baseMaterial;
	for (int i = 0; i < NTEXMAPS; ++i)
	{
		size_t numTextures = colladaStdMtl->GetTextureCount(textureChannelMap[i]);
		if (numTextures == 1)
		{
			BitmapTex* texture = ImportTexture(colladaStdMtl, colladaStdMtl->GetTexture(textureChannelMap[i], 0));
			FUAssert(texture != NULL, continue);
			AssignTextureToMtl(material, i, texture, colladaStdMtl);

			// Specific to opacity factors, if there is a texture AND an opacity factor animation,
			// dump the animation on the "output" scale parameter of the tex-map.
			if (i == ID_OP)
			{
//				IParamBlock* params = (IParamBlock*) texture->GetTexout()->GetReference(FSTextureOutput::param_block_ref_id);
//				ANIM->ImportAnimatedFloat(params, FSTextureOutput::output_amount, translucencyPointer, opacityConv, !useTranslucencyFactor);
			}
		}
		else if (numTextures > 1)
		{
			Texmap *compositeTexMap = (Texmap*)GetDocument()->MaxCreate(TEXMAP_CLASS_ID, FSCompositeMap::classID);
			IParamBlock2* compParams = compositeTexMap->GetParamBlock(FSCompositeMap::comptex_params);

			compParams->SetCount(FSCompositeMap::comptex_tex, (int)numTextures);
			for (uint32 j = 0; j < numTextures; ++j)
			{
				Texmap* newTexture = ImportTexture(colladaStdMtl, colladaStdMtl->GetTexture(textureChannelMap[i], j));
				compParams->SetValue(FSCompositeMap::comptex_tex, 0, newTexture, j);
			}

			AssignTextureToMtl(material, i, compositeTexMap, colladaStdMtl);
		}
	}
	return material;
}


// Import a COLLADA texture into the Max material object
void MaterialImporter::AssignTextureToMtl(Mtl* material, int slot, Texmap* texture, const FCDEffectStandard* effect)
{
	if (material == NULL) return;
	if (slot < 0 || slot >= NTEXMAPS) return;

	// Assign it to the material
	material->SetSubTexmap(slot, texture);

	// For diffuse textures, view them in the viewport
	if (slot == ID_DI)
	{
		// From Sparks Knowledge-base: "Topic: How to Activate a Texmap in viewport using API ??"
		material->SetActiveTexmap(texture);
		material->SetMtlFlag(MTL_TEX_DISPLAY_ENABLED);
		material->NotifyDependents(FOREVER, (PartID) PART_ALL, REFMSG_CHANGE);
	}

	if (texture->GetInterface(BITMAPTEX_INTERFACE) != NULL)
	{
		BitmapTex* bmTexture = (BitmapTex*)texture;
		bitmapTextureMaps[slot].push_back((BitmapTex*) texture);
		// Read in the transparency mode for opacity textures
		if (slot == ID_OP)
		{
			BOOL isAlphaTranslucency = effect->GetTransparencyMode() == FCDEffectStandard::A_ONE;
			bmTexture->SetAlphaAsMono(isAlphaTranslucency);
			bmTexture->SetAlphaAsRGB(!isAlphaTranslucency); // [glaforte 22-08-2006] Are both calls needed? This is a radio box in the UI.
		}
	}
		
	if (material->ClassID().PartA() == DMTL2_CLASS_ID || material->ClassID().PartA() == DMTL_CLASS_ID)
	{
		StdMat2* stdMat = (StdMat2*) material;
		// Override the default amount set here, the final amount will
		// be decided by the amount multipliers on the textures them selves
		stdMat->SetTexmapAmt(slot, 1.0f, 0);
	}
	
}

void MaterialImporter::ImportUVParam(const FCDETechnique* extraUVParams, IParamBlock* pblock, int id, const char* name, FCDConversionFunctor* conv)
{
	const FCDENode* node = extraUVParams->FindParameter(name);
	if (node != NULL) 
	{
		float value = FUStringConversion::ToFloat(node->GetContent());
		if (conv != NULL) value = (*conv)(value);
		pblock->SetValue(id, TIME_INITIAL_POSE, value);
		ANIM->ImportAnimatedFloat(pblock, id, const_cast<FCDAnimatedCustom*>(node->GetAnimated()), conv);
	}
}

void MaterialImporter::ImportUVOffsetParam(const FCDETechnique* extraUVTechnique, IParamBlock* pblock, const int targId, const char* offsetName, const char* frameName, float repeat, bool isMirrored)
{
	const FCDENode* translateFrame = extraUVTechnique->FindParameter(frameName);
	const FCDENode* offset = extraUVTechnique->FindParameter(offsetName);
	
	// In max, we can either support translate frameU/V OR offsetU/V, but not both
	bool bHasTransFrame = translateFrame != NULL;
	if (bHasTransFrame && offset)
	{
		// We have too many parameters, try and remove one.
		if (!IsEquivalent(FUStringConversion::ToFloat(offset->GetContent()), 0.0f) ||
			(offset->GetAnimated() != NULL &&
			 offset->GetAnimated()->HasCurve()))
		{
			// We definately use the offset, try and remove the frame translate
			if (!IsEquivalent(FUStringConversion::ToFloat(translateFrame->GetContent()), 0.0f) ||
				(translateFrame->GetAnimated() != NULL &&
				 translateFrame->GetAnimated()->HasCurve()))
			{
				// ERROR: unsupported values
				FUError::Error(FUError::WARNING_LEVEL, FUError::WARNING_UNSUPPORTED_TEXTURE_UVS);
			}
			// at this point, we ditch the translate, regardless of whether it affects
			// the final value.  This is because that makes more sense in terms of naming
			bHasTransFrame = false;
		}
	}

	if (bHasTransFrame)
	{
		// If we do not tile, then the U is just offset up.  In maya-nese, this is called the frame translate.+
		FCDConversionScaleFunctor offsetFunctor(1.0f / repeat);
		ImportUVParam(extraUVTechnique, pblock, targId, frameName, &offsetFunctor);
	}
	else
	{
		FSConvert::FCDConversionMayaToMaxUV offsetFunctor(repeat, isMirrored);
		ImportUVParam(extraUVTechnique, pblock, targId, offsetName, &offsetFunctor);
	}
}

// Create a Max texture based on the FCDTexture and return it.
BitmapTex* MaterialImporter::ImportTexture(const FCDEffectStandard* UNUSED(effect), const FCDTexture* colladaTexture)
{
	if (colladaTexture == NULL || colladaTexture->GetImage() == NULL) return NULL;


	// Pre-process the image filename to transform the '/' in '\'
	const FCDImage* image = colladaTexture->GetImage();
	colladaTexture->GetSet();

	// Create the BitmapTex object
	BitmapTex* bitmapTextureMap = NewDefaultBitmapTex();
	bitmapTextureMap->SetMapName(const_cast<char*>(image->GetFilename().c_str()));
	bitmapTextureMap->LoadMapFiles(0);
	const int setIndex = colladaTexture->GetSet()->GetValue();
	bitmapTextureMap->GetUVGen()->SetMapChannel(setIndex+1);
	
	// Set the output amount, this is what controls a textures effect on the material
	// (for blending etc)
	const FCDETechnique* colladaMaxTechnique = colladaTexture->GetExtra()->GetDefaultType()->FindTechnique(DAEMAX_MAX_PROFILE);
	if (colladaMaxTechnique != NULL)
	{
		const FCDENode* amountParameter = colladaMaxTechnique->FindParameter(DAEMAX_AMOUNT_TEXTURE_PARAMETER);
		if (amountParameter != NULL)
		{
			float amount = FUStringConversion::ToFloat(amountParameter->GetContent());
			TextureOutput* texOut = bitmapTextureMap->GetTexout();
			
			try
			{
				// This isn't as bad as below, but still dodgy
				IParamBlock* params = (IParamBlock*)texOut->GetReference(FSTextureOutput::param_block_ref_id);
				params->SetValue(FSTextureOutput::bump_amount, TIME_INITIAL_POSE, amount);
				params->SetValue(FSTextureOutput::output_amount, TIME_INITIAL_POSE, amount);
			}
			catch (...) 
			{
				// Ok, so miracle cast didnt work, default to what we can do.
				bitmapTextureMap->SetOutputLevel(0, amount);
			}
		}
	}

	const FCDETechnique* colladaUVTechnique = colladaTexture->GetExtra()->GetDefaultType()->FindTechnique(DAEMAYA_MAYA_PROFILE);
	if (colladaUVTechnique != NULL)
	{
		UVGen* uvGen = bitmapTextureMap->GetTheUVGen();
		FUAssert(uvGen != NULL && uvGen->ClassID().PartA() == STDUV_CLASS_ID, return bitmapTextureMap);
		StdUVGen* stdGen = (StdUVGen*)uvGen;

		IParamBlock* uvParams = (IParamBlock*)stdGen->GetReference(FSStdUVGen::pblock);

		// reset all flags
		stdGen->SetFlag(U_WRAP|V_WRAP|U_MIRROR|V_MIRROR, 0);

		const FCDENode* mirrorU = colladaUVTechnique->FindParameter(DAEMAYA_TEXTURE_MIRRORU_PARAMETER);
		if (mirrorU != NULL) stdGen->SetFlag(U_MIRROR, FUStringConversion::ToInt32(mirrorU->GetContent()));
		bool isMirrorU = stdGen->GetFlag(U_MIRROR) != 0;

		const FCDENode* mirrorV = colladaUVTechnique->FindParameter(DAEMAYA_TEXTURE_MIRRORV_PARAMETER);
		if (mirrorV != NULL) stdGen->SetFlag(V_MIRROR, FUStringConversion::ToInt32(mirrorV->GetContent()));
		bool isMirrorV = stdGen->GetFlag(V_MIRROR) != 0;
		
		if (!isMirrorU)
		{
			const FCDENode* tileU = colladaUVTechnique->FindParameter(DAEMAYA_TEXTURE_WRAPU_PARAMETER);
			if (tileU != NULL) stdGen->SetFlag(U_WRAP, FUStringConversion::ToInt32(tileU->GetContent()));
		} 
		if (!isMirrorV) 
		{
			const FCDENode* tileV = colladaUVTechnique->FindParameter(DAEMAYA_TEXTURE_WRAPV_PARAMETER);
			if (tileV != NULL) stdGen->SetFlag(V_WRAP, FUStringConversion::ToInt32(tileV->GetContent()));
		}
		
		// Calculate and import and calculate the tiling values.
		// We need to half those when dealing with mirror, because 3dsMax automatically doubles them.
		FCDConversionScaleFunctor halfFunctor(0.5f);
		float repeatU = 1, repeatV = 1;
		ImportUVParam(colladaUVTechnique, uvParams, FSStdUVGen::u_tiling, DAEMAYA_TEXTURE_REPEATU_PARAMETER, isMirrorU ? &halfFunctor : NULL);
		ImportUVParam(colladaUVTechnique, uvParams, FSStdUVGen::v_tiling, DAEMAYA_TEXTURE_REPEATV_PARAMETER, isMirrorV ? &halfFunctor : NULL);

		repeatU = uvParams->GetFloat(FSStdUVGen::u_tiling);
		repeatV = uvParams->GetFloat(FSStdUVGen::v_tiling);
		ImportUVOffsetParam(colladaUVTechnique, uvParams, FSStdUVGen::u_offset, DAEMAYA_TEXTURE_OFFSETU_PARAMETER, DAEMAYA_TEXTURE_TRANSFRAMEU_PARAMETER, repeatU, isMirrorU);
		ImportUVOffsetParam(colladaUVTechnique, uvParams, FSStdUVGen::v_offset, DAEMAYA_TEXTURE_OFFSETV_PARAMETER, DAEMAYA_TEXTURE_TRANSFRAMEV_PARAMETER, repeatV, isMirrorU);

		// Import the rotation around W.
		FCDConversionScaleFunctor degToRadFunctor(FMath::DegToRad(1.0f));
		ImportUVParam(colladaUVTechnique, uvParams, FSStdUVGen::w_angle, DAEMAYA_TEXTURE_ROTATEUV_PARAMETER, &degToRadFunctor);

		const FCDENode* noiseU = colladaUVTechnique->FindParameter(DAEMAYA_TEXTURE_NOISEU_PARAMETER);
		const FCDENode* noiseV = colladaUVTechnique->FindParameter(DAEMAYA_TEXTURE_NOISEV_PARAMETER);
		if (noiseU || noiseV)
		{
			stdGen->SetFlag(UV_NOISE, 1);
			const char* name = (noiseU != NULL) ? DAEMAYA_TEXTURE_NOISEU_PARAMETER : DAEMAYA_TEXTURE_NOISEV_PARAMETER;
			ImportUVParam(colladaUVTechnique, uvParams, FSStdUVGen::noise_amt, name);
		}
	}
	return bitmapTextureMap;
}

// Assign to this material the map channels from the geometry's definition
void MaterialImporter::AssignMapChannels(FCDMaterialInstance* colladaInstance, MapChannelMap& mapChannels)
{
	FCDMaterial* materialCopy = colladaInstance->GetMaterial();
	if (materialCopy == NULL) return;
	FCDEffect* colladaEffect = materialCopy->GetEffect();
	if (colladaEffect == NULL) return;
	if (colladaEffect->HasProfile(FUDaeProfileType::CG) || colladaEffect->HasProfile(FUDaeProfileType::HLSL)) return;
	FCDEffectStandard* colladaStdMtl = (FCDEffectStandard*) colladaEffect->FindProfile(FUDaeProfileType::COMMON);
	if (colladaStdMtl == NULL) return;

	for (size_t maxIndex = 0; maxIndex < NTEXMAPS; ++maxIndex)
	{
		uint32 daeIndex = textureChannelMap[maxIndex];
		size_t daeTexCount = colladaStdMtl->GetTextureCount(daeIndex);
		size_t maxTexCount = bitmapTextureMaps[maxIndex].size();
		size_t texCount = min(maxTexCount, daeTexCount);
		for (size_t i = 0; i < texCount; ++i)
		{
			BitmapTex* bitmapTextureMap = bitmapTextureMaps[maxIndex][i];
			if (bitmapTextureMap == NULL) continue;
			const FCDTexture* colladaTexture = colladaStdMtl->GetTexture(daeIndex, i);
			size_t bindingCount = colladaInstance->GetVertexInputBindingCount();
			for (size_t b = 0; b < bindingCount; ++b) 
			{
				FCDMaterialInstanceBindVertexInput* binding = colladaInstance->GetVertexInputBinding(b);
				if (binding->inputSemantic == FUDaeGeometryInput::TEXCOORD && binding->semantic == colladaTexture->GetSet()->GetSemantic())
				{
					MapChannelMap::iterator itC = mapChannels.find(binding->inputSet);
					int mapChannel = (itC != mapChannels.end()) ? (*itC).second : 1;
					bitmapTextureMap->GetUVGen()->SetMapChannel(mapChannel); 
				}
			}
		}
	}
}

bool getDxMaterialParamID(Mtl* mtl, fm::string pname, ParamID &pid)
{
	TSTR maxParamName(pname.c_str());

	bool paramfound = false;
	IParamBlock2 * pblock = mtl->GetParamBlock(0);
	for (int i=0; i< pblock->NumParams();i++)
	{
		ParamID _pid = pblock->IndextoID(i);
		ParamDef def = pblock->GetParamDef(_pid);

		if (def.ctrl_type != TYPE_INTLISTBOX && _tcscmp(maxParamName.data(),def.int_name)==0)
		{
			pid = _pid;
			paramfound = true;
			break;
		}
	}

	return paramfound;
}

void MaterialImporter::ImportMaterialInstanceBindings(FCDMaterialInstance* materialInstance)
{
	if (material == NULL) return;

	if (material->ClassID() == COLLADA_EFFECT_ID)
	{
		ColladaEffect* colladaEffect = (ColladaEffect*)material;

		size_t techCount = colladaEffect->getTechniqueCount();
		for (size_t i = 0; i < techCount; ++i)
		{
			ColladaEffectTechnique* tech = colladaEffect->getTechnique(i);
			if (tech == NULL) continue;
			size_t passCount = tech->getPassCount();
			for (size_t j = 0; j < passCount; ++j)
			{
				ColladaEffectPass* pass = tech->getPass(j);
				if (pass == NULL || pass->getParameterCollection() == NULL) continue;
				ColladaEffectParameterList& params = pass->getParameterCollection()->getParameters();
				for (size_t k = 0; k < params.size(); ++k)
				{
					const FCDMaterialInstanceBind* binding = materialInstance->FindBinding(params[k]->getSemanticString());
					if (params[k]->hasNode() && binding != NULL)
					{
						ColladaEffectParameterNode* np = (ColladaEffectParameterNode*)params[k];
						fm::string uri = binding->target;
						if (!uri.empty() && uri[0] == '#') uri = uri.substr(1);
						np->setNodeId(uri);
					}
				} // for each parameter
			} // for each pass
		} // for each technique
	}
}

Mtl* MaterialImporter::CreateFXMaterial(const FCDMaterial* colladaMaterial, fstring& filename, Mtl* stub)
{
	material = NewDxMaterial(filename.c_str(), stub);
	TSTR str = _T(colladaMaterial->GetName().c_str());
	stub->SetName(str+"-std");
	material->SetName(colladaMaterial->GetName().c_str());
	
	IParamBlock2 * pblock = material->GetParamBlock(0);
	int numParams = pblock->NumParams();

	size_t parameterCount = colladaMaterial->GetEffectParameterCount();
	for (size_t p = 0; p < parameterCount; ++p)
	{
		const FCDEffectParameter* parameter = colladaMaterial->GetEffectParameter(p);
		const fm::string& ref = parameter->GetReference();
		TSTR pname(ref.c_str());
		ParamID pid = (ParamID) 0;
		bool paramfound = false;

		for (int i=0; i< numParams;i++)
		{
			ParamDef def = pblock->GetParamDef(pblock->IndextoID(i));

			if (def.ctrl_type != TYPE_INTLISTBOX && _tcscmp(pname.data(),def.int_name)==0)
			{
				pid = pblock->IndextoID(i);
				paramfound = true;
				break;
			}
		}

		if (paramfound == false) continue;

		switch (parameter->GetType())
		{
		case FCDEffectParameter::BOOLEAN:
			{
				const FCDEffectParameterBool* param = (const FCDEffectParameterBool*) parameter;
				pblock->SetValue(pid, 0, param->GetValue());
				// animation not supported
				break;
			}
		case FCDEffectParameter::SAMPLER:
			{
				break;
			}
		case FCDEffectParameter::INTEGER:
			{
				const FCDEffectParameterInt* param = (const FCDEffectParameterInt*) parameter;
				pblock->SetValue(pid, 0, param->GetValue());
				// animation not supported
				break;
			}
		case FCDEffectParameter::FLOAT:
			{
				const FCDEffectParameterFloat* param = (const FCDEffectParameterFloat*) parameter;
				pblock->SetValue(pid, 0, param->GetValue());
				ANIM->ImportAnimatedFloat(pblock, pid, param->GetValue());
				break;
			}
		case FCDEffectParameter::FLOAT2:
			{
				const FCDEffectParameterFloat2* param = (const FCDEffectParameterFloat2*) parameter;
				Point3 pt(param->GetValue()->x, param->GetValue()->y, 0.0f);
				pblock->SetValue(pid, 0, pt);
				// animation not supported
				break;
			}
		case FCDEffectParameter::FLOAT3:
			{
				const FCDEffectParameterFloat3* param = (const FCDEffectParameterFloat3*) parameter;
				Point3 pt = ToPoint3(param->GetValue());
				pblock->SetValue(pid, 0, pt);
				ANIM->ImportAnimatedPoint3(pblock, pblock->GetAnimNum(pid), param->GetValue());
				break;
			}
		case FCDEffectParameter::VECTOR:
			{
				const FCDEffectParameterVector* param = (const FCDEffectParameterVector*) parameter;
				const FMVector4& vec = param->GetValue();
				Point4 pt(vec[0], vec[1], vec[2], vec[3]);
				pblock->SetValue(pid, 0, pt);
				ANIM->ImportAnimatedPoint4(pblock, pblock->GetAnimNum(pid), param->GetValue());
				break;
			}
		case FCDEffectParameter::MATRIX:
			{
				break;
			}
		case FCDEffectParameter::STRING:
			{
				break;
			}
		case FCDEffectParameter::SURFACE:
			{
				const FCDEffectParameterSurface* param = (const FCDEffectParameterSurface*) parameter;
				const FCDImage* image = param->GetImage();
				if (image != NULL)
				{
					TSTR filename(image->GetFilename().c_str());
					PBBitmap bmp;
					bmp.bi.SetName(filename.data());
					pblock->SetValue(pid, 0, &bmp);
				}
				break;
			}
		}
	}

	return material;
}

XRefMaterialImporter::XRefMaterialImporter(Mtl *_material) : MaterialImporter(NULL)
{
	material = _material;
}
