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

// Scene Node Importer Class

#ifndef _NODE_IMPORTER_H_
#define _NODE_IMPORTER_H_

#include "EntityImporter.h"

class FCDEntityInstance;
class FCDSceneNode;
class GeometryImporter;
class NodeImporter;
class TransformImporter;

typedef fm::pvector<NodeImporter> NodeImporterList;
typedef fm::pvector<GeometryImporter> GeometryImporterList;

class NodeImporter : public EntityImporter
{
private:
	TransformImporter* transform;
	ImpNode* importNode;
	bool addNode;
	bool isGroupNode, isGroupMember;

	// Max' nodes always contain a COLLADA scene node, but in very
	// specific circumstances, can also take in a COLLADA scene node as pivots
	FCDSceneNode* colladaSceneNode;
	FCDSceneNode* colladaPivotNode;

	// NodeImporter tree that matches the scene graph created in Max
	NodeImporterList children;

	// The entity or lack thereof for this node
	EntityImporter* entityImporter;
	FCDEntityInstance* colladaInstance;

public:
	NodeImporter(DocumentImporter* document, NodeImporter* parent);
	~NodeImporter();

	// EntityImporter override
	virtual Type GetType() { return NODE; }

	// Accessors
	inline ImpNode* GetImportNode() { return importNode; }
	//inline void SetValidNodeFloat(bool flag) { validNode = flag; }
	inline bool HasPivot() { return colladaPivotNode != NULL; }
	inline bool IsPivot(FCDSceneNode* node) { return (colladaPivotNode != NULL) ? colladaPivotNode == node : false; }
	inline void SetAddNode(bool doAdd) { addNode = doAdd; }

	// Some scene graph transfrom calculations
	Matrix3 GetWorldTransform(TimeValue t);
	Matrix3 GetLocalTransform(TimeValue t);
	Matrix3 GetPivotTransform();
	Box3 GetBoundingBox(Matrix3& localTransform);

	// Instantiate a COLLADA entity for this node.
	// Each Max node can only have one object.
	// Returns whether the node should exists.
	bool Instantiate(FCDEntityInstance* colladaInstance);
	void Instantiate(FCDSceneNode* colladaSceneNode);

	// Main import methods
	ImpNode* ImportSceneNode(FCDSceneNode* colladaSceneNode);
	void ImportInstances();
	void ImportTransforms();
	void CreateDummies();
	void Finalize(FCDEntityInstance* instance=NULL);

private:
	// Assign a material for this node and if necessary: create a MultiMtl
	void AssignMaterial();

	// Determine whether the given COLLADA node could be a pivot
	static bool IsPossiblePivot(FCDSceneNode* colladaSceneNode);
};

#endif // _NODE_IMPORTER_H_