/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _MATERIAL_EXPORTER_H_
#define _MATERIAL_EXPORTER_H_

#ifndef _BASE_EXPORTER_H_
#include "BaseExporter.h"
#endif // _BASE_EXPORTER_H_

class DocumentExporter;
class BitmapTex;
class Mtl;
class AnimParam;
class ColladaEffect;
class ColladaEffectPass;
class ColladaEffectShader;
class FCDEffect;
class FCDEffectParameter;
class FCDEffectProfile;
class FCDEffectProfileFX;
class FCDEffectPassShader;
class FCDEffectStandard;
class FCDEffectTechnique;
class FCDGeometryPolygons;
class FCDImage;
class FCDMaterial;
class FCDMaterialInstance;
class FCDETechnique;
struct ExportedMaterial;

typedef fm::map<fm::string, TSTR> FilenameIDMap;
typedef fm::map<Mtl*, FCDMaterial*> ExportedMaterialMap;
typedef fm::map<COLORREF, FCDMaterial*> ExportedColorsMap;
typedef fm::map<fstring, FCDImage*> ExportedImageMap;
typedef fm::pvector<Mtl> MaterialList;

// Class specialized for material/texture/image export
class MaterialExporter : public BaseExporter
{
public:
							MaterialExporter(DocumentExporter* document);
	virtual					~MaterialExporter();

	// Main entry point, exports all the information about the given scene's materials
	void					ExportMaterial(Mtl* material);

	// Add a material from an object color.
	FCDMaterial*			ExportColorMaterial(const IPoint3& matColor);
	FCDMaterial*			ExportColorMaterial(const COLORREF matColor);

	// Fill in the given FCDMaterialInstance
	void					ExportMaterialInstance(FCDMaterialInstance* colladaMaterialInstance, ExportedMaterial* exportedMaterial, FCDMaterial* colladaMaterial);

	// Finds the buffered information about the material with the given id
	FCDMaterial*			FindExportedMaterial(Mtl* mat);

	FCDImage*				ExportImage(Texmap* map);
	FCDImage*				ExportImage(BitmapInfo& map);

	Mtl*					GetSubMaterialById(Mtl* mtl, int index);

protected:

	// Node builders
	void					ExportSimpleMaterial(Mtl* mat);
	
	void					ExportUnknownMaterial(FCDEffectStandard* commonProfile, Mtl* mtl);
	void					ExportStdMat(FCDEffectStandard* stdProfile, StdMat2* mtl, float = 1.0f, bool inited = false);

	void					ExportColladaEffect(FCDMaterial* material, ColladaEffect* baseMaterial);
	void					ExportNewParams(FCDEffectProfileFX* profile, FCDMaterial* instance, ColladaEffectPass* pass);
	void					ExportShaderPassBindings(FCDEffectPassShader* fxShader, ColladaEffectShader* maxShader);

	void					ExportEffectHLSL(FCDMaterial* colladaMaterial, Mtl* mtl);
	void					ExportTransparencyFactor(FCDEffectStandard* stdProfile, Texmap* opacityTextures);

	void					ExportCommonEffect(FCDEffectStandard*, Mtl* mtl, float weight = 1.0f, bool inited = false);
	void					ExportUVParam(FCDETechnique* extraUVParams, IParamBlock* pblock, int id, const char* name, FCDConversionFunctor* conv=NULL);
	void					ExportMap(unsigned int idx, Texmap* map, FCDEffectStandard* stdProfile, float weighting);
	
	void					ExportCustomAttributes(Mtl* mtl, FCDMaterial* colladaMaterial);

	// Material binds node builders
	void					ExportMaterialInstanceFX(FCDMaterialInstance* colladaMaterialInstance, IDxMaterial* material);
	void					ExportColladaEffectMaterialInstanceBindings(FCDMaterialInstance* colladaMaterialInstance, ColladaEffect* colladaEffect);
	void					ExportMaterialInstanceStandard(FCDMaterialInstance* colladaMaterialInstance, Mtl* material, ExportedMaterial* exportedMaterial);

private:
	ExportedImageMap		exportedImageMap;
	ExportedMaterialMap		exportedMaterialMap;
	MaterialList			xRefedMaterialList;
	ExportedColorsMap		exportedColorMap;
};

#endif // _MATERIAL_EXPORTER_H_