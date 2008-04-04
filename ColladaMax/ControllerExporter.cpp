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

#include "StdAfx.h"
#include "AnimationExporter.h"
#include "ColladaModifiers.h"
#include "DocumentExporter.h"
#include "GeometryExporter.h"
#include "EntityExporter.h"
#include "NodeExporter.h"
#include "ColladaOptions.h"
#include "XRefFunctions.h"
#include "Common/ConversionFunctions.h"
#include "FCDocument/FCDAnimationCurve.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDController.h"
#include "FCDocument/FCDEntity.h"
#include "FCDocument/FCDEntityInstance.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDGeometryMesh.h"
#include "FCDocument/FCDGeometrySource.h"
#include "FCDocument/FCDGeometryInstance.h"
#include "FCDocument/FCDControllerInstance.h"
#include "FCDocument/FCDMorphController.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDSkinController.h"
#include "ControllerExporter.h"
#include "IMorpher/IMorpher.h"
#include <modstack.h>
#include "IInstanceMgr.h"

ControllerExporter::ControllerExporter(DocumentExporter* _document)
:	BaseExporter(_document)
,	boneIndex(0)
{
	modifiers.reserve(3);
}

ControllerExporter::~ControllerExporter()
{
	// Delete all the controller information structures.
	size_t controllerCount = CDOC->GetControllerLibrary()->GetEntityCount();
	for (size_t i = 0; i < controllerCount; ++i)
	{
		FCDController* controller = CDOC->GetControllerLibrary()->GetEntity(i);
		ControllerInfo* info = (ControllerInfo*) controller->GetUserHandle();
		SAFE_DELETE(info);
		controller->SetUserHandle(NULL);
	}

	CLEAR_POINTER_VECTOR(modifiers);
}

void ControllerExporter::CalculateForcedNodes(INode* UNUSED(node), Object* object, NodeList& forcedNodes)
{
	// Look for possible skin and morpher modifiers
	ResolveModifiers(object);

	for (ColladaModifierStack::iterator itM = modifiers.begin(); itM != modifiers.end(); ++itM)
	{
		ColladaModifier* mod = (*itM);
        if (mod->GetClassID() == COLLADA_SKIN_CLASS_ID)
		{
			// Here we can call GetInterface without the node, as
			// we access no node specific data.

			// Attempt to locate a (any) node referencing this object.
			// We do not need any instance specific data, but physique
			// requires a node handle even for operations that only
			// access bone information.
			ULONG handle;
			mod->GetModifier()->NotifyDependents(FOREVER, (PartID)&handle, REFMSG_GET_NODE_HANDLE);
			INode* node = GetCOREInterface()->GetINodeByHandle(handle);
			assert(node != NULL);

			ColladaSkinInterface* iskin = ((ColladaSkin*)mod)->GetInterface(node);
			int boneCount = iskin->GetNumBones();
			for (int i = 0; i < boneCount; ++i)
			{
				INode* boneNode = iskin->GetBone(i);
				if (boneNode != NULL) forcedNodes.push_back(boneNode);
			}

			iskin->ReleaseMe();
		}
	}
}

FCDEntity* ControllerExporter::ExportController(INode* node, Object* object)
{
	if (object == NULL || node == NULL) 
	{
		return NULL;
	}

	// current object modifiers resolution
	ResolveModifiers(object);

	// Retrieve the initial mesh
	Object* initialMesh = (!modifiers.empty()) ? modifiers.front()->GetInitialPose() : object;

	// FCollada doesn't support external references for controller sources
	// For the moment we dereference the XRef if any
	// TODO: Change as soon as FCollada upgrades
	initialMesh = XRefFunctions::GetXRefObjectSource(initialMesh);

	// Export the initial mesh.
	FCDGeometry* geometry = (FCDGeometry*) document->GetGeometryExporter()->ExportMesh(node, initialMesh, !modifiers.empty());
	FCDEntity* entity = geometry;
	for (ColladaModifierStack::iterator itM = modifiers.begin(); itM != modifiers.end() && entity != NULL; ++itM)
	{
		ColladaModifier* modifier = (*itM);
		FCDController* colladaController = CDOC->GetControllerLibrary()->AddEntity();
		fm::string suggId;
		if (modifier->GetClassID() == COLLADA_SKIN_CLASS_ID)
		{
			// Create an empty skin controller
			ColladaSkin* skin = (ColladaSkin*) modifier;
			colladaController->SetUserHandle(new ControllerInfo(node, skin->GetModifier()));
			FCDSkinController* colladaSkin = colladaController->CreateSkinController();
			colladaSkin->SetTarget(entity);
			suggId = entity->GetDaeId() + "-skin";
		}

		else if (modifier->GetClassID() == COLLADA_MORPHER_CLASS_ID)
		{
			// Create an empty morph controller
			ColladaMorph* morph = (ColladaMorph*) modifier;
			colladaController->SetUserHandle(new ControllerInfo(node, morph->GetModifier()));
			FCDMorphController* morpher = colladaController->CreateMorphController();
			morpher->SetBaseTarget(entity);
			suggId = entity->GetDaeId() + "-morpher";
		}
		else FUFail(continue); // Cannot be here.

		ENTE->ExportEntity(modifier->GetModifier(), colladaController, suggId.c_str());
		entity = colladaController;
	}

	return entity;
}

void ControllerExporter::LinkControllers()
{
	fm::pvector<FCDEntityInstance> gatherList;
	fm::pvector<FCDSceneNode> nodeQueue;
	FCDVisualSceneNodeLibrary* library = CDOC->GetVisualSceneLibrary();
	for (size_t i = 0; i < library->GetEntityCount(); i++)
	{
		nodeQueue.push_back(library->GetEntity(i));
	}

	while (!nodeQueue.empty())
	{
		FCDSceneNode* sceneNode = nodeQueue.back();
		nodeQueue.pop_back();

		// Link children first, as I think we depend on them...
		for (size_t c = 0; c < sceneNode->GetChildrenCount(); c++)
		{
			FCDSceneNode* child = sceneNode->GetChild(c);
			nodeQueue.push_back(child);
		}

		// Now hook up our exports.
		size_t count = sceneNode->GetInstanceCount();
		for (size_t e = 0; e < count; e++)
		{
			FCDEntityInstance* instance = sceneNode->GetInstance(e);
			if (instance && instance->HasType(FCDControllerInstance::GetClassType()))
			{
				gatherList.push_back(instance);
			}
		}
	}

	// Iterate over all the FCollada controllers and fill them in.
	size_t controllerCount = CDOC->GetControllerLibrary()->GetEntityCount();
	for (size_t i = 0; i < controllerCount; ++i)
	{
		FCDController* controller = CDOC->GetControllerLibrary()->GetEntity(i);
		ControllerInfo* info = (ControllerInfo*) controller->GetUserHandle();
		if (info != NULL)
		{
			INode*  inode  = info->node;
			for (FCDEntityInstance** itr = gatherList.begin(); itr != gatherList.end(); itr++)
			{
				if ((*itr)->GetEntity() == controller)
				{
					LinkController(controller, inode, (FCDControllerInstance*)*itr);
				}
			}
		}
	}
}

bool ControllerExporter::LinkController(FCDController* controller, INode* inode, FCDControllerInstance* instance)
{
	FUAssert(controller != NULL, return false);

	if (controller->IsSkin())
		{
		ColladaSkin modifier;
		modifier.Resolve(inode->GetObjectRef());
		LinkSkin(controller->GetSkinController(), instance, inode, &modifier);
	}
	else if (controller->IsMorph())
	{
		ColladaMorph modifier;
		modifier.Resolve(inode->GetObjectRef());
		LinkMorph(controller->GetMorphController(), instance, inode, &modifier);
	}
	else return false;
	return true;
}

void ControllerExporter::LinkSkin(FCDSkinController* colladaSkin,
								  FCDControllerInstance* instance, 
								  INode* node, 
								  ColladaSkin* modifier)
{
	if (!modifier->IsResolved() || colladaSkin == NULL || instance == NULL || node == NULL)
		return;   // only if it has a skin modifier

	// Morphs only link with an instance! 
	FCDEntity* baseTarget = colladaSkin->GetTarget();
	if (baseTarget != NULL && baseTarget->GetType() == FCDEntity::CONTROLLER)
	{
		LinkController((FCDController*)baseTarget, node, instance);
	}

	// Calculate the initial skin local transformation for the first instance.
	// Because everything is uncertain, bake in also the first instance's current pivot
	Matrix3 bindShapeTM;
	ColladaSkinInterface* iskin = modifier->GetInterface(node);
	iskin->GetSkinInitTM(bindShapeTM, true);

	// Fill in the joint information: the bone scene node and its inverse bind-pose transforms
	INodeTab controllerInstanceNodes;
	IInstanceMgr::GetInstanceMgr()->GetInstances(*node, controllerInstanceNodes);

	// calls ISkin::GetNumBones, not GetNumBonesFlat.
	// I don't know why Max exposes a bone list containing NULL entries
	// via GetBoneFlat and GetNumBonesFlat functions. Maybe the indexes in the
	// "flat" list are used somewhere else... -Etienne.
	size_t jointCount = iskin->GetNumBones();
	size_t colladaJointCount = colladaSkin->GetJointCount();
	bool newEntity = (colladaJointCount != jointCount);
	fm::map<int, int>boneIndexRemap;

	// reserve the maximum bone count
	colladaSkin->SetBindShapeTransform(ToFMMatrix44(bindShapeTM));
	if (newEntity) colladaSkin->SetJointCount(0);

	// for each entry
	for (int i = 0; i < (int) jointCount; ++i)
	{
		// there should not be any null bone.
		// the ISkin::GetBone, not GetBoneFlat, function is called here.
		INode* boneNode = iskin->GetBone(i);
		if (boneNode == NULL)
		{
			continue;
		}

		FCDSceneNode* sceneNode = NULL;
		sceneNode = document->GetNodeExporter()->FindExportedNode(boneNode);
		// if the boneNode is valid, then we should have it in the document.
		FUAssert(sceneNode != NULL, continue);

		int idx = (int) instance->FindJointIndex(sceneNode);
		if (idx >= 0) boneIndexRemap.insert(i, idx);
		else
		{
			boneIndexRemap.insert(i, (int)instance->GetJointCount());
			instance->AddJoint(sceneNode);
			if (newEntity)
			{
				// Calculate the bind-pose transformation.
				// FCollada will do the inversion.
				Matrix3 bindPose;
				if (!iskin->GetBoneInitTM(boneNode, bindPose))
				{
					bindPose = NDEX->GetWorldTransform(boneNode);
				}
	
				// We NEED a sub Id, and we can assume that Bone(i) is unique
				if (sceneNode->GetSubId().empty()) sceneNode->SetSubId("Bone" + TO_STRING(++boneIndex));

				bindPose.Invert();
				colladaSkin->AddJoint(sceneNode->GetSubId(), ToFMMatrix44(bindPose));
			}
		}
	}

	// Generate the entity instance skeleton roots.
	instance->CalculateRootIds();

	// Fill in the vertex influence information.
	// There should always be as many influences as there are vertices.
	if (newEntity)
	{
		int vertexCount = iskin->GetNumVertices();
		colladaSkin->SetInfluenceCount(vertexCount);
		for (int i = 0; i < vertexCount; ++i)
		{
			FCDSkinControllerVertex* vertex = colladaSkin->GetVertexInfluence(i);
			int pairCount = iskin->GetNumAssignedBones(i);
			vertex->SetPairCount(pairCount);
			for (int p = 0; p < pairCount; ++p)
			{
				FCDJointWeightPair* pair = vertex->GetPair(p);
				pair->weight = iskin->GetBoneWeight(i, p);
				pair->jointIndex = iskin->GetAssignedBone(i, p);
			}
		}
	}



	// Dont forget to release ourselves
	iskin->ReleaseMe();
}

void ControllerExporter::LinkMorph(FCDMorphController* colladaMorph, 
								   FCDControllerInstance* instance, 
								   INode* node, 
								   ColladaMorph* modifier)
{
	if (!modifier->IsResolved() || colladaMorph == NULL || node == NULL)
		return;   // only if it has a skin modifier

	// Link our base targets if its a skin
	FCDEntity* baseTarget = colladaMorph->GetBaseTarget();
	if (baseTarget != NULL && baseTarget->GetType() == FCDEntity::CONTROLLER)
	{
		LinkController((FCDController*)baseTarget, node, instance);
	}

	// Prepare the FCollada structure to hold the morph targets
	MorphR3* imorph = modifier->GetInterface();
	size_t targetCount = imorph->chanBank.size();
	colladaMorph->SetMethod(FUDaeMorphMethod::NORMALIZED);

	// Export all the morph targets
	for (size_t i = 0; i < targetCount; ++i)
	{
		morphChannel& channel = imorph->chanBank[i];
		if (!channel.mActive || channel.mNumPoints == 0) continue;

		// the target mesh geometry for the morpher
		FCDGeometry* targetMesh = NULL;
		INode* targetMaxNode = channel.mConnection;
		if (targetMaxNode == NULL)
		{
			// the object instance has been deleted, BUT we still have the mesh information in
			// the morpher modifier and its morph channels.
			FloatList targetVertices;
			targetVertices.resize(3 * channel.mNumPoints);
			FloatList::iterator listIt = targetVertices.begin();

			// each point represents an OFFSET to the original mesh point
			for (int p = 0; p < channel.mNumPoints; ++p)
			{
				const Point3& pt = channel.mPoints[p];
				(*listIt++) = pt.x;
				(*listIt++) = pt.y;
				(*listIt++) = pt.z;
			}

			// create the new geometry based on the original geometry
			targetMesh = (FCDGeometry*)document->GetGeometryExporter()->ExportMesh(node, node->GetObjectRef());
			if (targetMesh == NULL) continue;

			targetMesh->SetDaeId(colladaMorph->GetParent()->GetDaeId() + "-" + (const char*) channel.mName + "-" + TO_STRING(i));

			// overwrite the position data
			FCDGeometrySource* colladaPositions = targetMesh->GetMesh()->GetPositionSource();
			colladaPositions->SetData(targetVertices, 3);
		}
		else
		{
			FCDSceneNode* colladaTargetNode = document->GetNodeExporter()->FindExportedNode(targetMaxNode);
			if (colladaTargetNode != NULL)
			{
				if (colladaTargetNode->GetInstanceCount() == 0)
				{
					// we have a target node, but it wasn't instanced (maybe hidden)
					// export that geometry, we need it.
					targetMesh = (FCDGeometry*) document->GetGeometryExporter()->ExportMesh(targetMaxNode, targetMaxNode->GetObjectRef());
				}

				// Retrieve the mesh instance for the target scene node.
				// NOTE: 3dsMax supports only one instance per target scene node.
				for (size_t t = 0; targetMesh == NULL && t < colladaTargetNode->GetInstanceCount(); ++t)
				{
					FCDEntityInstance* instance = colladaTargetNode->GetInstance(t);
					FCDEntity* entity = instance->GetEntity();
					if (entity->GetObjectType().Includes(FCDController::GetClassType()))
					{
						FCDController* c = (FCDController*) entity;
						targetMesh = c->GetBaseGeometry();
					}
					else if (entity->GetObjectType().Includes(FCDGeometry::GetClassType()))
					{
						targetMesh = (FCDGeometry*) entity;
					}
				}

				// You also need to look at/for pivots.
				for (size_t p = 0; targetMesh == NULL && p < colladaTargetNode->GetChildrenCount(); ++p)
				{
					FCDSceneNode* possiblePivotNode = colladaTargetNode->GetChild(p);
					for (size_t t = 0; targetMesh == NULL && t < possiblePivotNode->GetInstanceCount(); ++t)
					{
						FCDEntityInstance* instance = possiblePivotNode->GetInstance(t);
						FCDEntity* entity = instance->GetEntity();
						if (entity->GetObjectType().Includes(FCDController::GetClassType()))
						{
							FCDController* c = (FCDController*) entity;
							targetMesh = c->GetBaseGeometry();
						}
						else if (entity->GetObjectType().Includes(FCDGeometry::GetClassType()))
						{
							targetMesh = (FCDGeometry*) entity;
						}
					}
				}
			}
			else
			{
				// we have a target node, but it wasn't exported at all (maybe hidden)
				// export that geometry, we need it.
				targetMesh = (FCDGeometry*) document->GetGeometryExporter()->ExportMesh(targetMaxNode, targetMaxNode->GetObjectRef());
			}
		}

		// now export the animations from the mesh
		if (targetMesh != NULL)
		{
			// Fill up the FCollada morph target structure
			FCDMorphTarget* colladaTarget = colladaMorph->AddTarget(targetMesh);
			if (colladaTarget != NULL)
			{
				Control* weightController = channel.cblock->GetController(morphChannel::cblock_weight_index);
				float weight = channel.cblock->GetFloat(morphChannel::cblock_weight_index, TIME_EXPORT_START);
				colladaTarget->SetWeight(weight * 0.01f);
				ANIM->ExportAnimatedFloat(colladaMorph->GetParent()->GetDaeId() + "-weights", weightController, colladaTarget->GetWeight(), (int) i, &FSConvert::FromPercent);
			}
		}
	}
}

void ControllerExporter::ResolveModifiers(Object* obj)
{
	CLEAR_POINTER_VECTOR(modifiers);
    
	// TODO: Obviously, we need to resolve modifiers in a smarter fashion...
	ColladaSkin skin; skin.Resolve(obj);
	ColladaMorph morph; morph.Resolve(obj);
		
	if (skin.IsResolved() && morph.IsResolved())
	{
		// Remember that 3dsMax has the mod stack reversed in its internal structures.
		if (skin.GetModifierStackIndex() > morph.GetModifierStackIndex())
		{
			modifiers.push_back(new ColladaSkin(skin));
			modifiers.push_back(new ColladaMorph(morph));
		}
		else
		{
			modifiers.push_back(new ColladaMorph(morph));
			modifiers.push_back(new ColladaSkin(skin));
		}
	}
	else if (morph.IsResolved()) modifiers.push_back(new ColladaMorph(morph));
	else if (skin.IsResolved()) modifiers.push_back(new ColladaSkin(skin));
}

bool ControllerExporter::IsSkinned(Object* obj)
{
	ColladaSkin s;
	s.Resolve(obj);
	return s.IsResolved();
}

bool ControllerExporter::DoOverrideXRefExport(Object* obj)
{
	ColladaSkin s; ColladaMorph m;
	s.Resolve(obj); m.Resolve(obj);
	return (s.IsResolved() || m.IsResolved());
}

void ControllerExporter::ReactivateSkins()
{
	// Iterate over all the FCollada controllers and fill them in.
	size_t controllerCount = CDOC->GetControllerLibrary()->GetEntityCount();
	for (size_t i = 0; i < controllerCount; ++i)
	{
		FCDController* controller = CDOC->GetControllerLibrary()->GetEntity(i);
		ControllerInfo* info = (ControllerInfo*) controller->GetUserHandle();
		if (info != NULL)
		{
			info->modifier->EnableModInViews();
			info->modifier->EnableModInRender();
			info->modifier->EnableMod();
		}
	}
}

// ST - COLLADA MODIFIERS moved to seperate file
