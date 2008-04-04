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

// Morpher Modifier Importer Class

#include "StdAfx.h"
#include "modstack.h"
#include "AnimationImporter.h"
#include "DocumentImporter.h"
#include "EntityImporterFactory.h"
#include "GeometryImporter.h"
#include "MorpherImporter.h"
#include "NodeImporter.h"
#include "SkinImporter.h"
#include "Common/ConversionFunctions.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDGeometryMesh.h"
#include "FCDocument/FCDGeometrySource.h"
#include "FCDocument/FCDMorphController.h"
#include "IMorpher/IMorpher.h"

MorpherImporter::MorpherImporter(DocumentImporter* document, NodeImporter* parent)
:	EntityImporter(document, parent)
{
	colladaMorph = NULL;
	derivedObject = NULL;
	morpherModifier = NULL;
	target = NULL;
}

MorpherImporter::~MorpherImporter()
{
	SAFE_DELETE(target);

	colladaMorph = NULL;
	derivedObject = NULL;
	morpherModifier = NULL;
}

// Retrieves the object to reference in the parent node(s).
Object* MorpherImporter::GetObject()
{
	return derivedObject;
}

void MorpherImporter::AddParent(NodeImporter* parent)
{
	EntityImporter::AddParent(parent);
	if (target != NULL) target->AddParent(parent);
}

// Main entry point for the class.
// Create a Max mesh-derived object, with the morpher modifier from a COLLADA morph controller.
Object* MorpherImporter::ImportMorphController(FCDMorphController* _colladaMorph)
{
	colladaMorph = _colladaMorph;

	// Import the COLLADA target for this morpher
	FCDEntity* colladaTarget = colladaMorph->GetBaseTarget();
	if (colladaTarget == NULL) return NULL;
	target = EntityImporterFactory::Create(colladaTarget, GetDocument(), GetParent());
	if (target == NULL) return NULL;

	// Create the morpher modifier
	morpherModifier = (MorphR3*) GetDocument()->MaxCreate(OSM_CLASS_ID, MORPHER_CLASS_ID);
	if (morpherModifier == NULL) return NULL;

	// Seems like that mFileVersion is never set. Great.
	// if (morpherModifier->mFileVersion != MORPHER_FILE_VERSION) return NULL; // Ask your favorite programmer to upgrade/downgrade the morpher interface

	Object* object = target->GetObject();
	if (object->ClassID() == derivObjClassID || object->ClassID() == WSMDerivObjClassID)
	{
		// Object is a derived object, just attach ourselves to it
		derivedObject = (IDerivedObject*) object;
	}
	else
	{
		// Create the derived object for the target and the modifier
		derivedObject = CreateDerivedObject(object);
	}
	derivedObject->AddModifier(morpherModifier);
	morpherModifier->cache.MakeCache(object);
	return derivedObject;
}

bool MorpherImporter::SetTargetCurrentImport(INode* currImport)
{
	if (target == NULL) return false;
	if (target->GetType() == EntityImporter::GEOMETRY)
	{
		GeometryImporter *geom = (GeometryImporter*)target;
		geom->SetCurrentImport(currImport);
		return true;
	}
	else if (target->GetType() == EntityImporter::SKIN)
	{
		SkinImporter *skin = (SkinImporter*)target;
		skin->SetTargetCurrentImport(currImport);
		return true;
	}

	return false;
}

// Finalize the import of the morpher modifier. Call after the scene graph has been created
void MorpherImporter::Finalize(FCDEntityInstance* instance)
{
	// Finalize the target controller/mesh
	if (target != NULL) target->Finalize(instance);

	if (IsFinalized()) return;

	// Initialize the object cache for the morpher
	// This function expects the base object, not a derived one
	Object* object = target->GetObject();
	while (object->SuperClassID() == GEN_DERIVOB_CLASS_ID)
	{
		object = ((IDerivedObject*)object)->GetObjRef();
	}
	morpherModifier->cache.MakeCache(object);

	// There is a maximum number of channels supported by the 3dsMax morpher:
	// Calculate the number of channels to process
	size_t colladaTargetCount = colladaMorph->GetTargetCount();
	int channelCount = (int) min(colladaTargetCount, morpherModifier->chanBank.size());
	for (int i = 0; i < channelCount; ++i)
	{
		const FCDGeometry* colladaTarget = colladaMorph->GetTarget(i)->GetGeometry();
		if (!colladaTarget->IsMesh()) continue;
        morphChannel* channel = &morpherModifier->chanBank[i];
		
		// The 3dsMax morpher prefers geometries that are instantiated in scene nodes,
		// unlike COLLADA, so check for a scene node that uses this mesh.
		GeometryImporter* targetGeometry = (GeometryImporter*) GetDocument()->FindInstance(colladaTarget->GetDaeId());
		if (targetGeometry != NULL)
		{
			if (targetGeometry->GetType() != EntityImporter::GEOMETRY) continue;
			NodeImporter* targetNode = targetGeometry->GetParent();
			if (targetNode == NULL) continue;

			// Create the morph channel from this target scene node
			INode* targetINode = targetNode->GetImportNode()->GetINode();
			channel->buildFromNode(targetINode);
			channel->mConnection = targetINode;
		}
		else
		{
			// Manually initializes this channel
			InitializeChannelGeometry(channel, colladaTarget);
		}

		// Set the morph weight directly or as a controller
		const FCDParameterAnimatableFloat& colladaWeight = colladaMorph->GetTarget(i)->GetWeight();
		if (colladaWeight.IsAnimated())
		{
			FCDAnimated* animated = const_cast<FCDAnimated*>(colladaWeight.GetAnimated());
			animated->SetArrayElement(i);
			ANIM->ImportAnimatedFloat(channel->cblock, 0u, animated, &FSConvert::ToPercent);
		}
		else
		{
			channel->cblock->SetValue(0, 0, *colladaWeight * 100);
		}
	}

	EntityImporter::Finalize(instance);
}

// COLLADA doesn't require that a mesh be instantiated to be a morph target:
// Manually fill in the channel with the COLLADA mesh data.
// Heavily based on morphChannel::buildFromNode.
void MorpherImporter::InitializeChannelGeometry(morphChannel* channel, const FCDGeometry* geometry)
{
	if (channel == NULL || geometry == NULL || !geometry->IsMesh()) return;

	// Retrieve the COLLADA mesh vertex positions
	const FCDGeometryMesh* mesh = geometry->GetMesh();
	const FCDGeometrySource* positionSource = mesh->GetPositionSource();
	if (positionSource == NULL) return;
	if (positionSource->GetStride() < 3 || positionSource->GetDataCount() == 0) return;

	// Verify that the COLLADA source size matches the original mesh's vertex count
	int vertexCount = (int) positionSource->GetValueCount();
	if (vertexCount != channel->mp->cache.count) return;

	// Set the channel's name and the necessary flags
	channel->mName = geometry->GetName().c_str();
	channel->mInvalid = FALSE;
	channel->mActive = TRUE;
	channel->mModded = TRUE;

	// Allocate the channel's buffers and fill them with the relative vertex positions
	channel->AllocBuffers(vertexCount, vertexCount);
	channel->mSel.SetSize(vertexCount);
	channel->mSel.ClearAll();
	channel->mNumPoints = vertexCount;
	for (int i = 0; i < vertexCount; ++i)
	{
		const float* p = positionSource->GetValue(i);
		channel->mPoints[i] = Point3(p[0], p[1], p[2]);
		channel->mDeltas[i] = (channel->mPoints[i] - channel->mp->cache.points[i]) / 100.0f;
		channel->mSel.Set(i, FALSE);
		channel->mWeights[i] = 1.0f; // Not too sure of what the value should be here.
	}

	// Update all the UI stuff. Important? Must it be done for every channel?
	channel->mp->NotifyDependents(FOREVER,(PartID) PART_ALL,REFMSG_CHANGE);
	channel->mp->NotifyDependents(FOREVER,(PartID) PART_ALL,REFMSG_SUBANIM_STRUCTURE_CHANGED);
	channel->mp->Update_channelFULL();
	channel->mp->Update_channelParams();
}