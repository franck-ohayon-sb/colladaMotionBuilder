/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/*
* Helper class to export COLLADA meshes.
*/

#ifndef __MESH_EXPORTER_INCLUDED__
#define __MESH_EXPORTER_INCLUDED__

#include <maya/MFnMesh.h>

class FCDGeometryMesh;
class FCDGeometrySource;
class FCDGeometryPolygonsInput;
struct DaeColourSet;
typedef fm::pvector<DaeColourSet> DaeColourSetList;

// A generic Input attribute
class DaeInput
{
public:
	FCDGeometrySource* source;
	FCDGeometryPolygonsInput* input;
	int idx;

	DaeInput(FCDGeometrySource* _source, int _idx = -1)
		: source(_source), input(NULL), idx(_idx) {}
};

// The mesh exporter structure
class CMeshExporter
{
public:
	CMeshExporter(DaeEntity* _element, MFnMesh& _meshFn, FCDGeometryMesh* _colladaMesh, DaeDoc* _fDoc);
	~CMeshExporter();

	void SetBakeMatrix(const MMatrix& _bakeMatrix);

	// Export the whole mesh.
	void ExportMesh();

	// Export the per-vertex data
	void ExportVertexPositions();
	bool ExportVertexNormals();
	void ExportTexCoords(const MStringArray& uvSetNames, const StringList& texcoordIds);
	void ExportVertexBlindData();
	void ExportColorSets(DaeColourSetList& colorSets);

	// Generate the COLLADA ids for specific source elements.
	StringList GenerateTexCoordIds(const MStringArray& uvSetNames);

	// Are the faces double-sided?
	bool IsDoubleSided();

private:
	DaeEntity* element;
	MFnMesh& meshFn;
	FCDGeometryMesh* colladaMesh;
	DaeDoc* doc;
	fm::vector<DaeInput> accumulator;
};

#endif // __MESH_EXPORTER_INCLUDED__
