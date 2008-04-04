/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __MESH_HELPER_INCLUDED__
#define __MESH_HELPER_INCLUDED__

class FCDGeometrySource;
class DaeDoc;

struct DaeColourSet
{
	MString name;
	MObject polyColorPerVertexNode;
	MPlug arrayPlug;
	MIntArray indices;
	bool isBlankSet;
	bool isVertexColor;
	int32 whiteColorIndex;
};
typedef fm::pvector<DaeColourSet> DaeColourSetList;

class CMeshHelper
{
public:
	// Find all the history nodes of a specific type
	static void GetHistoryNodes(const MObject& mesh, MFn::Type nodeType, MObjectArray& historyNodes);

	// Get the mesh's color sets information (Maya 7.0+)
	static void GetMeshValidColorSets(const MObject& mesh, DaeColourSetList& colorSets);
	static void GetMeshColorSet(DaeDoc* doc, const MObject& mesh, FCDGeometrySource* source, DaeColourSet& colorSet);
};

#endif
