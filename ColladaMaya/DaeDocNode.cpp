/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "DaeDocNode.h"
#include "DaeEntityManager.h"
#include "TranslatorHelpers/CDagHelper.h"

#include <maya/MFnCompoundAttribute.h>
#include <maya/MFnTypedAttribute.h>

//
// DaeDocNode
//

MTypeId DaeDocNode::id(0x0005052B);
MObject DaeDocNode::rootAttribute;
MObject DaeDocNode::filenameAttribute;
MObject DaeDocNode::entityIdAttribute;
MObject DaeDocNode::entityLinkAttribute;

// Plug-in interface
void* DaeDocNode::creator()
{
	return new DaeDocNode();
}

MStatus DaeDocNode::initialize()
{
	MStatus status;
	MFnCompoundAttribute complexFn;
	MFnTypedAttribute typedFn;

	// Create the leaf document filename attribute
	filenameAttribute = typedFn.create("filename", "fn", MFnData::kString);
	typedFn.setStorable(true);
	
	// Create the leaf attribute for the entity ids and links
	entityIdAttribute = typedFn.create("entity", "e", MFnData::kString);
	typedFn.setStorable(true);
	typedFn.setArray(true);

	// entityLinkAttribute = typedFn.create("collada", "dae", MFnData::kString);
	// typedFn.setStorable(false);
	// typedFn.setArray(false);

	// Create the root attribute for the per-document information and links
	rootAttribute = complexFn.create("document", "doc");
	complexFn.addChild(filenameAttribute);
	complexFn.addChild(entityIdAttribute);
	complexFn.setArray(true);
	complexFn.setStorable(true);

	// Add the plug tree to the node definition.
	status = addAttribute(rootAttribute);
	if (status != MStatus::kSuccess)
	{
		status.perror("DaeDocNode::initialize() - addAttribute(rootAttribute)");
		return status;
	}

	// Create the global COLLADA documents information node.
	MObject colladaNodeObject = CDagHelper::GetNode("colladaDocuments");
	if (colladaNodeObject.isNull())
	{
		MFnDependencyNode colladaNodeFn;
		colladaNodeFn.create(DaeDocNode::id, "colladaDocuments");
	}

	return MStatus::kSuccess;
}

DaeDocNode::DaeDocNode()
{
}

DaeDocNode::~DaeDocNode()
{
	CLEAR_POINTER_VECTOR(documents);
}

// Document Management
DaeDoc* DaeDocNode::FindDocument(uint logicalIndex)
{
	for (DaeDocList::iterator it = documents.begin(); it != documents.end(); ++it)
	{
		if ((*it)->GetLogicalIndex() == logicalIndex) return *it;
	}
	return NULL;
}

DaeDoc* DaeDocNode::FindDocument(const MString& filename)
{
	for (DaeDocList::iterator it = documents.begin(); it != documents.end(); ++it)
	{
		if ((*it)->GetFilename() == filename) return *it;
	}
	return NULL;
}

DaeDoc* DaeDocNode::FindDocument(FCDocument* colladaDocument)
{
	for (DaeDocList::iterator it = documents.begin(); it != documents.end(); ++it)
	{
		if ((*it)->GetCOLLADADocument() == colladaDocument) return *it;
	}
	return NULL;
}

DaeDoc* DaeDocNode::NewDocument(const MString& filename)
{
	uint nextLogicalIndex = Synchronize();

	// Create the COLLADA document object.
	DaeDoc* document = new DaeDoc(this, filename, nextLogicalIndex);
	documents.push_back(document);

	// Store the document filename information in the correct plug
	MPlug rootPlug(thisMObject(), rootAttribute);
	MPlug documentRootPlug = rootPlug.elementByLogicalIndex(nextLogicalIndex);
	MPlug filenamePlug = documentRootPlug.child(filenameAttribute);
	CDagHelper::SetPlugValue(filenamePlug, fm::string(filename.asChar()));
	return document;
}

void DaeDocNode::ReleaseDocument(DaeDoc* document)
{
	uint logicalIndex = document->GetLogicalIndex();

	// Release the COLLADA document object
	DaeDocList::iterator it = documents.find(document);
	if (it != documents.end()) documents.erase(it);
	SAFE_DELETE(document);

	// Remove the filename information from the node attributes
	MPlug rootPlug(thisMObject(), rootAttribute);
	MPlug documentRootPlug = rootPlug.elementByLogicalIndex(logicalIndex);
	MPlug filenamePlug = documentRootPlug.child(filenameAttribute);
	CDagHelper::SetPlugValue(filenamePlug, emptyString);

	// Disconnect any connection(s) left in the entity attributes
	MPlug entitiesPlug = documentRootPlug.child(entityIdAttribute);
	if (entitiesPlug.numElements() > 0)
	{
		CDagHelper::SetArrayPlugSize(entitiesPlug, 0);
	}
}

void DaeDocNode::ReleaseAllEntityNodes()
{
	for (DaeDocList::iterator it = documents.begin(); it != documents.end(); ++it)
	{
		(*it)->GetEntityManager()->ReleaseNodes();
	}
}

void DaeDocNode::LinkEntity(DaeDoc* document, DaeEntity* entity, MObject& node)
{
	MStatus status;

	// Retrieve the logical indices for the document and the entity.
	uint docIndex = document->GetLogicalIndex();
	uint entIndex = document->GetEntityManager()->GetEntityIndex(entity);

	// Assign the COLLADA id to the correct plug.
	MPlug rootPlug(thisMObject(), rootAttribute);
	MPlug documentRootPlug = rootPlug.elementByLogicalIndex(docIndex);
	MPlug entitiesPlug = documentRootPlug.child(entityIdAttribute);
	MPlug entityPlug = entitiesPlug.elementByLogicalIndex(entIndex);
	CDagHelper::SetPlugValue(entityPlug, entity->entity->GetDaeId());

	// Add to the Maya object the entity linkage plug.
	MFnDependencyNode nodeFn(node);
	nodeFn.findPlug("collada", &status);
	if (status != MStatus::kSuccess)
	{
		MStatus st;
		MString result;
		if (node.hasFn(MFn::kDagNode))
		{
			MDagPath p = MDagPath::getAPathTo(node);
			MString cmd = MString("addAttr -sn \"dae\" -ln \"collada\" -dt \"string\" ") + p.fullPathName();
			st = MGlobal::executeCommand(MString("addAttr -sn \"dae\" -ln \"collada\" -dt \"string\" ") + p.fullPathName(), result);
		}
		else
		{
			st = MGlobal::executeCommand(MString("addAttr -sn \"dae\" -ln \"collada\" -dt \"string\" ") + nodeFn.name(), result);
		}
		MString err1 = st.errorString();
		MString err2 = status.errorString();
//		nodeFn.addAttribute(entityLinkAttribute, MFnDependencyNode::kLocalDynamicAttr);
	}
	MPlug linkagePlug = nodeFn.findPlug("collada");
	CDagHelper::Connect(entityPlug, linkagePlug);
}

DaeEntity* DaeDocNode::FindEntity(const MObject& node, DaeDoc** document)
{
	MStatus status;

	// Retrieve the entity linkage plug on this node.
	MFnDependencyNode nodeFn(node);
	MPlug linkagePlug = nodeFn.findPlug("collada", &status);
	if (status != MStatus::kSuccess) return NULL;

	// Follow the connection back to the COLLADA document node.
	MPlug entityPlug;
	CDagHelper::GetPlugConnectedTo(linkagePlug, entityPlug);
	if (entityPlug.node().isNull()) return NULL;
	
	// Retrieve the indexation information from the plug tree.
	uint entityIndex = entityPlug.logicalIndex();
	MPlug entitiesPlug = entityPlug.array();
	MPlug documentPlug = entitiesPlug.parent();
	uint documentIndex = documentPlug.logicalIndex();

	// Retrieve the document and the entity structure
	DaeDoc* doc = FindDocument(documentIndex);
	if (doc == NULL) return NULL;
	DaeEntityManager* manager = doc->GetEntityManager();
	if (entityIndex >= manager->GetEntityCount()) return NULL;
	if (document != NULL) *document = doc;

	// Reset the entity's node pointer before returning it:
	// Maya modifies these without warnings.
	DaeEntity* e = manager->GetEntity(entityIndex);
	if (e->GetNode() == node) return e; // Something about MB step that kill extra preservation.
	return NULL;
}

uint DaeDocNode::Synchronize()
{
	MStatus stat;
	FUAssert(!thisMObject().isNull(), return 0);

	// Find the next available logical index.
	MPlug rootPlug(thisMObject(), rootAttribute);
	UInt32List takenLogicalIndices;
	for (uint i = 0; i < rootPlug.numElements(); ++i)
	{
		MPlug physicalPlug = rootPlug.elementByPhysicalIndex(i);
		MPlug docFilename = physicalPlug.child(filenameAttribute, &stat);
		if (stat == MStatus::kSuccess)
		{
			MString filename;
			docFilename.getValue(filename);
			uint logicalIndex = physicalPlug.logicalIndex();
			if (FindDocument(logicalIndex) != NULL || filename.length() != 0)
			{
				takenLogicalIndices.push_back(logicalIndex);
			}
			// Note that if we have no document and the filename does exist, we should import this document, somehow,
			// and attempt to re-create the DaeEntities.
		}
	}

	size_t count = takenLogicalIndices.size();
	for (uint32 i = 0; i < count; ++i)
	{
		if (takenLogicalIndices.find(i) == takenLogicalIndices.end()) return i;
	}
	return (uint) count;
}
