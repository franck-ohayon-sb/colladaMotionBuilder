/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "AnimationExporter.h"
#include "CameraExporter.h"
#include "ColladaExporter.h"
#include "ControllerExporter.h"
#include "LightExporter.h"
#include "GeometryExporter.h"
#include "NodeExporter.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDController.h"
#include "FCDocument/FCDControllerInstance.h"
#include "FCDocument/FCDGeometryInstance.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDSkinController.h"
#include "FCDocument/FCDAnimationCurve.h"

//
// NodeExporter
//

NodeExporter::NodeExporter(ColladaExporter* _base)
:	EntityExporter(_base)
,	scene(NULL), colladaScene(NULL)
{
}

NodeExporter::~NodeExporter()
{
	colladaScene = NULL;
	exportedNodes.clear();
}

void NodeExporter::PreprocessScene(FBScene* _scene)
{
	scene = (FBScene*) _scene;
	bool doEvaluate = false;
	FBMatrix identity; identity.Identity();

	// Figure out all the nodes that must be sampled.
	FindNodesToSample(scene);

	// Early pass to remove all transforms from controlled nodes: then evaluate the scene to update the bind shapes.
	typedef fm::pvector<FBModel> FBModelQueue;
	FBModelQueue queue; queue.reserve(128);
	queue.push_back(scene->RootModel);
	while (!queue.empty())
	{
		FBModel* node = queue.back();
		queue.pop_back();

		// Queue up the child nodes.
		int childCount = node->Children.GetCount();
		for (int i = 0; i < childCount; ++i) queue.push_back(node->Children[i]);

		// Check for controlled mesh.
		FBGeometry* geometry = node->Geometry;
		if (geometry != NULL && geometry->VertexCount() > 0)
		{
			FBCluster* cluster = node->Cluster;
			if (cluster->LinkGetCount() > 0)
			{
				node->SetMatrix(identity, kModelTransformation, true);
				node->IsDeformable = false;
				doEvaluate = true;
			}
		}
	}

	if (doEvaluate) scene->Evaluate();
}

void NodeExporter::PostprocessScene(FBScene* scene)
{
	bool doEvaluate = false;

	// Early pass to remove all transforms from controlled nodes: then evaluate the scene to update the bind shapes.
	typedef fm::pvector<FBModel> FBModelQueue;
	FBModelQueue queue; queue.reserve(128);
	queue.push_back(scene->RootModel);
	while (!queue.empty())
	{
		FBModel* node = queue.back();
		queue.pop_back();

		// Queue up the child nodes.
		int childCount = node->Children.GetCount();
		for (int i = 0; i < childCount; ++i) queue.push_back(node->Children[i]);

		// Check for controlled mesh.
		FBGeometry* geometry = node->Geometry;
		if (geometry != NULL && geometry->VertexCount() > 0)
		{
			// Temporarily set the deformation flag.
			bool isDeformable = node->IsDeformable;
			node->IsDeformable = true;

			FBCluster* cluster = node->Cluster;
			if (cluster->LinkGetCount() > 0)
			{
				isDeformable = true;
				doEvaluate = true;
			}

			node->IsDeformable = isDeformable;
		}
	}

	if (doEvaluate) scene->Evaluate();
}

void NodeExporter::ExportScene(FBScene* _scene)
{
	scene = (FBScene*) _scene;

	// Create the base FCollada visual scene
	colladaScene = CDOC->AddVisualScene();
	exportedNodes.insert(scene->RootModel, CountedNode(colladaScene, 1));
	ExportEntity(colladaScene, scene->RootModel);
	
	// Initialize AnimClip
	ANIM->colladaAnimationClip = NULL;
	ANIM->createColladaAnimationClip = true;
	
	// Export its children.
	int childCount = scene->RootModel->Children.GetCount();
	for (int i = 0; i < childCount; ++i)
	{
		FBModel* node = scene->RootModel->Children[i];
		ExportNode(colladaScene, node);
	}

	// Export the global entities.
	LGHT->ExportAmbientLight(colladaScene);
	if (GetOptions()->ExportSystemCameras())
	{
		CAMR->ExportSystemCameras(colladaScene);
	}

	if (colladaScene->GetChildrenCount() == 0)
	{
		// Don't keep an empty scene.
		SAFE_RELEASE(colladaScene);
	}
	else
	{
		// Process the scene graph for controllers to isolate.
		PostProcessNode(colladaScene);
	}
}

FCDSceneNode* NodeExporter::ExportNode(FCDSceneNode* colladaParent, FBModel* node)
{
	// Check the arguments and look for an already exported node.
	bool instantiate = colladaParent != NULL;
	ExportedNodeMap::iterator it = exportedNodes.find(node);
	FCDSceneNode* colladaNode;
	if (it == exportedNodes.end())
	{
		// Create the FCollada node.
		FCDSceneNode* createUnder = (instantiate) ? colladaParent : colladaScene;
		colladaNode = createUnder->AddChildNode();
		
		// check if we want to export this node
		FBComponent* entity = (FBComponent*)node;
		fm::string fullName = (const char*)entity->GetFullName();
		fm::string name = (const char*)entity->Name;
		

		colladaNode->SetExported(true);

		if (GetOptions()->isUsingBoneList())
		{
			colladaNode->SetExported(false);

			for (fm::vector<const char*>::iterator it = boneNameExported->begin(); it != boneNameExported->end(); ++it)
			{
				fm::string VecName(*it);

//#define DEBUG_MOBU

#ifdef DEBUG_MOBU
				if (entity->Selected)
#else
				if (VecName == name)
#endif
				{
					std::size_t found = std::string::npos;
					for (fm::vector<fm::string>::iterator it = GetCharactersNamespace()->begin(); it != GetCharactersNamespace()->end(); ++it)
					{
						found = fullName.find(*it);
					}

					if (found == std::string::npos)
					{
						colladaNode->SetExported(true);
						break;
					}
				}
			}
		}


		ExportEntity(colladaNode, node);
		exportedNodes.insert(node, CountedNode(colladaNode, instantiate ? 1 : 0));

	}
	else 
	{
		if (it->second.second == 0 && instantiate)
		{
			// Need to reparent.
			FCDSceneNode* originalParent = it->second.first->GetParent();
			originalParent->RemoveChildNode(it->second.first);
		}
		if (instantiate)
		{
			colladaParent->AddChildNode(it->second.first);
			++(it->second.second);
		}
		return it->second.first;
	}

	// Export the child nodes.
	int childCount = node->Children.GetCount();
	for (int i = 0; i < childCount; ++i)
	{
		FBModel* childNode = node->Children[i];
		ExportNode(colladaNode, childNode);
	}

	// Export the local instance and the transforms.
	if (colladaNode->IsExported())
	{
		ExportTransforms(colladaNode, node);
		ExportInstance(colladaNode, node);
	}

	return colladaNode;
}

void NodeExporter::ExportInstance(FCDSceneNode* colladaNode, FBModel* node)
{
	// Export the instance at this model.
	FCDEntity* entity = NULL;
	if (node->Is(FBLight::TypeInfo))
	{
		// Directional lights, in MotionBuilder, point in the negative Y axis.
		FBLight* light = (FBLight*) node;
		if (light->LightType == kFBLightTypeSpot || light->LightType == kFBLightTypeInfinite)
		{
			if (node->Children.GetCount() > 0)
			{
				colladaNode = colladaNode->AddChildNode();
				colladaNode->SetDaeId(fm::string((char*) node->Name.AsString()) + "-pivot");
			}
			FCDTRotation* pivotRotation = (FCDTRotation*) colladaNode->AddTransform(FCDTransform::ROTATION);
			pivotRotation->SetAxis(FMVector3::XAxis);
			pivotRotation->SetAngle(-90.0f);
		}
		entity = LGHT->ExportLight((FBLight*) node);
	}
	else if (node->Is(FBCamera::TypeInfo)) entity = CAMR->ExportCamera((FBCamera*) node);
	else if (node->Is(FBModelMarker::TypeInfo)) {} // Locator
	else if (node->Is(FBModelNull::TypeInfo)) {} // Locator
	else if (node->Is(FBModelOptical::TypeInfo)) {}  // Locator
	else if (node->Is(FBModelPath3D::TypeInfo)) {} // Spline?
	else if (node->Is(FBModelRoot::TypeInfo)) { FUFail(""); } // should not happen.
	else if (node->Is(FBModelSkeleton::TypeInfo)) colladaNode->SetJointFlag(true);
	else 
	{
		// This should be a mesh: these are exported through the controller exporter.
		FBGeometry* geometry = node->Geometry;
		if (geometry->VertexCount() > 0)
		{
			if (!GetOptions()->isExportingOnlyAnimAndScene())
				entity = CTRL->ExportController(node, geometry);
		}
	}

	// Add the instance.
	if (entity != NULL)
	{
		FCDSceneNode* colladaPivot = ExportPivot(colladaNode, node); // may return the colladaNode if not pivoted.
		FCDEntityInstance* instance = colladaPivot->AddInstance(entity);
		if (instance->HasType(FCDGeometryInstance::GetClassType())) GEOM->ExportGeometryInstance(node, (FCDGeometryInstance*) instance, entity);
		if (instance->HasType(FCDControllerInstance::GetClassType()) && (!GetOptions()->isExportingOnlyAnimAndScene())) CTRL->ExportControllerInstance(node, (FCDControllerInstance*)instance, entity);
	}
}

void NodeExporter::ExportTransforms(FCDSceneNode* colladaNode, FBModel* node)
{
	// Detect constraints.
	FBModel* lookAtNode = node->LookAt;
	FBModel* upOrientationNode = node->UpVector;
	
	bool isConstrained = lookAtNode != NULL || upOrientationNode != NULL;

	if (node->Is(FBCamera::TypeInfo))
	{
		FBCamera* camera = (FBCamera*) node;
		FMMatrix44 transform;

		FBMatrix transform1;
		camera->GetCameraMatrix(transform1, kFBModelView);
		transform = ToFMMatrix44(transform1);

		transform = transform.Transposed().Inverted(); // Transpose because MotionBuilder is row-major.
		
		if (node->Parent != NULL)
		{
			// Take out the parent's transform and the known local transform.
			FMMatrix44 parentTransform = GetParentTransform(node->Parent, false);
			transform = parentTransform.Inverted() * transform;
			if (!isConstrained)
			{
				FBMatrix localTransform;
				node->GetMatrix(localTransform, kModelTransformation, false);
				transform = transform * ToFMMatrix44(localTransform).Inverted();
			}
		}

		FCDTMatrix* colladaTranform = (FCDTMatrix*) colladaNode->AddTransform(FCDTransform::MATRIX);
		colladaTranform->SetTransform(transform);

		if (isConstrained)
		{
			ANIM->AddModelToSample(node, colladaNode, colladaTranform->GetTransform());
			return;
		}
	}
	
	if (isConstrained || GetOptions()->isExportingBakedMatrix())
	{

		/*
		FBMatrix localTransform;
		node->GetMatrix(localTransform, kModelTransformation, false);
		FCDTMatrix* colladaTranform = (FCDTMatrix*) colladaNode->AddTransform(FCDTransform::MATRIX);
		colladaTranform->SetTransform(ToFMMatrix44(localTransform));
		ANIM->AddModelToSample(node, colladaNode, colladaTranform->GetTransform());
		*/

		// Get Pre Rotation
		int rotationOrder;
		FMVector3 preRotation, postRotation, scalePivotUpdateOffset, rotationPivot, scalePivot, rotationOffset, scaleOffset;
		bool isRotationDOFActive, isMotionBuilder55Limits;

		const int* orderedRotationIndices;
		const FMVector3* rotationAxises[3] = { &FMVector3::XAxis, &FMVector3::YAxis, &FMVector3::ZAxis };


		GetPropertyValue(node, "PreRotation", preRotation);
		GetPropertyValue(node, "RotationOrder", rotationOrder);
		GetPropertyValue(node, "RotationOffset", rotationOffset);
		GetPropertyValue(node, "RotationActive", isRotationDOFActive);
		GetPropertyValue(node, "RotationSpaceForLimitOnly", isMotionBuilder55Limits);


		switch ((Values)rotationOrder)
		{
			case EulerXYZ:
			case SphericXYZ: { static const int x[3] = { 0, 1, 2 }; orderedRotationIndices = x; break; }
			case EulerXZY: { static const int x[3] = { 0, 2, 1 }; orderedRotationIndices = x; break; }
			case EulerYZX: { static const int x[3] = { 1, 2, 0 }; orderedRotationIndices = x; break; }
			case EulerYXZ: { static const int x[3] = { 1, 0, 2 }; orderedRotationIndices = x; break; }
			case EulerZXY: { static const int x[3] = { 2, 0, 1 }; orderedRotationIndices = x; break; }
			case EulerZYX: { static const int x[3] = { 2, 1, 0 }; orderedRotationIndices = x; break; }
			default: { static const int x[3] = { 0, 1, 2 }; FUFail(orderedRotationIndices = x); break; }
		}

		FMMatrix44 PreRot = FMMatrix44::Identity;
		if (!isMotionBuilder55Limits && isRotationDOFActive && !IsEquivalent(preRotation, FMVector3::Zero))
		{
			
			for (int i = 2; i >= 0; --i)
			{
				FCDTRotation* rotation = new FCDTRotation(nullptr, NULL);
				rotation->SetAxis(*rotationAxises[orderedRotationIndices[i]]);
				rotation->SetAngle(preRotation[orderedRotationIndices[i]]);

				PreRot *= rotation->ToMatrix();
				delete rotation;
				rotation = NULL;
			}
		}

		FMMatrix44 Rot = FMMatrix44::Identity;
		// Animatable node rotation
		if (ANIM->IsAnimated(&node->Rotation) || !IsEquivalent(ToFMVector3(node->Rotation), FMVector3::Zero))
		{
			FMVector3 angles = ToFMVector3(node->Rotation);
			for (int i = 2; i >= 0; --i)
			{
				FCDTRotation* rotation = new FCDTRotation(nullptr, NULL);
				rotation->SetAxis(*rotationAxises[orderedRotationIndices[i]]);
				rotation->SetAngle(angles[orderedRotationIndices[i]]);
				Rot *= rotation->ToMatrix();

				delete rotation;
				rotation = NULL;
			}
		}

		// Translation
		FMMatrix44 Trans = FMMatrix44::Identity;
		if (ANIM->IsAnimated(&node->Translation) || !IsEquivalent(ToFMVector3(node->Translation), FMVector3::Zero))
		{
			FCDTTranslation* translation = new FCDTTranslation(nullptr, NULL);
			translation->SetTranslation(ToFMVector3(node->Translation)*GetOptions()->getScaleUnit());
			Trans = translation->ToMatrix();

			delete translation;
			translation = NULL;
		}


		FMMatrix44 FinalMatrix = Trans * PreRot * Rot;

		FCDTMatrix* colladaTranform = (FCDTMatrix*) colladaNode->AddTransform(FCDTransform::MATRIX);
		colladaTranform->SetTransform(FinalMatrix);
		ANIM->AddModelToSample(node, colladaNode, colladaTranform->GetTransform());
		
	}
	else
	{
		// For decomposed transforms, we need to force sampling if this node has IK.
		bool forceSampling = GetOptions()->isCharacterControlerUsedToRetrieveIK()? false: HasIK(node);

		// Motion Builder has four animatable transforms: Visibility, Translation,
		// Rotation and Scaling. It also has a series of non-animatable pivots.
		//
		int rotationOrder;
		FMVector3 preRotation, postRotation, scalePivotUpdateOffset, rotationPivot, scalePivot, rotationOffset, scaleOffset;
		bool isRotationDOFActive, isMotionBuilder55Limits;
		GetPropertyValue(node, "PreRotation", preRotation);
		GetPropertyValue(node, "PostRotation", postRotation);
		GetPropertyValue(node, "RotationOrder", rotationOrder);
		GetPropertyValue(node, "RotationOffset", rotationOffset);
		GetPropertyValue(node, "RotationActive", isRotationDOFActive);
		GetPropertyValue(node, "RotationSpaceForLimitOnly", isMotionBuilder55Limits);
		GetPropertyValue(node, "RotationPivot", rotationPivot);
		GetPropertyValue(node, "ScalingPivot", scalePivot);
		GetPropertyValue(node, "ScalingOffset", scaleOffset);
		GetPropertyValue(node, "ScalingPivotUpdateOffset", scalePivotUpdateOffset);
		scalePivotUpdateOffset -= scalePivot;

		// Consider the rotation order.
		const int* orderedRotationIndices;
		const FMVector3* rotationAxises[3] = { &FMVector3::XAxis, &FMVector3::YAxis, &FMVector3::ZAxis };
		
		switch ((Values) rotationOrder)
		{
		case EulerXYZ:
		case SphericXYZ: { static const int x[3] = { 0, 1, 2 }; orderedRotationIndices = x; break; }
		case EulerXZY: { static const int x[3] = { 0, 2, 1 }; orderedRotationIndices = x; break; }
		case EulerYZX: { static const int x[3] = { 1, 2, 0 }; orderedRotationIndices = x; break; }
		case EulerYXZ: { static const int x[3] = { 1, 0, 2 }; orderedRotationIndices = x; break; }
		case EulerZXY: { static const int x[3] = { 2, 0, 1 }; orderedRotationIndices = x; break; }
		case EulerZYX: { static const int x[3] = { 2, 1, 0 }; orderedRotationIndices = x; break; }
		default: { static const int x[3] = { 0, 1, 2 }; FUFail(orderedRotationIndices = x); break; }
		}

		// Retrieve the scale inheritance.
		// 0 -> Local(RrSs), 1 -> Parent(RSrs), 2 -> Scale Compensate.
		int scaleInheritance;
		GetPropertyValue(node, "InheritType", scaleInheritance);
		FMVector3 parentScale(FMVector3::One);
		if (scaleInheritance != 1)
		{
			FMMatrix44 parentTransform = GetParentTransform(node->Parent, false);
			FMVector3 r, t; float inverted;
			parentTransform.Decompose(parentScale, r, t, inverted);
		}

		// Translation
		if (ANIM->IsAnimated(&node->Translation) || !IsEquivalent(ToFMVector3(node->Translation), FMVector3::Zero))
		{
			FCDTTranslation* translation = (FCDTTranslation*) colladaNode->AddTransform(FCDTransform::TRANSLATION);
			translation->SetTranslation(ToFMVector3(node->Translation)*GetOptions()->getScaleUnit());

			fstring translate("translate");
			translation->SetSubId(translate);

			
			FCDConversionScaleFunctor scaleFunc(GetOptions()->getScaleUnit());
			ANIMATABLE(&node->Translation, colladaNode, translation->GetTranslation(), -1, &scaleFunc, forceSampling);
		}

		// Rotation Offset
		if (!IsEquivalent(rotationOffset, FMVector3::Zero))
		{
			FCDTTranslation* translate = (FCDTTranslation*) colladaNode->AddTransform(FCDTransform::TRANSLATION);
			translate->SetTranslation(rotationOffset*GetOptions()->getScaleUnit());
		}

		// Pre-rotation
		if (!isMotionBuilder55Limits && isRotationDOFActive && !IsEquivalent(preRotation, FMVector3::Zero))
		{
			for (int i = 2; i >= 0; --i)
			{
				FCDTRotation* rotation = (FCDTRotation*) colladaNode->AddTransform(FCDTransform::ROTATION);
				rotation->SetAxis(*rotationAxises[orderedRotationIndices[i]]);
				rotation->SetAngle(preRotation[orderedRotationIndices[i]]);
	
				fstring jointOrient("jointOrient");
				fstring XYZ(orderedRotationIndices[i] == 0 ? "X": orderedRotationIndices[i] == 1 ? "Y": "Z");
				fstring final(jointOrient + XYZ);
				rotation->SetSubId(final);
			}
		}
	
		// Scale inheritance. TODO: needs to support all animations on parent scales.
		if (scaleInheritance == 0 && !IsEquivalent(parentScale, FMVector3::One))
		{
			FCDTScale* scalePivot = (FCDTScale*) colladaNode->AddTransform(FCDTransform::SCALE);
			scalePivot->SetScale(1.0f / parentScale.x, 1.0f / parentScale.y, 1.0f / parentScale.z);
		}

		// Rotation Pivot 
		if (!IsEquivalent(rotationPivot, FMVector3::Zero))
		{
			FCDTTranslation* pivotTranslate = (FCDTTranslation*) colladaNode->AddTransform(FCDTransform::TRANSLATION);
			pivotTranslate->SetTranslation(rotationPivot*GetOptions()->getScaleUnit());
		}

		// Animatable node rotation
		if (ANIM->IsAnimated(&node->Rotation) || !IsEquivalent(ToFMVector3(node->Rotation), FMVector3::Zero))
		{
			FMVector3 angles = ToFMVector3(node->Rotation);
			for (int i = 2; i >= 0; --i)
			{
				FCDTRotation* rotation = (FCDTRotation*) colladaNode->AddTransform(FCDTransform::ROTATION);
				rotation->SetAxis(*rotationAxises[orderedRotationIndices[i]]);
				rotation->SetAngle(angles[orderedRotationIndices[i]]);

				fstring rotate("rotate");
				fstring XYZ(orderedRotationIndices[i] == 0 ? "X" : orderedRotationIndices[i] == 1 ? "Y" : "Z");
				fstring final(rotate + XYZ);
				rotation->SetSubId(final);

				ANIMATABLE_INDEXED_ANGLE(&node->Rotation, orderedRotationIndices[i], colladaNode, rotation->GetAngleAxis(), -1, NULL, forceSampling);
			}
		}

		// Rotation Pivot Reverse
		if (!IsEquivalent(rotationPivot, FMVector3::Zero))
		{
			FCDTTranslation* pivotTranslate = (FCDTTranslation*) colladaNode->AddTransform(FCDTransform::TRANSLATION);
			pivotTranslate->SetTranslation(-rotationPivot*GetOptions()->getScaleUnit());
		}

		// Scale inheritance. TODO: needs to support all animations on parent scales.
		if (scaleInheritance == 0 && !IsEquivalent(parentScale, FMVector3::One))
		{
			FCDTScale* scalePivot = (FCDTScale*) colladaNode->AddTransform(FCDTransform::SCALE);
			scalePivot->SetScale(parentScale);
		}

		// Post-rotation. This is like a reverse rotation pivot.
		if (!isMotionBuilder55Limits && isRotationDOFActive && !IsEquivalent(postRotation, FMVector3::Zero))
		{
			for (int i = 2; i >= 0; --i)
			{
				FCDTRotation* rotation = (FCDTRotation*) colladaNode->AddTransform(FCDTransform::ROTATION);
				rotation->SetAxis(*rotationAxises[orderedRotationIndices[2 - i]]);
				rotation->SetAngle(-postRotation[orderedRotationIndices[2 - i]]);
			}
		}

		// Scale Offset
		if (!IsEquivalent(scaleOffset, FMVector3::Zero))
		{
			FCDTTranslation* translate = (FCDTTranslation*) colladaNode->AddTransform(FCDTransform::TRANSLATION);
			translate->SetTranslation(scaleOffset*GetOptions()->getScaleUnit());
		}

		// Scale Pivot updated offset
		if (!IsEquivalent(scalePivotUpdateOffset, FMVector3::Zero))
		{
			FCDTTranslation* pivotTranslate = (FCDTTranslation*) colladaNode->AddTransform(FCDTransform::TRANSLATION);
			pivotTranslate->SetTranslation(scalePivotUpdateOffset*GetOptions()->getScaleUnit());
		}

		// Scale Pivot 
		if (!IsEquivalent(scalePivot, FMVector3::Zero))
		{
			FCDTTranslation* pivotTranslate = (FCDTTranslation*) colladaNode->AddTransform(FCDTransform::TRANSLATION);
			pivotTranslate->SetTranslation(scalePivot*GetOptions()->getScaleUnit());
		}

		// Scale
		if (ANIM->IsAnimated(&node->Scaling) || !IsEquivalent(ToFMVector3(node->Scaling), FMVector3::One))
		{
			FCDTScale* scale = (FCDTScale*) colladaNode->AddTransform(FCDTransform::SCALE);
			scale->SetScale(ToFMVector3(node->Scaling));
			ANIMATABLE(&node->Scaling, colladaNode, scale->GetScale(), -1, NULL, forceSampling);
		}

		// Scale Pivot 
		if (!IsEquivalent(scalePivot, FMVector3::Zero))
		{
			FCDTTranslation* pivotTranslate = (FCDTTranslation*) colladaNode->AddTransform(FCDTransform::TRANSLATION);
			pivotTranslate->SetTranslation(-scalePivot*GetOptions()->getScaleUnit());
		}

		// Scale Pivot updated offset Reverse
		if (!IsEquivalent(scalePivotUpdateOffset, FMVector3::Zero))
		{
			FCDTTranslation* pivotTranslate = (FCDTTranslation*) colladaNode->AddTransform(FCDTransform::TRANSLATION);
			pivotTranslate->SetTranslation(-scalePivotUpdateOffset*GetOptions()->getScaleUnit());
		}
	}

	// Visibility
	bool b = node->Visibility;
	colladaNode->SetVisibility(b);
	ANIMATABLE(&node->Visibility, colladaNode, colladaNode->GetVisibility(), -1, NULL, false);
}

FCDSceneNode* NodeExporter::ExportPivot(FCDSceneNode* colladaNode, FBModel* node)
{
	// Retrieve the pivot transform.
	FMVector3 pivotTranslation, pivotRotation, pivotScale;
	ExtractPivot(node, pivotTranslation, pivotRotation, pivotScale);
	if (IsEquivalent(pivotTranslation, FMVector3::Zero) && IsEquivalent(pivotRotation, FMVector3::Zero)  && IsEquivalent(pivotScale, FMVector3::One))
	{
		return colladaNode;
	}

	// Create the pivot node and its (non-animatable) transforms.
	FCDSceneNode* colladaPivot = colladaNode->AddChildNode();
	colladaPivot->SetDaeId(colladaNode->GetDaeId() + "-pivot");
	if (!IsEquivalent(pivotTranslation, FMVector3::Zero))
	{
		FCDTTranslation* translation = (FCDTTranslation*) colladaPivot->AddTransform(FCDTransform::TRANSLATION);
		translation->SetTranslation(pivotTranslation*GetOptions()->getScaleUnit());
	}
	if (!IsEquivalent(pivotRotation, FMVector3::Zero))
	{
		const FMVector3* axises[3] = { &FMVector3::XAxis, &FMVector3::YAxis, &FMVector3::ZAxis };
		for (int i = 2; i >= 0; --i)
		{
			FCDTRotation* rotate = (FCDTRotation*) colladaPivot->AddTransform(FCDTransform::ROTATION);
			rotate->SetAxis((*axises)[i]);
			rotate->SetAngle(pivotRotation[i]);
		}
	}
	if (!IsEquivalent(pivotScale, FMVector3::One))
	{
		FCDTScale* scale = (FCDTScale*) colladaPivot->AddTransform(FCDTransform::SCALE);
		scale->SetScale(pivotScale);
	}
	return colladaPivot;
}

void NodeExporter::ExtractPivot(FBModel* node, FMVector3& pivotTranslation, FMVector3& pivotRotation, FMVector3& pivotScale)
{
	if (!GetPropertyValue(node, "GeometricTranslation", pivotTranslation)) pivotTranslation = FMVector3::Zero;
	if (!GetPropertyValue(node, "GeometricRotation", pivotRotation)) pivotRotation = FMVector3::Zero;
	if (!GetPropertyValue(node, "GeometricScaling", pivotScale)) pivotScale = FMVector3::One;
}

void NodeExporter::PostProcessNode(FCDSceneNode* colladaNode)
{
	// Purely FCollada process to isolate skin controllers.
	colladaNode->SetUserHandle((void*) 0x1); // process nodes only once.

	// Start by recursively processing the children.
	// Process in reverse order, because PostProcessNode may remove child nodes.
	size_t childrenCount = colladaNode->GetChildrenCount();
	for (size_t i = childrenCount; i > 0; --i)
	{
		FCDSceneNode* child = colladaNode->GetChild(i - 1);
		if (child->GetUserHandle() != (void*) 0x1) PostProcessNode(child);
	}

	// Process the instances for nodes and controllers
	size_t instanceCount = colladaNode->GetInstanceCount();
	for (size_t j = 0; j < instanceCount; ++j)
	{
		FCDEntityInstance* instance = colladaNode->GetInstance(j);
		if (instance->GetEntityType() == FCDEntity::SCENE_NODE)
		{
			FCDSceneNode* child = (FCDSceneNode*) instance->GetEntity();
			if (child->GetUserHandle() != (void*) 0x1) PostProcessNode(child);
		}
		else if (instance->GetEntityType() == FCDEntity::CONTROLLER)
		{
			FCDController* controller = (FCDController*) instance->GetEntity();
			if (controller->IsSkin())
			{
				// Calculate the transform we really want for this skinned mesh instance.
				FCDSkinController* skin = controller->GetSkinController();
				FMMatrix44 localTransform = colladaNode->CalculateLocalTransform();
				localTransform *= skin->GetBindShapeTransform().Inverted();
				FMMatrix44 parentTransform = colladaNode->GetParent()->CalculateWorldTransform();

				FCDSceneNode* clone = NULL;
				if (colladaNode->GetParent() == colladaScene && childrenCount == 0 && instanceCount == 1)
				{
					// Replace the local transforms by one matrix.
					while (colladaNode->GetTransformCount() != 0) colladaNode->GetTransform(0)->Release();
					clone = colladaNode;
				}
				else
				{
					// Make a new child under the visual scene for this node.
					clone = colladaScene->AddChildNode();
					clone->SetDaeId(colladaNode->GetDaeId() + "-clone");
					clone->SetName(colladaNode->GetName());
					clone->SetSubId(colladaNode->GetSubId());

					// Move the instance to the sibling.
					FCDEntityInstance* instanceClone = clone->AddInstance(instance->GetEntity());
					instance->Clone(instanceClone);
					instance->Release();
					instance = instanceClone;

					if (childrenCount == 0 && instanceCount == 1) SAFE_RELEASE(colladaNode);
				}

				if (!IsEquivalent(localTransform, FMMatrix44::Identity))
				{
					// No post-skinning transforms in MotionBuilder.
				}
			}
		}
	}
}

FMMatrix44 NodeExporter::GetParentTransform(FBModel* node, bool isLocal)
{
	if (node == NULL)
	{
		return FMMatrix44::Identity;
	}
	else if (node->Is(FBCamera::TypeInfo))
	{
		FMMatrix44 mx;
		FBCamera* camera = (FBCamera*) node;
		FBMatrix mx1;
		camera->GetCameraMatrix(mx1, kFBModelView);
		mx = ToFMMatrix44(mx1);
		mx = mx.Transposed();
		if (isLocal)
		{
			FMMatrix44 p = GetParentTransform(node->Parent);
			return p.Inverted() * mx;
		}
		return mx;
	}
	else
	{
		FBMatrix mx;
		node->GetMatrix(mx, kModelTransformation, !isLocal);
		return ToFMMatrix44(mx);
	}
}

void NodeExporter::FindNodesToSample(FBScene* scene)
{
	// Check for Actors.
	int actorCount = scene->Actors.GetCount();
	for (int i = 0; i < actorCount; ++i)
	{
		FBActor* actor = scene->Actors[i];
		FBMarkerSet* markers = actor->MarkerSet;
		int markerPlacementCount = (int) kFBLastNodeId;
		for (int j = 0; j < markerPlacementCount; ++j)
		{
			int markerCount = markers->GetMarkerCount((FBSkeletonNodeId) j);
			for (int k = 0; k < markerCount; ++k)
			{
				FBModel* marker = markers->GetMarkerModel((FBSkeletonNodeId) j, k);
				if (marker == NULL) continue;

				// Don't insert the marker to be sampled, but insert all its parents.
				while (marker->Parent != NULL && marker->Parent != scene->RootModel)
				{
					marker = marker->Parent;
					nodesToSample.insert(marker, 0);
				}
			}
		}
	}

	// Check for Characters
	int characterCount = scene->Characters.GetCount();
	for (int i = 0; i < characterCount; ++i)
	{
		FBCharacter* character = scene->Characters[i];

		FBModel* hipsEffectorModel = character->GetEffectorModel(FBEffectorId::kFBHipsEffectorId);
		FBComponent* hipsEntity = (FBComponent*)hipsEffectorModel;
		fm::string fullName = (const char*)hipsEntity->GetFullName();
		std::size_t found = fullName.find_last_of(":");
		
		GetCharactersNamespace()->push_back(fullName.substr(0, found));
		

		int markerPlacementCount = (int) kFBLastNodeId;
		for (int j = 0; j < markerPlacementCount; ++j)
		{
			FBModel* model = character->GetModel((FBBodyNodeId) j);
			if (model != NULL)
			{
				nodesToSample.insert(model, 0);
			}
		}
	}

	// Check for Constraints
	FBConstraintManager& constraintManager = FBConstraintManager::TheOne();
	int constraintCount = constraintManager.TypeGetCount();
	for (int i = 0; i < constraintCount; ++i)
	{
		FBConstraint* constraint = constraintManager.TypeCreateConstraint(i);
		int groupCount = constraint->ReferenceGroupGetCount();
		if (groupCount >= 2)
		{
			// First reference is the base of the constraint.
			// Second reference is the end of the constraint.
			FBModel* start = constraint->ReferenceGet(0, 0);
			FBModel* end = constraint->ReferenceGet(1, 0);
			if (start == NULL || end == NULL) continue;

			// Make sure that end and start are related.
			bool related = false;
			for (FBModel* it = end; it != NULL && !related; it = it->Parent) related = it == start;
			if (related)
			{
				for (FBModel* it = end; it != start; it = it->Parent) nodesToSample.insert(it, 0);
				nodesToSample.insert(start, 0);
			}
		}
	}
}

bool NodeExporter::HasIK(FBModel* node)
{
	NodeSet::iterator it = nodesToSample.find(node);
	return it != nodesToSample.end();
}

