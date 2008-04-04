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

// Class specialized for material/texture/image export

#include "StdAfx.h"
#include "AnimationExporter.h"
#include "ColladaAttrib.h"
#include "ColladaExporter.h"
#include "DocumentExporter.h"
#include "EntityExporter.h"
#include "ColladaOptions.h"
#include "GeometryExporter.h"
#include "MaterialExporter.h"
#include "NodeExporter.h"
#include "Common/StandardMtl.h"
#include "Common/MultiMtl.h"
#include "Common/DX9Mtl.h"
#include "Common/ConversionFunctions.h"
#include "XRefFunctions.h"
#include "TextureOutput.h"
#include "ColladaFX/ColladaEffect.h"
#include "ColladaFX/ColladaEffectPass.h"
#include "ColladaFX/ColladaEffectParameter.h"
#include "ColladaFX/ColladaEffectShader.h"
#include <IGlobalDXDisplayManager.h>

#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDAnimation.h"
#include "FCDocument/FCDAnimationChannel.h"
#include "FCDocument/FCDAnimationCurve.h"
#include "FCDocument/FCDEffect.h"
#include "FCDocument/FCDEffectCode.h"
#include "FCDocument/FCDEffectParameter.h"
#include "FCDocument/FCDEffectParameterFactory.h"
#include "FCDocument/FCDEffectParameterSampler.h"
#include "FCDocument/FCDEffectParameterSurface.h"
#include "FCDocument/FCDEffectPass.h"
#include "FCDocument/FCDEffectPassShader.h"
#include "FCDocument/FCDEffectProfile.h"
#include "FCDocument/FCDEffectStandard.h"
#include "FCDocument/FCDEffectProfileFX.h"
#include "FCDocument/FCDEffectTechnique.h"
#include "FCDocument/FCDExtra.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDGeometryMesh.h"
#include "FCDocument/FCDGeometryPolygons.h"
#include "FCDocument/FCDGeometryPolygonsInput.h"
#include "FCDocument/FCDGeometrySource.h"
#include "FCDocument/FCDImage.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDMaterial.h"
#include "FCDocument/FCDMaterialInstance.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDTexture.h"
#include "FUtils/FUDaeEnum.h"
#include "FUtils/FUFileManager.h"
#include "FUtils/FUUri.h"
#include "FUtils/FUXmlWriter.h"

// MaxSDK
#include <bitmap.h>
#include "stdmat.h"
#include "icustattribcontainer.h"
#include "ColladaAttrib.h"
#include "idxmaterial.h"
#include <algorithm>

//
// Conversion functors
//

class ShininessConversionFunctor : public FCDConversionFunctor
{
public:
	virtual float operator() (float v)
	{
		// According to my derivation analysis, using MS Excel: the
		// best way to appromixate this Blinn-Phong factor into the micro-facet Blinn
		// factor is using the formulas below. This only works in very specific
		// angles, otherwise the values are slightly off: but it is better than nothing!
		//
		// To avoid a C0 discontinuity: do a linear interpolation of the two
		// formulas when x is between 0.06f and 0.16f.
		//
		float out = 0.0f;
		v = FMath::Clamp(v, 0.0001f, 1.0f);
		if (v >= 0.06f) out = 0.64893f * expf(-3.3611f * v);
		if (v <= 0.16f)
		{
			float factor = max(0.0f, (v - 0.06f) / (0.16f - 0.06f));
			float f = -0.157f * logf(v) + 0.0226f;
			out = out * factor + (1.0f - factor) * f;
		}
		return out;
	}
};

// Normal_Bump
#define GNORMAL_CLASS_ID	Class_ID(0x243e22c6, 0x63f6a014)

#define BLEND_COLOR(src, blend, amt) { \
				src[0] += (blend[0] - src[0]) * amt; \
				src[1] += (blend[1] - src[1]) * amt; \
				src[2] += (blend[2] - src[2]) * amt; \
				}

FUDaeTextureChannel::Channel MaxIdToFCDChannel(StdMat2* mtl, int id) 
{
	FUAssert(mtl != NULL, return FUDaeTextureChannel::UNKNOWN);

	// Order of channels depends on shader.
	bool stdShader = true;
	Shader* shader = mtl->GetShader();
	if (shader)
	{
		Class_ID shaderClassId = shader->ClassID();
		unsigned long partA = shaderClassId.PartA();
		if (shaderClassId.PartB() != 0 ||
				!(
				partA == FSStandardMaterial::STD2_PHONG_CLASS_ID ||
				partA == FSStandardMaterial::STD2_BLINN_SHADER_CLASS_ID ||
				partA == FSStandardMaterial::STD2_METAL_SHADER_CLASS_ID ||
				partA == FSStandardMaterial::STD2_ANISOTROPIC_SHADER_CLASS_ID ||
				partA == FSStandardMaterial::STD2_MULTI_LAYER_SHADER_CLASS_ID ||
				partA == FSStandardMaterial::STD2_OREN_NAYER_BLINN_CLASS_ID
				)
			)
		{
			stdShader = false;
		}
	}

	if (!stdShader)
	{
		// First attempt for a non-max shader is name-matching.
		// this is because StdIDToChannel may need some extra help
		// if for some weird reason there are several different texture
		// slots for the same effect.  Quite why that happens is
		// completely beyond me, but it does happen...  Also, as it 
		// turns out, it is better not to trust the 3rd party implementations
		TSTR subName = mtl->GetSubTexmapSlotName(id);
		if (subName.Length() > 0)
		{
			fm::string upperName = _strupr(subName.data());
			size_t firstSpace = upperName.find(' ');
			if (firstSpace != fm::string::npos) ((fm::vector<char, true>*)&upperName)->resize(firstSpace);
			if (IsEquivalent(upperName, "BASE")) return FUDaeTextureChannel::DIFFUSE;
			else if (IsEquivalent(upperName, "GLOSS")) return FUDaeTextureChannel::SPECULAR;
			else
			{
				FUDaeTextureChannel::Channel channel = FUDaeTextureChannel::FromString(upperName);
				if (channel != FUDaeTextureChannel::UNKNOWN) return channel;
			}
		}
	}
#ifdef _DEBUG
	else
	{
		// What do we have here, ay?
		TSTR subName = mtl->GetSubTexmapSlotName(id);
		subName;
	}
#endif

	// If we are here, we are either a std max mtl, or a 
	// non-std mtl we couldnt match a name to.
	int cid = -1;
	for (int i = 0; i < NTEXMAPS; i++)
	{
		//int x = mtl->StdIDToChannel(i);
		if (mtl->StdIDToChannel(i) == id)
		{
			cid = i;
			break;
		}
	}		
	switch (cid)
	{
	case ID_AM: return FUDaeTextureChannel::AMBIENT;
	case ID_DI: return FUDaeTextureChannel::DIFFUSE;
	case ID_SP: return FUDaeTextureChannel::SPECULAR;
	case ID_SH: return FUDaeTextureChannel::SHININESS;
	case ID_SS: return FUDaeTextureChannel::SPECULAR_LEVEL;
	case ID_SI: return FUDaeTextureChannel::EMISSION;
	case ID_OP: return FUDaeTextureChannel::TRANSPARENT;
	case ID_FI: return FUDaeTextureChannel::FILTER;
	case ID_BU: return FUDaeTextureChannel::BUMP;
	case ID_RL: return FUDaeTextureChannel::REFLECTION;
	case ID_RR: return FUDaeTextureChannel::REFRACTION;
	case ID_DP: return FUDaeTextureChannel::DISPLACEMENT;
	default: return FUDaeTextureChannel::UNKNOWN;
	}

};

FCDEffectStandard::LightingType MaxShadeToFCDLight(int id)
{
	switch (id)
	{
	case SHADE_CONST: return FCDEffectStandard::CONSTANT;
	case SHADE_METAL: return FCDEffectStandard::BLINN;
	case SHADE_BLINN: return FCDEffectStandard::BLINN;
	case SHADE_PHONG: return FCDEffectStandard::PHONG;
	}
	return FCDEffectStandard::PHONG;
}

FCDEffectStandard::LightingType MaxShadeToFCDLight(Class_ID id)
{
	switch (id.PartA())
	{
	case FSStandardMaterial::STD2_BLINN_SHADER_CLASS_ID:	
	case FSStandardMaterial::STD2_OREN_NAYER_BLINN_CLASS_ID:
	case FSStandardMaterial::STD2_METAL_SHADER_CLASS_ID:	return FCDEffectStandard::BLINN;
	case FSStandardMaterial::STD2_PHONG_CLASS_ID:			return FCDEffectStandard::PHONG;
	default:
		return FCDEffectStandard::PHONG;
	}
}

MaterialExporter::MaterialExporter(DocumentExporter* _document)
:	BaseExporter(_document)
{
}

MaterialExporter::~MaterialExporter()
{
	exportedImageMap.clear();
	exportedMaterialMap.clear();
	exportedColorMap.clear();
}

// Find the buffered information about the material with the given id
FCDMaterial* MaterialExporter::FindExportedMaterial(Mtl* mat)
{
	ExportedMaterialMap::iterator matIt = exportedMaterialMap.find(mat);
	return (matIt != exportedMaterialMap.end()) ? (*matIt).second : NULL;
}

FCDMaterial* MaterialExporter::ExportColorMaterial(const IPoint3& matColor)
{
	COLORREF color = RGB(matColor.x, matColor.y, matColor.z);
	return ExportColorMaterial(color);
}

FCDMaterial* MaterialExporter::ExportColorMaterial(const COLORREF matColor)
{
	// Only export the colors once, check for a repeat.
	ExportedColorsMap::iterator it = exportedColorMap.find(matColor);
	if (it != exportedColorMap.end()) return (*it).second;

	// Create a COLLADA material for this color.
	FCDMaterial* colladaMaterial = CDOC->GetMaterialLibrary()->AddEntity();
	FUSStringBuilder builder("ColorMaterial_"); builder.appendHex(matColor);
	colladaMaterial->SetDaeId(builder.ToCharPtr());
	colladaMaterial->SetName(builder.ToCharPtr());
	colladaMaterial->SetUserHandle(NULL);
	exportedColorMap.insert(matColor, colladaMaterial);

	// Create the profile_COMMON effect.
	FCDEffect* colladaEffect = CDOC->GetEffectLibrary()->AddEntity();
	builder.append("-fx");
	colladaEffect->SetDaeId(builder.ToCharPtr());
	colladaMaterial->SetEffect(colladaEffect);
	FCDEffectStandard* stdEffect = (FCDEffectStandard*) colladaEffect->AddProfile(FUDaeProfileType::COMMON);
	stdEffect->SetLightingType(FCDEffectStandard::PHONG);
	stdEffect->SetAmbientColor(FMVector4(GetRValue(matColor) / 255.0f, GetGValue(matColor) / 255.0f, GetBValue(matColor) / 255.0f, 1.0f));
	stdEffect->SetDiffuseColor(stdEffect->GetAmbientColor());
	// COLLADA is now meant to default to the A_ONE schema
	// default to non-tranparent
	stdEffect->SetTransparencyMode(FCDEffectStandard::A_ONE);
	stdEffect->SetTranslucencyColor(FMVector4(1.0f, 1.0f, 1.0f, 1.0f));
	stdEffect->SetReflectivityColor(FMVector4(0.0f, 0.0f, 0.0f, 1.0f));
	stdEffect->SetTranslucencyFactor(1.0f);
	
	// those are really rules of thumb for color materials to imitate a max color
	stdEffect->SetSpecularColor(FMVector4(1.0f, 1.0f, 1.0f, 1.0f)); // specular color = white
	stdEffect->SetSpecularFactor(0.2f);	// specular = color*  factor
	stdEffect->SetShininess(10.0f);		// glossiness
	// --
	return colladaMaterial;
}


FCDImage* MaterialExporter::ExportImage(Texmap* map)
{
	if (map == NULL) return NULL;

	if (map->ClassID() == Class_ID(BMTEX_CLASS_ID, 0x00))
	{
		BitmapTex* baseBitmap = (BitmapTex*) map;
		if (baseBitmap == NULL) return NULL;

		// get a valid filename
		BitmapInfo myBinfo;
		myBinfo.SetName(baseBitmap->GetMapName());
		return ExportImage(myBinfo);
	}
	else if (map->ClassID() == Class_ID(MIX_CLASS_ID, 0x00))
	{
		return NULL;
	}

	// not recognized
	return NULL;
}

FCDImage* MaterialExporter::ExportImage(BitmapInfo& bi)
{
	const TCHAR* fname = bi.Name();
	if (fname != NULL && _tcslen(fname) != 0 && _tcsicmp(fname, _T("none")) != 0)
	{

		BMMGetFullFilename(&bi);
		fstring filename = fstring(bi.Name());
		// Export the equivalent <image> node in the image library and add
		// the <init_from> element to the sampler's surface definition.
		// "None" is a special Max token, to indicate no texture assigned
		FCDImage* image = NULL;
		ExportedImageMap::iterator imgIt = exportedImageMap.find(filename);
		if (imgIt == exportedImageMap.end())
		{
			// generate an interesting name: strip the filename away from the filepath.
			size_t slashIndex = filename.rfind('/');
			size_t backSlashIndex = filename.rfind('\\');
			if (slashIndex == fstring::npos) slashIndex = backSlashIndex;
			else if (backSlashIndex != fstring::npos && backSlashIndex > slashIndex) slashIndex = backSlashIndex;
			fstring basename = (slashIndex != fstring::npos) ? filename.substr(slashIndex + 1) : filename;

			// image not exported
			image = CDOC->GetImageLibrary()->AddEntity();
			image->SetFilename(filename);
			image->SetName(basename);
			image->SetDaeId(basename);
			exportedImageMap.insert(filename, image);
		}
		else
			image = imgIt->second;

		return image;
	}
	return NULL;
}

// Export the given material node and its children
void MaterialExporter::ExportMaterial(Mtl* mat)
{
	// check for XRefs
	if (XRefFunctions::IsXRefMaterial(mat))
	{
		if (OPTS->ExportXRefs())
		{
			// we don't export XRef materials, but remember them for later use
			xRefedMaterialList.push_back(mat);
			return;
		}
		else
		{
			mat = XRefFunctions::GetXRefMaterialSource(mat);
		}
	}

	if (mat->IsMultiMtl())
	{
		// This material type is used for meshes with multiple materials:
		// only export the individual materials.
		for (int j = 0; j < mat->NumSubMtls(); j++)
		{
			Mtl* submat = mat->GetSubMtl(j);
			if (submat != NULL) ExportMaterial(submat);
		}
	}
	else
	{
		// add <material> node for this material in the library
		ExportSimpleMaterial(mat);
	}
}


void MaterialExporter::ExportCustomAttributes(Mtl* UNUSED(material), FCDMaterial* UNUSED(colladaMaterial))
{
}

// Export one material node for the given material.
void MaterialExporter::ExportSimpleMaterial(Mtl* baseMaterial)
{
	// check if this is not an XRef
	if (XRefFunctions::IsXRefMaterial(baseMaterial))
	{
		if (OPTS->ExportXRefs())
		{
			// don't generate the FCDMaterial
			return;
		}
		else
		{
			baseMaterial = XRefFunctions::GetXRefMaterialSource(baseMaterial);
		}
	}

	if (FindExportedMaterial(baseMaterial) != NULL) return;

	// Create the material
	FCDMaterial* material = CDOC->GetMaterialLibrary()->AddEntity();
	material->SetUserHandle(baseMaterial);
	material->SetName(baseMaterial->GetName().data());
	ENTE->ExportEntity(baseMaterial, material, baseMaterial->GetName().data());

	// Add it to the exported materials list
	exportedMaterialMap.insert(baseMaterial, material);

	// Create the effect for this material
	FCDEffect* matEffect = CDOC->GetEffectLibrary()->AddEntity();
	matEffect->SetDaeId(material->GetDaeId() + "-fx");
	matEffect->SetName(material->GetName());
	material->SetEffect(matEffect);

	// Write out the custom attributes
	ExportCustomAttributes(baseMaterial, material);

	// does the material have an HLSL profile?
	IDxMaterial* dxm = static_cast<IDxMaterial*>(baseMaterial->GetInterface(IDXMATERIAL_INTERFACE));
	if (dxm != NULL && dxm->GetEffectFilename() != NULL)
	{
		if (baseMaterial->NumSubMtls() > 0 && baseMaterial->GetSubMtl(0) != NULL)
		{
			// Create common profile
			Mtl* workingMaterial = baseMaterial->GetSubMtl(0);
			ExportCustomAttributes(workingMaterial, material);
			ExportCommonEffect((FCDEffectStandard*)matEffect->AddProfile(FUDaeProfileType::COMMON), workingMaterial);
		}

		// Create HLSL profile and add the effect instantiation elements
		ExportEffectHLSL(material, baseMaterial);
	}
	else if (baseMaterial->ClassID() == COLLADA_EFFECT_ID)
	{
		ExportColladaEffect(material, (ColladaEffect*)baseMaterial);
	}
	else
	{
		// Create common profile
		ExportCommonEffect((FCDEffectStandard*)matEffect->AddProfile(FUDaeProfileType::COMMON), baseMaterial);
	}
}

// Add a standard profile node to the given effect
void MaterialExporter::ExportCommonEffect(FCDEffectStandard* commonProfile, Mtl* material, float weight, bool inited)
{
	// create a standard profile
	FUAssert(commonProfile != NULL, return);
	//commonProfile = (FCDEffectStandard*) effect->AddProfile(FUDaeProfileType::COMMON);
	
	// Export all submaterials first.
	int subMaterialCount = material->NumSubMtls();
	for (int i = 0; i < subMaterialCount; ++i)
	{
		Mtl* subMaterial = material->GetSubMtl(i);
		float subMtlWeight = weight;
		if (material->ClassID() == FSCompositeMtl::classID)
		{
			IParamBlock2 *pblock = material->GetParamBlock(0);
			if (pblock != NULL)
			{
				if (!pblock->GetInt(FSCompositeMtl::compmat_map_on))
					subMtlWeight = 0;
				else
					subMtlWeight *= pblock->GetFloat(FSCompositeMtl::compmat_amount) / 100.0f;
			}
		}
		if (subMaterial != NULL) 
		{
			ExportCommonEffect(commonProfile, subMaterial, subMtlWeight, inited);
			inited |= true;
		}
	}

	Class_ID materialId = material->ClassID();
	if (materialId.PartA() == DMTL2_CLASS_ID || materialId.PartA() == DMTL_CLASS_ID)
	{
		ExportStdMat(commonProfile, (StdMat2*) material, weight, inited);
	}
	else
	{
		// This must be some strange 'RayTrace' or render-only material
		// Export whatever information we can from the Max material interface
		ExportUnknownMaterial(commonProfile, material);
	}
}

void MaterialExporter::ExportStdMat(FCDEffectStandard* stdProfile, StdMat2* material, float weighting, bool inited)
{
	FUAssert(stdProfile != NULL && material != NULL, return);

	Shader* shader = material->GetShader();
	
	if (!inited) 
	{
		stdProfile->SetLightingType(MaxShadeToFCDLight(shader->ClassID()));
	}

	// Retrieve the interesting parameter blocks
	IParamBlock2* shaderParameters = (IParamBlock2*) shader->GetReference(0);
	IParamBlock2* extendedParameters = (IParamBlock2*) material->GetReference(FSStandardMaterial::EXTENDED_PB_REF);

	TimeValue initTime = TIME_EXPORT_START;

	// Common parameters
	if (!inited)
	{
		// Export animated values:
		// [staylor] Bug326:  We cannot make -any- assumptions about shader
		// structure here.  In this case, try name matching instead.
		
		FCDConversionScaleFunctor scaleConversion(weighting);
		
		// create defaults.
		stdProfile->SetAmbientColor(scaleConversion(ToFMVector4(shader->GetAmbientClr(initTime))));
		stdProfile->SetDiffuseColor(scaleConversion(ToFMVector4(shader->GetDiffuseClr(initTime))));
		stdProfile->SetSpecularColor(scaleConversion(ToFMVector4(shader->GetSpecularClr(initTime))));
		stdProfile->SetShininess(scaleConversion(shader->GetGlossiness(initTime)));
		stdProfile->SetSpecularFactor(scaleConversion(shader->GetSpecularLevel(initTime)));
		stdProfile->SetTranslucencyFactor(scaleConversion(material->GetOpacity(initTime)));

		// Blinn and Phong have different shininess meanings.
		ShininessConversionFunctor blinnShininess;
		FCDConversionFunctor* shininessFunctor = &FSConvert::ToPercent;
		if (stdProfile->GetLightingType() == FCDEffectStandard::BLINN) shininessFunctor = &blinnShininess;

		// Properties now defaulted, try to export.
		ANIM->ExportProperty(_T("ambient"), shaderParameters, TSTR("Ambient Color"), 0, stdProfile->GetAmbientColorParam()->GetValue(), &scaleConversion);
		ANIM->ExportProperty(_T("diffuse"), shaderParameters, TSTR("Diffuse Color"), 0, stdProfile->GetDiffuseColorParam()->GetValue(), &scaleConversion);
		ANIM->ExportProperty(_T("specular"), shaderParameters, TSTR("Specular Color"), 0, stdProfile->GetSpecularColorParam()->GetValue(), &scaleConversion);
		ANIM->ExportProperty(_T("glossiness"), shaderParameters, TSTR("Glossiness"), 0, stdProfile->GetShininessParam()->GetValue(), shininessFunctor);
		ANIM->ExportProperty(_T("spec_level"), shaderParameters, TSTR("Specular Level"), 0, stdProfile->GetSpecularFactorParam()->GetValue(), NULL);
		ANIM->ExportProperty(_T("transparency"), extendedParameters, TSTR("Opacity"), 0, stdProfile->GetTranslucencyFactorParam()->GetValue(), NULL);
	
		BOOL useEmissionColor = shader->IsSelfIllumClrOn();
		stdProfile->SetIsEmissionFactor(useEmissionColor == FALSE);
		if (useEmissionColor)
		{
			stdProfile->SetEmissionColor(scaleConversion(ToFMVector4(shader->GetSelfIllumClr(initTime))));
			ANIM->ExportProperty(_T("self_illumination"), shaderParameters, TSTR("Self-Illum Color"), 0, stdProfile->GetEmissionColorParam()->GetValue(), &scaleConversion);
		}
		else
		{
			stdProfile->SetEmissionFactor(scaleConversion(shader->GetSelfIllum(initTime)));
			ANIM->ExportProperty(_T("self_illumination_f"), shaderParameters, TSTR("Self-Illumination"), 0, stdProfile->GetEmissionFactorParam()->GetValue(), NULL);
		}

		// Reset defaults
		stdProfile->SetTranslucencyColor(FMVector4(1.0f, 1.0f, 1.0f, 1.0f));
		stdProfile->SetReflectivityColor(FMVector4(0.0f, 0.0f, 0.0f, 1.0f));

		stdProfile->AddExtraAttribute(DAEMAX_MAX_PROFILE, DAEMAX_FACETED_MATERIAL_PARAMETER, FUStringConversion::ToString(material->IsFaceted() == TRUE).c_str());
		stdProfile->AddExtraAttribute(DAEMAX_MAX_PROFILE, DAESHD_DOUBLESIDED_PARAMETER, FUStringConversion::ToString(material->GetTwoSided() == TRUE).c_str());
		stdProfile->AddExtraAttribute(DAEMAX_MAX_PROFILE, DAEMAX_WIREFRAME_MATERIAL_PARAMETER, FUStringConversion::ToString(material->GetWire() == TRUE).c_str());
		stdProfile->AddExtraAttribute(DAEMAX_MAX_PROFILE, DAEMAX_FACEMAP_MATERIAL_PARAMETER, FUStringConversion::ToString(material->GetFaceMap() == TRUE).c_str());
	}

	// Export child maps
	int numSubTexMaps = material->NumSubTexmaps();
	for (int i = 0; i < numSubTexMaps; i++)
	{
		FUDaeTextureChannel::Channel channel = MaxIdToFCDChannel(material, i);
		if (channel == FUDaeTextureChannel::UNKNOWN) 
			continue;
		Texmap* map = material->GetSubTexmap(i);
			
		// special cases
		switch (channel)
		{
		case FUDaeTextureChannel::EMISSION:
			if (!material->GetSelfIllumColorOn())// Emission
			{
				// emission factor
				ANIM->ExportProperty(_T("self_illum_level"), shaderParameters, FSStandardMaterial::shdr_self_illum_amnt, 0, stdProfile->GetEmissionFactorParam()->GetValue(), NULL);
				stdProfile->SetIsEmissionFactor(true);
				continue;
			}
			break;
		case FUDaeTextureChannel::TRANSPARENT:
			ExportTransparencyFactor(stdProfile, map);
			break;
		}

		// Calculate weighting here.  Let execution continue
		// even if map is disabled, in order to manage colour blending
		float colorAmount, mapAmount = 0;
		bool exportMap = material->GetMapState(i) == 2;
		if (exportMap)
		{
			mapAmount = material->GetTexmapAmt(i, 0);
			colorAmount = weighting * (1.0f - mapAmount);
			mapAmount *= weighting;
		}
		else colorAmount = weighting;

		// If our id maps to a colour value, modulate that by our colourAmount.
		switch (channel)
		{
		case FUDaeTextureChannel::AMBIENT: BLEND_COLOR(stdProfile->GetAmbientColor(), material->GetAmbient(initTime), colorAmount); break;
		case FUDaeTextureChannel::DIFFUSE: BLEND_COLOR(stdProfile->GetDiffuseColor(), material->GetDiffuse(initTime), colorAmount); break;
		case FUDaeTextureChannel::SPECULAR: BLEND_COLOR(stdProfile->GetSpecularColor(), material->GetSpecular(initTime), colorAmount); break;
		}
		
		// export now.
		if (exportMap) ExportMap(channel, map, stdProfile, weighting * mapAmount);
	}
}


void MaterialExporter::ExportUVParam(FCDETechnique* extraUVParams, IParamBlock* pblock, int id, const char* name, FCDConversionFunctor* conv)
{
	float v = pblock->GetFloat(id, TIME_EXPORT_START);
	FCDENode* pNode = extraUVParams->AddParameter(name, conv == NULL ? v : (*conv)(v));
	ANIM->ExportAnimatedFloat(name, pblock->GetController(id), pNode->GetAnimated(), conv);
}

void MaterialExporter::ExportMap(unsigned int idx, Texmap* map, FCDEffectStandard* stdProfile, float weighting)
{
	FUAssert(idx < FUDaeTextureChannel::COUNT && stdProfile != NULL, return);
	if (map == NULL) return;
	//if (weighting <= 0) return;

	// Get any weighting control directly on this texmap
	float amount = weighting;
	Control* weightControl = NULL;
	if (map != NULL && GetIBitmapTextInterface(map) != NULL)
	{
		try
		{
			BitmapTex *bmTex = (BitmapTex*)map;
			TextureOutput* texOut = bmTex->GetTexout();
			IParamBlock* params = (IParamBlock*)texOut->GetReference(FSTextureOutput::param_block_ref_id);

			if (idx == ID_BU)
			{
				weightControl = params->GetController(FSTextureOutput::bump_amount);
				amount *= params->GetFloat(FSTextureOutput::bump_amount);
			}
			else
			{
				weightControl = params->GetController(FSTextureOutput::output_amount);
				amount *= params->GetFloat(FSTextureOutput::output_amount);
			}
		}
		catch(...) 
		{ 
			// No saving this one.
			FUFail(;);
		}
	}

	// Export image
	FCDImage* image = ExportImage(map);
	if (image)
	{
		FCDTexture* colladaTexture = stdProfile->AddTexture(idx);
		colladaTexture->SetImage(image);

		int mapChannel = 1;
		if (map != NULL)
		{
			mapChannel = map->GetMapChannel();

			UVGen* uvGen = map->GetTheUVGen();
			if (uvGen != NULL && uvGen->ClassID().PartA() == STDUV_CLASS_ID)
			{
				StdUVGen* stdGen = (StdUVGen*)uvGen;
				FCDETechnique* extraUVParams = colladaTexture->GetExtra()->GetDefaultType()->AddTechnique(DAEMAYA_MAYA_PROFILE);

				int uvFlags = stdGen->GetTextureTiling();
				extraUVParams->AddParameter(DAEMAYA_TEXTURE_MIRRORU_PARAMETER, (uvFlags&U_MIRROR) != 0);
				extraUVParams->AddParameter(DAEMAYA_TEXTURE_MIRRORV_PARAMETER, (uvFlags&V_MIRROR) != 0);
				extraUVParams->AddParameter(DAEMAYA_TEXTURE_WRAPU_PARAMETER, (uvFlags&(U_WRAP|U_MIRROR)) != 0);
				extraUVParams->AddParameter(DAEMAYA_TEXTURE_WRAPV_PARAMETER, (uvFlags&(V_WRAP|V_MIRROR)) != 0);
				
				IParamBlock* uvParams = (IParamBlock*)stdGen->GetReference(FSStdUVGen::pblock);	
				FUAssert(uvParams != NULL, return);
				
				// Calculate and export the tiling values.
				// We need to double those when dealing with mirror, because 3dsMax does that.
				float repeatU = uvParams->GetFloat(FSStdUVGen::u_tiling, TIME_EXPORT_START);
				float repeatV = uvParams->GetFloat(FSStdUVGen::v_tiling, TIME_EXPORT_START);
				FCDConversionScaleFunctor doubleFunctor(2.0f);
				ExportUVParam(extraUVParams, uvParams, FSStdUVGen::u_tiling, DAEMAYA_TEXTURE_REPEATU_PARAMETER, (uvFlags&U_MIRROR) != 0 ? &doubleFunctor : NULL);
				ExportUVParam(extraUVParams, uvParams, FSStdUVGen::v_tiling, DAEMAYA_TEXTURE_REPEATV_PARAMETER, (uvFlags&V_MIRROR) != 0 ? &doubleFunctor : NULL);

				if (uvFlags&(U_WRAP|U_MIRROR))
				{
					// If we are tiling, then the UV will wrap.  In Maya-nese, this means that
					// the UV's are offset.
					FSConvert::FCDConversionMaxToMayaUV offsetUFunctor(repeatU, uvFlags&U_MIRROR);
					ExportUVParam(extraUVParams, uvParams, FSStdUVGen::u_offset, DAEMAYA_TEXTURE_OFFSETU_PARAMETER, &offsetUFunctor);
				}
				else
				{
					// If we do not tile, then the U is just offset up.  In maya-nese, this is called the frame translate.+
					FCDConversionScaleFunctor offsetUFunctor(repeatU);
					ExportUVParam(extraUVParams, uvParams, FSStdUVGen::u_offset, DAEMAYA_TEXTURE_TRANSFRAMEU_PARAMETER, &offsetUFunctor);
				}

				if (uvFlags&(V_WRAP|V_MIRROR))
				{
					// If we are tiling, then the UV will wrap.  In Maya-nese, this means that
					// the UV's are offset.
					FSConvert::FCDConversionMaxToMayaUV offsetVFunctor(repeatV, uvFlags&V_MIRROR);
					ExportUVParam(extraUVParams, uvParams, FSStdUVGen::v_offset, DAEMAYA_TEXTURE_OFFSETV_PARAMETER, &offsetVFunctor);
				}
				else
				{
					// If we do not tile, then the U is just offset up.  In maya-nese, this is called the frame translate.
					FCDConversionScaleFunctor offsetVFunctor(repeatV);
					ExportUVParam(extraUVParams, uvParams, FSStdUVGen::v_offset, DAEMAYA_TEXTURE_TRANSFRAMEV_PARAMETER, &offsetVFunctor);
				}

				// Export the rotation around W.
				FCDConversionScaleFunctor radToDegFunctor(FMath::RadToDeg(1.0f));
				ExportUVParam(extraUVParams, uvParams, FSStdUVGen::w_angle, DAEMAYA_TEXTURE_ROTATEUV_PARAMETER, &radToDegFunctor);

				if (uvFlags&UV_NOISE)
				{
					ExportUVParam(extraUVParams, uvParams, FSStdUVGen::noise_amt, DAEMAYA_TEXTURE_NOISEU_PARAMETER);
					ExportUVParam(extraUVParams, uvParams, FSStdUVGen::noise_amt, DAEMAYA_TEXTURE_NOISEV_PARAMETER);
				}
			}
		}
		fm::string semantic = fm::string("CHANNEL") + TO_STRING(mapChannel);
		colladaTexture->GetSet()->SetSemantic(semantic);

		FCDConversionScaleFunctor sclFactor(weighting);
		FCDETechnique* colladaMaxTechnique = colladaTexture->GetExtra()->GetDefaultType()->AddTechnique(DAEMAX_MAX_PROFILE);
		FCDENode* amountNode = colladaMaxTechnique->AddParameter(DAEMAX_AMOUNT_TEXTURE_PARAMETER, amount);
		ANIM->ExportAnimatedFloat(_T("texamount"), weightControl, amountNode->GetAnimated(), &sclFactor);

		#define ADDFLOATPARAM(plugName, colladaName) \
		plug = placement2d.findPlug(plugName, &status); \
		if (status == MStatus::kSuccess) { \
			float value; \
			plug.getValue(value); \
			FCDENode* colladaParameter = colladaMayaTechnique->AddParameter(colladaName, value); \
			ANIM->AddPlugAnimation(placementNode, plugName, colladaParameter->GetAnimated(), kSingle); }
	}

	// Export any and all submaps
	float subAmount = amount;
	int numSubTex = map->NumSubTexmaps();
	for (int subIdx = 0; subIdx < numSubTex; subIdx++)
	{
		Texmap* subMap = map->GetSubTexmap(subIdx);

		// Handle special cases.
		Class_ID mapClassId = map->ClassID();
		if (mapClassId.PartB() == 0)
		{
			switch (mapClassId.PartA())
			{
			case MIX_CLASS_ID:
				{
					IParamBlock2* mixParams = map->GetParamBlock(0);
					if (!mixParams) break;
					Color color;
					switch (subIdx)
					{
					case 0: 
						subAmount = amount; 
						color = mixParams->GetColor(FSMixMap::mix_color1);
						break;
					case 1:
						subAmount = amount * mixParams->GetFloat(FSMixMap::mix_mix);
						color = mixParams->GetColor(FSMixMap::mix_color2);
						break;
					case 2:
						// Scale to 0, as this isn't an actual texture, but a mix modifier
						// In that case, we want to keep a texture reference for the user
						// to figure out if they want
						subAmount = 0; 
						break;
					}
					// Handle the simple case, which is blending colors.
					if (subMap == NULL && subAmount > 0)
					{
						switch (idx)
						{
						case FUDaeTextureChannel::AMBIENT: BLEND_COLOR(stdProfile->GetAmbientColor(), color, subAmount); break;
						case FUDaeTextureChannel::DIFFUSE: BLEND_COLOR(stdProfile->GetDiffuseColor(), color, subAmount); break;
						case FUDaeTextureChannel::SPECULAR: BLEND_COLOR(stdProfile->GetSpecularColor(), color, subAmount); break;
						}
					}
				}
				break;
			}
		}
		else subAmount = amount;

		if (subMap != NULL)
		{
			if (map->SubTexmapOn(subIdx) == 0) subAmount = 0;
			ExportMap(idx, subMap, stdProfile, subAmount);
		}
	}
}

// utility macro for the next method
#define _RGB(param) param.r, param.g, param.b, 1.0f

// For materials that we have not yet abstracted out, export as much information as possible.
void MaterialExporter::ExportUnknownMaterial(FCDEffectStandard* stdProfile, Mtl* material)
{
	if (stdProfile == NULL || material == NULL) return;
	float specularLevel = material->GetShinStr();
	bool isPhong = specularLevel > 0.2f;
	
	// Create the common and Max-specific program nodes
	stdProfile->SetLightingType((isPhong) ? FCDEffectStandard::PHONG : FCDEffectStandard::LAMBERT);
	
	if (material->GetSelfIllumColorOn())
	{
		Color emission = material->GetSelfIllumColor();
		stdProfile->SetEmissionColor(FMVector4(_RGB(emission)));
	}

	Color ambient = material->GetAmbient();
	stdProfile->SetAmbientColor(FMVector4(_RGB(ambient)));
	Color diffuse = material->GetDiffuse();
	stdProfile->SetDiffuseColor(FMVector4(_RGB(diffuse)));

	if (isPhong)
	{
		Color spec = material->GetSpecular();
		stdProfile->SetSpecularColor(FMVector4(_RGB(spec)));
		float shine = material->GetShininess()*  100.0f;
		stdProfile->SetShininess(shine);
		
		if (!material->GetSelfIllumColorOn())
		{
			stdProfile->SetIsEmissionFactor(true);
			float emissionFactor = material->GetSelfIllum();
			stdProfile->SetEmissionFactor(emissionFactor);
		}

		stdProfile->SetSpecularFactor(specularLevel);
	}
}
#undef _RGB

Mtl* MaterialExporter::GetSubMaterialById(Mtl* mtl, int materialId)
{
	if (!mtl->IsMultiMtl()) return NULL;
	IParamBlock2* subMaterialParameters = (IParamBlock2*) mtl->GetReference(FSMultiMaterial::PBLOCK_REF);
	if (subMaterialParameters == NULL) return mtl->GetSubMtl(materialId);

	int subMaterialCount = mtl->NumSubMtls();
	for (int subMaterialIndex = 0; subMaterialIndex < subMaterialCount; ++subMaterialIndex)
	{
		int subMatId = subMaterialParameters->GetInt(FSMultiMaterial::multi_ids, TIME_EXPORT_START, subMaterialIndex);
		if (subMatId == materialId)
		{
			Mtl* material = mtl->GetSubMtl(subMaterialIndex);
			if (material != NULL) return material;
		}
	}

	return NULL;
}

void MaterialExporter::ExportMaterialInstance(FCDMaterialInstance* colladaMaterialInstance, ExportedMaterial* exportedMaterial, FCDMaterial* colladaMaterial)
{
	if (colladaMaterialInstance == NULL || exportedMaterial == NULL || colladaMaterial == NULL) return;

	// Retrieve the 3dsMax material from the COLLADA material.
	Mtl* material = (Mtl*) colladaMaterial->GetUserHandle();
	if (material == NULL) return;

	while (material != NULL && material->IsMultiMtl())
	{
		material = GetSubMaterialById(material, exportedMaterial->materialId);
	}

	// Iterate through all the (sub-)materials and look for the shaders of this node.
	IDxMaterial* dxm = (IDxMaterial*) material->GetInterface(IDXMATERIAL_INTERFACE);
	if (dxm != NULL && dxm->GetEffectFilename() != NULL && GetGlobalDXDisplayManager()->IsDirectXActive())
	{
		// HLSL material: export both the HLSL bindings and the standard bindings for the 'fall-back' material.
		ExportMaterialInstanceFX(colladaMaterialInstance, dxm);
		if (material->NumSubMtls() > 0 && material->GetSubMtl(0) != NULL)
		{
			ExportMaterialInstanceStandard(colladaMaterialInstance, material->GetSubMtl(0), exportedMaterial);
		}
	}
	else if (material->ClassID() == COLLADA_EFFECT_ID)
	{
		ExportColladaEffectMaterialInstanceBindings(colladaMaterialInstance, (ColladaEffect*)material);
	}
	else
	{
		ExportMaterialInstanceStandard(colladaMaterialInstance, material, exportedMaterial);
	}
}

void MaterialExporter::ExportMaterialInstanceFX(FCDMaterialInstance* colladaMaterialInstance, IDxMaterial* material)
{
	if (material == NULL || colladaMaterialInstance == NULL) return;

	// Bind the scene nodes for lights and cameras to the material instance.
	int bindingCount = material->GetNumberOfLightParams();
	for (int i = 0; i < bindingCount; ++i)
	{
		// Retrieve the scene graph node for the light binding.
		INode* lightNode = material->GetLightNode(i);
		if (lightNode == NULL) continue;
		FCDSceneNode* colladaNode = document->GetNodeExporter()->FindExportedNode(lightNode);
		if (colladaNode == NULL) continue;

        // Retrieve the property's name.
		const TCHAR* parameterName = material->GetLightParameterName(i);
		if (parameterName == NULL || *parameterName == 0) continue;

		// Bind the scene node with the FX parameter name: we are usually interested only in the translation.
		colladaMaterialInstance->AddBinding(parameterName, colladaNode->GetDaeId());
	}

	// Do not export the texture coordinate - sampler bindings, since we have no access to the semantics and use nVidia's import profile.
}

void MaterialExporter::ExportColladaEffectMaterialInstanceBindings(FCDMaterialInstance* colladaMaterialInstance, ColladaEffect* colladaEffect)
{
	if (colladaMaterialInstance == NULL || colladaEffect == NULL) return;

	// add bindings for ColladaEffectParameterNode
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
				if (params[k]->hasNode() && params[k]->getNode() != NULL)
				{
					FCDSceneNode* colladaNode = document->GetNodeExporter()->FindExportedNode(params[k]->getNode());
					if (colladaNode != NULL)
					{
						// semantics should be unique
						const char* semantic = params[k]->getSemanticString();
						if (colladaMaterialInstance->FindBinding(semantic) == NULL)
							colladaMaterialInstance->AddBinding(semantic, fm::string("#") + colladaNode->GetDaeId());
					}
				}
			} // for each parameter
		} // for each pass
	} // for each technique

	// export "vertex input bindings" for varying parameters
	// no need to import these, the ColladaEffectRenderer automatically takes care of it
	const FCDEntity* entity = colladaMaterialInstance->GetParent()->GetEntity();
	if (entity != NULL && entity->HasType(FCDGeometry::GetClassType()))
	{
		const FCDGeometry* geom = (FCDGeometry*)entity;
		if (geom->IsMesh())
		{
			const FCDGeometryMesh* mesh = geom->GetMesh();
			FCDGeometryPolygonsInputConstList colors, texcoords;
			size_t polyCount = mesh->GetPolygonsCount();
			for (size_t i = 0; i < polyCount; ++i)
			{
				const FCDGeometryPolygons* poly = mesh->GetPolygons(i);
				if (poly == NULL) continue;
				poly->FindInputs(FUDaeGeometryInput::COLOR, colors);
				poly->FindInputs(FUDaeGeometryInput::TEXCOORD, texcoords);
			}
			uint32 inputCount = 0;
			const fm::string semantic = "TEXCOORD";
			if (colors.size() > 0)
			{
				// COLOR0
				// note: The COLOR channel is fed to the shaders by the TEXCOORD0 channel since
				// we don't have access to Max's DX render data.
				colladaMaterialInstance->AddVertexInputBinding(
					(semantic + FUStringConversion::ToString(inputCount)).c_str(), 
					FUDaeGeometryInput::COLOR,
					colors[0]->GetSet());
				++ inputCount;
			}
			for (size_t i = 0; i < texcoords.size() && inputCount < 8; ++i)
			{
				// TEXCOORD[0-7]
				colladaMaterialInstance->AddVertexInputBinding(
					(semantic + FUStringConversion::ToString(inputCount)).c_str(), 
					FUDaeGeometryInput::TEXCOORD,
					texcoords[i]->GetSet());
				++ inputCount;
			}
		}
	}
}

void MaterialExporter::ExportMaterialInstanceStandard(FCDMaterialInstance* colladaMaterialInstance, Mtl* material, ExportedMaterial *exportedMaterial)
{
	(void)exportedMaterial; // unused

	// Make a list of the material and all its sub-materials in order
	// to extract all the texture information structures.
	fm::pvector<Mtl> materials;
	materials.push_back(material);
	for (int i = 0; i < material->NumSubMtls(); ++i)
	{
		Mtl* subMaterial = material->GetSubMtl(i);
		if (subMaterial != NULL) materials.push_back(subMaterial);
	}
	int materialCount = (int) materials.size();

	// Retrieve the list of unique map channels to bind
	Int32List channelsToBind;
	for (int j = 0; j < materialCount; ++j)
	{
		Mtl* mtl = materials[j];
		for (int i = 0; i < mtl->NumSubTexmaps(); i++)
		{
			Texmap* dtm = mtl->GetSubTexmap(i);
			if (dtm == NULL) continue;

			// Retrieve this texture's map channel and verify the uniqueness
			int mapChannel = dtm->GetMapChannel();
			Int32List::iterator itC = channelsToBind.find(mapChannel);
			if (itC == channelsToBind.end()) channelsToBind.push_back(mapChannel);
		}
	}


	// For all the unique map channels of the material, bind the semantics to the texture coordinate sources
	FUSStringBuilder builder;
	for (Int32List::iterator itC = channelsToBind.begin(); itC != channelsToBind.end(); ++itC)
	{
		builder.set("CHANNEL"); builder.append(*itC);
		colladaMaterialInstance->AddVertexInputBinding(builder.ToCharPtr(), FUDaeGeometryInput::TEXCOORD, *itC);
	}
}

/**
* Export ColladaEffect material.
* @param material The FCDMaterial host.
* @param baseMaterial The ColladaEffect material instance to export.
*/
void MaterialExporter::ExportColladaEffect(FCDMaterial* material, ColladaEffect* colladaEffect)
{
	if (material == NULL || colladaEffect == NULL) return;
	FCDEffect* matEffect = material->GetEffect();
	if (matEffect == NULL) return;

	FCDEffectProfileFX* profile = (FCDEffectProfileFX*)matEffect->AddProfile(FUDaeProfileType::CG);
	if (profile == NULL) return;
	// ColladaEffect plugin is supported only by D3D9 display
	profile->SetPlatform(FC("PC-DX9"));
	
	size_t techCount = colladaEffect->getTechniqueCount();
	for (size_t i = 0; i < techCount; ++i)
	{
		ColladaEffectTechnique* tech = colladaEffect->getTechnique(i);
		if (tech == NULL) continue;
		FCDEffectTechnique* cfxTechnique = profile->AddTechnique();
		cfxTechnique->SetName(tech->getName());
		size_t passCount = tech->getPassCount();

		for (size_t j = 0; j < passCount; ++j)
		{
			ColladaEffectPass* pass = tech->getPass(j);
			if (pass == NULL) continue;
			FCDEffectPass* cfxPass = cfxTechnique->AddPass();
			// export params generators and modifiers
			ExportNewParams(profile, material, pass);
			// setup pass and shaders
			cfxPass->SetPassName(pass->getName());
			if (pass->isEffect())
			{
				// CgFX
				ColladaEffectShader* effect = pass->getShader(ColladaEffectPass::param_effect);
				FCDEffectCode* code = NULL;
				bool codeAdded = false;
				if (effect != NULL)
				{
					ColladaEffectShaderInfo& info = effect->getInfo();
					if (cgIsProgram(info.cgVertexProgram) == CG_TRUE)
					{
						FCDEffectPassShader* vertexShader = cfxPass->AddVertexShader();
						const char* profileSz = cgGetProgramString(info.cgVertexProgram, CG_PROGRAM_PROFILE);
						vertexShader->SetCompilerTarget(profileSz == NULL ? "" : profileSz);
						vertexShader->SetName(cgGetProgramString(info.cgVertexProgram, CG_PROGRAM_ENTRY));
						if (!codeAdded)
						{
							code = profile->AddCode();
							code->SetFilename(info.filename);
							codeAdded = true;
						}
						if (codeAdded) vertexShader->SetCode(code);
						ExportShaderPassBindings(vertexShader, effect);
					}
					if (cgIsProgram(info.cgFragmentProgram) == CG_TRUE)
					{
						FCDEffectPassShader* fragShader = cfxPass->AddFragmentShader();
						const char* profileSz = cgGetProgramString(info.cgFragmentProgram, CG_PROGRAM_PROFILE);
						fragShader->SetCompilerTarget(profileSz == NULL ? "" : profileSz);
						fragShader->SetName(cgGetProgramString(info.cgFragmentProgram, CG_PROGRAM_ENTRY));
						if (!codeAdded)
						{
							code = profile->AddCode();
							code->SetFilename(info.filename);
							codeAdded = true;
						}
						if (codeAdded) fragShader->SetCode(code);
						ExportShaderPassBindings(fragShader, effect);
					}
				}
			} // CgFX shader
			else
			{
				// Cg
				for (int i = 0; i < 2; ++i)
				{
					bool isVertex = (i==0);
					ColladaEffectShader* shader = pass->getShader(isVertex ? ColladaEffectPass::param_vs : ColladaEffectPass::param_fs);
					if (shader == NULL) continue;
					ColladaEffectShaderInfo& info = shader->getInfo();
					if (cgIsProgram(info.cgProgram) == CG_TRUE)
					{
						FCDEffectPassShader* cfxShader = NULL;
						if (isVertex) cfxShader = cfxPass->AddVertexShader();
						else cfxShader = cfxPass->AddFragmentShader();
						const char* profileSz = cgGetProgramString(info.cgProgram, CG_PROGRAM_PROFILE);
						cfxShader->SetCompilerTarget(profileSz == NULL ? "" : profileSz);
						cfxShader->SetName(cgGetProgramString(info.cgProgram, CG_PROGRAM_ENTRY));

						FCDEffectCode* code = profile->AddCode();
						code->SetFilename(info.filename);
						cfxShader->SetCode(code);

						ExportShaderPassBindings(cfxShader, shader);
					}
				}
			} // Cg shader

			// TODO. Export render states here.

		} // for each pass
	} // for each technique

	// export a technique hint
	ColladaEffectTechnique* curTech = colladaEffect->getCurrentTechnique();
	if (curTech != NULL)
	{
		FCDMaterialTechniqueHint hint;
		hint.platform = FC("PC-DX9");
		hint.technique = curTech->getName();
		material->GetTechniqueHints().push_back(hint);
	}

}

void MaterialExporter::ExportNewParams(FCDEffectProfileFX* profile, FCDMaterial* instance, ColladaEffectPass* pass)
{
	// call this method before ExportShaderPassBindings
	if (profile == NULL || pass == NULL || pass->getParameterCollection() == NULL)
		return;

	ColladaEffectParameterList& params = pass->getParameterCollection()->getParameters();
	size_t paramCount = params.size();

	for (size_t i = 0; i < paramCount; ++i)
	{
		ColladaEffectParameter* param = params[i];
		if (param == NULL) continue;

		param->setColladaParent(instance);
		param->doExport(document, profile, instance);
	}
}

void MaterialExporter::ExportShaderPassBindings(FCDEffectPassShader* fxShader, ColladaEffectShader* maxShader)
{
	if (fxShader == NULL || maxShader == NULL)
		return;

	ColladaEffectParameterList& params = maxShader->getParameters();
	for (size_t i = 0; i < params.size(); ++i)
	{
		ColladaEffectParameter* param = params[i];
		if (param == NULL) continue;
		if ((fxShader->IsVertexShader() && param->isBound(ColladaEffectParameter::kVERTEX)) ||
			(fxShader->IsFragmentShader() && param->isBound(ColladaEffectParameter::kFRAGMENT)))
		{
			FCDEffectPassBind* binding = fxShader->AddBinding();
			binding->reference = param->getReferenceString();
			binding->symbol = param->getProgramEntry();
		}
	}
}

/**
* Export the HLSL properties.
* @param colladaMaterial Must have an effect attached to it.
* @param material The corresponding material.
*/
void MaterialExporter::ExportEffectHLSL(FCDMaterial* colladaMaterial, Mtl* material)
{
	if (colladaMaterial == NULL || material == NULL) return;

	IDxMaterial* dxm = (IDxMaterial*) material->GetInterface(IDXMATERIAL_INTERFACE);
	FCDEffect* matEffect = colladaMaterial->GetEffect();
	if (matEffect == NULL || dxm == NULL) return;

	// Add the NV_import information for the effect
	fstring filename = dxm->GetEffectFilename();

	if (OPTS->ExportRelativePaths()) filename = CDOC->GetFileManager()->GetCurrentUri().MakeRelative(filename);
	else filename = CDOC->GetFileManager()->GetCurrentUri().MakeAbsolute(filename);

	FUXmlWriter::ConvertFilename(filename);
	FCDETechnique* techniqueNode = matEffect->GetExtra()->GetDefaultType()->AddTechnique(DAENV_NVIMPORT_PROFILE);
	FCDENode* importNode = techniqueNode->AddChildNode(DAENV_IMPORT_ELEMENT);
	importNode->AddAttribute(DAENV_PROFILE_PROPERTY, DAENV_HLSL_PROFILE);
	importNode->AddAttribute(DAENV_FILENAME_PROPERTY, filename);

	// Iterate over the editable effect parameters
	IParamBlock2* fxParameters = material->GetParamBlock(DX9Material::FXPARAMS_PBLOCK_REF);
	FUAssert(fxParameters != NULL, return);

	int paramCount = fxParameters->NumParams();
	for (int _i = 0; _i < paramCount; ++_i)
	{
		ParamID i = fxParameters->IndextoID(_i);
		ParamDef& def = fxParameters->GetParamDef(i);
		FCDEffectParameter* param = NULL;
		switch ((int) def.type)
		{
		case TYPE_FLOAT: { // float parameter
			FCDEffectParameterFloat* parameter = (FCDEffectParameterFloat*) (param = colladaMaterial->AddEffectParameter(FCDEffectParameter::FLOAT));
			float value = fxParameters->GetFloat(i, TIME_EXPORT_START, 0);
			parameter->SetValue(value);
			ANIM->ExportProperty(def.int_name, fxParameters, i, 0, parameter->GetValue(), NULL);
			break; }

		case TYPE_INT: {
			switch ((int) def.ctrl_type)
			{
			case TYPE_INTLISTBOX: { // light parameter
				UNUSED(FCDEffectParameterFloat3* parameter =) (FCDEffectParameterFloat3*) (param = colladaMaterial->AddEffectParameter(FCDEffectParameter::FLOAT3));
				break; }

			case TYPE_SPINNER:
			default: { // int parameter
				FCDEffectParameterInt* parameter = (FCDEffectParameterInt*) (param = colladaMaterial->AddEffectParameter(FCDEffectParameter::INTEGER));
				int value = fxParameters->GetInt(i, TIME_EXPORT_START, 0);
				parameter->SetValue(value);
				break; }
			}
			break; }

		case TYPE_POINT3:
		case TYPE_RGBA: { // float3 parameter
			FCDEffectParameterFloat3* parameter = (FCDEffectParameterFloat3*) (param = colladaMaterial->AddEffectParameter(FCDEffectParameter::FLOAT3));
			Point3 value = fxParameters->GetPoint3(i, TIME_EXPORT_START, 0);
			parameter->SetValue(ToFMVector3(value));
			ANIM->ExportProperty(def.int_name, fxParameters, i, 0, parameter->GetValue(), NULL);
			break; }

		case TYPE_POINT4:
		case TYPE_FRGBA: { // color parameter
			FCDEffectParameterVector* parameter = (FCDEffectParameterVector*) (param = colladaMaterial->AddEffectParameter(FCDEffectParameter::VECTOR));
			Point4 value = fxParameters->GetPoint4(i, TIME_EXPORT_START, 0);
			parameter->SetValue(ToFMVector4(value));
			ANIM->ExportProperty(def.int_name, fxParameters, i, 0, parameter->GetValue(), NULL);
			break; }

		case TYPE_MATRIX3: { // matrix parameter
			FCDEffectParameterMatrix* parameter = (FCDEffectParameterMatrix*) (param = colladaMaterial->AddEffectParameter(FCDEffectParameter::MATRIX));
			Matrix3 value = fxParameters->GetMatrix3(i, TIME_EXPORT_START, 0);
			parameter->SetValue(ToFMMatrix44(value));
			break; }

		case TYPE_BITMAP: { // surface parameter
			FCDEffectParameterSurface* surface = (FCDEffectParameterSurface*) (param = colladaMaterial->AddEffectParameter(FCDEffectParameter::SURFACE));
			PBBitmap* bitmap = fxParameters->GetBitmap(i, TIME_EXPORT_START, 0);
			if (bitmap != NULL)
			{
				FCDImage* image = ExportImage(bitmap->bi);
				if (image != NULL)
				{
					// for the moment use the simple FROM init method
					surface->SetInitMethod(FCDEffectParameterSurfaceInitFactory::Create(FCDEffectParameterSurfaceInitFactory::FROM));
					surface->AddImage(image);
				}
				else surface->SetInitMethod(FCDEffectParameterSurfaceInitFactory::Create(FCDEffectParameterSurfaceInitFactory::AS_NULL));
			}
			else surface->SetInitMethod(FCDEffectParameterSurfaceInitFactory::Create(FCDEffectParameterSurfaceInitFactory::AS_NULL));
			break; }

		case TYPE_BOOL: { // boolean parameter
			FCDEffectParameterBool* boolean = (FCDEffectParameterBool*) (param = colladaMaterial->AddEffectParameter(FCDEffectParameter::BOOLEAN));
			int value; Interval dummy;
			fxParameters->GetValue(i, TIME_EXPORT_START, value, dummy, 0);
			boolean->SetValue(value != 0);
			break; }

		default:
			// Add support for this missing fx parameter type!
			FUFail(break);
		}

		if (param != NULL)
		{
			// Add this parameter to the COLLADA material, as a <set_param>.
			param->SetModifier();
			param->SetReference(def.int_name);
		}
	}
}

void MaterialExporter::ExportTransparencyFactor(FCDEffectStandard* stdProfile, Texmap *tmap)
{
	// Only one transparency factor is supported, so retrieve only the first texture from this bucket.
	if (tmap == NULL) return;
	BitmapTex* bmap = GetIBitmapTextInterface(tmap);
	BOOL isAlphaTransparency = true;
	if (bmap != NULL)isAlphaTransparency = bmap->GetAlphaAsMono(TRUE);

	// As far as my tests are concerned, the only real way to get alpha-transparency is through
	// the mono-channel output as Alpha.
	stdProfile->SetTransparencyMode(isAlphaTransparency ? FCDEffectStandard::A_ONE : FCDEffectStandard::RGB_ZERO);
}
