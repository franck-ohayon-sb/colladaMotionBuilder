/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _MATERIAL_EXPORTER_H_
#define _MATERIAL_EXPORTER_H_

class FCDEffect;
class FCDImage;
class FCDMaterial;
class FCDTexture;

#ifndef _ENTITY_EXPORTER_H_
#include "EntityExporter.h"
#endif // _ENTITY_EXPORTER_H_

typedef fm::pvector<FBTexture> FBTextureList;

class MaterialExporter : public EntityExporter
{
private:
	struct CachedMaterial
	{
		FBMaterial* material;
		FBTextureList textures;

		bool operator< (const CachedMaterial& other) const;
		bool operator== (const CachedMaterial& other) const;
	};

	typedef fm::map<CachedMaterial, FCDMaterial*> ExportedMaterialMap;
	ExportedMaterialMap exportedMaterials;
	typedef fm::map<fm::string, FCDImage*> ExportedImageMap;
	ExportedImageMap exportedImages;

	// MotionBuilder's default material
	FBMaterial* defaultMaterial;

public:
	MaterialExporter(ColladaExporter* base);
	virtual ~MaterialExporter();

	FCDMaterial* ExportMaterial(FBMaterial* material, FBTextureList& textures);
	FCDImage* ExportImage(FBTexture* texture);

	FBMaterial* GetDefaultMaterial();

private:
	void ExportTexture(FCDEffect* colladaEffect, FCDTexture* colladaTexture, FBTexture* texture);
};

#endif // _MATERIAL_EXPORTER_H_