/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "ColladaExporter.h"
#include "ControllerExporter.h"
#include "GeometryExporter.h"
#include "NodeExporter.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDController.h"
#include "FCDocument/FCDControllerInstance.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDGeometryMesh.h"
#include "FCDocument/FCDGeometrySource.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDSkinController.h"

//
// ControllerExporter
//

ControllerExporter::ControllerExporter(ColladaExporter* base)
:	EntityExporter(base)
{
}

ControllerExporter::~ControllerExporter()
{
}

// Entity export.
FCDEntity* ControllerExporter::ExportController(FBModel* node, FBGeometry* geometry)
{
	// Attempt to export the mesh.
	FCDEntity* colladaTarget = GEOM->ExportGeometry(geometry);
	if (colladaTarget == NULL || !colladaTarget->HasType(FCDGeometry::GetClassType())) return NULL;

	// Don't support skins on sampled meshes.
	if (!geometry->Is(FBMesh::TypeInfo)) return colladaTarget;

	// Temporarily set the deformation flag.
	bool isDeformable = node->IsDeformable;
	node->IsDeformable = true;
	FBCluster cluster(node);
	if (cluster.LinkGetCount() > 0)
	{
		colladaTarget = (FCDController*) ExportSkinController(node, cluster, colladaTarget);
	}
	node->IsDeformable = isDeformable;
	return colladaTarget;
}

// Instance export.
void ControllerExporter::ExportControllerInstance(FBModel* node, FCDControllerInstance* colladaInstance, FCDEntity* colladaEntity)
{
	// Retrieve the FCollada controller.
	if (!colladaEntity->HasType(FCDController::GetClassType())) return;
	FBCluster cluster(node);

	// Fill in the joint sub-id list.
	int jointCount = cluster.LinkGetCount();
	for (int i = 0; i < jointCount; ++i)
	{
		FBModel* joint = cluster.LinkGetModel(i);
		if (joint != NULL)
		{
			FCDSceneNode* colladaNode = NODE->ExportNode(NULL, joint); // NULL parent means don't instantiate.
			colladaInstance->AddJoint(colladaNode);
		}
	}
	colladaInstance->CalculateRootIds();
}

FCDEntity* ControllerExporter::ExportSkinController(FBModel* node, FBCluster& cluster, FCDEntity* colladaTarget)
{
	// Retrieve the mesh object's bind pose.
	FBPose* bindPose = FindBindPose(node); // NULL bindPose is valid.
	if (!colladaTarget->HasType(FCDGeometry::GetClassType())) return colladaTarget;
	FCDGeometry* colladaGeometry = (FCDGeometry*) colladaTarget;

	// Create the FCollada controller entity and its skin.
	FCDController* colladaController = CDOC->GetControllerLibrary()->AddEntity();
	ExportEntity(colladaController, &cluster);
	FCDSkinController* colladaSkin = colladaController->CreateSkinController();
	colladaSkin->SetTarget(colladaTarget);
	
	// Find the skin mesh bind pose.
	int nodeCount = 0;
	if (bindPose != NULL)
	{
		nodeCount = bindPose->GetNodeCount();
		FBMatrix skinBind;
		for (int j = 0; j < nodeCount; ++j)
		{
			if (node == bindPose->GetNodeObject(j))
			{
				skinBind = bindPose->GetNodeMatrix(j);
				break;
			}
		}
		
		colladaSkin->SetBindShapeTransform(ToFMMatrix44(skinBind));
	}

	// Check for a pivot on the controller.
	const FMMatrix44& bindShapeTM = colladaSkin->GetBindShapeTransform();
	FMVector3 pivotTranslation, pivotRotation, pivotScale;
	NODE->ExtractPivot(node, pivotTranslation, pivotRotation, pivotScale);
	FMMatrix44 pivotTM;
	pivotTM.Recompose(pivotScale, pivotRotation * FMath::DegToRad(1.0f), pivotTranslation);
	colladaSkin->SetBindShapeTransform(bindShapeTM * pivotTM);

	// Retrieve the mesh and pre-size the influence.
	FCDGeometryMesh* colladaMesh = colladaGeometry->GetMesh();
	size_t vertexCount = colladaMesh->GetVertexSource(0)->GetValueCount();
	colladaSkin->SetInfluenceCount(vertexCount);

	// Fill in the joint and influence information.
	int jointCount = cluster.LinkGetCount();
	for (int i = 0; i < jointCount; ++i)
	{
		cluster.ClusterBegin(i);

		FBModel* joint = cluster.LinkGetModel(i);
		if (joint == NULL) continue;

		// Add this joint to the skin controller.
		FCDSceneNode* colladaNode = NODE->ExportNode(NULL, joint); // NULL parent means don't instantiate.
		fm::string subId = FCDObjectWithId::CleanSubId((char*) joint->Name);
		colladaNode->SetSubId(subId);
		FMMatrix44 jointBindInverted(FMMatrix44::Identity);
		bool attemptRecompose = true;
		if (bindPose != NULL)
		{
			for (int j = 0; j < nodeCount; ++j)
			{
				if (joint == bindPose->GetNodeObject(j))
				{
					FBMatrix mm = bindPose->GetNodeMatrix(j);
					jointBindInverted = ToFMMatrix44(mm).Inverted();
					attemptRecompose = false;
					break;
				}
			}
		}
		if (attemptRecompose)
		{
			// Support cluster bind-poses.
			FBVector3d t, r, s;
			cluster.VertexGetTransform(t, r, s);
			jointBindInverted.Recompose(ToFMVector3(s), ToFMVector3(r) * FMath::DegToRad(1.0f), ToFMVector3(t));
			jointBindInverted *= bindShapeTM.Inverted();
		}
		colladaSkin->AddJoint(subId, jointBindInverted);

		int influenceCount = cluster.VertexGetCount();	
		for (int j = 0; j < influenceCount; ++j) 
		{
			// Retrieve the controller per-vertex influence structure and add this influence.
			int vertex = cluster.VertexGetNumber(j);
			float weight = (float) cluster.VertexGetWeight(j);
	
			// Look for duplicates: strangely MB allows for those (?!?)
			FCDSkinControllerVertex* colladaInfluence = colladaSkin->GetVertexInfluence(vertex);
			size_t pairCount = colladaInfluence->GetPairCount(), k = 0;
			for (; k < pairCount; ++k)
			{
				if (colladaInfluence->GetPair(k)->jointIndex == i)
				{
					colladaInfluence->GetPair(k)->weight += weight;
					break;
				}
			}
			if (k == pairCount) colladaInfluence->AddPair(i, weight);
		}
		cluster.ClusterEnd(true);
	}

	// Add the non-skinned influences.
	for (size_t i = 0; i < vertexCount; ++i)
	{
		FCDSkinControllerVertex* colladaInfluence = colladaSkin->GetVertexInfluence(i);
		float total = 0.0f;
		size_t pairCount = colladaInfluence->GetPairCount();
		for (size_t j = 0; j < pairCount; ++j) total += colladaInfluence->GetPair(j)->weight;
		if (total < 0.99f) colladaInfluence->AddPair(-1, 1.0f - total);
	}

	return colladaController;
}

FBPose* ControllerExporter::FindBindPose(FBModel* node)
{
	// Find the bind pose object for this model.
	FBScene* scene = NODE->GetScene();
	int poseCount = scene->Poses.GetCount();
	for (int i = 0; i < poseCount; ++i)
	{
		// Look for bind-poses, rather than animation poses.
		FBPose* pose = scene->Poses[i];
		if (pose->Type != kFBBindPose) continue;

		int nodeCount = pose->GetNodeCount();
		for (int j = 0; j < nodeCount; ++j)
		{
			// Only one bind pose should contain this mesh node?
			if (pose->GetNodeObject(j) == node) return pose;
		}
	}
	return NULL;
}

