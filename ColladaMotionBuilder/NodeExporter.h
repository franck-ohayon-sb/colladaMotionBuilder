/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _NODE_EXPORTER_H_
#define _NODE_EXPORTER_H_

class FCDSceneNode;
class ColladaExporter;

#ifndef _ENTITY_EXPORTER_H_
#include "EntityExporter.h"
#endif // _ENTITY_EXPORTER_H_

class NodeExporter : public EntityExporter
{
private:
	typedef fm::pair<FCDSceneNode*, uint32> CountedNode;
	typedef fm::map<FBModel*, CountedNode> ExportedNodeMap;
	typedef fm::map<FBModel*, uint32> NodeSet;

	FBScene* scene;
	FCDSceneNode* colladaScene;
	ExportedNodeMap exportedNodes;
	NodeSet nodesToSample;


	typedef fm::vector<const char *> BoneListName;
	BoneListName* boneNameExported;

public:

	enum Values
	{
		EulerXYZ = 0,
		EulerXZY,
		EulerYZX,
		EulerYXZ,
		EulerZXY,
		EulerZYX,
		SphericXYZ
	};
	
	NodeExporter(ColladaExporter* base);
	virtual ~NodeExporter();

	// Accessor.
	FBScene* GetScene() { return scene; }

	// Main entry point: exports the main Motion Builder scene.
	void PreprocessScene(FBScene* scene);
	void ExportScene(FBScene* scene);
	void PostprocessScene(FBScene* scene);

	// Recursively exports the scene nodes.
	FCDSceneNode* ExportNode(FCDSceneNode* colladaParent, FBModel* node);
	FCDSceneNode* ExportPivot(FCDSceneNode* colladaNode, FBModel* node);
	void ExtractPivot(FBModel* node, FMVector3& pivotTranslation, FMVector3& pivotRotation, FMVector3& pivotScale);
	void ExportInstance(FCDSceneNode* colladaNode, FBModel* node);

	// Cameras have annoying transforms. Need to encapsulate the transform retrieval.
	FMMatrix44 GetParentTransform(FBModel* node, bool isLocal=true);

	void SetBoneListToExport(BoneListName* list){ boneNameExported = list; }

private:
	void ExportTransforms(FCDSceneNode* colladaNode, FBModel* node);
	void PostProcessNode(FCDSceneNode* colladaNode);
	void FindNodesToSample(FBScene* scene);
	bool HasIK(FBModel* node);
};

#endif // _NODE_EXPORTER_H_