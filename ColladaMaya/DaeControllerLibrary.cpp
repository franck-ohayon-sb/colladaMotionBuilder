/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"

// COLLADA Controller Library
//
#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MFnSet.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnMesh.h>
#include <maya/MFnPointArrayData.h>
#include <maya/MFnWeightGeometryFilter.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MItGeometry.h>
#include <maya/MItMeshVertex.h>
#include <maya/MItSelectionList.h>
#include <maya/MSelectionList.h>
#include <maya/MFnComponentListData.h>
#include <maya/MFnSingleIndexedComponent.h>

#include "DaeGeometry.h"
#include "DaeTransform.h"
#include "CExportOptions.h"
#include "CMeshExporter.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "TranslatorHelpers/CShaderHelper.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDController.h"
#include "FCDocument/FCDControllerInstance.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDGeometryInstance.h"
#include "FCDocument/FCDGeometryMesh.h"
#include "FCDocument/FCDGeometryPolygonsTools.h"
#include "FCDocument/FCDGeometrySource.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDMorphController.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDSkinController.h"
#include "FCDocument/FCDExtra.h"

#define WEIGHT_TOLERANCE 0.001f

#if MAYA_API_VERSION >= 650
#define Weight double
#define WeightArray MDoubleArray
#else
#define Weight float
#define WeightArray MFloatArray
#endif

// Animation Conversion Functions
// 

DaeControllerLibrary::DaeControllerLibrary(DaeDoc* _doc)
:	doc(_doc), boneCounter(0)
{
}

DaeControllerLibrary::~DaeControllerLibrary()
{
	importedMorphControllers.clear();
	skinControllers.clear();
	doc = NULL;
}

void DaeControllerLibrary::Import()
{
	FCDControllerLibrary* library = CDOC->GetControllerLibrary();
	for (size_t i = 0; i < library->GetEntityCount(); ++i)
	{
		FCDController* controller = library->GetEntity(i);
		if (controller->GetUserHandle() == NULL) ImportController(controller);
	}
}

// Create a controller with geometry
DaeEntity* DaeControllerLibrary::ImportController(FCDController* colladaController)
{
	// Verify that we haven't imported this controller already
	if (colladaController->GetUserHandle() != NULL)
	{
		return (DaeEntity*) colladaController->GetUserHandle();
	}

	// Get the child element's type
	bool isSkinController = colladaController->IsSkin();
	bool isMorphController = colladaController->IsMorph();
	if (!isMorphController && !isSkinController) return NULL;

	// Retrieve the target entity
	FCDEntity* targetEntity = NULL;
	if (isSkinController)
	{
		FCDSkinController* skinController = colladaController->GetSkinController();
		targetEntity = skinController->GetTarget();
	}
	else if (isMorphController)
	{
		FCDMorphController* morphController = colladaController->GetMorphController();
		targetEntity = morphController->GetBaseTarget();
	}
	if (targetEntity == NULL) return NULL;

	// Create the connection object for this controller
	DaeController* controller = new DaeController(doc, colladaController, MObject::kNullObj);
	if (isSkinController) skinControllers.push_back(controller);
	else if (isMorphController) importedMorphControllers.push_back(controller);

	// Create the target. The target can be a geometry or another controller!
	FCDGeometry* baseMesh = NULL;
	if (targetEntity->GetObjectType().Includes(FCDGeometry::GetClassType()))
	{
		if (isMorphController)
		{
			// clone the base geometry if it is the same as any of the targets
			FCDGeometry* baseGeometry = (FCDGeometry*) targetEntity;

			FCDMorphController* morphController = colladaController->GetMorphController();
			size_t morphTargetCount = morphController->GetTargetCount();
			for (size_t m = 0; m < morphTargetCount; ++m)
			{
				FCDMorphTarget* morphTarget = morphController->GetTarget(m);
				if (morphTarget->GetGeometry() == baseGeometry)
				{
					FCDGeometry* clonedBaseGeometry = baseGeometry->GetDocument()->GetGeometryLibrary()->AddEntity();
					baseGeometry->Clone(clonedBaseGeometry);
					colladaController->GetMorphController()->SetBaseTarget(clonedBaseGeometry);
					targetEntity = clonedBaseGeometry;
					break;
				}
			}
		}
		baseMesh = (FCDGeometry*) targetEntity;
		controller->target = doc->GetGeometryLibrary()->ImportGeometry(baseMesh);
	}
	else if (targetEntity->GetObjectType().Includes(FCDController::GetClassType()))
	{
		baseMesh = ((FCDController*) targetEntity)->GetBaseGeometry();
		controller->target = ImportController((FCDController*) targetEntity);
	}
	if (controller->target == NULL) return NULL;
	else if (controller->target->GetNode().isNull())
	{
		MGlobal::displayError(MString("Controller's target failed to load properly: ") + targetEntity->GetDaeId());
		return controller;
	}

	// Create the controller itself, tied on the target
	MObject controllerNode;
	if (isMorphController)
	{
		controllerNode = AttachMorphController(controller, baseMesh);
	}
	else if (isSkinController)
	{
		controllerNode = AttachSkinController(controller);
	}

	CDagHelper::SetPlugValue(controller->GetNode(), "visibility", false);
	doc->GetEntityManager()->ImportEntity(controllerNode, controller);
	return controller;
}

void DaeControllerLibrary::InstantiateController(DaeEntity* sceneNode, FCDGeometryInstance* colladaInstance, FCDController* colladaController)
{
	DaeController* controller = (DaeController*) colladaController->GetUserHandle();
	if (controller == NULL) controller = (DaeController*) ImportController(colladaController);
	if (controller == NULL) return;

	FCDGeometry* colladaGeometry = colladaController->GetBaseGeometry();

	// Set-up the skin's bind shape transformation
	bool oneSkinAlready = false;
	while (colladaController != NULL)
	{
		if (!oneSkinAlready && colladaController->IsSkin())
		{
			// Retrieve the skin controller and the transform elements.
			FCDSkinController* skinController = colladaController->GetSkinController();
			DaeTransform* transform = sceneNode->transform;
			if (skinController == NULL || transform == NULL) return;

			LinkSkinControllerInstance((FCDControllerInstance*)colladaInstance, skinController, controller);

			// Retrieve the skin's bind shape transformation and the current node transformation
			MMatrix bindShapeMatrix = MConvert::ToMMatrix(skinController->GetBindShapeTransform());
			MFnTransform transformFn(transform->GetTransform());
			MMatrix currentTransformMatrix = transformFn.transformationMatrix();

			// The bind shape transformation is in world-space, multiplying it with
			// the local transformation will return the original Maya node transform.
			FCDSceneNode* parentNode = (FCDSceneNode*) &*(sceneNode->entity);
			MMatrix localTransformation = MConvert::ToMMatrix(parentNode->ToMatrix());
			localTransformation = bindShapeMatrix * localTransformation;
			transformFn.set(MTransformationMatrix(localTransformation));

			transformFn.findPlug("translate").setLocked(true);
			transformFn.findPlug("rotate").setLocked(true);
			transformFn.findPlug("rotateOrder").setLocked(true);
			transformFn.findPlug("scale").setLocked(true);
			oneSkinAlready = true;
		}

		colladaController = DynamicCast<FCDController>(colladaController->GetBaseTarget());
	}

	// Parent the ctrl to this scene node
	controller->Instantiate(sceneNode);

	// Instantiate the polygon materials
	if (colladaGeometry != NULL)
	{
		doc->GetGeometryLibrary()->InstantiateGeometryMaterials(colladaInstance, colladaGeometry, controller, sceneNode);
	}
}

MObject DaeControllerLibrary::AttachMorphController(DaeController* controller, FCDGeometry* baseMesh)
{
	MStatus status;

	// Attach a blend shape controller to the specified target
	FCDController* colladaController = (FCDController*) &*(controller->entity);
	FCDMorphController* colladaMorph = colladaController->GetMorphController();
	DaeEntity* target = controller->target;
	
	// Read in the target information
	uint deformerCount = (uint) colladaMorph->GetTargetCount();

	// Create the blend shape
	CDagHelper::SetPlugValue(target->GetNode(), "visibility", true);
	MFnDependencyNode targetFn(target->GetNode());
	MString targetNodeName = targetFn.name(); // Need to force a unique name, because of a broken implementation of MFnBlendShapeDeformer::create.
	targetFn.setName(MString("Base__") + targetNodeName);
	MObjectArray deformedObjects(1, targetFn.object());

	MStatus rc;
	MFnBlendShapeDeformer deformerFn;
	deformerFn.create(deformedObjects, MFnBlendShapeDeformer::kLocalOrigin, MFnBlendShapeDeformer::kNormal, &rc);
	if (rc != MStatus::kSuccess || deformerFn.object() == MObject::kNullObj)
	{
		MGlobal::displayError(MString("Unable to create deformer object for <morph> controller: ") + colladaController->GetDaeId());
		return MObject::kNullObj;
	}
	deformerFn.setName(colladaController->GetName().c_str());
	MPlug topologyCheckPlug = deformerFn.findPlug("tc"); // "topologyCheck"
	topologyCheckPlug.setValue(true);
	targetFn.setName(targetNodeName); // Reset back the target's name.

	// Retrieve the new target geometry path.
	// Hack it into the scene element, to support skinned, morphed characters
	MPlug p = deformerFn.findPlug("outputGeometry");
	p = p.elementByPhysicalIndex(0);
	MObject outputGeometryObject = CDagHelper::GetNodeConnectedTo(p);
	controller->SetNode(outputGeometryObject);

	MObject newTargetObj = MObject::kNullObj;
	MStatus myStatus;
	MPlug tweakPlug = targetFn.findPlug("tweakLocation", &myStatus);
	if (myStatus)
	{
		MObject tweakNode = CDagHelper::GetNodeConnectedTo(tweakPlug);
		if (tweakNode != MObject::kNullObj)
		{
			MFnDependencyNode tweakDagNode(tweakNode, &myStatus);
			if (myStatus)
			{
				MPlug tweakInputPlug = tweakDagNode.findPlug("input", &myStatus); 
				if (myStatus)
				{
					// we only have 1 base geometry, so this is valid
					MPlug tweakInputPlug1 = tweakInputPlug.elementByPhysicalIndex(0);
					MPlug inputGeometryArrayPlug = CDagHelper::GetChildPlug(tweakInputPlug1, "ig", &myStatus);
					if (myStatus)
					{
						MObject groupNodeObj = CDagHelper::GetNodeConnectedTo(inputGeometryArrayPlug);
						MFnDependencyNode groupNode(groupNodeObj, &myStatus);
						if (myStatus)
						{
							MPlug groupInputGeometryPlug = groupNode.findPlug("inputGeometry", &myStatus);
							if (myStatus)
							{
								MObject geometryNodeObj = CDagHelper::GetNodeConnectedTo(groupInputGeometryPlug);
								MFnDependencyNode geometryNode(geometryNodeObj, &myStatus);
								if (myStatus)
								{
									MPlug targetColladaPlug = targetFn.findPlug("collada");
									MPlug colladaPlug;
									if (CDagHelper::GetPlugConnectedTo(targetColladaPlug, colladaPlug))
									{
										// disconnect the collada plug and attach to the new node
										CDagHelper::Disconnect(colladaPlug, targetColladaPlug);
										doc->GetEntityManager()->ImportEntity(geometryNodeObj, target);
										newTargetObj = geometryNodeObj;

										// remove the Base___
										MString geomNodeName = geometryNode.name();
										geomNodeName = geomNodeName.substring(6, geomNodeName.length());
										geometryNode.setName(geomNodeName);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	// set the weights and connect any weight animations
	MPlug weightArrayPlug = deformerFn.findPlug("w");
	MPlug inputTargetArrayPlug = deformerFn.findPlug("it"); // "inputTarget"
	MPlug inputTargetPlug = inputTargetArrayPlug.elementByLogicalIndex(0);
	MPlug inputTargetGroupArrayPlug = CDagHelper::GetChildPlug(inputTargetPlug, "itg"); // "inputTargetGroup"
	for (uint i = 0; i < deformerCount; ++i)
	{
		FCDMorphTarget* colladaBlendShape = colladaMorph->GetTarget(i);
		float& targetWeight = colladaBlendShape->GetWeight();
		FCDGeometry* blendShape = colladaBlendShape->GetGeometry();
		if (!blendShape->IsMesh()) continue; // No support yet for non-mesh blend shapes.
		FCDGeometryMesh* blendShapeMesh = blendShape->GetMesh();
		if (blendShapeMesh->GetPolygonsCount() == 0)
		{
			// Morph targets without tessellation are imported as component/vertex lists.
			MPlug inputTargetGroupPlug = inputTargetGroupArrayPlug.elementByLogicalIndex(i);
			MPlug inputTargetInputArrayPlug = CDagHelper::GetChildPlug(inputTargetGroupPlug, "iti"); // "inputTargetInput"
			MPlug inputTargetInputPlug = inputTargetInputArrayPlug.elementByLogicalIndex(6000); // 6000 represent a [0,1] range?
			MPlug targetVertexListPlug = CDagHelper::GetChildPlug(inputTargetInputPlug, "ipt"); // "inputPointsTarget"
			MPlug targetComponentListPlug = CDagHelper::GetChildPlug(inputTargetInputPlug, "ict"); // "inputComponentTarget"
			ImportMorphTarget(targetVertexListPlug, targetComponentListPlug, baseMesh, blendShapeMesh);
		}
		else
		{
			// If tessellation is present: create the mesh DAG node.
			DaeGeometry* geometry = (DaeGeometry*) doc->GetGeometryLibrary()->ImportGeometry(blendShape);
			if (geometry != NULL && !geometry->GetNode().isNull())
			{
				MFnDependencyNode geometryTargetFn(geometry->GetNode());

				// Maya requires visible geometry in order to add it as a blend shape to a deformed mesh, but this geometry
				// may not be instanced (or maybe willingly invisible): retrieve the old visibility flag and reset it later.
				bool oldVisibility; CDagHelper::GetPlugValue(geometryTargetFn.object(), "visibility", oldVisibility);
				MString oldName = geometryTargetFn.name();
				geometryTargetFn.setName(MString("Target") + i + MString("__") + oldName);
				CDagHelper::SetPlugValue(geometry->GetNode(), "visibility", true);

				// Add the morph target to the Maya blend shape deformer.
				deformerFn.addTarget(target->GetNode(), i, geometryTargetFn.object(), 1.0);

				// Reset the visibility/name of the target mesh node.
				if (!oldVisibility) CDagHelper::SetPlugValue(geometryTargetFn.object(), "visibility", false);
				geometryTargetFn.setName(oldName);
			}
		}
	
		FCDAnimated* animatedWeight = colladaBlendShape->GetWeight().GetAnimated();
		MPlug weightPlug = weightArrayPlug.elementByLogicalIndex(i);
		weightPlug.setValue(targetWeight);
		ANIM->AddPlugAnimation(weightPlug, animatedWeight, kSingle); // "weight"
	}

	if (newTargetObj != MObject::kNullObj)
	{
		CDagHelper::SetPlugValue(newTargetObj, "visibility", false);
		CDagHelper::SetPlugValue(newTargetObj, "intermediateObject", false);
		target->SetNode(newTargetObj);
	}

	return deformerFn.object();
}


MObject DaeControllerLibrary::AttachSkinController(DaeController* controller)
{
	MStatus status;
	DaeEntity* target = controller->target;
	MFnMesh originalMeshFn(target->GetNode(), &status);
	if (status)
	{
		// Rename original mesh
		MString meshName = originalMeshFn.name();
		originalMeshFn.setName(meshName + "Orig");
		target->SetNode(originalMeshFn.object());
		return AttachSkinController(controller, meshName);
	}
	else return MObject::kNullObj;
}

// Attach a skin controller to the specified target
MObject DaeControllerLibrary::AttachSkinController(DaeController* controller, MString meshName)
{
	MStatus status;

	FCDController* colladaController = (FCDController*) &*(controller->entity);
	FCDSkinController* colladaSkin = colladaController->GetSkinController();
	FCDGeometry* colladaGeometry = colladaController->GetBaseGeometry();
	DaeEntity* target = controller->target;

	// Find the target geometry
	if (colladaGeometry == NULL || colladaSkin == NULL) return MObject::kNullObj;
	DaeGeometry* geometry = (DaeGeometry*) colladaGeometry->GetUserHandle();
	if (geometry == NULL) return MObject::kNullObj;
	MFnMesh originalMeshFn(target->GetNode(), &status);
	if (!status) return MObject::kNullObj;

	// need to make sure that the geometry does not have locked normals so they are deformed properly
	// NOTE: Maya is smart enough to maintain the hard edges after unlocking
	int verticesCount = originalMeshFn.numVertices();
	MIntArray vertexList(verticesCount);
	for (int i = 0; i < verticesCount; i++)
	{
		vertexList[i] = i;
	}
	originalMeshFn.unlockVertexNormals(vertexList);

	// Here is most of the array data we need to create our skin deformer
	FMMatrix44 bindShapeMatrix = colladaSkin->GetBindShapeTransform();
	uint influenceCount = (uint) colladaSkin->GetJointCount();
	if (colladaSkin->GetInfluenceCount() == 0 || influenceCount == 0)
	{
		MGlobal::displayError(MString("Unable to find weights or joints for controller: ") + colladaController->GetDaeId());
		return MObject::kNullObj;
	}
	
	// Add a skinCluster history node and a tweak history node
	MFnSkinCluster skinClusterFn;
	MObject skinCluster = skinClusterFn.create("skinCluster");
	skinClusterFn.setName(colladaController->GetName().c_str());
	CDagHelper::SetPlugValue(skinCluster, "maxInfluences", (int)influenceCount);
	CDagHelper::SetPlugValue(skinCluster, "useComponentsMatrix", true);
	MObject parentTransform = originalMeshFn.parent(0);
	MFnDependencyNode tweakNodeFn;
	MObject tweakNode = tweakNodeFn.create("tweak");

	// Create the empty skinned mesh node
	uint meshVertexCount = geometry->points.length();
	MFnMesh skinnedMeshFn;
	MObject skinnedMesh = skinnedMeshFn.create(0, 0, MPointArray(), MIntArray(), MIntArray(), parentTransform, &status);
	controller->SetNode(skinnedMeshFn.object());
	skinnedMeshFn.setName(meshName);
	
	// [GLaforte 23-09-2005] COLLADA doesn't recognize that scale factors should affect only one bone.

	// Connect the tweak node to the skin cluster using a groupId/groupParts structure
	MPlug outputGeometryPlug = tweakNodeFn.findPlug("outputGeometry");
	outputGeometryPlug = outputGeometryPlug.elementByLogicalIndex(outputGeometryPlug.numElements());
	CDagHelper::GroupConnect(outputGeometryPlug, skinCluster, skinnedMesh);

	// Connect the input mesh to the tweak node
	MPlug worldSpaceMeshPlug = originalMeshFn.findPlug("worldMesh");
	CDagHelper::GroupConnect(worldSpaceMeshPlug, tweakNode, skinnedMesh);

	CDagHelper::SetPlugValue(target->GetNode(), "intermediateObject", true);

	// Connect the tweak to the skinned mesh
	MPlug tweakPlug = tweakNodeFn.findPlug("vlist");
	tweakPlug = tweakPlug.elementByLogicalIndex(0);
	tweakPlug = CDagHelper::GetChildPlug(tweakPlug, "vt"); // "vertex"
	tweakPlug = tweakPlug.elementByLogicalIndex(0);

	CDagHelper::Connect(tweakPlug, skinnedMesh, "tweakLocation");

	// Connect the skin cluster to the skinned mesh
	outputGeometryPlug = skinClusterFn.findPlug("outputGeometry", &status);
	outputGeometryPlug = outputGeometryPlug.elementByLogicalIndex(0, &status);
	CDagHelper::Connect(outputGeometryPlug, skinnedMeshFn.object(), "inMesh");

	MMatrix bindShapeMatrixCopy = MConvert::ToMMatrix(bindShapeMatrix);
	CDagHelper::SetPlugValue(skinClusterFn.object(), "gm", bindShapeMatrixCopy);
	skinnedMeshFn.updateSurface();
	skinnedMeshFn.syncObject();

	// Now, at this point we have an unweighted skin cluster. We now need 
	// to override those default values with any explicit weights and bind 
	// position/pose information that's been included
	//
	MPlug globalWeightPlug = skinClusterFn.findPlug("weightList");
	CDagHelper::SetArrayPlugSize(globalWeightPlug, meshVertexCount);
	MPlug normalizeWeightsPlug = skinClusterFn.findPlug("normalizeWeights");
	int weightListPlugIndex = CDagHelper::GetChildPlugIndex(globalWeightPlug, "w"); // "weights"
	normalizeWeightsPlug.setValue(false);
	uint matchCount = (uint) colladaSkin->GetInfluenceCount();
	for (uint i = 0; i < matchCount; ++i)
	{
		MPlug vertexWeightPlug = globalWeightPlug.elementByLogicalIndex(i);
		MPlug weightListPlug = vertexWeightPlug.child(weightListPlugIndex);
		CDagHelper::SetArrayPlugSize(weightListPlug, influenceCount);

		// Enforce the given weights.
		const FCDSkinControllerVertex* influence = colladaSkin->GetVertexInfluence(i);
		uint pairCount = (uint) influence->GetPairCount();
		for (uint j = 0; j < pairCount; ++j)
		{
			skinClusterFn.setLocked(false);
			const FCDJointWeightPair* pair = influence->GetPair(j);
			MPlug w = weightListPlug.elementByLogicalIndex(pair->jointIndex);
			w.setValue(pair->weight);
		}
	}

	originalMeshFn.updateSurface();
	originalMeshFn.syncObject();

	return skinClusterFn.object();
}

bool DaeControllerLibrary::ObjectMatchesInstance(const DaeEntityList& influences, const MDagPathArray& paths)
{
	// Redundant error check
	size_t numInfluences = influences.size();
	if (numInfluences != paths.length())
		return false;

	// compare the nodes to ensure equality
	for (uint i = 0; i < numInfluences; i++)
	{
		MObject anInfluence = paths[i].node();
		if (anInfluence != influences[i]->GetNode())
			return false;
	}
	return true;
}

//
// Init the maya skinned Object with its skin instance data.
//
void DaeControllerLibrary::LinkSkinControllerInstance(FCDControllerInstance* instance, FCDSkinController* skinController, DaeController* controller)
{
	MStatus stat;
	MFnMesh skinnedMeshFn(controller->GetNode());
	MPlug inMeshPlug = skinnedMeshFn.findPlug("inMesh", &stat);

	if (stat == MS::kSuccess && inMeshPlug.isConnected())
	{
		
		// walk the tree of stuff upstream from this plug
		MItDependencyGraph dgIt(inMeshPlug,
								MFn::kInvalid,
								MItDependencyGraph::kUpstream,
								MItDependencyGraph::kDepthFirst,
								MItDependencyGraph::kPlugLevel,
								&stat);

		if (MS::kSuccess == stat)
		{
			dgIt.disablePruningOnFilter();

			for (; ! dgIt.isDone(); dgIt.next())
			{
				MObject thisNode = dgIt.thisNode();

				// go until we find a skinCluster
				if (thisNode.apiType() == MFn::kSkinClusterFilter)
				{
					MFnSkinCluster skinCluster(thisNode);

					// TODO: Cloning, and all that rubbish
					size_t influenceCount = instance->GetJointCount();

					// Load our influences
					DaeEntityList jointElementList;
					jointElementList.reserve(influenceCount);
					for (size_t j = 0; j < influenceCount; ++j)
					{
						DaeEntity* e = doc->GetSceneGraph()->ImportNode(instance->GetJoint(j));
						jointElementList.push_back(e);
					}

					// If we already have influences, ensure they are correct.
					MDagPathArray infPaths;
					size_t existingInfluenceCount = skinCluster.influenceObjects(infPaths, NULL);
					if (influenceCount == existingInfluenceCount)					
					{
						if (!ObjectMatchesInstance(jointElementList, infPaths))
						{
							AttachSkinController(controller, skinnedMeshFn.name());
							// Start Again here...
							LinkSkinControllerInstance(instance, skinController, controller);
							// In essence, we are a new controller instance.
							// All the objects we reference have been reset,
							// so reset our instance count too.
							controller->instanceCount = 0;
						}
						return;
					}

					// Else, continue hooking everything up
					for (uint j = 0; j < influenceCount; ++j)
					{
						if (jointElementList[j] != NULL)
						{
							MFnDagNode jointFn(jointElementList[j]->GetNode());
							MPlug jointMatrixPlug = jointFn.findPlug("worldMatrix");
							jointMatrixPlug = jointMatrixPlug.elementByLogicalIndex(0);
							CDagHelper::ConnectToList(jointMatrixPlug, skinCluster.object(), "matrix");
						}
					}

					// Finally, setup the bind pose on the joints themselves (noting that Maya only allows
					// any given joint to have one bind pose, so again, strictly speaking we should check
					// that this joint does not already have a bind pose, and if it does ... well, there's
					// no totally correct way to support this given we can't create new bones or we'll mess
					// up the skeletal structure).
					//
					for (uint j = 0; j < influenceCount; j++)
					{
						DaeEntity* e = jointElementList[j];
						if (e != NULL)
						{
							MMatrix bindPoseInverse = MConvert::ToMMatrix(skinController->GetJoint(j)->GetBindPoseInverse());
							CDagHelper::SetBindPoseInverse(e->GetNode(), bindPoseInverse);
						}
					}

					skinnedMeshFn.updateSurface();
					skinnedMeshFn.syncObject();

					// Only do this once
					break;
				}
			}
		}
	}
}

bool DaeControllerLibrary::HasController(const MObject& node)
{
	return HasSkinController(node) || HasMorphController(node);
}

bool DaeControllerLibrary::HasSkinController(const MObject& node)
{
	MStatus status;
	MPlug plug = MFnDependencyNode(node).findPlug("inMesh", &status);
	if (!status || !plug.isConnected()) return false;

	MItDependencyGraph dgIt(plug, MFn::kInvalid, MItDependencyGraph::kUpstream, MItDependencyGraph::kBreadthFirst,
			MItDependencyGraph::kNodeLevel, &status);
	if (status != MS::kSuccess) return false;

	dgIt.disablePruningOnFilter();
	while (!dgIt.isDone())
	{
		MObject thisNode = dgIt.thisNode();
		if (thisNode.hasFn(MFn::kSkinClusterFilter) || thisNode.hasFn(MFn::kJointCluster))
		{
			return true;
		}

		dgIt.next();
	}
	return false;
}

bool DaeControllerLibrary::HasMorphController(const MObject& node)
{
	MPlug plug = MFnDependencyNode(node).findPlug("inMesh");
	if (plug.isConnected())
	{
		MItDependencyGraph dgIt(plug, MFn::kInvalid, MItDependencyGraph::kUpstream, MItDependencyGraph::kDepthFirst, MItDependencyGraph::kPlugLevel);

		dgIt.disablePruningOnFilter(); 
		for (; ! dgIt.isDone(); dgIt.next())
		{
			MObject thisNode = dgIt.thisNode();
			if (thisNode.hasFn(MFn::kBlendShape)) return true;
		}
	}

	return false;
}

// Export any controller associated with the given plug. Returns true
// if a controller was found.
//
struct DaeControllerStackItem
{
	bool isSkin;
	MObject skinControllerNode;
	MObjectArray morphControllerNodes;
	Int32List nodeStates;
};
typedef fm::pvector<DaeControllerStackItem> DaeControllerStack;

struct DaeControllerMeshItem
{
	MObject mesh;
	bool isIntermediate;
	bool isVisible;
};
typedef fm::vector<DaeControllerMeshItem> DaeControllerMeshStack;

void DaeControllerLibrary::ExportJoint(FCDSceneNode* sceneNode, const MDagPath& dagPath, bool isLocal)
{
	FUAssert(sceneNode != NULL, return);

	sceneNode->SetJointFlag(true);

	// Export the segment-scale-compensate flag.
	bool segmentScaleCompensate;
	CDagHelper::GetPlugValue(dagPath.transform(), "ssc", segmentScaleCompensate);
	FCDETechnique* mayaTechnique = sceneNode->GetExtra()->GetDefaultType()->AddTechnique(DAEMAYA_MAYA_PROFILE);
	mayaTechnique->AddParameter(DAEMAYA_SEGMENTSCALECOMP_PARAMETER, segmentScaleCompensate); // not animatable.
}

DaeEntity* DaeControllerLibrary::ExportController(FCDSceneNode* sceneNode, const MDagPath& dagPath, bool isSkin, 
												  bool instantiate)
{
	// Controllers are tricky: they cannot be on an animated node and in the case of skin controllers,
	// the bind-shape matrix must be taken out.
	DaeEntity* entity = ExportController(sceneNode, dagPath.node());
	if (entity == NULL) return NULL;

	FCDSceneNode* pivotNode = NULL;
	MFnTransform transformFn(dagPath.transform());

	if (isSkin)
	{
		// Calculate the number of visible children.
		uint visibleChildCount = 0;
		for (uint i = 0; i < transformFn.childCount(); ++i)
		{
			bool isVisible = false;
			MFnDagNode dagFn(transformFn.child(i));
			dagFn.findPlug("visibility").getValue(isVisible);
			if (!dagFn.isIntermediateObject() && (!CExportOptions::ExportInvisibleNodes() || isVisible)) 
				++visibleChildCount;
		}

		if (visibleChildCount > 1)
		{
			FCDSceneNode* parentNode = sceneNode->GetParent();
			FUAssert(parentNode != NULL, return NULL); // shouldn't happen.
			pivotNode = parentNode->AddChildNode();

			// Create a new node entity for the pivot.
			DaeEntity* pivotEntity = new DaeEntity(doc, pivotNode, dagPath.transform());

			// Process the previously imported scene node
			if (!pivotEntity->isOriginal)
			{
				// Take out the transforms: they are fair-game, for now.
				while (pivotNode->GetTransformCount() > 0) pivotNode->GetTransform(0)->Release();
				
				// Mark the instances/children to be able to later delete the unused ones.
				size_t pivotInstanceCount = pivotNode->GetInstanceCount();
				for (size_t pi = 0; pi < pivotInstanceCount; ++pi)
				{
					pivotNode->GetInstance(pi)->SetUserHandle((void*) 1);
				}
			}
			else
			{
				// Generate a new COLLADA id.
				pivotNode->SetDaeId(doc->DagPathToColladaId(dagPath).asChar());
			}

			pivotNode->SetName(MConvert::ToFChar(doc->DagPathToColladaName(dagPath)));
			pivotEntity->instanceCount = 1;
			doc->GetSceneGraph()->AddElement(pivotEntity);
			pivotEntity->transform = new DaeTransform(doc, pivotNode);
			pivotEntity->transform->SetTransform(transformFn.object());
			pivotEntity->transform->SetTransformation(transformFn.transformation());
		}
		else
		{
			// Reset the local transformation.
			DaeEntity* currentEntity = (DaeEntity*) sceneNode->GetUserHandle();
			currentEntity->transform->SetTransformation(transformFn.transformation());
		}
	}

	// Export the instance on the pivot, if it exists.
	if (pivotNode == NULL) pivotNode = sceneNode;
	if (instantiate) doc->GetSceneGraph()->ExportInstance(pivotNode, entity->entity);

	return entity;
}

DaeEntity* DaeControllerLibrary::ExportController(FCDSceneNode* sceneNode, const MObject& node)
{
	DaeControllerStack stack;
	DaeControllerMeshStack meshStack;

	// Iterate upstream finding all the nodes which affect the mesh.
	MStatus stat; 
	MPlug plug = MFnDependencyNode(node).findPlug("inMesh");
	if (plug.isConnected())
	{
		MItDependencyGraph dgIt(plug, MFn::kInvalid, MItDependencyGraph::kUpstream, MItDependencyGraph::kBreadthFirst, MItDependencyGraph::kNodeLevel, &stat);
		if (MS::kSuccess != stat) return NULL;

		dgIt.disablePruningOnFilter(); 
		for (; ! dgIt.isDone(); dgIt.next())
		{
			MObject thisNode = dgIt.thisNode();
			if (thisNode.hasFn(MFn::kSkinClusterFilter) || thisNode.hasFn(MFn::kJointCluster))
			{
				int nodeState;
				CDagHelper::GetPlugValue(thisNode, "nodeState", nodeState);

				// Append and disable the skin controller node.
				DaeControllerStackItem* item = new DaeControllerStackItem();
				item->isSkin = true;
				item->skinControllerNode = thisNode;
				item->nodeStates.push_back(nodeState);
				stack.push_back(item);

				CDagHelper::SetPlugValue(thisNode, "nodeState", 1); // pass-through.
			}
			else if (thisNode.hasFn(MFn::kBlendShape))
			{
				int nodeState;
				CDagHelper::GetPlugValue(thisNode, "nodeState", nodeState);

				// Check for subsequent, multiple blend shape deformers.
				if (stack.size() > 0 && !stack.back()->isSkin)
				{
					stack.back()->morphControllerNodes.append(thisNode);
				}
				else
				{
					DaeControllerStackItem* item = new DaeControllerStackItem();
					item->isSkin = false;
					item->morphControllerNodes.append(thisNode);
					stack.push_back(item);
				}

				stack.back()->nodeStates.push_back(nodeState);
				CDagHelper::SetPlugValue(thisNode, "nodeState", 1); // pass-through.
			}
			else if (thisNode.hasFn(MFn::kMesh))
			{
				// Queue up this mesh and set valid object parameters
				DaeControllerMeshItem item;
				item.mesh = thisNode;
				CDagHelper::GetPlugValue(thisNode, "io", item.isIntermediate);
				CDagHelper::GetPlugValue(thisNode, "visibility", item.isVisible);
				CDagHelper::SetPlugValue(thisNode, "io", false);
				CDagHelper::SetPlugValue(thisNode, "visibility", true);
				meshStack.push_back(item);
			}
		}
	}

	// At the end, export the base mesh
	DaeEntity* target = doc->GetGeometryLibrary()->ExportMesh(node);

	// Export the controller stack
	bool alreadyHasSkin = false; // We cannot express more than one skin per geometry instance...
	for (intptr_t i = stack.size() - 1; i >= 0; --i)
	{
		DaeControllerStackItem* item = stack[i];
		if (item->isSkin)
		{
			if (CExportOptions::ExportJointsAndSkin() && !alreadyHasSkin)
			{
				// Correctly avoid chained joint-clusters: only export the first joint cluster
				// which exports the subsequent joint-clusters with it.
				if (!item->skinControllerNode.hasFn(MFn::kJointCluster) || i == 0 || !stack[i-1]->isSkin
					|| !stack[i-1]->skinControllerNode.hasFn(MFn::kJointCluster))
				{
					target = ExportSkinController(item->skinControllerNode, MDagPath::getAPathTo(node), target);
					doc->GetEntityManager()->ExportEntity(item->skinControllerNode, target->entity);
					alreadyHasSkin = true;
				}
			}
		}
		else
		{
			target = ExportMorphController(sceneNode, item->morphControllerNodes, target);
			doc->GetEntityManager()->ExportEntity(item->morphControllerNodes[0], target->entity);
		}
	}

	// Reset all the intermediate mesh parameters.
	while (!meshStack.empty())
	{
		DaeControllerMeshItem& item = meshStack.back();

		CDagHelper::SetPlugValue(item.mesh, "io", item.isIntermediate);
		CDagHelper::SetPlugValue(item.mesh, "visibility", item.isVisible);

		meshStack.pop_back();
	}

	// Reset all the controller node states
	for (size_t i = 0; i < stack.size(); ++i)
	{
		DaeControllerStackItem* item = stack[i];
		if (item->isSkin)
		{
			CDagHelper::SetPlugValue(item->skinControllerNode, "nodeState", item->nodeStates.front());
		}
		else
		{
			for (uint j = 0; j < item->morphControllerNodes.length(); ++j)
			{
				CDagHelper::SetPlugValue(item->morphControllerNodes[j], "nodeState", item->nodeStates[j]);
			}
		}
		SAFE_DELETE(item);
	}
	stack.clear();
	
	if (target != NULL) target->SetNode(node);
	return target;
}

void DaeControllerLibrary::CompleteInstanceExport(FCDControllerInstance* colladaInstance, FCDController* colladaController)
{
	if (colladaController->IsSkin())
	{
		// Gather our required info.
		FCDSkinController* colladaSkin = colladaController->GetSkinController();
		DaeController*  daeController = (DaeController*)colladaController->GetUserHandle();
		
		// We don't preserve any <skeleton> information.
		colladaInstance->ResetJoints();

		uint count = daeController->influences.length();

		bool initColladaSkin = (colladaSkin->GetJointCount() != count);
		if (initColladaSkin) colladaSkin->SetJointCount(count);

		for (uint i = 0; i < count; ++i)
		{
			DaeEntity* element = doc->GetSceneGraph()->FindElement(daeController->influences[i]);
			if (element != NULL && element->entity != NULL && element->entity->GetObjectType() == FCDSceneNode::GetClassType())
			{
				FCDSceneNode* sceneNode = (FCDSceneNode*) &*(element->entity);
				colladaInstance->AddJoint(sceneNode);
				if (initColladaSkin)
				{
					if (sceneNode->GetSubId().empty())
					{
						FUSStringBuilder boneId("bone"); 
						boneId.append(boneCounter);
						boneCounter++;
						sceneNode->SetSubId(boneId.ToCharPtr());
					}
					FCDSkinControllerJoint* joint = colladaSkin->GetJoint(i);
					joint->SetBindPoseInverse(daeController->bindPoses[i]);
					joint->SetId(sceneNode->GetSubId());
				}
			}
#ifdef _DEBUG
			else
			{
				MString ss = daeController->influences[i].fullPathName();
				const char* tt = ss.asChar();
				FUFail(tt);
			}
#endif // _DEBUG
		}
		colladaInstance->CalculateRootIds();
	}
	else
	{
		// Nothing to export for morph instances.
	}
}

void DaeControllerLibrary::CompleteControllerExport()
{
	// Attach the skin controllers next
	for (DaeControllerList::iterator it = skinControllers.begin(); it != skinControllers.end(); ++it)
	{
		DaeController* c = (*it);
		FCDSkinController* colladaSkin = ((FCDController*) &*(c->entity))->GetSkinController();
		if (colladaSkin != NULL)
		{
			for (FCDControllerInstanceList::iterator itI = c->instances.begin(); itI != c->instances.end(); ++itI)
			{
				CompleteInstanceExport(*itI, (FCDController*) &*c->entity);
			}
		}
	}
	skinControllers.clear();
}

// As a first step, add all the necessary nodes that must be exported to a list in the scene graph.
void DaeControllerLibrary::AddForceNodes(const MDagPath& dagPath)
{
	MFnMesh meshFn(dagPath);

	// Iterate upstream finding all the nodes which affect the mesh.
	MStatus stat; 
	MPlug plug = meshFn.findPlug("inMesh");
	if (plug.isConnected())
	{
		MItDependencyGraph dgIt(plug, MFn::kInvalid, MItDependencyGraph::kUpstream, MItDependencyGraph::kDepthFirst, MItDependencyGraph::kPlugLevel, &stat);
		if (stat == MS::kSuccess)
		{
			dgIt.disablePruningOnFilter(); 
			for (; ! dgIt.isDone(); dgIt.next())
			{
				MObject thisNode = dgIt.thisNode();
				if (thisNode.apiType() == MFn::kSkinClusterFilter)
				{
					MFnSkinCluster clusterFn(thisNode);
					MDagPathArray jointPaths;
					clusterFn.influenceObjects(jointPaths, &stat);
					if (stat == MS::kSuccess)
					{
						uint count = jointPaths.length();
						for (uint i = 0; i < count; ++i) doc->GetSceneGraph()->AddForcedNode(jointPaths[i]);
					}
				}
				else if (thisNode.apiType() == MFn::kJointCluster)
				{
					MObject joint = CDagHelper::GetNodeConnectedTo(thisNode, "matrix");
					MDagPath jointPath = MDagPath::getAPathTo(joint);
					doc->GetSceneGraph()->AddForcedNode(jointPath);
				}
			}
		}
	}
}

// Export a skin/joint cluster
DaeEntity* DaeControllerLibrary::ExportSkinController(MObject controllerNode, MDagPath outputShape, DaeEntity* target)
{
	MFnMesh meshFn(outputShape);

	// Figure out which type of skin controller we currently have: mesh-centric or joint-centric
	MStatus status;
	bool isJointCluster = controllerNode.hasFn(MFn::kJointCluster);
	bool isSkinCluster = controllerNode.hasFn(MFn::kSkinClusterFilter);
	if (!isJointCluster && !isSkinCluster) return NULL;

	// Retrieve the instance information for this skinned character
	MFnGeometryFilter clusterFn(controllerNode);
	uint idx = clusterFn.indexForOutputShape(outputShape.node());
	if (idx == ~0u)
	{
		// Alternate method for retrieving the index?
		MPlug meshConnection = meshFn.findPlug("inMesh");
		MItDependencyGraph dgIt(meshConnection, MFn::kMesh, MItDependencyGraph::kUpstream, MItDependencyGraph::kBreadthFirst, MItDependencyGraph::kNodeLevel, &status);
		CHECK_MSTATUS_AND_RETURN(status, NULL);
		dgIt.disablePruningOnFilter(); 
		for (; ! dgIt.isDone(); dgIt.next())
		{
			MObject thisNode = dgIt.thisNode();
			idx = clusterFn.indexForOutputShape(thisNode);
			if (idx != ~0u)
			{
				outputShape = MDagPath::getAPathTo(thisNode);
				break;
			}
		}
	}

	// Create the COLLADA controller and its ColladaMaya equivalent.
	FCDController* colladaController = CDOC->GetControllerLibrary()->AddEntity();
	DaeController* newTarget = new DaeController(doc, colladaController, controllerNode);
	if (newTarget->isOriginal) colladaController->SetDaeId(target->entity->GetDaeId() + "-skin");

	// Initialize the COLLADA skin controller
	MString controllerName = doc->MayaNameToColladaName((isSkinCluster) ? clusterFn.name() : outputShape.partialPathName());
	colladaController->SetName(MConvert::ToFChar(controllerName));
	skinControllers.push_back(newTarget);
	newTarget->target = target;
	FCDSkinController* colladaSkin = colladaController->CreateSkinController();
	colladaSkin->SetTarget(target->entity);

	// Add the bind-shape bind matrix
	MMatrix bindShapeMatrix;
	MPlug bindShapeMatrixPlug = clusterFn.findPlug("geomMatrix");
	if (isJointCluster) bindShapeMatrixPlug = bindShapeMatrixPlug.elementByLogicalIndex(idx);
	CDagHelper::GetPlugValue(bindShapeMatrixPlug, bindShapeMatrix);
	colladaSkin->SetBindShapeTransform(MConvert::ToFMatrix(bindShapeMatrix));

	// Gather the list of joints
	MDagPathArray& influences = newTarget->influences;
	MObjectArray weightFilters;
	if (controllerNode.apiType() == MFn::kSkinClusterFilter)
	{
		MFnSkinCluster skinClusterFn(controllerNode);
		skinClusterFn.influenceObjects(influences, &status);
	}
	else if (controllerNode.apiType() == MFn::kJointCluster)
	{
		GetJointClusterInfluences(controllerNode, influences, weightFilters, idx);
	}
	uint numInfluences = influences.length();

	// Gather the bind matrices
	for (uint i = 0; i < numInfluences; ++i)
	{
		MObject influence = influences[i].node();
		MMatrix bindPose = CDagHelper::GetBindPoseInverse(controllerNode, influence);
		newTarget->bindPoses.push_back(MConvert::ToFMatrix(bindPose));
	}

	// Collect all the weights
	colladaSkin->SetInfluenceCount(meshFn.numVertices());
	FCDSkinControllerVertex* colladaInfluences = colladaSkin->GetVertexInfluences();
	if (controllerNode.apiType() == MFn::kSkinClusterFilter)
	{
		MFnSkinCluster skinClusterFn(controllerNode);
		for (MItGeometry componentIt(outputShape); !componentIt.isDone(); componentIt.next())
		{
			FCDSkinControllerVertex& vertex = colladaInfluences[componentIt.index()];

			WeightArray weights;
			if (skinClusterFn.getWeights(outputShape, componentIt.component(), weights, numInfluences) != MS::kSuccess)
			{
				weights.clear();
			}

			uint weightCount = weights.length();
			for (uint i = 0; i < weightCount; ++i)
			{
				if (!IsEquivalent(weights[i], Weight(0)))
				{
					vertex.AddPair(i, (float) weights[i]);
				}
			}
		}
	}
	else if (controllerNode.apiType() == MFn::kJointCluster)
	{
		// Get the weight for each of the clusters
		uint weightFilterCount = weightFilters.length();
		for (uint i = 0; i < weightFilterCount; ++i)
		{
			MFnWeightGeometryFilter filterFn(weightFilters[i]);
			MObject deformSet = filterFn.deformerSet();
			MFnSet setFn(deformSet);

			// Get all the components affected by this joint cluster
			MSelectionList clusterSetList;
			setFn.getMembers(clusterSetList, true);

			MDagPath shapePath;
			MObject components;
			uint setListCount = clusterSetList.length();
			for (uint s = 0; s < setListCount; ++s)
			{
				clusterSetList.getDagPath(s, shapePath, components);
				if (shapePath.node() == outputShape.node()) break;
			}

			MFloatArray weights;
			filterFn.getWeights(idx, components, weights);

			uint counter = 0;
			for (MItGeometry componentIt(shapePath, components); !componentIt.isDone() && counter < weights.length(); componentIt.next())
			{
				// append the weight at its correct position: i
				float weight = weights[counter++];
				if (!IsEquivalent(weight, 0.0f))
				{
					FCDSkinControllerVertex& vertex = colladaInfluences[componentIt.index()];
					vertex.AddPair(i, weight);
				}
			}
		}
	}
	return newTarget;
}

void DaeControllerLibrary::GetJointClusterInfluences(const MObject& controllerNode, MDagPathArray& influences, MObjectArray& weightFilters, uint clusterIndex)
{
	MObject cluster(controllerNode);
	while (cluster.apiType() == MFn::kJointCluster)
	{
		weightFilters.append(cluster);

		MObject joint = CDagHelper::GetNodeConnectedTo(cluster, "matrix");
		MDagPath jointPath = MDagPath::getAPathTo(joint);
		influences.append(jointPath);

		MStatus rc;
		MPlug p = CShaderHelper::FindPlug(MFnDependencyNode(cluster), "input", &rc);
		if (rc != MStatus::kSuccess) { MGlobal::displayError("Unable to get joint cluster input plug."); return; }

		p = p.elementByLogicalIndex(clusterIndex, &rc);
		if (rc != MStatus::kSuccess) { MGlobal::displayError("Unable to get joint cluster input plug first element."); return; }

		p = CDagHelper::GetChildPlug(p, "ig", &rc); // "inputGeometry"
		if (rc != MStatus::kSuccess) { MGlobal::displayError("Unable to get joint cluster input geometry plug."); return; }

		cluster = CDagHelper::GetSourceNodeConnectedTo(p);
	}
}

DaeEntity* DaeControllerLibrary::ExportMorphController(FCDSceneNode* sceneNode, MObjectArray& controllerNodes, DaeEntity* target)
{
	FloatList blendWeights;
	fm::vector<MPlug> weightPlugs;

	typedef fm::pvector<FCDGeometry> FCDGeometryList;
	FCDGeometryList blendTargets;

	// Retrieve the source mesh
	FCDGeometry* baseMesh = NULL;
	if (target->entity->HasType(FCDGeometry::GetClassType()))
	{
		baseMesh = (FCDGeometry*) &*target->entity;
	}
	else if (target->entity->HasType(FCDController::GetClassType()))
	{
		baseMesh = ((FCDController*) &*target->entity)->GetBaseGeometry();
	}
	if (baseMesh == NULL || !baseMesh->IsMesh()) return target; // We require a base geometry for non-instanced target export.

	// Retrieve the target informations
	uint controllerNodeCount = controllerNodes.length();
	for (uint i = 0; i < controllerNodeCount; ++i)
	{
		MFnDependencyNode controllerFn(controllerNodes[i]);
		MPlug inputTargetArrayPlug = controllerFn.findPlug("it"); // "inputTarget"
		if (inputTargetArrayPlug.attribute().isNull()) continue;
		MPlug weightArrayPlug = controllerFn.findPlug("w"); // "weight"
		uint nextIndex = 0;

		uint inputTargetCount = inputTargetArrayPlug.numElements();
		for (uint j = 0; j < inputTargetCount; ++j)
		{
			MPlug inputTargetPlug = inputTargetArrayPlug.elementByPhysicalIndex(j);
			if (inputTargetPlug.attribute().isNull()) continue;
			MPlug inputTargetGroupArrayPlug = CDagHelper::GetChildPlug(inputTargetPlug, "itg"); // "inputTargetGroup"
			if (inputTargetGroupArrayPlug.attribute().isNull()) continue;

			uint inputTargetGroupCount = inputTargetGroupArrayPlug.numElements();
			for (uint k = 0; k < inputTargetGroupCount; ++k)
			{
				MPlug inputTargetGroupPlug = inputTargetGroupArrayPlug.elementByPhysicalIndex(k);
				if (inputTargetGroupPlug.attribute().isNull()) continue;
				MPlug inputTargetInputArrayPlug = CDagHelper::GetChildPlug(inputTargetGroupPlug, "iti"); // "inputTargetInput"
				if (inputTargetInputArrayPlug.attribute().isNull()) continue;

				uint inputTargetInputCount = inputTargetInputArrayPlug.numElements();
				for (uint m = 0; m < inputTargetInputCount; ++m)
				{
					MPlug inputTargetInput = inputTargetInputArrayPlug.elementByPhysicalIndex(m);
					if (inputTargetInput.attribute().isNull()) continue;
					MPlug inputTargetGeometryPlug = CDagHelper::GetChildPlug(inputTargetInput, "igt"); // "inputTargetInput"
					if (inputTargetGeometryPlug.attribute().isNull()) continue;

					// We now have a valid Maya blend shape target.
					uint currentIndex = nextIndex++;
					FCDGeometry* blendShapeTarget = NULL;
					bool isInstanced = CDagHelper::HasConnection(inputTargetGeometryPlug, false, true);
					if (isInstanced)
					{
						// Find the first attached mesh DAG node.
						MItDependencyGraph itDG(inputTargetGeometryPlug, MFn::kMesh, MItDependencyGraph::kUpstream, MItDependencyGraph::kDepthFirst, MItDependencyGraph::kNodeLevel);
						MObject targetObj = itDG.thisNode();
						if (targetObj.isNull()) continue;

						// This mesh is our morph target.
						MDagPath targetPath = MDagPath::getAPathTo(targetObj);
						if (doc->GetSceneGraph()->EnterDagNode(sceneNode, targetPath, false))
						{
							doc->GetGeometryLibrary()->ExportMesh(sceneNode, targetPath, true, false);
						}

						DaeEntity* meshElement = doc->GetSceneGraph()->FindElement(targetPath);
						if (meshElement == NULL) continue;
						if (meshElement->entity == NULL) continue;
						if (meshElement->entity->GetObjectType() != FCDGeometry::GetClassType()) continue;

						// Grab the FCDGeometry as our morph target.
						blendShapeTarget = (FCDGeometry*) &*(meshElement->entity);
					}
					else
					{
						MPlug targetVertexListPlug = CDagHelper::GetChildPlug(inputTargetInput, "ipt"); // "inputPointsTarget"
						if (inputTargetInput.attribute().isNull()) continue;
						MPlug targetComponentListPlug = CDagHelper::GetChildPlug(inputTargetInput, "ict"); // "inputComponentTarget"
						if (inputTargetInput.attribute().isNull()) continue;
						
						// We have a dangling geometry: create a FCDGeometry and fill it with the vertex positions.
						blendShapeTarget = ExportMorphTarget(targetVertexListPlug, targetComponentListPlug, currentIndex, baseMesh);
					}
					
					if (blendShapeTarget != NULL)
					{
						MPlug weightPlug = weightArrayPlug.elementByLogicalIndex(currentIndex);
						if (weightPlug.attribute().isNull()) continue;

						blendTargets.push_back(blendShapeTarget);
						weightPlugs.push_back(weightPlug);
					}
				}
			}
		}
	}

	size_t targetCount = blendTargets.size();
	if (targetCount == 0) return target;

	// Create the COLLADA controller and the ColladaMaya equivalent
	FCDController* colladaController = CDOC->GetControllerLibrary()->AddEntity();
	DaeController* newTarget = new DaeController(doc, colladaController, MObject::kNullObj);
	if (newTarget->isOriginal) colladaController->SetDaeId(target->entity->GetDaeId() + "-morph");
	colladaController->SetName(MConvert::ToFChar(doc->MayaNameToColladaName(MFnDependencyNode(controllerNodes[0]).name())));
	FCDMorphController* colladaMorph = colladaController->CreateMorphController();
	colladaMorph->SetBaseTarget(target->entity);
	colladaMorph->SetMethod(FUDaeMorphMethod::NORMALIZED);

	newTarget->target = target;

	// Export the morph target weight animations
	for (size_t j = 0; j < targetCount; ++j)
	{
		MPlug& weightPlug = weightPlugs[j];
		MPlug enveloppePlug = MFnDependencyNode(weightPlug.node()).findPlug("en"); // "enveloppe"
		float enveloppe = 1.0f;
		if (!enveloppePlug.attribute().isNull()) enveloppePlug.getValue(enveloppe);
		float weight; weightPlug.getValue(weight);
		FCDMorphTarget* colladaMorphTarget = colladaMorph->AddTarget(blendTargets[j], enveloppe * weight);
		if (colladaMorphTarget != NULL)
		{
			ANIM->AddPlugAnimation(weightPlug, colladaMorphTarget->GetWeight(), kSingle);
		}
	}

	return newTarget;
}

FCDGeometry* DaeControllerLibrary::ExportMorphTarget(MPlug& targetVertexListPlug, MPlug& targetComponentListPlug, uint currentIndex, FCDGeometry* baseMesh)
{
	MStatus status;

	// Retrieve the data objects for the vertex list.
	MObject pointArrayData, componentListData;
	CHECK_MSTATUS(targetVertexListPlug.getValue(pointArrayData));
	if (pointArrayData.isNull()) return NULL;
	CHECK_MSTATUS(targetComponentListPlug.getValue(componentListData));
	if (componentListData.isNull()) return NULL;

	MFnPointArrayData vlistFn(pointArrayData, &status); CHECK_MSTATUS(status);
	uint relativePointCount = vlistFn.length(&status); CHECK_MSTATUS(status);
	if (relativePointCount == 0) return NULL;
	MFnComponentListData componentListFn(componentListData, &status); CHECK_MSTATUS(status);
	uint componentCount = componentListFn.length(&status); CHECK_MSTATUS(status);
	if (componentCount == 0) return NULL;

	// Create a new FCollada geometry.
	FUStringBuilder name(MConvert::ToFChar(MFnDependencyNode(targetVertexListPlug.node()).name()));
	name.append(FC("-target")); name.append(currentIndex);
	FCDGeometry* morphTarget = CDOC->GetGeometryLibrary()->AddEntity();
	morphTarget->SetName(name.ToString());
	fm::string id = TO_STRING(morphTarget->GetName());
	morphTarget->SetDaeId(id);

	// Create the mesh and fill in the position source with the source mesh's positions.
	FCDGeometryMesh* targetMesh = morphTarget->CreateMesh();
	FCDGeometrySource* positionSource = targetMesh->AddVertexSource(FUDaeGeometryInput::POSITION);
	FUSStringBuilder sourceId(id); sourceId.append("-positions");
	positionSource->SetDaeId(sourceId.ToString());
	positionSource->SetStride(3);
	FCDGeometrySource* baseSource = baseMesh->GetMesh()->GetPositionSource();
	size_t valueCount = baseSource->GetValueCount();
	positionSource->SetValueCount(valueCount);
	float* data = positionSource->GetData();
	memcpy(data, baseSource->GetData(), sizeof(float) * baseSource->GetDataCount());

	uint offset = 0;
	for (uint c = 0; c < componentCount; ++c)
	{
		MFnSingleIndexedComponent component(componentListFn[c]);
		MIntArray elements;
		CHECK_MSTATUS(component.getElements(elements));
		uint elementCount = elements.length();
		for (uint i = 0; i < elementCount; ++i)
		{
			const MPoint& p = vlistFn[offset + i];
			uint pointIndex = elements[i];
			data[3 * pointIndex + 0] += (float) p.x;
			data[3 * pointIndex + 1] += (float) p.y;
			data[3 * pointIndex + 2] += (float) p.z;
		}
		offset += elementCount;
	}

	return morphTarget;
}

void DaeControllerLibrary::ImportMorphTarget(MPlug& vertexListPlug, MPlug& componentListPlug, FCDGeometry* baseGeom, FCDGeometryMesh* targetMesh)
{
	MStatus status;
	if (baseGeom == NULL || !baseGeom->IsMesh()) return;

	// Retrieve the base mesh/morph target's vertex position data.
	FCDGeometryMesh* baseMesh = baseGeom->GetMesh();
	FCDGeometrySource* basePositionSource = baseMesh->FindSourceByType(FUDaeGeometryInput::POSITION);
	FCDGeometrySource* targetPositionSource = targetMesh->FindSourceByType(FUDaeGeometryInput::POSITION);
	if (basePositionSource == NULL || targetPositionSource == NULL) return;
	float* baseData = basePositionSource->GetData();
	uint32 baseStride = basePositionSource->GetStride();
	float* targetData = targetPositionSource->GetData();
	uint32 targetStride = targetPositionSource->GetStride();
	uint32 commonStride = min(baseStride, targetStride);
	size_t count = min(basePositionSource->GetValueCount(), targetPositionSource->GetValueCount());
	if (commonStride == 0 || count == 0) return;

	// Fill in the list of morphed vertices and their relative positions.
	MIntArray elementList;
	MPointArray pointList;
	MPoint p;
	for (size_t i = 0; i < count; ++i)
	{
		p.x = targetData[0] - baseData[0];
		bool hasValue = !IsEquivalent(p.x, 0.0);
		if (commonStride > 1)
		{
			p.y = targetData[1] - baseData[1];
			hasValue |= !IsEquivalent(p.y, 0.0);
			if (commonStride > 2)
			{
				p.z = targetData[2] - baseData[2];
				hasValue |= !IsEquivalent(p.z, 0.0);
			}
		}

		baseData += baseStride;
		targetData += targetStride;

		if (hasValue)
		{
			pointList.append(p);
			elementList.append((int) i);
		}
	}

	// Finalize the component list.
	MFnSingleIndexedComponent componentDataFn;
	componentDataFn.create(MFn::kMeshVertComponent, &status); CHECK_MSTATUS(status);
	CHECK_MSTATUS(componentDataFn.addElements(elementList));
	MFnComponentListData componentListDataFn;
	componentListDataFn.create(&status); CHECK_MSTATUS(status);
	MObject objectTemp = componentDataFn.object();
	MObject objectTempList = componentListDataFn.object();
	CHECK_MSTATUS(componentListDataFn.add(objectTemp));
	CHECK_MSTATUS(componentListPlug.setValue(objectTempList));

	// Finalize the vertex list.
	MFnPointArrayData pointListFn;
	pointListFn.create(&status); CHECK_MSTATUS(status);
	CHECK_MSTATUS(pointListFn.set(pointList));
	MObject temp = pointListFn.object();
	CHECK_MSTATUS(vertexListPlug.setValue(temp));
}

void DaeControllerLibrary::LeanMemory()
{
	FCDControllerLibrary* library = CDOC->GetControllerLibrary();
	for (size_t i = 0; i < library->GetEntityCount(); ++i)
	{
		FCDController* controller = library->GetEntity(i);
		if (controller->IsSkin())
		{
			LeanSkin(controller->GetSkinController());
		}
	}
}

void DaeControllerLibrary::LeanSkin(FCDSkinController* colladaSkin)
{
	// Release the unnecessary skinning information
	colladaSkin->SetJointCount(0);
	colladaSkin->SetInfluenceCount(0);
}
