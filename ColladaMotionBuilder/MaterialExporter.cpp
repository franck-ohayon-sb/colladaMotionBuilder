/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "AnimationExporter.h"
#include "ColladaExporter.h"
#include "MaterialExporter.h"
#include "NodeExporter.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDEffect.h"
#include "FCDocument/FCDEffectStandard.h"
#include "FCDocument/FCDExtra.h"
#include "FCDocument/FCDImage.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDMaterial.h"
#include "FCDocument/FCDTexture.h"
#include "FUtils/FUDaeSyntax.h"
#include "PropertyExplorer.h"

//
// MaterialExporter
//

MaterialExporter::MaterialExporter(ColladaExporter* base)
:	EntityExporter(base)
,	defaultMaterial(NULL)
{
}

MaterialExporter::~MaterialExporter()
{
	exportedMaterials.clear();
	exportedImages.clear();
}

FCDMaterial* MaterialExporter::ExportMaterial(FBMaterial* material, FBTextureList& textures)
{
	if (material == NULL && textures.empty()) material = GetDefaultMaterial();

	// Create the cache structure and verify whether this was already exported.
	CachedMaterial cache;
	cache.material = material;
	cache.textures = textures;
	ExportedMaterialMap::iterator it = exportedMaterials.find(cache);
	if (it != exportedMaterials.end()) return it->second;

	// Create a new COLLADA material and cache it right away.
	FCDMaterial* colladaMaterial = CDOC->GetMaterialLibrary()->AddEntity();
	exportedMaterials.insert(cache, colladaMaterial);
	ExportEntity(colladaMaterial, (material != NULL) ? (FBComponent*) material : (FBComponent*) textures.front());

	// Create an effect for it and a profile_COMMON slot.
	FCDEffect* colladaEffect = CDOC->GetEffectLibrary()->AddEntity();
	colladaMaterial->SetEffect(colladaEffect);
	colladaEffect->SetDaeId(colladaMaterial->GetDaeId() + "-fx");
	colladaEffect->SetName(colladaMaterial->GetName() + FC("-fx"));
	FCDEffectStandard* stdFx = (FCDEffectStandard*) colladaEffect->AddProfile(FUDaeProfileType::COMMON);
	stdFx->SetLightingType(FCDEffectStandard::PHONG);
	if (material != NULL)
	{
		stdFx->SetEmissionColor(ToFMVector4(material->Emissive));
		ANIMATABLE(&material->Emissive, colladaEffect, stdFx->GetEmissionColorParam()->GetValue(), -1, NULL, false);
		stdFx->SetAmbientColor(ToFMVector4(material->Ambient));
		ANIMATABLE(&material->Ambient, colladaEffect, stdFx->GetAmbientColorParam()->GetValue(), -1, NULL, false);
		stdFx->SetDiffuseColor(ToFMVector4(material->Diffuse));
		ANIMATABLE(&material->Diffuse, colladaEffect, stdFx->GetDiffuseColorParam()->GetValue(), -1, NULL, false);
		stdFx->SetSpecularColor(ToFMVector4(material->Specular));
		ANIMATABLE(&material->Specular, colladaEffect, stdFx->GetSpecularColorParam()->GetValue(), -1, NULL, false);
		stdFx->SetShininess((float) material->Shininess);
		ANIMATABLE(&material->Shininess, colladaEffect, stdFx->GetShininessParam()->GetValue(), -1, NULL, false);
		stdFx->SetReflectivityFactor((float) material->Reflect);
		ANIMATABLE(&material->Reflect, colladaEffect, stdFx->GetReflectivityFactorParam()->GetValue(), -1, NULL, false);
		stdFx->SetTranslucencyFactor((float) material->Opacity);
		ANIMATABLE(&material->Opacity, colladaEffect, stdFx->GetTranslucencyFactorParam()->GetValue(), -1, NULL, false);
		stdFx->SetTransparencyMode(FCDEffectStandard::A_ONE);
	}

	// Add the corresponding textures.
	for (FBTexture** itT = textures.begin(); itT != textures.end(); ++itT)
	{
		FUDaeTextureChannel::Channel channel;
		switch ((*itT)->Usage)
		{
		case kFBTextureUsageLightMap:
		case kFBTextureUsageShadowMap: channel = FUDaeTextureChannel::AMBIENT; break;
		case kFBTextureUsageSphericalReflection: channel = FUDaeTextureChannel::REFLECTION; break;
		case kFBTextureUsageSphereMap: channel = FUDaeTextureChannel::BUMP; break; // Not sure about this one..
		case kFBTextureUsageAll:
		case kFBTextureUsageColor:
		default: channel = FUDaeTextureChannel::DIFFUSE; break; 
		}

		// Export the image entity and attach it in the correct bucket.
		FCDTexture* texture = stdFx->AddTexture((uint32) channel);
		ExportTexture(colladaEffect, texture, *itT);
	}

	return colladaMaterial;
}

FCDImage* MaterialExporter::ExportImage(FBTexture* texture)
{
	// Retrieve the filename and look into our already-exported image list for it.
	fm::string filename = (char*) texture->Filename;
	ExportedImageMap::iterator it = exportedImages.find(filename);
	if (it != exportedImages.end()) return it->second;

	// Create the FCollada image.
	FCDImage* image = CDOC->GetImageLibrary()->AddEntity();
	image->SetDaeId((char*) texture->Name);
	image->SetFilename(filename);
	exportedImages.insert(filename, image);

	return image;
}

void MaterialExporter::ExportTexture(FCDEffect* colladaEffect, FCDTexture* colladaTexture, FBTexture* texture)
{
	// Export the under-lying image.
	FCDImage* image = ExportImage(texture);
	colladaTexture->SetImage(image);
	colladaTexture->GetSet()->SetSemantic("TEX0");

	if (texture->Mapping == kFBTextureMappingUV)
	{
		// Write out the texture placement parameters, which is a Maya extension.
		FCDExtra* extra = colladaTexture->GetExtra();
		FCDETechnique* colladaTechnique = extra->GetDefaultType()->AddTechnique(DAEMAYA_MAYA_PROFILE);

		// 2D Wrap or Clamp - Not animatable.
		int wrapU = 0, wrapV = 0;
		GetPropertyValue(texture, "WrapModeU", wrapU); // Strangely, 0 -> WRAP and 1 -> CLAMP.
		GetPropertyValue(texture, "WrapModeV", wrapV);
		colladaTechnique->AddParameter(DAEMAYA_TEXTURE_WRAPU_PARAMETER, wrapU == 0);
		colladaTechnique->AddParameter(DAEMAYA_TEXTURE_WRAPV_PARAMETER, wrapV == 0);

		// Texture transforms
		FMVector3 translation, rotation, scaling, dummy; float inverted;
		GetPropertyValue(texture, "Rotation", rotation);

		// texture->Translation and texture->Scaling don't contain the real values, but their animations are fine.
		// The order differs from the usual TRS: in MotionBuilder, texture transforms are TSR, but we already know the rotation..
		FMMatrix44 textureTransform(texture->GetMatrix());
		FMMatrix44 removeRotationTransform = FMMatrix44::AxisRotationMatrix(FMVector3::ZAxis, FMath::DegToRad(-rotation[2]));
		textureTransform = (textureTransform.Transposed() * removeRotationTransform);
		textureTransform.Decompose(scaling, dummy, translation, inverted);

		// Swap UV can be emulated, somewhat, by doing a rotation and a flip, in the pre-transforms.
		if (texture->SwapUV)
		{
			colladaTechnique->AddParameter(DAEMAYA_TEXTURE_COVERAGEU_PARAMETER, -1);
			colladaTechnique->AddParameter(DAEMAYA_TEXTURE_ROTFRAME_PARAMETER, 90);
		}

		// Export the transform values and their animations.
		FCDENode* node = colladaTechnique->AddParameter(DAEMAYA_TEXTURE_OFFSETU_PARAMETER, translation[0]);
		ANIMATABLE_CUSTOM(&texture->Translation, 0, colladaEffect, node->GetAnimated(), NULL);
		node = colladaTechnique->AddParameter(DAEMAYA_TEXTURE_OFFSETV_PARAMETER, translation[1]);
		ANIMATABLE_CUSTOM(&texture->Translation, 1, colladaEffect, node->GetAnimated(), NULL);
		node = colladaTechnique->AddParameter(DAEMAYA_TEXTURE_ROTATEUV_PARAMETER, rotation[2]); // Z-axis only is modifiable in the UI.
		ANIMATABLE_CUSTOM(&texture->Rotation, 2, colladaEffect, node->GetAnimated(), NULL);
		node = colladaTechnique->AddParameter(DAEMAYA_TEXTURE_REPEATU_PARAMETER, scaling[0]);
		ANIMATABLE_CUSTOM(&texture->Scaling, 0, colladaEffect, node->GetAnimated(), NULL);
		node = colladaTechnique->AddParameter(DAEMAYA_TEXTURE_REPEATV_PARAMETER, scaling[1]);
		ANIMATABLE_CUSTOM(&texture->Scaling, 1, colladaEffect, node->GetAnimated(), NULL);
	}
}

//
// MaterialExporter::CachedMaterial
//

bool MaterialExporter::CachedMaterial::operator< (const MaterialExporter::CachedMaterial& other) const
{
	if ((size_t) material != (size_t) other.material) return (size_t) material < (size_t) other.material;
	size_t count = textures.size();
	if (count != other.textures.size()) return count < other.textures.size();
	size_t total[2] = { 0, 0 };
	for (size_t i = 0; i < count; ++i) total[0] ^= (size_t) textures[i];
	for (size_t i = 0; i < count; ++i) total[1] ^= (size_t) other.textures[i];
	return total[0] < total[1];
}

bool MaterialExporter::CachedMaterial::operator== (const MaterialExporter::CachedMaterial& other) const
{
	if (material != other.material) return false;
	if (textures.size() != other.textures.size()) return false;
	size_t count = textures.size();
	for (size_t i = 0; i < count; ++i) 
	{
		bool found = false;
		for (size_t j = 0; j < count && !found; ++j)
		{
			found = textures[i] == other.textures[j];
		}
		if (!found) return false;
	}
	return true;
}

FBMaterial* MaterialExporter::GetDefaultMaterial()
{
	if (defaultMaterial == NULL)
	{
		// Look for the aptly-named material.
		FBScene* scene = NODE->GetScene();
		int materialCount = scene->Materials.GetCount();
		for (int i = 0; i < materialCount; ++i)
		{
			FBMaterial* material = scene->Materials[i];
			const char* materialName = material->Name;
			if (IsEquivalent(materialName, "DefaultMaterial"))
			{
				defaultMaterial = material;
				break;
			}
		}
	}
	return defaultMaterial;
}
