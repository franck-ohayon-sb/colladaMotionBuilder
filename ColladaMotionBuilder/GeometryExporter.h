/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _GEOMETRY_EXPORTER_H_
#define _GEOMETRY_EXPORTER_H_

class FCDEntity;
class FCDGeometry;
class FCDGeometryInstance;
class FCDGeometryMesh;
class FCDGeometryPolygons;
class FCDGeometrySource;
class FCDMaterialInstance;

#ifndef _ENTITY_EXPORTER_H_
#include "EntityExporter.h"
#endif // _ENTITY_EXPORTER_H_

typedef fm::pvector<FBTexture> FBTextureList;

class GeometryExporter : public EntityExporter
{
private:
	FCDGeometrySource* positionSource;
	FCDGeometrySource* normalSource;
	FCDGeometrySource* texCoordSource;

	// We calculate the data counts from the tesellation indices
	size_t positionCount;
	size_t normalCount;
	size_t texCoordCount;

public:
	GeometryExporter(ColladaExporter* base);
	virtual ~GeometryExporter();

	// Entity export.
	FCDEntity* ExportGeometry(FBGeometry* geometry);

	// Instance export.
	void ExportGeometryInstance(FBModel* node, FCDGeometryInstance* colladaInstance, FCDEntity* colladaEntity);

private:
	// Typed entity export.
	FCDEntity* ExportMesh(FBGeometry* mesh);
	FCDEntity* ExportNurbs(FBNurbs* nurbs);
	FCDEntity* ExportPatch(FBPatch* patch);

	// Mesh entity element export.
	void ExportMeshVertices(FBGeometry* geometry, FCDGeometryMesh* collladaGeometry);
	void ExportMeshTessellation(FBGeometry* geometry, FCDGeometryMesh* collladaGeometry);
	FBFastTessellator* GetTessellator(FBGeometry* geometry);

	// Instance export.
	void ExportMaterialInstance(FCDMaterialInstance* colladaInstance, FCDGeometryPolygons* colladaPolygons, const FBTextureList& textures);
};

#endif // _GEOMETRY_EXPORTER_H_
