/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_MATERIAL_LIBRARY_INCLUDED__
#define __DAE_MATERIAL_LIBRARY_INCLUDED__

#ifndef __DAE_BASE_LIBRARY_INCLUDED__
#include "DaeBaseLibrary.h"
#endif // __DAE_BASE_LIBRARY_INCLUDED__
#ifndef _FU_DAE_ENUM_H_
#include "FUtils/FUDaeEnum.h"
#endif // _FU_DAE_ENUM_H_

class CFXShaderNode;
class DaeDoc;
class DaeMaterial;
class FCDEffect;
class FCDEffectStandard;
class FCDEffectProfileFX;
class FCDEffectPass;
class FCDEffectPassShader;
class FCDEffectParameterSurface;
class FCDEffectProfile;
class FCDGeometry;
class FCDGeometryPolygons;
class FCDImage;
class FCDMaterial;
class FCDMaterialInstance;
class FCDTexture;
typedef fm::map<MObject, DaeMaterial*> ShaderMaterialMap;
typedef FUObjectContainer<FCDTexture> FCDTextureContainer;

class DaeMaterial : public DaeEntity
{
private:
	MObject shaderGroup;
	MString shaderGroupName;

public:
	MObjectArray textures;
	Int32List textureSets;
	StringList textureSetNames;
	FCDEffectProfile* profile;
	bool isStandard;
	bool isColladaFX;

public:
	DaeMaterial(DaeDoc* doc, FCDEntity* entity, const MObject& node)
		:	DaeEntity(doc, entity, node), shaderGroup(MObject::kNullObj), profile(NULL), isStandard(false), isColladaFX(false) {}
	virtual ~DaeMaterial() {}

	MObject GetShaderGroup() const;
	void SetShaderGroup(const MObject& node);
	virtual void ReleaseNode();
};

typedef fm::map<FCDImage*, MObject> TextureMap;
typedef fm::map<fm::string, MString> ShadingNodeMap;
typedef fm::map<MString, FCDocument*> FCDocumentFilenameMap;

class DaeMaterialLibrary : public DaeBaseLibrary
{
public:
	DaeMaterialLibrary(DaeDoc* doc);
	~DaeMaterialLibrary();

	// Export all the non-XRef materials into the current document.
	void Export();

	// Add a shading network to this library and return the export Id
	DaeMaterial* ExportMaterial(MObject shadingEngine);
	DaeMaterial* ExportMaterial(MObject shadingEngine, FCDocument* colladaDocument);

	// Assign a material to a SelectionList, returns the shader set
	void Import();
	DaeMaterial* ImportMaterial(FCDMaterial* colladaMaterial);
	DaeMaterial* InstantiateMaterial(FCDGeometryInstance* colladaInstance, FCDMaterialInstance* colladaMaterialInstance);
	DaeMaterial* CreateMaterial(FCDGeometryInstance* colladaInstance, FCDMaterialInstance* colladaMaterialInstance, FCDMaterial* colladaMaterial);

	// Material instance bindings
	void ImportVertexInputBindings(FCDMaterialInstance* materialInstance, FCDGeometryPolygons* polygons, MObject& shader);
	void ExportTextureBinding(FCDGeometry* geometry, FCDMaterialInstance* instance, const MObject& shaderNode);

	void GetShaderTextures(const MObject& shader, const char* attribute, MObjectArray& textures, MIntArray& blendModes);
		
private:
	// Export a shader, by type
	FCDEffect* ExportStandardShader(FCDocument* colladaDocument, MObject shadingNetwork);
	FCDEffect* ExportConstantShader(FCDocument* colladaDocument, MObject shadingNetwork);
	FCDEffect* ExportColladaFXShader(FCDocument* colladaDocument, MObject shadingNetwork, FCDMaterial* instance);
	FCDEffect* ExportColladaFXPasses(FCDocument* colladaDocument, MObject shadingNetwork, FCDMaterial* instance);

	void ExportPass(FCDEffectProfileFX* profile, FCDEffectPass* pass, MObject shaderObject, FCDMaterial* instance);
	void ExportNewParameters(FCDEffectProfileFX* profile, const MObject& shader, FCDMaterial* instance);

	// Find and export any textures connected to a material attr
	// Returns the top file texture (if any) for translucency factor export (alpha gain/alpha offset).
	MObject ExportTexturedParameter(const MObject& node, const char* attr, FCDEffectStandard* effect, FUDaeTextureChannel::Channel channel, int& nextTextureIndex);
	
	// Create a material and return it's Maya name
	MObject ImportStandardShader(FCDGeometryInstance* colladaInstance, FCDMaterialInstance* colladaMaterialInstance, FCDEffectStandard* standardEffect, FCDMaterial* colladaMaterial);
	MObject ImportColladaFxShader(FCDEffectProfileFX* profile, FCDEffect* effect, FCDMaterial* colladaMaterial);
	void InstantiateStandardShader(DaeMaterial* material, FCDMaterialInstance* colladaMaterialInstance);
	void InstantiateColladaFxShader(DaeMaterial* material, FCDMaterialInstance* colladaMaterialInstance);
	MObjectArray ImportTextures(FCDEffect* cgEffect, FCDEffectProfileFX* cgEffectProfile, FCDMaterial* colladaMaterial);
	void AddDynAttribute(FCDEffectProfileFX* cgEffect, FCDEffectPassShader* colladaShader, FCDMaterial* colladaMaterial, CFXShaderNode* shader, MObject shaderNode);
	
	FCDEffectParameterSurface* writeSurfaceTexture(FCDEffectProfileFX* profile, MObject textureNode, FCDMaterial* material, const fm::string& type);
	void ExportProgramBinding(FCDEffectPassShader* shader, const MString& param, MFnDependencyNode& nodeFn);

private:
	// A lookup table of shading network's we've already processed
	ShaderMaterialMap exportedShaderMap;
	TextureMap importedTextures;
	FCDocumentFilenameMap externalLibraries;

	void ExportTransparency(FCDocument* colladaDocument, MObject shadingNetwork, const MColor& transparentColor, 
			FCDEffectStandard* fx, const char* transparencyPlugName, int& nextTextureIndex);
};

#endif 
