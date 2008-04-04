/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"

#include <maya/MItDependencyNodes.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MFnIkHandle.h>
#include "DaeDocNode.h"
#include "DaeTransform.h"
#include "CExportOptions.h"
#include "CReferenceManager.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "TranslatorHelpers/CSetHelper.h"
#include "FCDocument/FCDCamera.h"
#include "FCDocument/FCDController.h"
#include "FCDocument/FCDEntityReference.h"
#include "FCDocument/FCDExtra.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDLight.h"
#include "FCDocument/FCDSceneNodeIterator.h"
#include "FCDocument/FCDSkinController.h"
#include "FCDocument/FCDPhysicsModel.h"
#include "FCDocument/FCDPhysicsScene.h"
#include "FCDocument/FCDPhysicsModelInstance.h"
#include "FCDocument/FCDPhysicsRigidBodyInstance.h"
#include "FCDocument/FCDPhysicsRigidConstraintInstance.h"

// Scene Graph Main Class
//
DaeSceneGraph::DaeSceneGraph(DaeDoc* _doc)
:	DaeBaseLibrary(_doc)
{
}

DaeSceneGraph::~DaeSceneGraph()
{
	dagNodes.clear();
}

void DaeSceneGraph::AddForcedNode(const MDagPath& dagPath)
{
	MDagPath p = dagPath;
	while (p.length() > 0 && !IsForcedNode(p))
	{
		forcedNodes.append(p);
		p.pop();
	}
}

void DaeSceneGraph::FindForcedNodes(bool selectionOnly)
{
	MStatus status;

	if (selectionOnly)
	{
		MSelectionList selectedItems;
		MGlobal::getActiveSelectionList(selectedItems);
		uint selectedCount = selectedItems.length();
		MDagPathArray queue;
		for (uint i = 0; i < selectedCount; ++i)
		{
			MDagPath selectedPath;
			status = selectedItems.getDagPath(i, selectedPath);
			if (status == MStatus::kSuccess) queue.append(selectedPath);
		}

		while (queue.length() > 0)
		{
			MDagPath selectedPath = queue[queue.length() - 1];
			queue.remove(queue.length() - 1);

			// Queue up the children.
			uint childCount = selectedPath.childCount();
			for (uint i = 0; i < childCount; ++i)
			{
				MObject node = selectedPath.child(i);
				MDagPath childPath = selectedPath; childPath.push(node);
				queue.append(childPath);
			}

			// Look for a mesh
			if (selectedPath.node().hasFn(MFn::kMesh))
			{
				// Only the controller library exports forced nodes for now.
				doc->GetControllerLibrary()->AddForceNodes(selectedPath);
			}
		}
	}
	else
	{
		for (MItDag dagIt(MItDag::kBreadthFirst); !dagIt.isDone(); dagIt.next())
		{
			MDagPath currentPath;
			status = dagIt.getPath(currentPath);
			if (status == MStatus::kSuccess)
			{
				if (currentPath.node().hasFn(MFn::kMesh))
				{
					// Only the controller library exports forced nodes for now.
					doc->GetControllerLibrary()->AddForceNodes(currentPath);
				}
			}
		}
	}
}

bool DaeSceneGraph::IsForcedNode(const MDagPath& dagPath)
{
	uint count = forcedNodes.length();
	for (uint i = 0; i < count; ++i)
	{
		if (dagPath == forcedNodes[i]) return true;
	}
	return false;
}

// Scene Traversal

void DaeSceneGraph::Export(bool selectedOnly)
{
	MSelectionList targets;
	MSelectionList allTargets;
	MObjectArray expressions;

	// Add all the expressions
	MItDependencyNodes depIter(MFn::kExpression);
	for (; !depIter.isDone(); depIter.next())
	{
		MObject item = depIter.item();
		if (item.hasFn(MFn::kExpression))
		{
			expressions.append(item);
		}
	}

	// Create a selection list containing only the root nodes (implies export all!)
	for (MItDag it(MItDag::kBreadthFirst); it.depth() <= 1 && it.item() != MObject::kNullObj; it.next())
	{
		if (it.depth() == 0) continue;

		MDagPath path;
		MStatus s = it.getPath(path);
		if (s == MStatus::kSuccess) allTargets.add(path);
	}

	// now fill in the targets, either the same as allTargets, or it is export selection only
	if (selectedOnly)
	{
		// Export the selection:
		// Grab the selected DAG components
		//
		if (MStatus::kFailure == MGlobal::getActiveSelectionList(targets)) 
		{
			MGlobal::displayError("MGlobal::getActiveSelectionList");
			return;
		}
		
		// For all the non-transforms selected, make sure to extend to the transforms underneath.
		MDagPathArray additions;
		MIntArray removals;
		for (uint32 i = 0; i < targets.length(); ++i)
		{
			MDagPath itemPath;
			targets.getDagPath(i, itemPath);
			if (!itemPath.node().hasFn(MFn::kTransform))
			{
				MDagPath transformPath = itemPath;
				while (transformPath.length() > 0)
				{
					transformPath.pop();
					if (!targets.hasItem(transformPath))
					{
						additions.append(transformPath);
						break;
					}
				}
				removals.append(i);
			}
		}
		for (uint32 i = 0; i < removals.length(); ++i) targets.remove(removals[i] - i);
		for (uint32 i = 0; i < additions.length(); ++i) targets.add(additions[i]);

		// Add all the forced nodes to the list.
		uint32 forceNodeCount = forcedNodes.length();
		for (uint32 i = 0; i < forceNodeCount; ++i)
		{
			MDagPath p = forcedNodes[i];
			if (targets.hasItem(p)) continue;
			targets.add(p);
		}

		// Add additional selection paths for any objects in our
		// selection which have been instanced (either directly, or
		// via instancing of an ancestor) - as otherwise, the selection
		// will only include ONE of the DAG paths 
		//
		AddInstancedDagPaths(targets);

		// remove any selected nodes CONTAINED within other selected
		// heirarchies (to ensure we don't export subtrees multiple times)
		//
		RemoveMultiplyIncludedDagPaths(targets);
	}
	else
	{
		targets = allTargets;
	}

	// Now, take a full pass through, exporting everything in the
	// selection we haven't exported yet (note we do not filter out geometry
	// in this pass as we still need to create instances of the geometry
	// in the scene itself) ...
	// 
	
	// Start by caching the expressions that will be sampled
	uint expressionCount = expressions.length();
	for (uint i = 0; i < expressionCount; ++i)
	{
		// expressions only for sampling
		doc->GetAnimationCache()->SampleExpression(expressions[i]);
	}

	// current will change while traversing the path and represents which visual node objects will be added to.
	FCDSceneNode* current = StartExport(); 

	// Export all/selected DAG nodes
	uint length = allTargets.length();
	for (uint i = 0; i < length; ++i)
	{
		MDagPath dagPath;
		if (allTargets.getDagPath(i, dagPath) != MStatus::kFailure)
		{
			ExportPath(current, dagPath, targets, true, targets.hasItem(dagPath));
		}
	}

	EndExport();
}

FCDSceneNode* DaeSceneGraph::StartExport()
{	
	FCDSceneNode* current = CDOC->AddVisualScene();

	// Setup the reference information
	GetReferenceManager()->Synchronize();

	// Assign a name to the scene
	MString sceneName;
	MGlobal::executeCommand(MString("file -q -ns"), sceneName);
	if (sceneName.length() != 0) current->SetName(MConvert::ToFChar(sceneName));

	return current;
}

void DaeSceneGraph::EndExport()
{
	MStatus status;

	FCDSceneNode* current = CDOC->GetVisualSceneInstance();
	if (current->GetChildrenCount() > 0)
	{
		// Export the transforms
		for (DaeEntityList::iterator it = dagNodes.begin(); it != dagNodes.end(); ++it)
		{
			if ((*it)->transform != NULL)
			{
				(*it)->transform->ExportTransform();
			}
		}

		// Complete the export of controllers and animations
		doc->GetControllerLibrary()->CompleteControllerExport();
		doc->GetAnimationCache()->SamplePlugs();
		doc->GetAnimationLibrary()->PostSampling();

		// Clear the unnecessary transforms
		for (DaeEntityList::iterator it = dagNodes.begin(); it != dagNodes.end(); ++it)
		{
			if ((*it)->transform != NULL)
			{
				(*it)->transform->ClearTransform();
			}
		}

		// Write out the scene parameters
		CDOC->SetStartTime((float) CAnimationHelper::AnimationStartTime().as(MTime::kSeconds));
		CDOC->SetEndTime((float) CAnimationHelper::AnimationEndTime().as(MTime::kSeconds));
	}
	else
	{
		// Delete an empty visual scene.
		SAFE_RELEASE(current);
	}

	forcedNodes.clear();
}


// Traversal methods
//
void DaeSceneGraph::ExportPath(FCDSceneNode*& sceneNode, const MDagPath& dagPath, const MSelectionList& targets, 
								bool isRoot, bool exportNode)
{
	// Does this dagPath already exist? If so, only recurse if FollowInstancedChildren() is set.
	MFnDagNode dagFn(dagPath);
	bool isSceneRoot = dagPath.length() == 0;
	bool hasPreviousInstance = FindElement(dagPath) != NULL;

	// Ignore default and intermediate nodes
	if ((dagFn.isDefaultNode() && !isSceneRoot) || dagFn.isIntermediateObject())
	{
		return;
	}

	MString nodeName = dagPath.partialPathName();
	if (nodeName == MString("nimaInternalPhysics"))
	{
		// Skip this node, which is only used
		// by Nima as a work-around for a Maya bug.
		return;
	}

	MFn::Type type = dagPath.apiType();

	// If we are not already forcing this node, its children
	// check whether we should be forcing it (skinning of hidden joints).
	bool isVisible;
	bool isForced = IsForcedNode(dagPath);
	CDagHelper::GetPlugValue(dagPath.node(), "visibility", isVisible);
	if (!isForced)
	{
		// Check for visibility
		if (!CExportOptions::ExportInvisibleNodes() && !isVisible)
		{
			if (!CAnimationHelper::IsAnimated(doc->GetAnimationCache(), dagPath.node(), "visibility"))
			{
				return;
			}
		}
		else if (!isVisible && !CExportOptions::ExportDefaultCameras())
		{
			// Check for the default camera transform names.
			if (nodeName == "persp" || nodeName == "top" || nodeName == "side" || nodeName == "front") return;
		}
	}
	isForced &= !isVisible;

	// Check for a file reference
	bool isLocal = !dagFn.isFromReferencedFile();
	if (CExportOptions::ExportXRefs() && CExportOptions::DereferenceXRefs()) isLocal = true;
	if (!isLocal && !CExportOptions::ExportXRefs()) return;

	if (!isForced)
	{
		// We don't want to process manipulators
		if (dagPath.hasFn(MFn::kManipulator) || dagPath.hasFn(MFn::kViewManip)) return;

		// Check for constraints which are not exported
		if (!CExportOptions::ExportConstraints() && dagPath.hasFn(MFn::kConstraint)) return;

		// Check set membership exclusion/inclusion
		if (CSetHelper::isExcluded(dagPath)) return;
	}

	// If this is a DAG node (not a DAG shape) check to see whether we should enter
	bool animationExport = true;
	if (!isSceneRoot)
	{
		if (!exportNode || !EnterDagNode(sceneNode, dagPath))
		{
			animationExport = false;
		}
	}

	// Export the transform
	bool isTransform = dagPath.hasFn(MFn::kTransform);
	if (isTransform)
	{
		// Export the scene graph node for all transform-derivatives
		if (dagPath.hasFn(MFn::kJoint))
		{
			if (CExportOptions::ExportJointsAndSkin())
			{
				if (animationExport)
				{
					isLocal = BeginExportTransform(sceneNode, dagPath, isLocal);
					doc->GetControllerLibrary()->ExportJoint(sceneNode, dagPath, isLocal);
				}
				else
				{
					isLocal = true;
				}
			}
			else
			{
				isTransform = false; // we didn't do anything to this node
			}
		}
		else
		{
			// Taken out of VisitTransform.  If VisitTransform
			// returns without creating a child, then we -will- fail.
			if (!isForced && !isVisible && !isLocal) return;

			if (animationExport)
			{
				isLocal = BeginExportTransform(sceneNode, dagPath, isLocal);
			}
			else
			{
				isLocal = true;
			}
		}
	}

	// Export type-specific information
	switch (type)
	{
		case MFn::kLookAt:
		case MFn::kParentConstraint:
		case MFn::kOrientConstraint:
		case MFn::kConstraint:
		case MFn::kAimConstraint:
		case MFn::kPoleVectorConstraint:
		case MFn::kPointConstraint:
		case MFn::kNormalConstraint:
			doc->GetAnimationCache()->SampleConstraint(dagPath);
			break;

		case MFn::kAmbientLight:
		case MFn::kSpotLight:
		case MFn::kPointLight:
		case MFn::kDirectionalLight:
			if (CExportOptions::ExportLights())
			{
				if (animationExport) doc->GetLightLibrary()->ExportLight(sceneNode, dagPath, isLocal);
			}
			break;

		case MFn::kMesh:
			if (CExportOptions::ExportPolygonMeshes())
			{
				if (animationExport) doc->GetGeometryLibrary()->ExportMesh(sceneNode, dagPath, isLocal);
			}
			break;

		case MFn::kIkHandle:
			if (CExportOptions::ExportJointsAndSkin())
			{
				doc->GetAnimationCache()->SampleIKHandle(dagPath);
			}
			break;

		case MFn::kCamera:
			if (CExportOptions::ExportCameras())
			{
				if (animationExport) doc->GetCameraLibrary()->ExportCamera(sceneNode, dagPath, isLocal);
			}
			break;

		case MFn::kRigid:
			if (CExportOptions::ExportPhysics())
			{
				if (animationExport) doc->GetNativePhysicsScene()->ExportRigidBody(sceneNode, dagPath);
			}
			break;

		case MFn::kNurbsCurve:
			{
				if (animationExport) doc->GetGeometryLibrary()->ExportSpline(sceneNode, dagPath, isLocal);
			}
			break;
		case MFn::kNurbsSurface:
			{
				if (animationExport) doc->GetGeometryLibrary()->ExportNURBS(sceneNode, dagPath, isLocal);
			}
			break;
		case MFn::kEmitter:
			{
			}
			break;
		case MFn::kAir:
		case MFn::kDrag:
		case MFn::kField:
		case MFn::kGravity:
		case MFn::kNewton:
		case MFn::kRadial:
		case MFn::kTurbulence:
		case MFn::kUniform:
		case MFn::kVortex:
		case MFn::kVolumeAxis:
			{
			}
			break;
		default: break;
	}

	// Now, whip through this node's DAG children and export each of them ...
	//
	if (isLocal && !hasPreviousInstance)
	{
		uint childCount = dagFn.childCount();
		for (uint i = 0; i < childCount; ++i)
		{
			MObject kid = dagFn.child(i);
			MDagPath kidDagPath = dagPath;
			kidDagPath.push(kid);
			ExportPath(sceneNode, kidDagPath, targets, false, exportNode | targets.hasItem(kidDagPath));
		}
	}

	if (!isSceneRoot && isTransform)
	{
		// Leave this hierarchical node
		if (animationExport) EndExportTransform(sceneNode);
	}

	// AGEIA rigid bodies are non-MFn types, 
	// use special code to recognize them.
	// They are first exported as transforms (i.e. a <node>)
	// then the physical entity is attached to it
	//
	if (CExportOptions::ExportPhysics() && dagPath.hasFn(MFn::kTransform) && !isForced && doc->GetAgeiaPhysicsScene()->isRigidBody(dagFn))
	{
		if (animationExport) doc->GetAgeiaPhysicsScene()->ExportRigidBody(sceneNode, dagPath);
	}
}

// remove any DAG nodes which are multiply included in the selection list.
// (e.g. |transform1|shape1 is already included in |transform1)
//
MStatus DaeSceneGraph::RemoveMultiplyIncludedDagPaths(MSelectionList& selectionList)
{
	// As we're potentially deleting elements out of the selection list
	// it's easiest to avoid array bound check issues by walking the 
	// list backwards.
	MStatus status;
	int length=selectionList.length(&status);
	if (status != MStatus::kSuccess)
		return MStatus::kFailure;
	for (int i = length - 1; i >= 0; --i)
	{
		MDagPath dagIPath;
		if (selectionList.getDagPath(i, dagIPath) != MStatus::kSuccess)
			return MStatus::kFailure;
		uint dagIdepth = dagIPath.length();

		for (int j = i - 1; j >= 0; --j)
		{
			MDagPath dagJPath;
			if (selectionList.getDagPath(j, dagJPath) != MStatus::kSuccess)
				return MStatus::kFailure;
			uint dagJdepth = dagJPath.length();

			// Test if the longer of these two dag paths contains the shorter ...
			if (dagJdepth >= dagIdepth)
			{
				bool isParent = false;
				for (int depth = dagIdepth - 1; depth > 0 && !isParent; --depth)
				{
					dagJPath.pop();
                    isParent = dagJPath.node() == dagIPath.node();
				}

				if (isParent)
				{
					selectionList.remove(j);
					i--;
				}
			}
			else
			{
				bool isParent = false;
				MDagPath dagIt = dagIPath;
				for (int depth = dagIdepth - 1; depth > 0 && !isParent; --depth)
				{
					dagIt.pop();
                    isParent = dagJPath.node() == dagIt.node();
				}

				if (isParent)
				{
					selectionList.remove(i);
					break;
				}
			}
		}
	}
	return MStatus::kSuccess;
}


// Add additional dag paths for any instanced objects within our selection set
// as by default, the selection list only includes one of the dag paths.
//
MStatus DaeSceneGraph::AddInstancedDagPaths(MSelectionList& selectionList)
{
	MStatus status;
	int length=selectionList.length(&status);
	if (status != MStatus::kSuccess)
		return MStatus::kFailure;
	for (int i=0; i<length; i++)
	{
		MDagPath dagPath;
		if (selectionList.getDagPath(i, dagPath) != MStatus::kSuccess)
			return MStatus::kFailure;
		if (dagPath.isInstanced())
		{
			int includedInstance=dagPath.instanceNumber(&status);
			if (status != MStatus::kSuccess)
				return MStatus::kFailure;
			MObject object=dagPath.node(&status);
			if (status != MStatus::kSuccess)
				return MStatus::kFailure;
			MDagPathArray paths;
			if (MDagPath::getAllPathsTo(object, paths) != MStatus::kSuccess)
				return MStatus::kFailure;
			int numPaths=paths.length();
			for (int p=0; p<numPaths; p++)
				if (p!=includedInstance)
					selectionList.add(paths[ p]);
		}
	}
	return MStatus::kSuccess;
}



bool DaeSceneGraph::EnterDagNode(FCDSceneNode* sceneNode, const MDagPath& dagPath, bool instantiate)
{
	// Check for duplicate
	DaeEntity* duplicate = FindElement(dagPath);
	if (duplicate != NULL)
	{
		// Insert a new <instance> of this node
		if (instantiate && sceneNode != NULL)
		{
			ExportInstance(sceneNode, duplicate->entity);
		}

		// Do not enter this node
		return false;
	}

	// Enter this node.
	return true;
}

bool DaeSceneGraph::BeginExportTransform(FCDSceneNode*& sceneNode, const MDagPath& dagPath, bool isLocal)
{
	FUAssert(sceneNode != NULL, return false);

	uint referenceIndex = 0;
	CReference* reference = NULL;
	if (isLocal)
	{
		reference = GetReferenceManager()->FindRerootedReference(dagPath, &referenceIndex);
	}
	else
	{
		reference = GetReferenceManager()->FindReference(dagPath, &referenceIndex);
	}

	// Create the export node
	sceneNode = sceneNode->AddChildNode();
	DaeEntity* nodeEntity = new DaeEntity(doc, sceneNode, dagPath.transform());

	// Process the previously imported scene node
	if (!nodeEntity->isOriginal)
	{
		// Take out the transforms: they are fair-game, for now.
		while (sceneNode->GetTransformCount() > 0) sceneNode->GetTransform(0)->Release();
		
		// Mark the instances/children to be able to later delete the unused ones.
		size_t instanceCount = sceneNode->GetInstanceCount();
		for (size_t i = 0; i < instanceCount; ++i)
		{
			sceneNode->GetInstance(i)->SetUserHandle((void*) 1);
		}
	}
	else
	{
		// Generate a new COLLADA id.
		sceneNode->SetDaeId(doc->DagPathToColladaId(dagPath).asChar());
	}
	sceneNode->SetName(MConvert::ToFChar(doc->DagPathToColladaName(dagPath)));
	nodeEntity->SetNode(dagPath.transform());
	nodeEntity->instanceCount = 1;
	AddElement(nodeEntity);

	// Export the node transformation
	if (reference == NULL)
	{
		nodeEntity->transform = new DaeTransform(doc, sceneNode);
		nodeEntity->transform->SetTransform(dagPath.transform());
		doc->GetEntityManager()->ExportEntity(dagPath.transform(), sceneNode);
		return true;
	}
	else
	{
		MFnTransform fn(nodeEntity->GetNode());
		if (reference->file != NULL && referenceIndex < reference->file->originalTransformations.size())
		{
			// Create the differential transformation matrix
			const MTransformationMatrix& originalMx = reference->file->originalTransformations[referenceIndex];
			MTransformationMatrix currentMx = fn.transformation();
			MTransformationMatrix diffMx(originalMx.asMatrixInverse() * currentMx.asMatrix());

			// Export the node transformation
			nodeEntity->transform = new DaeTransform(doc, sceneNode);
			nodeEntity->transform->SetTransformation(diffMx);
		}

		// Instantiate the external reference: first look for a DAE link
		DaeEntity* entity = NODE->FindEntity(fn.object());
		if (entity != NULL && entity->entity != NULL) sceneNode->AddInstance(entity->entity);
		else
		{
			// Generate the XRef URI.
			FUUri uri(MConvert::ToFChar(reference->file->filename));
			if (reference->paths.length() > 1)
			{
				uri.SetFragment(MConvert::ToFChar(reference->file->rootNames[referenceIndex]));
			}

			// Add the external reference instance.
			FCDEntityInstance* instance = sceneNode->AddInstance(FCDEntity::SCENE_NODE);
			FCDEntityReference* xref = instance->GetEntityReference();
			xref->SetUri(uri);
		}
		return false;
	}
}

void DaeSceneGraph::EndExportTransform(FCDSceneNode*& sceneNode)
{
	FUAssert(sceneNode != NULL, return);

	// Check for instances and children marked for deletion.
	// This may happen when cloning an imported scene node.
	fm::pvector<FCDEntityInstance> instancesToDelete;
	size_t instanceCount = sceneNode->GetInstanceCount();
	for (size_t i = 0; i < instanceCount; ++i)
	{
		FCDEntityInstance* instance = sceneNode->GetInstance(i);
		if (instance->GetUserHandle() == (void*) 1) instancesToDelete.push_back(instance);
	}
	CLEAR_POINTER_VECTOR(instancesToDelete);

	// Step back one tree level.
	sceneNode = sceneNode->GetParent();
}

void DaeSceneGraph::ExportInstance(FCDSceneNode* sceneNode, FCDEntity* entity)
{
	if (entity != NULL && sceneNode != NULL)
	{
		// Retrieve a dag path to the Maya scene node.
		DaeEntity* mayaNode = (DaeEntity*) sceneNode->GetUserHandle();
		MDagPath nodePath;
		if (mayaNode != NULL) nodePath = MDagPath::getAPathTo(mayaNode->GetNode());

		// Check for an existing instance from a previously imported entity.
		FCDEntityInstance* colladaInstance = NULL;
		size_t instanceCount = sceneNode->GetInstanceCount();
		for (size_t i = 0; i < instanceCount; ++i)
		{
			FCDEntityInstance* instance = sceneNode->GetInstance(i);
			if (instance->GetUserHandle() == (void*) 0) continue;
			if (entity->GetType() == instance->GetEntityType() && entity->GetDaeId() == instance->GetEntity()->GetDaeId())
			{
				// We have a match: use this instance.
				colladaInstance = instance;
				instance->SetUserHandle((void*) 0);
				break;
			}
		}

		if (colladaInstance == NULL)
		{
			// Create a new instance.
			colladaInstance = sceneNode->AddInstance(entity);
		}
		else
		{
			colladaInstance->SetEntity(entity);
		}

		// For geometry-based instances: attach the material instance
		if (entity->GetObjectType().Includes(FCDController::GetClassType()))
		{
			FCDController* colladaController = (FCDController*) entity;
			doc->GetGeometryLibrary()->ExportInstanceInfo(nodePath, colladaController->GetBaseGeometry(), (FCDGeometryInstance*) colladaInstance);

			bool oneSkinAlready = false;
			while (colladaController != NULL)
			{
				DaeController* daeController = (DaeController*) colladaController->GetUserHandle();
				daeController->instances.push_back((FCDControllerInstance*) colladaInstance);

				// skins being exported from maya need to have the bindpose matrix subtracted
				// from the instance matrix.
				if (!oneSkinAlready && colladaController->IsSkin())
				{
					DaeTransform* transform = mayaNode->transform;
					FCDSkinController* skinController = colladaController->GetSkinController();
					MMatrix bindPoseMatrix = MConvert::ToMMatrix(skinController->GetBindShapeTransform());

					// Calculate the world transform of the node.
					MMatrix transformMx = bindPoseMatrix.inverse() * transform->GetTransformation().asMatrix();
					transform->SetTransform(MObject::kNullObj);
					transform->SetTransformation(MTransformationMatrix(transformMx));
					oneSkinAlready = true;
				}

				colladaController = DynamicCast<FCDController>(colladaController->GetBaseTarget());
			}
		}
		if (entity->GetObjectType().Includes(FCDGeometry::GetClassType()))
		{
			doc->GetGeometryLibrary()->ExportInstanceInfo(nodePath, (FCDGeometry*) entity, (FCDGeometryInstance*) colladaInstance);
		}
#if 0
		else if (entity->GetObjectType().Includes(FCDEmitter::GetClassType()))
		{
			doc->GetEmitterLibrary()->ExportInstanceInfo(nodePath, (FCDEmitter*) entity, (FCDEmitterInstance*) colladaInstance);
		}
#endif //0
	}
}

// Look-ups
//
DaeEntity* DaeSceneGraph::FindElement(const MDagPath& dagPath)
{
	static bool output = false;
	if (output) MGlobal::displayInfo(MString("Comparing against: ") + dagPath.fullPathName());
	for (DaeEntityList::iterator it = dagNodes.begin(); it != dagNodes.end(); ++it)
	{
		if (output) MGlobal::displayInfo(MString("Local value: ") + MFnDependencyNode((*it)->GetNode()).name());
		if ((*it)->GetNode() == dagPath.node()) return (*it);
	}
	return NULL;
}

void DaeSceneGraph::AddElement(DaeEntity* entity)
{
	dagNodes.push_back(entity);
}

// Import
//
void DaeSceneGraph::Import()
{
	FCDVisualSceneNodeLibrary* library = CDOC->GetVisualSceneLibrary();
	for (size_t i = 0; i < library->GetEntityCount(); ++i)
	{
		FCDSceneNodeIterator it(library->GetEntity(i));
		++it; // Skip the visual scene, itself.
		for (; !it.IsDone(); ++it)
		{
			FCDSceneNode* node = (*it);
			if (node->GetUserHandle() == NULL) ImportNode(node);
		}
	}
}

// Import a <node> and everything in it
//
DaeEntity* DaeSceneGraph::ImportNode(FCDSceneNode* colladaNode)
{
	// Verify that we haven't imported this node already
	if (colladaNode->GetUserHandle() != NULL)
	{
		return (DaeEntity*) colladaNode->GetUserHandle();
	}

	// Create the connection object for this node
	DaeEntity* element = new DaeEntity(doc, colladaNode, MObject::kNullObj);
	AddElement(element);
	element->transform = new DaeTransform(doc, colladaNode);

	// Scene nodes in COLLADA are DAG transforms in Maya.
	// However COLLADA also defines a few special node types
	// such as joints. Take a look at what type of node we 
	// need to create.
	if (colladaNode->GetJointFlag())
	{
		MFnDagNode dagFn;
		MObject obj = dagFn.create("joint", colladaNode->GetName().c_str());
		element->transform->SetTransform(obj);
	}
	else 
	{
		element->transform->CreateTransform(MObject::kNullObj, colladaNode->GetName().c_str());
	}
	element->SetNode(element->transform->GetTransform());

	// Import the node's transform.
	element->transform->ImportTransform(colladaNode);
	MObject entityNode = element->GetNode();
	doc->GetEntityManager()->ImportEntity(entityNode, element);

	// Create a locator for non-joint nodes that are empty
	if (colladaNode->GetChildrenCount() == 0 && colladaNode->GetInstanceCount() == 0 && !colladaNode->GetJointFlag())
	{
		MFnDagNode locatorFn;
		MObject locatorNode = element->GetNode();
		locatorFn.create("locator", locatorNode);
	}

	// Extra flags for joints-only.
	if (colladaNode->GetJointFlag())
	{
		// Import the segment-scale-compensate flag.
		FCDETechnique* mayaTechnique = colladaNode->GetExtra()->GetDefaultType()->FindTechnique(DAEMAYA_MAYA_PROFILE);
		if (mayaTechnique != NULL)
		{
			FCDENode* flagNode = mayaTechnique->FindParameter(DAEMAYA_SEGMENTSCALECOMP_PARAMETER);
			if (flagNode != NULL)
			{
				bool segmentScaleCompensate = FUStringConversion::ToBoolean(flagNode->GetContent()); // not animatable.
				CDagHelper::SetPlugValue(element->GetNode(), "ssc", segmentScaleCompensate);
				SAFE_RELEASE(flagNode); // Release this node so that the export extra preservation code doesn't duplicate it.
			}
		}
	}

	CDagHelper::SetPlugValue(element->GetNode(), "visibility", false);
	return element;
}

void DaeSceneGraph::InstantiateNode(DaeEntity* sceneNode, FCDSceneNode* colladaNode)
{
	DaeEntity* child = ImportNode(colladaNode);
	if (child == NULL) return;

	// Instantiate this node in the scene graph
	child->Instantiate(sceneNode);

	// Import the wanted visibility from the FCollada object
	CDagHelper::SetPlugValue(child->GetNode(), "visibility", !IsEquivalent(colladaNode->GetVisibility(), 0.0f));

	// Only instantiate the child entities and scene nodes once.
	if (child->instanceCount == 1)
	{
		// Instantiate the node's entities.
		for (int32 itI = 0; itI < (int32) colladaNode->GetInstanceCount(); ++itI)
		{
			FCDEntityInstance* instance = colladaNode->GetInstance(itI);
			if (instance->IsExternalReference())
			{
				// Load the external reference through the reference manager.
				// For COLLADA references, this should load the FCDocument object
				// and link the newly loaded entity with this instance object.
				// Use the instance's own document to retrieve the relative path in
				// order to correctly support nested XRefs.
				FCDEntityReference* reference = instance->GetEntityReference();
				FUUri uri(reference->GetUri().GetAbsoluteUri());
				GetReferenceManager()->CreateReference(MDagPath::getAPathTo(child->GetNode()), uri);
			}

			FCDEntity* entity = instance->GetEntity();
			if (entity == NULL) continue;

			// Instantiate this new entity
			if (entity->GetObjectType().Includes(FCDCamera::GetClassType()))
			{
				doc->GetCameraLibrary()->InstantiateCamera(child, (FCDCamera*) entity);
			}
			else if (entity->GetObjectType().Includes(FCDController::GetClassType()))
			{
				// For skin controllers: need to instantiate on a pivot node, if there are other instances or children.
				if (((FCDController*) entity)->IsSkin() && (colladaNode->GetInstanceCount() > 1 || colladaNode->GetChildrenCount() > 0))
				{
					FCDSceneNode* colladaPivotNode = colladaNode->AddChildNode();
					colladaPivotNode->SetName(entity->GetName() + FC("Modifiedpivot"));
					FCDEntityInstance* colladaPivotInstance = colladaPivotNode->AddInstance(entity);
					instance->Clone(colladaPivotInstance);
					SAFE_DELETE(instance);
					--itI;
					continue;
				}
				else
				{
					doc->GetControllerLibrary()->InstantiateController(child, (FCDGeometryInstance*) instance, (FCDController*) entity);
				}
			}
			else if (entity->GetObjectType().Includes(FCDGeometry::GetClassType()))
			{
				doc->GetGeometryLibrary()->InstantiateGeometry(child, (FCDGeometryInstance*) instance, (FCDGeometry*) entity);
			}
			else if (entity->GetObjectType().Includes(FCDLight::GetClassType()))
			{
				doc->GetLightLibrary()->InstantiateLight(child, (FCDLight*) entity);
			}
			else if (entity->GetObjectType().Includes(FCDSceneNode::GetClassType()))
			{
				InstantiateNode(child, (FCDSceneNode*) entity);
			}
		}

		// Instantiate the node's children.
		size_t childNodeCount = colladaNode->GetChildrenCount();
		for (size_t c = 0; c < childNodeCount; ++c)
		{
			InstantiateNode(child, colladaNode->GetChild(c));
		}
	}
}

void DaeSceneGraph::InstantiateVisualScene(FCDSceneNode* colladaScene)
{
	MStatus status;

	// Instantiate the visual scene child nodes.
	size_t childNodeCount = colladaScene->GetChildrenCount();
	for (size_t c = 0; c < childNodeCount; ++c)
	{
		InstantiateNode(NULL, colladaScene->GetChild(c));
	}

	if (CImportOptions::IsOpenMode())
	{
		if (CDOC->HasStartTime()) CAnimationHelper::SetAnimationStartTime(CDOC->GetStartTime());
		if (CDOC->HasEndTime()) CAnimationHelper::SetAnimationEndTime(CDOC->GetEndTime());
	}

	// Grab the layer information
	const FCDLayerList& layers = CDOC->GetLayers();
	for (FCDLayerList::const_iterator itL = layers.begin(); itL != layers.end(); ++itL)
	{
		const FCDLayer* layer = *itL;
		MObject displayLayer = CShaderHelper::FindDisplayLayer(layer->name.c_str());

		// Attach all the objects for this layers
		const StringList& objectIds = layer->objects;
		for (StringList::const_iterator itO = objectIds.begin(); itO != objectIds.end(); ++itO)
		{
			FCDEntity* colladaEntity = CDOC->FindEntity(*itO);
			if (colladaEntity != NULL)
			{
				DaeEntity* e = (DaeEntity*) colladaEntity->GetUserHandle();
				if (e != NULL && !e->GetNode().isNull())
				{
					CDagHelper::Connect(displayLayer, "drawInfo", e->GetNode(), "drawOverride");
				}
			}
		}
	}
}

// assumes that we have loaded the physics scene library
void DaeSceneGraph::InstantiatePhysicsScene(FCDPhysicsScene* colladaScene)
{
	if (CDOC->FindPhysicsModel("AgeiaPhysicsModel") == NULL) return;

	InstantiateRigidBodies(colladaScene);
	InstantiateConstraints(colladaScene);
}

void DaeSceneGraph::InstantiateRigidBodies(FCDPhysicsScene* colladaScene)
{
	// Traverse the physics models
	FCDPhysicsModelInstanceContainer& models = colladaScene->GetPhysicsModelInstances();
	for (FCDPhysicsModelInstanceContainer::iterator itM = models.begin(); itM != models.end(); itM++)
	{
		// only import Ageia physics for now
		if (!IsEquivalent((*itM)->GetEntity()->GetDaeId(), "AgeiaPhysicsModel"))
		{
			MGlobal::displayWarning(MString("Only support AgeiaPhysicsModel. Skipping: ") + (*itM)->GetEntity()->GetDaeId());
			continue;
		}

		// Traverse the rigid bodies and constraints
		size_t instanceCount = (*itM)->GetInstanceCount();
		for (size_t i = 0; i < instanceCount; ++i)
		{
			if ((*itM)->GetInstance(i)->GetType() == FCDEntityInstance::PHYSICS_RIGID_BODY)
			{
				FCDPhysicsRigidBodyInstance* instance = (FCDPhysicsRigidBodyInstance*) (*itM)->GetInstance(i);
				doc->GetPhysicsModelLibrary()->createRigidBody(instance->GetTargetNode(), instance);
			}
		}
	}

	// Forces NIMA to call the compute function in NxRigidSolverNode. this
	// prevents a cycle warning during an evaluation of the nxRigidSolver once
	// the constraints are added.
	MGlobal::viewFrame(0);
}

void DaeSceneGraph::InstantiateConstraints(FCDPhysicsScene* colladaScene)
{
	// Traverse the physics models
	FCDPhysicsModelInstanceContainer& models = colladaScene->GetPhysicsModelInstances();
	for (FCDPhysicsModelInstanceContainer::iterator itM = models.begin(); itM != models.end(); itM++)
	{
		if ((*itM)->GetEntity()->GetDaeId() != "AgeiaPhysicsModel")
		{
			MGlobal::displayWarning(MString("Only support AgeiaPhysicsModel. Skipping: ") + (*itM)->GetEntity()->GetDaeId());
			continue;
		}

		// Traverse the rigid bodies and constraints
		size_t instanceCount = (*itM)->GetInstanceCount();
		for (size_t i = 0; i < instanceCount; ++i)
		{
			if ((*itM)->GetInstance(i)->GetType() == FCDEntityInstance::PHYSICS_RIGID_CONSTRAINT)
			{
				FCDPhysicsRigidConstraintInstance* instance = (FCDPhysicsRigidConstraintInstance*) (*itM)->GetInstance(i);
				doc->GetPhysicsModelLibrary()->createRigidConstraint(instance->GetRigidConstraint());
			}
		}
	}
}
