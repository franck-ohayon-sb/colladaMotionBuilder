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

#ifndef _GEOMETRY_EXPORTER_H_
#define _GEOMETRY_EXPORTER_H_

#ifndef _BASE_EXPORTER_H_
#include "BaseExporter.h"
#endif // _BASE_EXPORTER_H_

class FCDEntity;
class FCDEntityInstance;
class FCDGeometry;
class FCDGeometryMesh;
class FCDGeometryPolygons;
class FCDGeometryPolygonsInput;
class FCDGeometrySource;
class Mtl;
class MNMesh;
class PolyObject;
class TriObject;
struct ExportedMaterial;

typedef fm::map<Object*, FCDGeometry*> ExportedGeometryMap;
//typedef fm::map<int, Mtl*> MaterialIdMap;
typedef fm::pvector<Mtl> MaterialList;

// since there's no base class defining both MeshNormalSpec & MNNormalSpec
// we have to create this dispatcher class
//
class NormalSpec
{
private:
	MeshNormalSpec *triangleSpec;
	bool isTriangles;
	MNNormalSpec *polySpec;
	bool isEditablePoly;

public:
	NormalSpec():triangleSpec(NULL),polySpec(NULL),isEditablePoly(false),isTriangles(false){}
	~NormalSpec(){ SetNull(); }

	// two main modifiers
	void SetTriangles(MeshNormalSpec *t);
	void SetPolygons(MNNormalSpec *p);

	void SetNull();
	bool IsNull();

	// dispatchers
	int GetNumNormals();
	Point3 GetNormal(int idx);
	int GetNormalIndex(int faceIdx, int cornerIdx);

}; // end NormalSpec class

class GeometryExporter : public BaseExporter
{
public:
	GeometryExporter(DocumentExporter* document);
	virtual ~GeometryExporter();

	FCDEntity* ExportMesh(INode* node, Object* object, bool affectsControllers = false);
	FCDEntity* ExportSpline(INode* node, Object* object);
	FCDEntity* ExportNURBS(INode* node, Object* object);

	void ExportInstance(INode* node, FCDEntityInstance* instance);

private:
	void ExportVertexPositions(FCDGeometryMesh* colladaMesh);
	void ExportNormals(FCDGeometryMesh* colladaMesh);
	void ExportEditablePolyNormals(MNMesh& mesh, FCDGeometrySource* source);
	void ExportTextureChannel(FCDGeometryMesh* colladaMesh, int channelIndex);
	void ExportPolygonsSet(FCDGeometryPolygons* colladaPolygons, int materialIndex, int numMaterials);
	void FlattenMaterials(Mtl* material, MaterialList& mtlMap, int materialIndex = -1);

	bool IsEditablePoly() { return triObject == NULL; }
	bool IsNURB(Object* object);

	void ClassifyObject(Object* mesh, bool affectsControllers);

	NormalSpec currentNormals;

	// Buffered and possibly created geometric object.
	GeomObject* geomObject;
	TriObject* triObject;
	PolyObject* polyObject;
	bool deleteObject;

	fm::pvector<ExportedMaterial> exportedMaterials;
};

// structure used to link together the COLLADA material and the 3dsMax MatID.
struct ExportedMaterial
{
	class FCDMaterial* colladaMaterial;
	int materialId;

	ExportedMaterial(FCDMaterial* _colladaMaterial, int _materialId)
		:	colladaMaterial(_colladaMaterial), materialId(_materialId) {}
};

#endif // _GEOMETRY_EXPORTER_H_
