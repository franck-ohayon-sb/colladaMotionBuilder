/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "DaeDocNode.h"
#include "DaeEntity.h"
#include "DaeEntityManager.h"
#include "DaeTransform.h"
#include "DaeAnimationCurve.h"
#include "CExportOptions.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDController.h"
#include "FCDocument/FCDEntityInstance.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDSceneNode.h"

//
// DaeEntity
//

DaeEntity::DaeEntity(DaeDoc* doc, FCDEntity* _entity, const MObject& _node)
:	entity(_entity), node(_node), isOriginal(true), instanceCount(0), transform(NULL)
{
	// Attach the FCDEntity with the DaeEntity.
	entity->SetUserHandle(this);

	if (doc->IsExport())
	{
		// Look for a previously imported entity
		DaeDoc* originalDocument = NULL;
		DaeEntity* originalEntity = doc->GetCOLLADANode()->FindEntity(GetNode(), &originalDocument);
		if (originalEntity != NULL && originalDocument != NULL && originalDocument != doc)
		{
			// Clone the previously imported entity
			originalEntity->entity->Clone(entity, false);
			isOriginal = false;
		}
	}

	if (doc->GetCOLLADADocument() != _entity->GetDocument())
	{
		// Register this entity within the correct entity manager.
		// This is necessary to correctly support XRef of DAE documents.
		DaeDoc* entityDocument = doc->GetCOLLADANode()->FindDocument(_entity->GetDocument());
		if (entityDocument != NULL) doc = entityDocument;
	}
	doc->GetEntityManager()->RegisterEntity(this);
}

DaeEntity::~DaeEntity()
{
	SAFE_DELETE(transform);
	entity = NULL;
}

MObject DaeEntity::GetNode() const
{
	if (node.isNull())
	{
		// Retrieve a MObject for this node using the name/path.
		DaeEntity* _this = const_cast<DaeEntity*>(this);
		if (nodeName.length() > 0)
		{
			_this->node = CDagHelper::GetNode(nodeName);
			_this->nodeName = "";
		}
		else
		{
			_this->node = nodePath.node();
			_this->nodePath.set(MDagPath()); // invalidates the DAG path.
		}
	}
	return node;
}

void DaeEntity::SetNode(const MObject& _node)
{
	node = _node;
	nodeName = "";
	nodePath.set(MDagPath());
}

void DaeEntity::ReleaseNode()
{
	if (node.isNull()) return;
	
	if (node.hasFn(MFn::kDagNode))
	{
		if (node.hasFn(MFn::kTransform))
		{
			// For transforms: simply retrieve the DAG path.
			nodePath = MDagPath::getAPathTo(node);
		}
		else
		{
			// For other DAG nodes: find the correct DAG path to the "shape".
			nodePath = MDagPath::getAPathTo(node);
			if (nodePath.node() != node)
			{
				nodePath.push(node);
			}
		}
	}
	else
	{
		// For DG nodes, simply use the node's name, which should be unique.
		nodeName = MFnDependencyNode(node).name();
	}

	node = MObject::kNullObj;
}

void DaeEntity::Instantiate(DaeEntity* sceneNode)
{
	MFnDagNode dagFn(GetNode());
	MFnDagNode dagOldParentFn(dagFn.parentCount() > 0 ? dagFn.parent(0) : MObject::kNullObj);

	if (sceneNode != NULL)
	{
		MFnDagNode dagParentFn(sceneNode->GetNode());
		MObject thisNode = GetNode();
		dagParentFn.addChild(thisNode, MFnDagNode::kNextPos, instanceCount > 0);
	}

	if (instanceCount == 0)
	{
		if (sceneNode != NULL && !dagOldParentFn.object().isNull())
		{
			// Remove the unused link and the unused node.
			// Yet: do not attempt to delete the world node,
			// this will not affect the workflow but all renderings will be black and Maya will be corrupted.
			MObject dagOldParentNode = dagFn.object();
			dagOldParentFn.removeChild(dagOldParentNode);
			if (dagOldParentFn.parentCount() > 0)
			{
				if (dagOldParentFn.childCount() == 0 && !dagOldParentFn.isFromReferencedFile())
				{
					// Delete the old parent
					MGlobal::executeCommand(MString("delete ") + dagOldParentFn.fullPathName());
				}
				else
				{
					CDagHelper::SetPlugValue(dagOldParentFn.object(), "intermediateObject", true);
					dagOldParentFn.setName("___DummyNode___");
				}
			}

			// Reset an overwritten name
			MFnDagNode dagSceneNode(sceneNode->GetNode());
			dagSceneNode.setName(sceneNode->entity->GetName().c_str());
		}
	}

	// Reset the visibility attribute
	CDagHelper::SetPlugValue(GetNode(), "visibility", true);
	CDagHelper::SetPlugValue(GetNode(), "intermediateObject", false);

	instanceCount++;
}

