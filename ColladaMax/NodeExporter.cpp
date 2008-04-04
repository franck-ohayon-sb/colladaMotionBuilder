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
#include "CameraExporter.h"
#include "ControllerExporter.h"
#include "DocumentExporter.h"
#include "EntityExporter.h"
#include "ColladaOptions.h"
#include "ForceExporter.h"
#include "GeometryExporter.h"
#include "LightExporter.h"
#include "MaterialExporter.h"
#include "NodeExporter.h"
#include "EmitterExporter.h"
#include "XRefFunctions.h"
#include "ColladaIDCreator.h"
#include "Common/ConversionFunctions.h"
#include "ColladaModifiers.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDController.h"
#include "FCDocument/FCDEntity.h"
#include "FCDocument/FCDEntityInstance.h"
#include "FCDocument/FCDEntityReference.h"
#include "FCDocument/FCDExternalReferenceManager.h"
#include "FCDocument/FCDExtra.h"
#include "FCDocument/FCDGeometryInstance.h"
#include "FCDocument/FCDEmitterInstance.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDForceField.h"
#include <decomp.h>
#include <ikctrl.h>
#include <iiksys.h>

using namespace FSConvert;

NodeExporter::NodeExporter(DocumentExporter* _document)
:	BaseExporter(_document), worldNode(NULL)
{
}

NodeExporter::~NodeExporter()
{
	CLEAR_POINTER_STD_PAIR_CONT(ExportNodeMap, exportNodes);
	worldNode = NULL;
}

void NodeExporter::ExportScene()
{
	// Retrieve the Max root node that contains the whole world.
	Interface* maxInterface = GetCOREInterface();
	worldNode = maxInterface->GetRootNode();

	// First pass: make a list of the nodes that must be exported.
	// Export the materials during this pass.
	CalculateExportNodes();

	// Second pass: export all the nodes, with empty objects and instances.
	ExportSceneGraph();
	ExportInstances();

	// Third pass: link all the information about the objects and instances.
	document->GetLightExporter()->LinkLights();
	document->GetCameraExporter()->LinkCameras();
	document->GetControllerExporter()->LinkControllers();

	FCDSceneNode* sceneRoot = CDOC->GetVisualSceneInstance();
	if (sceneRoot != NULL)
	{
		// Add the ambient lighting from the environment settings
		Point3 ambientCol = maxInterface->GetAmbient(0, FOREVER);
		if (!ambientCol.Equals(Point3::Origin))
		{
			FCDEntity* ambientLight = document->GetLightExporter()->ExportAmbientLight(ambientCol);
			if (ambientLight != NULL)
			{
				FCDSceneNode* ambientLightNode = CDOC->GetVisualSceneInstance()->AddChildNode();
				ambientLightNode->AddInstance(ambientLight);
			}
		}
		
		// Write out the frame_rate, inside an extra node.
		FCDExtra* extraInfo = CDOC->GetVisualSceneInstance()->GetExtra();
		FCDETechnique* maxTechnique = extraInfo->GetDefaultType()->AddTechnique(DAEMAX_MAX_PROFILE);
		maxTechnique->AddParameter(DAEMAX_FRAMERATE_PARAMETER, GetFrameRate());
	}
}

void NodeExporter::CalculateExportNodes()
{
	// Make a list of the selected nodes.
	NodeList selectedNodes;
	if (OPTS->ExportSelected())
	{
		int selectedNodeCount = GetCOREInterface()->GetSelNodeCount();
		for (int i = 0; i < selectedNodeCount; ++i)
		{
			selectedNodes.push_back(GetCOREInterface()->GetSelNode(i));
		}
	}

	// Process all the objects in the Max scene
	NodeList forcedNodes;
	for (int j = 0; j < worldNode->NumberOfChildren(); j++)
	{
		CalculateExportNodes(worldNode->GetChildNode(j), selectedNodes, forcedNodes);
	}
	
	// Process the forced nodes: if they are not already contained
	// in the export map, add them and their parent nodes to it.
	while (!forcedNodes.empty())
	{
		INode* node = forcedNodes.back();
		forcedNodes.pop_back();

		while (node != NULL && !node->IsRootNode() && exportNodes.find(node) == exportNodes.end())
		{
			exportNodes.insert(node, new ExportNode(node, false));
			node = node->GetParentNode();
		}
	}
}

bool NodeExporter::CalculateExportNodes(INode* currentNode, NodeList& selectedNodes, NodeList& forcedNodes)
{
	TSTR name = currentNode->GetName();

	// Verify whether this node or its parents are selected.
	bool isSelected = true;
	if (OPTS->ExportSelected())
	{
		NodeList::iterator it = selectedNodes.find(currentNode);
		if (it == selectedNodes.end())
		{
			isSelected = false;
		}
		else // Remove our node from the list (we are exported)
		{
			selectedNodes.erase(it);
		}
		//  This case is invalid, sub-objects will still be a single node. TODO
		if (GetCOREInterface()->IsSubObjectSelectionEnabled())
		{
			INode* parentNode = currentNode->GetParentNode();
			while (!isSelected && parentNode != NULL && parentNode != worldNode)
			{
				isSelected |= selectedNodes.find(parentNode) != selectedNodes.end();
				parentNode = parentNode->GetParentNode();
			}
		}
	}
	else
	{
		isSelected = !currentNode->IsHidden();
	}

	if (isSelected)
	{
		Mtl* nodeMaterial = currentNode->GetMtl();
		if (nodeMaterial != NULL)
		{
			// Export this node's material
			document->GetMaterialExporter()->ExportMaterial(nodeMaterial);
		}

		// Export this node's object
		//if (!currentNode->IsHidden())  This case is also invalid
		{
			Object* object = currentNode->GetObjectRef();
			if (object != NULL)
			{
				switch (ColladaMaxTypes::Get(currentNode, object))
				{
				case ColladaMaxTypes::LIGHT: document->GetLightExporter()->CalculateForcedNodes(currentNode, (LightObject*) object, forcedNodes); break;
				case ColladaMaxTypes::CAMERA: document->GetCameraExporter()->CalculateForcedNodes(currentNode, (CameraObject*) object, forcedNodes); break;
				case ColladaMaxTypes::MESH: document->GetControllerExporter()->CalculateForcedNodes(currentNode, object, forcedNodes); break;
				}
			}
		}
		exportNodes.insert(currentNode, new ExportNode(currentNode, true));
	}

	// Stack in the child nodes for export node verification
	bool forceThis = false;
	for (int j = 0; j < currentNode->NumberOfChildren(); j++)
	{
		// If any of our children are exported, we must be forced
		forceThis |= CalculateExportNodes(currentNode->GetChildNode(j), selectedNodes, forcedNodes);
	}
	if (!isSelected && forceThis) // we are not exported, but a child is.  Force export to maintain hierarchy
	{
		forcedNodes.push_back(currentNode);
	}
	return isSelected || forceThis;
}


void NodeExporter::ExportSceneGraph()
{
	if (!exportNodes.empty())
	{
		// Create the COLLADA visual scene
		FCDSceneNode* colladaScene = CDOC->AddVisualScene();
		TCHAR* sfnc = GetCOREInterface()->GetCurFileName().data();
		if (sfnc == NULL || _tcslen(sfnc) == 0) sfnc = _T("unnamed_scene");
		colladaScene->SetDaeId(sfnc);
		colladaScene->SetName(sfnc);

		// Process the scene nodes to export
		for (ExportNodeMap::iterator it = exportNodes.begin(); it != exportNodes.end(); ++it)
		{
			ExportNode& exportNode = *(*it).second;
			if (exportNode.colladaNode == NULL) ExportSceneNode(exportNode);
		}
	}
}

void NodeExporter::ExportSceneNode(ExportNode& exportNode)
{
	INode* node = exportNode.maxNode;

	// Retrieve the parent node information.
	INode* parentNode = node->GetParentNode();

	ExportNodeMap::iterator itP = exportNodes.find(parentNode);
	if (itP != exportNodes.end() && (*itP).second->colladaNode == NULL)
	{
		// The parent node has not yet been exported: force its export.
		ExportSceneNode(*(*itP).second);
	}
	FCDSceneNode* parent = exportNode.colladaParent = itP != exportNodes.end() ? (*itP).second->colladaNode : CDOC->GetVisualSceneInstance();

	// Create the FCollada scene node
	FCDSceneNode* colladaNode = exportNode.colladaNode = parent->AddChildNode();
	TCHAR* nodeName = node->GetName();
	ENTE->ExportEntity(node, colladaNode, (TO_STRING(nodeName) + "-node").c_str());
	colladaNode->SetName(nodeName);

	// Export the transform and create the pivot node, if needed
	//bool exportPivot = !OPTS->DoSampleMatrices(node);
	bool exportPivot = ExportTransforms(node, colladaNode, true);

	// Export the node note.
	TSTR userDefinedProperties;
	node->GetUserPropBuffer(userDefinedProperties);
	if (userDefinedProperties.length() > 0)
	{
		colladaNode->SetNote(userDefinedProperties.data());
	}

	// Write out the visibility controller
	// NOTE: In 3dsMax, the 'hidden' status and the visibility controller are completely unrelated.
	colladaNode->SetVisibility(node->GetVisibility(TIME_EXPORT_START) >= 0.5f);
	ANIM->ExportAnimatedFloat(colladaNode->GetName() + "-visibility", node->GetVisController(), colladaNode->GetVisibility());

	// Verify whether this is a Max bone.
	if (node->GetBoneNodeOnOff()) colladaNode->SetJointFlag(true);
	
	// Export this node's object
	if (exportNode.exportObject)
	{
		FCDSceneNode* colladaObjectNode = (exportPivot) ? ExportPivotNode(node, colladaNode) : colladaNode;
		Object* object = node->GetObjectRef();
		if (object != NULL)
		{
			ExportedEntityMap::iterator itE = exportedEntities.find(object);
			FCDEntity* entity = NULL;
			if (itE == exportedEntities.end())
			{
				switch (ColladaMaxTypes::Get(node, object, OPTS->ExportXRefs()))
				{
				case ColladaMaxTypes::LIGHT: entity = document->GetLightExporter()->ExportLight(node, (LightObject*) object); break;
				case ColladaMaxTypes::CAMERA: entity = document->GetCameraExporter()->ExportCamera(node, (CameraObject*) object); break;
				case ColladaMaxTypes::NURBS_CURVE: 
				case ColladaMaxTypes::SPLINE: entity = document->GetGeometryExporter()->ExportSpline(node, object); break;
				case ColladaMaxTypes::MESH: entity = document->GetControllerExporter()->ExportController(node, object); break;
				case ColladaMaxTypes::BONE: colladaNode->SetJointFlag(true); break;
				case ColladaMaxTypes::HELPER: ExportHelper(node, object, colladaObjectNode); break;
				case ColladaMaxTypes::XREF_OBJECT: {
						// TODO. Remove this when Collada controllers will support XRef sources
						if (document->GetControllerExporter()->DoOverrideXRefExport(object))
						{
							entity = document->GetControllerExporter()->ExportController(node, object);
							break;
						}

						fstring fnURL = XRefFunctions::GetURL(object);
						if (!fnURL.empty())
						{
							// for the moment, Max only support Geometry instanciation
							// material instanciation is performed in the material exporter.

							// XRefs may have some materials applied to them as well
							// so we must have a Collada entity describing this geometry XRef
							FCDEntity* xrefGeometry = document->GetGeometryExporter()->ExportMesh(node, object);
							FCDEntityReference* xref = NULL;
							if (xrefGeometry != NULL)
							{
								FCDEntityInstance* instance = colladaObjectNode->AddInstance(xrefGeometry);
								xref = instance->GetEntityReference();
								xref->SetUri(FUUri(fnURL));
								xref->SetEntityId(xrefGeometry->GetDaeId());

								// export the instance
								document->GetGeometryExporter()->ExportInstance(node, instance);
							}
							SAFE_RELEASE(xrefGeometry);

							if (xref != NULL && !XRefFunctions::IsXRefCOLLADA(object))
							{
								xref->SetEntityId(XRefFunctions::GetXRefObjectName(object));
							}
						}
					break; }
				} // end switch

				if (entity != NULL)
				{
					exportedEntities.insert(object, entity);
				}
			}
			else
			{
				entity = (*itE).second;
			}

			if (entity != NULL)
			{
				FCDEntityInstance* instance = colladaObjectNode->AddInstance(entity);
				instance->SetUserHandle(node);

				if (entity->GetObjectType().Includes(FCDController::GetClassType()) && node->NumberOfChildren() > 0)
				{
					// Continue iterating using a copy of the scene node:
					// in order to isolate the controller instance.
					colladaNode = parent->AddChildNode();
					colladaNode->SetDaeId(TO_STRING(nodeName) + "-copy");
					ExportTransforms(node, colladaNode, false);
				}
			}
		}
	}
}

void NodeExporter::ExportInstances()
{
    FCDSceneNode* visualScene = CDOC->GetVisualSceneInstance();
	if (visualScene != NULL)
	{
		// Iterate over the whole exported scene, looking for entity instances
		fm::pvector<FCDSceneNode> nodes;
		nodes.push_back(visualScene);
		while (!nodes.empty())
		{
			FCDSceneNode* node = nodes.back();
			nodes.pop_back();

			// Look for instances that need linking.
			size_t instanceCount = node->GetInstanceCount();
			for (size_t i = 0; i < instanceCount; ++i)
			{
				FCDEntityInstance* instance = node->GetInstance(i);
				if (instance->GetObjectType().Includes(FCDGeometryInstance::GetClassType()))
				{
					INode* maxNode = (INode*) instance->GetUserHandle();
					document->GetGeometryExporter()->ExportInstance(maxNode, instance);
				}
			}

			// Add all the children node to be processed
			nodes.insert(nodes.end(), node->GetChildren(), node->GetChildren() + node->GetChildrenCount());
		}
	}
}

void NodeExporter::ExportHelper(INode* node, Object* object, FCDSceneNode* colladaNode)
{
	if (object->SuperClassID() != HELPER_CLASS_ID)
		return;

	// Retrieve the helper's bounding box.
	Box3 boundingBox;
	object->GetLocalBoundBox(TIME_EXPORT_START, node, document->GetMaxInterface()->GetActiveViewport(), boundingBox);

	// Export the bounding box as extra information.
	FCDExtra* extra = colladaNode->GetExtra();
	FCDETechnique* technique = extra->GetDefaultType()->AddTechnique(DAEMAX_MAX_PROFILE);
	FCDENode* helperNode = technique->AddChildNode("helper");
	helperNode->AddParameter("bounding_min", ToFMVector3(boundingBox.Min()));
	helperNode->AddParameter("bounding_max", ToFMVector3(boundingBox.Max()));

	// Because we dont actually export an object for a helper, take any extra information
	// defined on it and export it on the colladaNode
	ENTE->ExportEntity(object, colladaNode, NULL);
}

bool NodeExporter::ExportTransforms(INode* node, FCDSceneNode* colladaNode, bool checkSkin)
{
	Object* obj = node->GetObjectRef();
	
	// PRS nodes for others
	Matrix3 tm = GetLocalTransform(node);

	// Check for skinned object. In this special case, the transform for
	// the first skinned instance will be saved with the controller.
	// See https://collada.org/phpBB2/viewtopic.php?t=515 for details.
	bool isAnimatable = true;
	bool hasSkinnedObject = checkSkin && document->GetControllerExporter()->IsSkinned(obj);
	if (hasSkinnedObject)
	{
		// Figure out whether this is the instance or the original
		INodeTab instanceNodes;
		IInstanceMgr::GetInstanceMgr()->GetInstances(*node, instanceNodes);
		if (instanceNodes.Count() == 0) return false;
		INode* firstInstanceNode = instanceNodes[0];
		if (node == firstInstanceNode) return false;

		// For all instances, export the difference between the first
		// instance's local transformation and the current local transformation.
		// Note that you cannot export animations for this.

		// Max is strange in this. It always moves back the skin to its original position,
		// when animating, whatever the new transform it might contain. So, we need to take out
		// both the initial TM and the first Instance's TM to figure out where to place this instance.

		// The first instance TM include its pivot TM
		Matrix3 firstInstanceTM = firstInstanceNode->GetObjTMBeforeWSM(0);
		INode *parentNode = firstInstanceNode->GetParentNode();
		if (parentNode != NULL && parentNode->IsRootNode() == FALSE)
		{
			Matrix3 parentTM = GetWorldTransform(parentNode);
			parentTM.Invert();
			firstInstanceTM *= parentTM;
		}

		firstInstanceTM.Invert();
		tm *= firstInstanceTM;
	}

	Control* c = (isAnimatable) ? node->GetTMController() : NULL;
	SClass_ID Test1 = c->SuperClassID();
	Class_ID Test2 = c->ClassID();
	Test1; Test2;
	ExportTransformElements(colladaNode, node, c, tm);
	return true;
}

void  NodeExporter::ExportTransformElements(FCDSceneNode* colladaNode, INode* node, Control* c, const Matrix3& tm)
{
	if (OPTS->DoSampleMatrices(node))
	{
		// Thanks to KevT for his work on baked matrices
		FCDTMatrix* matrix = (FCDTMatrix*) colladaNode->AddTransform(FCDTransform::MATRIX);
		matrix->SetTransform(ToFMMatrix44(tm));
		ANIM->ExportAnimatedTransform(colladaNode->GetDaeId() + "-transform", node, matrix->GetTransform());
	}
	else
	{
		// Decompose the transform
		AffineParts ap;
		decomp_affine(tm, &ap);
		ap.k *= ap.f; ap.f = 1.0f;

		// Translation
		FCDTTranslation* translation = (FCDTTranslation*) colladaNode->AddTransform(FCDTransform::TRANSLATION);
		translation->SetTranslation(ToFMVector3(ap.t));
		Control* translationController = (c != NULL) ? c->GetPositionController() : NULL;
		ANIM->ExportAnimatedPoint3(colladaNode->GetDaeId() + "-translation", translationController, translation->GetTranslation());

		// Rotation

		// first try with the Rotation controller
		Control* rotationController = (c != NULL) ? c->GetRotationController() : NULL;
		if (rotationController == NULL || !rotationController->IsAnimated())
		{
			// Save as axis-angle rotation.
			Matrix3 qtm;
			ap.q.MakeMatrix(qtm);

			AngAxis angAxisRot = AngAxis(qtm);
			if (!angAxisRot.axis.Equals(Point3::Origin) && !Equals(angAxisRot.angle, TOLERANCE))
			{
				// rotation
				FCDTRotation* rotation = (FCDTRotation*) colladaNode->AddTransform(FCDTransform::ROTATION);
				rotation->SetAxis(ToFMVector3(angAxisRot.axis));
				rotation->SetAngle(-FMath::RadToDeg(angAxisRot.angle));
			}
		}
		else
		{
			ExportRotationElement(rotationController, colladaNode);
		}

		
		// Check for animated scale. If not animated and equal to the identity, don't export.
		// Animated scale includes animated scale axis, so export that carefully.
		Control* scaleController = (c != NULL) ? c->GetScaleController() : NULL;
		bool hasAnimatedScale = scaleController != NULL && scaleController->IsAnimated();
		if (hasAnimatedScale || !ap.k.Equals(Point3(1.0f, 1.0f, 1.0f), TOLERANCE))
		{
			AngAxis scaleRotation(ap.u);
			if (scaleRotation.axis.Equals(Point3::Origin, TOLERANCE))
			{
				scaleRotation.axis = Point3(1.0f, 0.0f, 0.0f);
			}

			// Rotate to match the scale axis
			bool hasScaleAxis = hasAnimatedScale || !(Equals(scaleRotation.angle, 0.0f) ||
				(Equals(ap.k.x, ap.k.y, TOLERANCE) && Equals(ap.k.x, ap.k.z, TOLERANCE)));
			if (hasScaleAxis)
			{
				FCDTRotation* rotation = (FCDTRotation*) colladaNode->AddTransform(FCDTransform::ROTATION);
				rotation->SetAxis(ToFMVector3(scaleRotation.axis));
				rotation->SetAngle(-FMath::RadToDeg(scaleRotation.angle));
				rotation->SetSubId("ScaleAxisR");
				FCDAnimated* animatedScaleAxis = ANIM->ExportAnimatedAngAxis(colladaNode->GetDaeId() + "-sar", scaleController, ColladaMaxAnimatable::SCALE_ROT_AXIS_R, rotation->GetAngleAxis());

				// Once the animation has been exported, verify that there was, indeed, something animated.
				if (animatedScaleAxis == NULL && Equals(scaleRotation.angle, 0.0f))
				{
					SAFE_DELETE(rotation);
					hasScaleAxis = false;
				}
			}

			// Export the scale factors, the basis now follows the given scale axis
			FCDTScale* scale = (FCDTScale*) colladaNode->AddTransform(FCDTransform::SCALE);
			scale->SetScale(ToFMVector3(ap.k));
			ANIM->ExportAnimatedPoint3(colladaNode->GetDaeId() + "-scale", scaleController, scale->GetScale());

			// Rotate back to the rotation basis
			if (hasScaleAxis)
			{
				FCDTRotation* rotation = (FCDTRotation*) colladaNode->AddTransform(FCDTransform::ROTATION);
				rotation->SetAxis(ToFMVector3(scaleRotation.axis));
				rotation->SetAngle(FMath::RadToDeg(scaleRotation.angle));
				rotation->SetSubId("ScaleAxis");
				ANIM->ExportAnimatedAngAxis(colladaNode->GetDaeId() + "-sa", scaleController, ColladaMaxAnimatable::SCALE_ROT_AXIS, rotation->GetAngleAxis());
			}
		}
	}
}


void NodeExporter::ExportRotationElement(Control* rotController, FCDSceneNode* colladaNode)
{
	FUAssert(rotController != NULL && rotController->SuperClassID() == CTRL_ROTATION_CLASS_ID, return);
	if (!rotController->IsAnimated())
	{
		// Save as axis-angle rotation.
		Quat val;
		rotController->GetValue(0, &val, FOREVER, CTRL_ABSOLUTE);
		if (!val.IsIdentity())
		{
			// rotation
			AngAxis angAxisRot(val);
			FCDTRotation* rotation = (FCDTRotation*) colladaNode->AddTransform(FCDTransform::ROTATION);
			rotation->SetAxis(ToFMVector3(angAxisRot.axis));
			rotation->SetAngle(-FMath::RadToDeg(angAxisRot.angle));
		}
	}
	/*
	Unfortunately, this dosent take into account animated weights...
	We are better off just sampling this transform
	else if (rotController->ClassID() == Class_ID(ROTATION_LIST_CLASS_ID, 0))
	{
		int numSubs = rotController->NumSubs();
		for (int i = 0; i < numSubs; i++)
		{
			Control* aRotation = (Control*)rotController->SubAnim(i);
			ExportRotationElement(aRotation, colladaNode);
		}
	}
	*/
	else
	{
		// as eulerangles for output

		float angles[3];
		Quat val;
		rotController->GetValue(OPTS->AnimStart(), &val, FOREVER, CTRL_ABSOLUTE);
		QuatToEuler(val, angles, EULERTYPE_XYZ);

		// Export XYZ euler rotation in Z Y X order in the file
		FCDTRotation* rotation = (FCDTRotation*) colladaNode->AddTransform(FCDTransform::ROTATION);
		rotation->SetAxis(FMVector3::ZAxis);
		rotation->SetAngle(FMath::RadToDeg(angles[2]));
		rotation->SetSubId("RotZ");
		ANIM->ExportAnimatedAngle(colladaNode->GetDaeId() + "-rz", rotController, ColladaMaxAnimatable::ROTATION_Z, rotation->GetAngleAxis(), &FSConvert::RadToDeg);

		rotation = (FCDTRotation*) colladaNode->AddTransform(FCDTransform::ROTATION);
		rotation->SetAxis(FMVector3::YAxis);
		rotation->SetAngle(FMath::RadToDeg(angles[1]));
		rotation->SetSubId("RotY");
		ANIM->ExportAnimatedAngle(colladaNode->GetDaeId() + "-ry", rotController, ColladaMaxAnimatable::ROTATION_Y, rotation->GetAngleAxis(), &FSConvert::RadToDeg);

		rotation = (FCDTRotation*) colladaNode->AddTransform(FCDTransform::ROTATION);
		rotation->SetAxis(FMVector3::XAxis);
		rotation->SetAngle(FMath::RadToDeg(angles[0]));
		rotation->SetSubId("RotX");
		ANIM->ExportAnimatedAngle(colladaNode->GetDaeId() + "-rx", rotController, ColladaMaxAnimatable::ROTATION_X, rotation->GetAngleAxis(), &FSConvert::RadToDeg);
	}
}

FCDSceneNode* NodeExporter::ExportPivotNode(INode* node, FCDSceneNode* colladaNode)
{
	// Calculate the pivot transform. It should already be in local space.
	Matrix3 tm(1); ApplyPivotTM(node, tm);
	// only export the pivot node if the transform is not an identity
	// of if the node is a group head node (this is a temporary fix until we add a PIVOT type)
	if (tm.IsIdentity() && node->IsGroupHead() == FALSE) return colladaNode;

	// insert pivot node for the pivot.
	FCDSceneNode* colladaPivot = colladaNode->AddChildNode();
	colladaPivot->SetDaeId(colladaNode->GetDaeId() + "_PIVOT");
	colladaPivot->SetName(colladaNode->GetName() + FC("_PIVOT"));

	// Thanks to KevT for his work on baked matrices
	FCDTMatrix* matrix = (FCDTMatrix*) colladaPivot->AddTransform(FCDTransform::MATRIX);
	matrix->SetTransform(ToFMMatrix44(tm));
	ANIM->ExportWSMPivot(colladaPivot->GetDaeId() + "-transform", node, matrix->GetTransform());
	return colladaPivot;
}

FCDSceneNode* NodeExporter::FindExportedNode(INode* node)
{
	ExportNodeMap::iterator it = exportNodes.find(node);
	return (it != exportNodes.end()) ? (*it).second->colladaNode : NULL;
}

void NodeExporter::ApplyPivotTM(INode* maxNode, Matrix3& tm)
{

	// When sampling matrices, we apply the sample the ObjTMAfterWSM
	// to get the final 
	if (OPTS->BakeMatrices())
	{
		IDerivedObject* derivedObj = maxNode->GetWSMDerivedObject();
		if (derivedObj != NULL)
		{
			// If we have WSM attached, always export a pivot
			TimeValue t = OPTS->AnimStart();
			tm = maxNode->GetObjTMAfterWSM(t) * Inverse(maxNode->GetNodeTM(t));
			return;
		}
	}

	Point3 opos = maxNode->GetObjOffsetPos();
	Quat orot = maxNode->GetObjOffsetRot();
	ScaleValue oscale = maxNode->GetObjOffsetScale();

	// this should already be in local space
	// only do this if necessary to preserve identity tags
	ApplyScaling(tm, oscale);
	RotateMatrix(tm, orot);
	tm.Translate(opos);

	tm.ValidateFlags();
}

const Matrix3& NodeExporter::GetWorldTransform(INode* node)
{
	NodeTransformMap::iterator itT = worldTransforms.find(node);
	if (itT != worldTransforms.end()) return (*itT).second;

	Matrix3 tm;
	TimeValue t = OPTS->AnimStart();
	//if (OPTS->IncludeObjOffset(node)) tm = node->GetObjTMAfterWSM(t);
	//else tm = node->GetNodeTM(t);
	tm = node->GetNodeTM(t);

	NodeTransformMap::iterator out = worldTransforms.insert(node, tm);
	return out->second;
}

const Matrix3& NodeExporter::GetLocalTransform(INode* node)
{
	NodeTransformMap::iterator itT = localTransforms.find(node);
	if (itT != localTransforms.end()) return (*itT).second;
	INode *parent = node->GetParentNode();
	// Use NodeTM to be compatible with absolute controllers (lookats etc).
	// bake matricies also include pivots & WSM, but only if not bones.
	Matrix3 tm = GetWorldTransform(node);

	if (parent != NULL && !parent->IsRootNode())
	{
		tm *= Inverse(GetWorldTransform(parent));
	}
	NodeTransformMap::iterator out = localTransforms.insert(node, tm);
	return out->second;
}


