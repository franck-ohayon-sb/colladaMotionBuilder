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

#ifndef _MATERIAL_IMPORTER_H_
#define _MATERIAL_IMPORTER_H_

#include "EntityImporter.h"
#include "stdmat.h"

class FCDEffect;
class FCDEffectProfileFX;
class FCDMaterial;
class FCDMaterialInstance;
class FCDTexture;
class FCDEffectPassShader;
class FCDEffectStandard;
class DocumentImporter;
class BitmapTex;
class Mtl;
class ColladaEffectPass;
class FCDETechnique;
class FCDGeometryInstance;

class FCDConversionFunctor;

typedef fm::pvector<BitmapTex> BitmapTextureList;
typedef fm::map<int, int> MapChannelMap;

class MaterialImporter : public EntityImporter
{
private:
	const FCDEffect* colladaEffect;
	BitmapTextureList bitmapTextureMaps[NTEXMAPS];

protected:
	Mtl* material;

public:
	MaterialImporter(DocumentImporter* document);
	virtual ~MaterialImporter();
	virtual Type GetType() { return MATERIAL; }

	// Accessor
	Mtl* GetMaterial() { return material; }

	// Create a 3dsMax material for the given COLLADA material
	Mtl* ImportMaterial(const FCDMaterial* colladaMaterial, FCDMaterialInstance* materialInstance, FCDGeometryInstance* geometryInstance);

	// Assign to this material the map channels from the geometry's definition
	void AssignMapChannels(FCDMaterialInstance* colladaInstance, MapChannelMap& mapChannels);

	// Import material instance bindings (currently only affects ColladaEffect materials)
	void ImportMaterialInstanceBindings(FCDMaterialInstance* materialInstance);

private:
	// Create a 3dsMax material for the given COLLADA material with COMMON profile
	Mtl* ImportStdMaterial(const FCDMaterial* colladaMaterial, FCDMaterialInstance* materialInstance, FCDGeometryInstance* geometryInstance);
	// Create a 3dsMax DX material for the given COLLADA material with DX/NV profile
	Mtl* ImportFXMaterial(const FCDMaterial* colladaMaterial, FCDMaterialInstance* materialInstance, FCDGeometryInstance* geometryInstance);
	// Create a ColladaEffect material plugin instance
	Mtl* ImportColladaEffectMaterial(const FCDMaterial* colladaMaterial, FCDEffectProfileFX* fxProfile);
	void ImportColladaEffectParameters(const FCDMaterial* colladaMaterial, FCDEffectProfileFX* fxProfile, FCDEffectPassShader* shader, ColladaEffectPass* cfxPass);

	// Create a 3dsMax material for the given COLLADA material with COMMON profile
	Mtl* CreateStdMaterial(const FCDEffectStandard* colladaStdMtl, FCDMaterialInstance* materialInstance, FCDGeometryInstance* geometryInstance);
	Mtl* CreateFXMaterial(const FCDMaterial* , fstring& , Mtl*);


	// Retrieve a COLLADA texture's channel as a Max texture slot
	int GetTextureSlot(const FCDTexture* colladaTexture);

	// Import a COLLADA texture into the Max material object
	void ImportTexture(Mtl* material, int slot, const FCDEffectStandard* effect, const FCDTexture* colladaTexture);
	void ImportUVParam(const FCDETechnique* extraUVParams, IParamBlock* pblock, int id, const char* name, FCDConversionFunctor* conv=NULL);
	void ImportUVOffsetParam(const FCDETechnique* extraUVParams, IParamBlock* pblock, const int targId, const char* offsetName, const char* frameName, float repeat, bool isMirrored);
	BitmapTex* ImportTexture(const FCDEffectStandard* effect, const FCDTexture* colladaTexture);
	void AssignTextureToMtl(Mtl* material, int slot, Texmap* texture, const FCDEffectStandard* effect);

private:
//	xmlNode* GetMatCustAttrFXNode(Mtl* mat);
//	xmlNode* GetMatCustAttrImageNode(Mtl* mat);
//	xmlNode* GetMatCustAttrInstNode(Mtl* mat);
};

class XRefMaterialImporter : public MaterialImporter
{
public:
	XRefMaterialImporter(Mtl *_material);
	virtual ~XRefMaterialImporter(){};
	virtual Type GetType() { return XREF; }
};

#endif // _MATERIAL_IMPORTER_H_