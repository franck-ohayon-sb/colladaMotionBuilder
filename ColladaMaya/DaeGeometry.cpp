/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "DaeGeometry.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "FCDocument/FCDGeometry.h"

//
// DaeGeometry
//

DaeGeometry::DaeGeometry(DaeDoc* doc, FCDGeometry* entity, const MObject& node)
:	DaeEntity(doc, entity, node)
,	colladaGeometry(entity), colladaInstance(NULL)
{
}

DaeGeometry::~DaeGeometry()
{
	colladaGeometry = NULL;
	colladaInstance = NULL;
	children.clear();
}

void DaeGeometry::Instantiate(DaeEntity *sceneNode)
{
	if (children.length() == 0)
	{
		// no child -> only perform normal entity instantiation
		DaeEntity::Instantiate(sceneNode);
	}
	else
	{
		// a simple variation of the DaeEntity::Instantiate algorithm
		
		// do not add the node, use the scene node as parent transform
		MFnDagNode dagFn(children[0]);
		MFnDagNode dagOldParentFn(dagFn.parent(0));

		if (sceneNode != NULL)
		{
			MFnDagNode dagParentFn(sceneNode->GetNode());

			// append every child object (should be shape DAG nodes with their dummy transform)
			for (unsigned int i = 0; i < children.length(); i++)
			{
				dagParentFn.addChild(children[i], MFnDagNode::kNextPos, instanceCount > 0);
			}
		}

		if (instanceCount == 0)
		{
			if (sceneNode != NULL && !dagOldParentFn.object().isNull())
			{
				// Remove the unused link and the unused node.
				// Yet: do not attempt to delete the world node,
				// this will not affect the workflow but all renderings will be black and Maya will be corrupted.
				MObject __node = dagFn.object();
				dagOldParentFn.removeChild(__node);
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
}
