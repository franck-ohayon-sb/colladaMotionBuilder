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

// Static Geometry Importer Class

#ifndef _GEOMETRY_IMPORTER_H_
#define _GEOMETRY_IMPORTER_H_

#include "EntityImporter.h"

class DocumentImporter;
class MaterialImporter;
class TriObject;
class FCDGeometry;
class FCDGeometryPolygonsInput;
class FCDGeometrySpline;
class FCDMaterialInstance;
class MNMesh;

typedef fm::map<FCDGeometryPolygonsInput*, int> TextureChannelMap;
typedef fm::pvector<MaterialImporter> MaterialImporterList;
typedef fm::map<FUCrc32::crc32, uint16> MaterialIndexMap;

class GeometryImporter : public EntityImporter
{
private:
	FCDGeometry* colladaGeometry;
	Object* obj;

	// Contains the matches between the material's textures and the map channels
	TextureChannelMap textureChannels;

	// Flags used to control the content of the default color/texture coordinate channels
	bool hasMapChannel0;
	bool hasMapChannel1;

	INode* currentImport;

	bool isEditablePoly;

public:
	GeometryImporter(DocumentImporter* document, NodeImporter* parent);
	~GeometryImporter();

	// EntityImporter overrides
	virtual Type GetType() { return GEOMETRY; }
	virtual Object* GetObject() { return obj; }

	// Accessors
	// TriObject is isEditablePoly == false, PolyObject otherwise.
	GeomObject* GetMeshObject();
	bool IsEditablePoly(){ return isEditablePoly; }
	const TextureChannelMap& GetTextureChannels() { return textureChannels; }
	FCDGeometry *GetColladaGeometry() { return colladaGeometry; }

	// Main entry points for the class.
	// Create a Max mesh object from a COLLADA geometry and its polygons.
	Object* ImportGeometry(FCDGeometry* colladaGeometry);

	Mtl* CreateMaterial(const FCDEntityInstance* instance);

	// Finalize the import of the skin modifier. Call after the bones have been created
	virtual void Finalize(FCDEntityInstance* instance);

	void SetCurrentImport(INode *inode){ currentImport = inode; }

private:
	bool ImportVertexPositions(MNMesh& mesh);
	bool ImportNormals(MNMesh& mesh);
	bool ImportMapChannels(MNMesh& mesh);
	bool ImportVertexAnimations(GeomObject* polyObject, int masterControllerReferenceID, int vertexControllerBaseID);

	Object* ImportSpline(FCDGeometrySpline *spline);

	static Point3 ToPoint3(float* floatSource, size_t index, uint32 stride);

	void AssignMaterials(const FCDEntityInstance* instance, MaterialImporterList& nodeMaterials);

	int GetMaterialIndex(const FCDMaterialInstance* instance, MaterialImporterList& nodeMaterials, MaterialIndexMap& materialIndices);

};

#endif // _GEOMETRY_IMPORTER_H_