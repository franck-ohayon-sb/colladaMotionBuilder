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

#ifndef _NODE_EXPORTER_H_
#define _NODE_EXPORTER_H_

#ifndef _BASE_EXPORTER_H_
#include "BaseExporter.h"
#endif // _BASE_EXPORTER_H_

class FCDEntity;
class FCDSceneNode;
class INode;
struct ExportNode;

typedef fm::pvector<INode> NodeList;
typedef fm::map<INode*, ExportNode*> ExportNodeMap;
typedef fm::map<Object*, FCDEntity*> ExportedEntityMap;
typedef fm::map<INode*, Matrix3> NodeTransformMap;

class NodeExporter : public BaseExporter
{
public:
	NodeExporter(DocumentExporter* document);
	~NodeExporter();

	void CalculateExportNodes();
	bool CalculateExportNodes(INode* currentNode, NodeList& selectedNodes, NodeList& forcedNodes);

	void ExportScene();
	void ExportSceneGraph();
	void ExportSceneNode(ExportNode& node);
	void ExportInstances();

	void ExportHelper(INode* node, Object* object, FCDSceneNode* colladaNode);
	bool ExportTransforms(INode* node, FCDSceneNode* colladaNode, bool checkSkin);
	void ExportTransformElements(FCDSceneNode* colladaNode, INode* node, Control* c, const Matrix3& tm);
	FCDSceneNode* ExportPivotNode(INode* node, FCDSceneNode* colladaNode);

	FCDSceneNode* FindExportedNode(INode* node);
	const Matrix3& GetWorldTransform(INode* node);
	const Matrix3& GetLocalTransform(INode* node);

	void ApplyPivotTM(INode* maxNode, Matrix3& tm);


private:

	void ExportRotationElement(Control* rotController, FCDSceneNode* colladaNode);

	NodeList forcedNodes;
	ExportNodeMap exportNodes;
	ExportedEntityMap exportedEntities;
	NodeTransformMap worldTransforms;
	NodeTransformMap localTransforms;
	INode* worldNode;
};

struct ExportNode
{
	INode* maxNode;
	FCDSceneNode* colladaNode;
	FCDSceneNode* colladaParent;
	bool exportObject;
	ExportNode(INode* _maxNode, bool _exportObject = true)
		: maxNode(_maxNode), colladaParent(NULL), colladaNode(NULL), exportObject(_exportObject) {}
};

#endif // _NODE_EXPORTER_H