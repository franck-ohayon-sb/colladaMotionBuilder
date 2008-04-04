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

// Scene Node Importer Class

#include "StdAfx.h"
#include "ColladaIDCreator.h"
#include "AnimationImporter.h"
#include "DocumentImporter.h"
#include "EntityImporterFactory.h"
#include "GeometryImporter.h"
#include "MaterialImporter.h"
#include "TransformImporter.h"
#include "NodeImporter.h"
#include "MorpherImporter.h"
#include "SkinImporter.h"
#include "XRefImporter.h"
#include "XRefManager.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDController.h"
#include "FCDocument/FCDEntity.h"
#include "FCDocument/FCDEntityInstance.h"
#include "FCDocument/FCDEntityReference.h"
#include "FCDocument/FCDExtra.h"
#include "FCDocument/FCDTransform.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDTargetedEntity.h"

#include "FCDocument/FCDGeometry.h"

NodeImporter::NodeImporter(DocumentImporter* document, NodeImporter* parent)
:	EntityImporter(document, parent)
{
	importNode = document->GetImportInterface()->CreateNode();
	transform = new TransformImporter(document, importNode->GetINode());

	colladaSceneNode = colladaPivotNode = NULL;
	colladaInstance = NULL;
	entityImporter = NULL;
	addNode = false;
	isGroupNode = false;
	isGroupMember = false;
}

NodeImporter::~NodeImporter()
{
	SAFE_DELETE(transform);

	importNode = NULL;
	children.clear();
	colladaSceneNode = colladaPivotNode = NULL;
	colladaInstance = NULL;
	entityImporter = NULL;
}

Matrix3 NodeImporter::GetWorldTransform(TimeValue t)
{
	return importNode->GetINode()->GetNodeTM(t);
}

Matrix3 NodeImporter::GetLocalTransform(TimeValue t)
{
    Matrix3 transform = importNode->GetINode()->GetNodeTM(t);
	if (GetParent() != NULL)
	{
		Matrix3 parentTransform = GetParent()->importNode->GetINode()->GetNodeTM(t);
		parentTransform.Invert();
		transform *= parentTransform;
	}
	return transform;
}

Matrix3 NodeImporter::GetPivotTransform()
{
	INode* inode = importNode->GetINode();
	Point3 opos = inode->GetObjOffsetPos();
	Quat orot = inode->GetObjOffsetRot();
	ScaleValue oscale = inode->GetObjOffsetScale();

	// this should already be in local space
	Matrix3 tm(1);
	ApplyScaling(tm, oscale);
	RotateMatrix(tm, orot);
	tm.Translate(opos);
	return tm;
}

// Determine whether the given COLLADA node could be a pivot
bool NodeImporter::IsPossiblePivot(FCDSceneNode* colladaSceneNode)
{
	// A camera/light target node cannot be a pivot
	if (colladaSceneNode->IsTarget()) return false;

	// A joint cannot be a pivot either
	if (colladaSceneNode->GetJointFlag()) return false;

	// A pivot node must not have children
	if (colladaSceneNode->GetChildrenCount() != 0) return false;

	// A pivot can have zero or one entities
	if (colladaSceneNode->GetInstanceCount() > 1) return false;

	// If we are a targetted object, make sure that we dont pivot down so
	// that our target depends on us!
	if (colladaSceneNode->GetInstanceCount() == 1)
	{
		FCDEntityInstance* instance = colladaSceneNode->GetInstance(0);
		if (instance->IsExternalReference()) return false;
		FCDEntity* entity = instance->GetEntity();
		FUAssert(entity != NULL, return false);
		
		if (entity->HasType(FCDTargetedEntity::GetClassType()))
		{
			FCDTargetedEntity* targetedEntity = (FCDTargetedEntity*)entity;
			if (targetedEntity->HasTarget())
			{
				FCDSceneNode* target = targetedEntity->GetTargetNode();
				FCDSceneNode* camera = colladaSceneNode->GetParent(); // We can assume this is non-null

				if (camera->FindDaeId(target->GetDaeId())) return false;
			}
		}
	}

	// A pivot cannot be animated
	size_t transformCount = colladaSceneNode->GetTransformCount();
	for (size_t t = 0; t < transformCount; ++t)
	{
		if (colladaSceneNode->GetTransform(t)->IsAnimated()) return false;
		if (colladaSceneNode->GetTransform(t)->IsType(FCDTLookAt::GetClassType())) return false;
	}

	return true;
}

// Calculate the Axis-aligned bounding box for this Max scene node
Box3 NodeImporter::GetBoundingBox(Matrix3& localTransform)
{
	bool boundingBoxInitialized = false;
	Box3 boundingBox;
	if (!children.empty())
	{
		NodeImporter* firstChild = children.front();
		Matrix3 transform = firstChild->GetLocalTransform(0) * localTransform;
		boundingBox = children.front()->GetBoundingBox(transform);
		boundingBoxInitialized = true;
		size_t childCount = children.size();
		for (size_t i = 1; i < childCount; ++i)
		{
			transform = children[i]->GetLocalTransform(0) * localTransform;
			boundingBox += children[i]->GetBoundingBox(transform);
		}
	}

	if (entityImporter != NULL && entityImporter->GetType() == EntityImporter::GEOMETRY)
	{
		Matrix3 pivotedTransform = GetPivotTransform() * localTransform;
		GeometryImporter* geometryImporter = (GeometryImporter*) entityImporter;
		if (geometryImporter->GetColladaGeometry() != NULL && geometryImporter->GetMeshObject() != NULL)
		{
			Box3 geometryBoundingBox;
			if (geometryImporter->IsEditablePoly())
			{
				geometryBoundingBox = ((PolyObject*)geometryImporter->GetMeshObject())->GetMesh().getBoundingBox(&pivotedTransform);
			}
			else
			{
				geometryBoundingBox = ((TriObject*)geometryImporter->GetMeshObject())->GetMesh().getBoundingBox(&pivotedTransform);
			}
			 
			if (!boundingBoxInitialized) boundingBox = geometryBoundingBox;
			else boundingBox += geometryBoundingBox;
			boundingBoxInitialized = true;
		}
	}

	if (!boundingBoxInitialized)
	{
		boundingBox.pmin = Point3(-0.5, -0.5, -0.5);
		boundingBox.pmax = Point3(0.5, 0.5, 0.5);
		boundingBox.pmin = localTransform.PointTransform(boundingBox.pmin);
		boundingBox.pmax = localTransform.PointTransform(boundingBox.pmax);
	}
	return boundingBox;
}

// Import into Max a scene node
ImpNode* NodeImporter::ImportSceneNode(FCDSceneNode* _colladaSceneNode)
{
	colladaSceneNode = _colladaSceneNode;

	// Set the node's name
	const fm::string* name = &colladaSceneNode->GetName();
	if (name->empty()) name = &colladaSceneNode->GetDaeId();
	if (!name->empty()) importNode->SetName(name->c_str());

	// Look amongst the children for a valid pivot
	colladaPivotNode = NULL;
	size_t childrenCount = colladaSceneNode->GetChildrenCount();
	if (colladaSceneNode->GetInstanceCount() == 0)
	{
		// Groups of 1 are just confusing to a user...
		if (childrenCount > 1)
		{
			// No instances on a normal node is a group head node.
			//isGroupNode = !colladaSceneNode->IsJoint();
			addNode = true; // Always add with multiple children.
		}
		for (size_t c = 0; c < childrenCount; ++c)
		{
			if (IsPossiblePivot(colladaSceneNode->GetChild(c)))
			{
				// This node is now the pivot
				colladaPivotNode = colladaSceneNode->GetChild(c);
				isGroupNode = false;
				break;
			}
		}
	}

	// Import the transform
	ImportTransforms();

	// Create its children
	for (size_t c = 0; c < childrenCount; ++c)
	{
		FCDSceneNode* child = colladaSceneNode->GetChild(c);
		if (child == colladaPivotNode) continue;
		Instantiate(child);
	}

	return importNode;
}

void NodeImporter::ImportTransforms()
{
	// Import the node's transformation stack
	transform->ImportSceneNode(colladaSceneNode);

	if (colladaPivotNode != NULL)
	{
		// Import the pivot transformation into this node
		transform->ImportPivot(colladaPivotNode);
	}
}

void NodeImporter::ImportInstances()
{
	// Recurse through the children of this node
	for (NodeImporterList::iterator it = children.begin(); it != children.end(); ++it)
	{
		(*it)->ImportInstances();
	}

	// Create the attached object(s)
	FCDSceneNode* instancingNode = (colladaPivotNode == NULL) ? colladaSceneNode : colladaPivotNode;
	size_t instanceCount = instancingNode->GetInstanceCount();
	
	// Add if - We are instanced: or we are referenced by something else (that will be imported)]
	addNode |= !children.empty() || colladaSceneNode->IsTarget() || colladaSceneNode->GetJointFlag()
		|| (colladaSceneNode->GetChildrenCount() == 0 && instanceCount == 0) // node is a locator.
		|| (colladaSceneNode->GetChildrenCount() == 1 && instanceCount == 0 && colladaPivotNode != NULL);  // we are pivoting the locator..
	for (size_t i = 0; i < instanceCount; ++i)
	{
		FCDEntityInstance* instance = instancingNode->GetInstance(i);
		if (instance->IsExternalReference() || instance->GetEntityType() != FCDEntity::SCENE_NODE)
		{
			addNode |= !Instantiate(instance);
		}
		else
		{
			// Create transform nodes below this one for all the scene node instances
			NodeImporter* nodeImporter = new NodeImporter(GetDocument(), this);
			// We need an instance?
			nodeImporter->addNode = true;
			fm::string patchedDaeId = colladaSceneNode->GetDaeId() + "_instance_";
			nodeImporter->GetImportNode()->SetName(patchedDaeId.c_str());
			GetDocument()->AddInstance(patchedDaeId, nodeImporter);
			importNode->GetINode()->AttachChild(nodeImporter->GetImportNode()->GetINode(), FALSE);
			children.push_back(nodeImporter);
			nodeImporter->Instantiate(instance);
		}
	}
}

// Create a given COLLADA entity in this Max node
bool NodeImporter::Instantiate(FCDEntityInstance* _colladaInstance)
{
	// Only one object entity per node is supported by Max.
	if (entityImporter != NULL)
	{
		NodeImporter* instanceNodeImporter = new NodeImporter(GetDocument(), this);
		fm::string patchedDaeId = colladaSceneNode->GetDaeId() + "_instance";
		if (_colladaInstance->GetEntity() != NULL) patchedDaeId += _colladaInstance->GetEntity()->GetDaeId(); 
		instanceNodeImporter->GetImportNode()->SetName(patchedDaeId.c_str());
		GetDocument()->AddInstance(patchedDaeId, instanceNodeImporter);
		GetImportNode()->GetINode()->AttachChild(instanceNodeImporter->GetImportNode()->GetINode(), FALSE);
		children.push_back(instanceNodeImporter);
		instanceNodeImporter->addNode = instanceNodeImporter->Instantiate(_colladaInstance);;
		return true;
	}

	// Special case: XRef
	Object* object = NULL;
	bool deleteNode = false;
	if (_colladaInstance->IsExternalReference())
	{
		// Scene nodes are simply grafted onto our hierarchy
		if (_colladaInstance->GetEntityType() == FCDEntity::SCENE_NODE)
		{
			// Attach the children to this Max node.
			if (importNode->GetINode() != NULL)
			{
				INode* xrefNode = (INode*)GetDocument()->GetXRefManager()->GetXRefItem(_colladaInstance->GetEntityReference()->GetUri(), FCDEntity::SCENE_NODE);
				if (xrefNode != NULL) importNode->GetINode()->AttachChild(xrefNode, FALSE);
				else deleteNode = true;
			}
		}
		else
		{
			XRefImporter* xRefImporter = new XRefImporter(GetDocument(), this);
			if (xRefImporter->FindXRef(_colladaInstance->GetEntityReference()->GetUri(), FCDEntity::GEOMETRY))
			{
				object = xRefImporter->GetObject();
				colladaInstance = _colladaInstance;
				entityImporter = xRefImporter;
			}
			else
			{
				SAFE_DELETE(xRefImporter);
			}
		}
	}
	else
	{
		// Verify and process the COLLADA entity instance
		FCDEntity* colladaEntity = _colladaInstance->GetEntity();
		if (colladaEntity != NULL)
		{
			if (colladaEntity->GetType() == FCDEntity::SCENE_NODE)
			{
				Instantiate((FCDSceneNode*) colladaEntity);
				return false;
			}
			colladaInstance = _colladaInstance;

			// Retrieve/Create an instance of this entity
			fm::string colladaId = colladaEntity->GetDaeId();
			EntityImporter* instanceImporter = GetDocument()->FindInstance(colladaId);
			if (instanceImporter == NULL)
			{
				entityImporter = EntityImporterFactory::Create(colladaEntity, GetDocument(), this);
				if (entityImporter != NULL)
				{
					object = entityImporter->GetObject();

					if (!colladaId.empty())
					{
						// New instance for this entity, add it to the document's list of instances
						GetDocument()->AddInstance(colladaId, entityImporter);
					}
				}
				else 
				{
					deleteNode = true;
				}
			}
			else
			{
				// This is a second instance of this entity
				entityImporter = instanceImporter;
				entityImporter->AddParent(this);
				object = entityImporter->GetObject();
			}
		}
	}

	if (object != NULL) 
	{
		// add the custom COLLADA XRef attribute on the object
		// use the entity ID at import
		fm::string id;
		if (_colladaInstance->IsExternalReference())
		{
			id = _colladaInstance->GetEntityReference()->GetUri().GetFragment();
		}
		else
		{
			id = _colladaInstance->GetEntity()->GetDaeId();
		}

		importNode->Reference(object);
	}
	return deleteNode;
}

// Create a given COLLADA entity in this Max node
void NodeImporter::Instantiate(FCDSceneNode* colladaSceneNode)
{
	// Retrieve/Create an instance of this entity
	fm::string colladaId = colladaSceneNode->GetDaeId();
	//EntityImporter* instanceImporter = GetDocument()->FindInstance(colladaId);
	//if (instanceImporter == NULL)
	//{
		EntityImporter* importer = EntityImporterFactory::Create(colladaSceneNode, GetDocument(), this);
		if (importer == NULL || importer->GetType() != EntityImporter::NODE) return;

		// Attach the children to this Max node.
		NodeImporter* nodeImporter = (NodeImporter*) importer;
		importNode->GetINode()->AttachChild(nodeImporter->GetImportNode()->GetINode(), FALSE);
		children.push_back(nodeImporter);

		if (!colladaId.empty())
		{
			// New instance for this entity, add it to the document's list of instances
			GetDocument()->AddInstance(colladaId, importer);
		}
	//}
}

void NodeImporter::CreateDummies()
{
	// Create a dummy when there are no references for the node
	if (addNode && entityImporter == NULL)
	{
		Object* helper = NULL;
		if (colladaSceneNode != NULL && colladaSceneNode->IsTarget())
		{
			helper = (Object*) GetDocument()->MaxCreate(GEOMOBJECT_CLASS_ID, Class_ID(TARGET_CLASS_ID, 0));
		}
		else // generic dummy.
		{
			DummyObject* dummy = (DummyObject*) GetDocument()->MaxCreate(HELPER_CLASS_ID, Class_ID(DUMMY_CLASS_ID, 0));
			helper = dummy;

			// Check the helper bounding box extra information
			FCDSceneNode* objectNode = colladaPivotNode != NULL ? colladaPivotNode : colladaSceneNode;
			Box3 boundingBox;
			bool bboxFound = false;
			if (objectNode != NULL && objectNode->GetExtra() != NULL)
			{			
				FCDETechnique* maxExtra = objectNode->GetExtra()->GetDefaultType()->FindTechnique("MAX3D");
				if (maxExtra != NULL)
				{
					FCDENode* helperNode = maxExtra->FindChildNode("helper");
					if (helperNode != NULL)
					{
						FCDENode* bbminNode = helperNode->FindParameter("bounding_min");
						FCDENode* bbmaxNode = helperNode->FindParameter("bounding_max");
						if (bbminNode != NULL && bbmaxNode != NULL)
						{
							FMVector3 bbmin = FUStringConversion::ToVector3(bbminNode->GetContent());
							FMVector3 bbmax = FUStringConversion::ToVector3(bbmaxNode->GetContent());
							boundingBox.pmin = ToPoint3(bbmin);
							boundingBox.pmax = ToPoint3(bbmax);
							bboxFound = true;
						}
					}
				}
			}
		
			if (!bboxFound)
			{
				Matrix3 pivotTransform = GetPivotTransform();
				pivotTransform.Invert();
				boundingBox = GetBoundingBox(pivotTransform);
			}
			dummy->SetBox(boundingBox);
			dummy->EnableDisplay();
		}

		importNode->Reference(helper);
	}

	// Recurse through the children of this node
	for (NodeImporterList::iterator it = children.begin(); it != children.end(); ++it)
	{
		(*it)->CreateDummies();
	}
}

void NodeImporter::Finalize(FCDEntityInstance*)
{
//	if (!validNode) return;

	// Add this node to the 'imported' scene.
	// It will be verified and added to the main scene after this plugin is done.
	if (addNode) GetDocument()->GetImportInterface()->AddNodeToScene(importNode);

	// Check the group flag
	if (isGroupNode)
	{
		importNode->GetINode()->SetGroupHead(TRUE);
		for (NodeImporterList::iterator it = children.begin(); it != children.end(); ++it)
		{
			(*it)->isGroupMember = true;
		}
	}

	// Check the group member flag
	if (isGroupMember)
	{
		importNode->GetINode()->SetGroupMember(TRUE);
	}

	// Finalize the entity, this assigns materials for geometries, assigns bones for skin controllers, etc.
	if (entityImporter != NULL)
	{
		// needed information for geometry import
		if (entityImporter->GetType() == EntityImporter::GEOMETRY)
		{
			GeometryImporter *geom = (GeometryImporter *)entityImporter;
			geom->SetCurrentImport(importNode->GetINode());
		}
		else if (entityImporter->GetType() == EntityImporter::SKIN)
		{
			SkinImporter *skin = (SkinImporter*)entityImporter;
			skin->SetTargetCurrentImport(importNode->GetINode());
		}
		else if (entityImporter->GetType() == EntityImporter::MORPHER)
		{
			MorpherImporter *morph = (MorpherImporter*)entityImporter;
			morph->SetTargetCurrentImport(importNode->GetINode());
		}
		entityImporter->Finalize(colladaInstance);
	}

	// Support finalization of 3dsMax nodes without a COLLADA node.
	// This may happen in the special case where we create 3dsMax child nodes in order to support multiple instances in one COLLADA node.
	if (colladaSceneNode != NULL) 
	{
		// Check the joint flag
		if (colladaSceneNode->GetJointFlag())
		{
			bool hasDummy = entityImporter == NULL;
			importNode->GetINode()->ShowBone(hasDummy ? 2 : 1);
		}

		// Read in the extra user-properties note
		if (colladaSceneNode->HasNote())
		{
			const fm::string& colladaNote = colladaSceneNode->GetNote();
			importNode->GetINode()->SetUserPropBuffer(colladaNote.c_str());
		}

		// Finalize the import of the transforms
		transform->Finalize();

		// Recurse through the children of this node
		for (NodeImporterList::iterator it = children.begin(); it != children.end(); ++it)
		{
			(*it)->Finalize();
		}

		// Import the visibility track.
		INode* node = importNode->GetINode();
		importNode->GetINode()->SetVisibility(TIME_INITIAL_POSE, colladaSceneNode->GetVisibility());
		if (colladaSceneNode->GetVisibility().IsAnimated())
		{
			Control* visibilityController = node->GetVisController();
			if (visibilityController == NULL)
			{
				visibilityController = (Control*) GetDocument()->MaxCreate(CTRL_FLOAT_CLASS_ID, Class_ID(HYBRIDINTERP_FLOAT_CLASS_ID, 0));
				node->SetVisController(visibilityController);
			}
			ANIM->FillFloatController(visibilityController, colladaSceneNode->GetVisibility(), true);
		}

		// Import the FCollada enitty information for this scene node.
		EntityImporter::Finalize(colladaSceneNode, importNode->GetINode());
	}
}
