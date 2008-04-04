/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_SCENE_GRAPH__
#define __DAE_SCENE_GRAPH__

#include "DaeBaseLibrary.h"

class CReference;
class DaeDoc;
class DaeEntity;
class DaeTransform;
class FCDEntity;
class FCDEntityInstance;
class FCDSceneNode;
class MFnPlugin;

typedef fm::pvector<DaeEntity> DaeEntityList;

class DaeSceneGraph : public DaeBaseLibrary
{
public:
	DaeSceneGraph(DaeDoc* doc);
	virtual ~DaeSceneGraph();

	// Before the export, list the forced nodes.
	// Done during the AnimCache pass..
	void FindForcedNodes(bool selectionOnly);
	void AddForcedNode(const MDagPath& dagPath);
	bool IsForcedNode(const MDagPath& dagPath);

	// Export: Recursive Maya DAG export
	void Export(bool selectedOnly);
	bool EnterDagNode(FCDSceneNode* sceneNode, const MDagPath& dagPath, bool instantiate=true);
	void ExportInstance(FCDSceneNode* sceneNode, FCDEntity* entity);

	// Look-ups
	DaeEntity* FindElement(const MDagPath& dagPath);
	void AddElement(DaeEntity* entity);

	// Import: Recursive scene tree import
	void Import();
	DaeEntity* ImportNode(FCDSceneNode* colladaNode);
	void InstantiateNode(DaeEntity* sceneNode, FCDSceneNode* colladaNode);
	void InstantiateVisualScene(FCDSceneNode* colladaScene);
	void InstantiatePhysicsScene(FCDPhysicsScene* colladaScene);

private:
	void InstantiateRigidBodies(FCDPhysicsScene* colladaScene);
	void InstantiateConstraints(FCDPhysicsScene* colladaScene);

	// Export methods
	FCDSceneNode* StartExport();
	void EndExport();
	bool BeginExportTransform(FCDSceneNode*& sceneNode, const MDagPath& dagPath, bool isLocal);
	void EndExportTransform(FCDSceneNode*& sceneNode);
	void ExportPath(FCDSceneNode*& sceneNode, const MDagPath& dagPath, const MSelectionList& targets, 
			bool isRoot=false, bool exportNode=false); 

	// Utilities for pre-processing selection lists
	MStatus RemoveMultiplyIncludedDagPaths(MSelectionList& selectionList);
	MStatus AddInstancedDagPaths(MSelectionList& selectionList);

private:
	DaeEntityList dagNodes;
	MDagPathArray forcedNodes;
};

#endif // __DAE_SCENE_GRAPH__
