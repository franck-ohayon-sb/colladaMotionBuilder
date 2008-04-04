/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_GEOMETRY_LIBRARY_INCLUDED__
#define __DAE_GEOMETRY_LIBRARY_INCLUDED__

class DaeDoc;
class DaeExtraData;
class DaeGeometry;
class DaeIndexedData;
class FCDGeometry;
class FCDGeometryInstance;
class FCDGeometryMesh;
class FCDGeometrySpline;
class FCDGeometryNURBSSurface;
class FCDMaterialInstance;
class FCDNURBSSpline;
class MFnDependencyNode;
class MFnMesh;

typedef fm::pvector<DaeGeometry> DaeGeometryList;

class DaeGeometryLibrary 
{
public:
	DaeGeometryLibrary(DaeDoc* doc);
	~DaeGeometryLibrary();

	// Create a Maya mesh from the given COLLADA geometry entity
	void Import();
	DaeEntity* ImportGeometry(FCDGeometry* colladaGeometry);
	void ImportMesh(DaeGeometry* geometry, FCDGeometryMesh* colladaMesh);
	void ImportSpline(DaeGeometry* geometry, FCDNURBSSpline* colladaSpline, size_t curSpline);
	void InstantiateGeometry(DaeEntity* sceneNode, FCDGeometryInstance* instance, FCDGeometry* colladaGeometry);

	void InstantiateGeometryMaterials(FCDGeometryInstance* instance, FCDGeometry* colladaGeometry, DaeEntity* mayaInstance, DaeEntity* sceneNode);
	void InstantiateMeshMaterials(FCDGeometryInstance* instance, FCDGeometryMesh* colladaMesh, DaeGeometry* geometry, DaeEntity* mayaInstance, DaeEntity* sceneNode);

	// After import: release the unnecessary memory hogging data
	void LeanMemory();
	void LeanMesh(FCDGeometryMesh* colladaMesh);
	void LeanSpline(FCDGeometrySpline* colladaSpline);

	// Add geometry to this library and return the export Id
	DaeEntity* ExportMesh(const MObject& meshObject);
	void ExportInstanceInfo(const MDagPath& sceneNodePath, FCDGeometry* colladaGeometry, FCDGeometryInstance* colladaInstance);
	void ExportMesh(FCDSceneNode* sceneNode, const MDagPath& dagPath, bool isLocal, 
			bool instantiate=true);
	void ExportSpline(FCDSceneNode* sceneNode, const MDagPath& dagPath, bool isLocal);
	void ExportNURBS(FCDSceneNode* sceneNode, const MDagPath& dagPath, bool isLocal);

	// Export spline
	DaeEntity* IncludeSpline(const MObject& splineObject);

	
private:
	DaeDoc* doc;

	// Process mesh elements
	MObject ImportMeshObject(DaeGeometry* geometry, FCDGeometryMesh* colladaMesh);
	void ImportMeshNormals(MObject& mesh, DaeGeometry* geometry, FCDGeometryMesh* colladaMesh);
	void ImportMeshTexCoords(MObject& mesh, DaeGeometry* geometry, FCDGeometryMesh* colladaMesh);
	bool ImportMeshVertexColors(MObject& mesh, DaeGeometry* geometry, FCDGeometryMesh* colladaMesh);
	bool ImportColorSets(MObject& mesh, FCDGeometryMesh* colladaMesh);
	void ImportVertexBlindData(MObject& mesh, DaeGeometry* geometry);
	bool CheckMeshConsistency(MObject& mesh, FCDGeometryMesh* colladaMesh);
	MObject	AddPolyColorPerVertexHistoryNode(MObject& mesh, DaeGeometry* geometry);
	void AddPolyBlindDataNode(MObject& mesh, DaeExtraData* extra, DaeGeometry* geometry);

	// Process spline elements
	void ImportNURBS(DaeGeometry* geometry, FCDNURBSSpline *nurb, size_t index);
	
	// export object binding and animation for shaders
	void ExportObjectBinding(FCDMaterialInstance* instance, MFnDependencyNode shaderFn);
	void ExportVertexAttribute(FCDMaterialInstance* instance, MFnDependencyNode shaderFn, FCDGeometry* geometry);

	// Processes geometry elements.
	// Used by controllers to override positions and normals
	DaeGeometryList geometryList;
};

#endif 
