/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "XRefIncludes.h"
#include "XRefManager.h"
#include "XRefImporter.h"
#include "ColladaImporter.h"
#include "ColladaAttrib.h"

#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDMaterialInstance.h"
#include "FCDocument/FCDPlaceHolder.h"
#include "FCDocument/FCDEntityReference.h"
#include "FCDocument/FCDExternalReferenceManager.h"
#include "FUtils/FUFileManager.h"

#include <ctime>

// Stuff
fm::string GetColladaID(Animatable* maxEntity)
{
	if (maxEntity == NULL) return emptyString;

	ICustAttribContainer* customAttributeContainer = maxEntity->GetCustAttribContainer();
	if (customAttributeContainer != NULL)
	{
		int attributeCount = customAttributeContainer->GetNumCustAttribs();
		for (int i = 0; i < attributeCount; ++i)
		{
			CustAttrib* attribute = customAttributeContainer->GetCustAttrib(i);
			if (attribute->ClassID() == COLLADA_ATTRIB_CLASS_ID)
			{
				// This is a COLLADA custom attribute, mark it for later.
				ColladaAttrib* colladaAttribute = (ColladaAttrib*) attribute;
				return TO_FSTRING(colladaAttribute->GetDaeId());
			}
		}
	}
	return emptyString;
}

void FindNodesByObject(Object* obj, INode* n, fm::pvector<INode>& nodeList)
{
	if (n == NULL)
	{
		n = GetCOREInterface()->GetRootNode();
	}

	if (n->GetObjectRef() == obj)
	{
		nodeList.push_back(n);
	}

	for (int i = 0; i < n->NumberOfChildren(); i++)
	{
		INode* child = n->GetChildNode(i);
		if (child != NULL)
		{
			FindNodesByObject(obj, child, nodeList);
		}
	}
}

// XRefRecord implementation
class XRefRecord 
{
public:

#if USE_MAX_8
	IObjXRefRecord* ptr;
	XRefRecord(IObjXRefRecord* r):ptr(r){}
#elif USE_MAX_7
	// no records in Max 7
	void* ptr;
#endif

	XRefRecord() : ptr(NULL) {}
	~XRefRecord(){}
};

// static initialization
XRefManager*  XRefManager::m_sInstance = NULL;

XRefManager::XRefManager()
{
	document = NULL;
	xprtDocument = NULL;
}

XRefManager::~XRefManager()
{
	ClearExportDocument();
}


void XRefManager::ClearExportDocument()
{
	SAFE_DELETE(xprtDocument);
}

void XRefManager::BeginExport()
{
	// put any export initialization code here
	ClearExportDocument();
	xprtDocument = new FCDocument();
}

void XRefManager::FinalizeExport()
{
	// put any export finalization code here
	ClearExportDocument();
}


void GetAllNodes(INode* root, INodeTab& nodeList)
{
	nodeList.Append(1, &root);
	for (int i = 0; i < root->NumChildren(); ++i)
	{
		GetAllNodes(root->GetChildNode(i), nodeList);
	}
}

void XRefManager::InitImport(const FCDocument* colladaDocument)
{
	// build the URI list
	uriList.clear();

	document = (FCDocument*)colladaDocument;

#ifdef USE_MAX_7
	fm::vector< fm::vector<fm::string> > nodeNames;
#endif

	size_t phCount = colladaDocument->GetExternalReferenceManager()->GetPlaceHolderCount();

	// Scene has not been imported yet, add new record
	Interface* maxInterface = GetCOREInterface();

	for (size_t i = 0; i < phCount; i++)
	{
		const FCDPlaceHolder* ph = colladaDocument->GetExternalReferenceManager()->GetPlaceHolder(i);
		if (ph == NULL)
			continue;

		size_t xCount = ph->GetExternalReferenceCount();
		for (size_t j = 0; j < xCount; ++j)
		{
			const FCDEntityReference* xref = ph->GetExternalReference(j);
			if (xref == NULL)
				continue;

			// ABSOLUTE file URL
			FUUri uri = xref->GetUri();
			fstring xrefFilePath = uri.GetAbsolutePath();

			// Check whether this one is already loaded.
			size_t nXRefScenes = xRefScenes.size();
			size_t i;
			for (i = 0; i < nXRefScenes; ++i)
			{
				if (xRefScenes[i].first == xrefFilePath) break;
			}
			if (i != nXRefScenes) continue;

			// For .MAX external references, let 3dsMax do the XRef directly.
			fstring fileExtension = FUFileManager::GetFileExtension(xrefFilePath);
			if (IsEquivalentI(fileExtension, FC("max")))
			{
				XRefScene scene;
				scene.first = xrefFilePath;
				xRefScenes.push_back(scene);
				continue;
			}

#ifdef USE_MAX_7
			nodeNames.resize(nodeNames.size() + 1);
			fm::vector<fm::string>& theseNodeNames = nodeNames.back();
#endif

			// Get all current nodes
			INode* root = maxInterface->GetRootNode();
			INodeTab allOrigNodes;
			GetAllNodes(root, allOrigNodes);

			// Import the scene, and save the list of new nodes.
			Class_ID importerId = COLLADAIMPORTER_CLASS_ID;
			if (maxInterface->ImportFromFile(xrefFilePath.c_str(), 1, &importerId))
			{
				INodeTab allNodes;
				GetAllNodes(root, allNodes);

				// Find all the new nodes by comparing the all lists with each other.
				xRefScenes.resize(nXRefScenes + 1);
				xRefScenes.back().first = xrefFilePath;
				XRefNodeList& newXRefNodes = xRefScenes.back().second;

				INodeTab xRefNodes;
				for (int i = 0; i < allNodes.Count(); i++)
				{
					INode* aNode = allNodes[i];
					bool contained = false;
					for (int n = 0; n < allOrigNodes.Count(); ++n)
					{
						if (allOrigNodes[n] == aNode) { contained = true; break; }
					}
					if (!contained)
					{
						XRefNode* nodeData = new XRefNode();
						nodeData->node = aNode;
						nodeData->nodeDaeId = GetColladaID(aNode);
						nodeData->objectDaeId = GetColladaID(aNode->GetObjectRef());
						nodeData->materialDaeId = GetColladaID(aNode->GetMtl());
						newXRefNodes.push_back(nodeData);
						xRefNodes.Append(1, &aNode);
					}
				}

				fstring maxFileName = xrefFilePath + ".xreffile.max";
				fm::string maxFileCharStr = TO_STRING(maxFileName);
				((SimpleAttribClassDesc*)GetSimpleAttribDesc())->IsXRefSave(true);
				maxInterface->FileSaveNodes(&xRefNodes, (TCHAR*) (maxFileCharStr.c_str()));

				// Right, now scene is node, convert all nodes to XRefs

				// Dont need those old nodes anymore
				MtlBaseLib& mtlLib = maxInterface->GetMaterialLibrary();
				for (int i = 0; i < xRefNodes.Count(); i++)
				{
					INode* node = xRefNodes[i];
#ifdef USE_MAX_7
					theseNodeNames.push_back(node->GetName());
#endif
					Mtl* mtl = node->GetMtl();
					if (mtl != NULL)
					{
						// Explicitly remove the material, they do not get deleted with the node.
						// However, the ColladaAttribs MUST be deleted.  In order to let max not spaz
						// out, we delete the cust attrib container, and maybe remve from mtlLib
						// If it is still present when node is XRefed (in 10 lines) we may crash.
						// Materials are a pro
						mtlLib.Remove(mtl);
						mtl->DeleteCustAttribContainer();
						//node->SetMtl(NULL);
						//mtl->DeleteMe(); // Clear our link to the ColladaAttrib
					}
					maxInterface->DeleteNode(node, FALSE, FALSE);
					xRefNodes[i] = NULL;
				}
			}
		}
	}

	for (int i = 0; i < (int) xRefScenes.size(); ++i)
	{
		fstring maxFileName = xRefScenes[i].first;
		fstring extension = FUFileManager::GetFileExtension(maxFileName);
		if (!IsEquivalentI(extension, FC("max"))) maxFileName += FC(".xreffile.max");
		TCHAR* maxFileChar = (TCHAR*) maxFileName.c_str();
		XRefNodeList& xRefNodes = xRefScenes[i].second;

#ifdef USE_MAX_7
		for (size_t j = 0; j < xRefNodes.size(); ++j)
		{
			// Explicitly remove from scene before loading the XRef counterpart
			// failure to do so may result in bad param block desc function
			IXRefObject* xRefObject = GetObjXRefManager()->AddXRefObject(maxFileChar, (char*)nodeNames[i][j].c_str(), true);
			fm::pvector<INode> nodes;
			FindNodesByObject(xRefObject, maxInterface->GetRootNode(), nodes);
			FUAssert(nodes.size()== 1, continue);
			xRefNodes[j]->node = nodes.front();
		}
#else 

		// Ok, So Max8.  In Max8 we are forced to import all scenes into max before starting with
		// the XRef section.  The reason for this is otherwise, Max will save the entire material
		// library to the XRef file each time.  Loading the file will then conflict our SimpleAttribs
		// attached to the materials, as imported materials will conflict with the same versions already here.
		// We cannot load the same instance twice, our ParamBlockDesc require the PBIds to be constant, 
		// and unique.  So be good, avoid crashes, and just iterate 2x


		// Imports select nodes.
		maxInterface->ClearNodeSelection(FALSE);

		// Explicitly specify all nodes.  Maybe this will prevent loading the Mtl Library
		IObjXRefManager8* mgr = IObjXRefManager8::GetInstance();
		Tab<TCHAR*> unUsed;
		mgr->AddXRefItemsFromFile(maxFileChar, FALSE, &unUsed, XREF_XREF_MODIFIERS | XREF_SELECT_NODES);
		size_t expectedCount = xRefNodes.size();
		if (expectedCount == 0)
		{
			// For .MAX XRef.
			size_t selCount = maxInterface->GetSelNodeCount();
			xRefNodes.resize(selCount);
			for (size_t i = 0; i < selCount; ++i)
			{
				INode* node = maxInterface->GetSelNode((int) i);
				xRefNodes[i] = new XRefNode();
				xRefNodes[i]->node = node;
				xRefNodes[i]->nodeDaeId = xRefNodes[i]->objectDaeId = node->GetName();
				if (node->GetMtl() != NULL) xRefNodes[i]->materialDaeId = node->GetMtl()->GetName().data();
			}
		}
		else
		{
			FUAssert((size_t) maxInterface->GetSelNodeCount() == expectedCount, continue);
			for (size_t i = 0; i < expectedCount; ++i)
			{
				xRefNodes[i]->node = maxInterface->GetSelNode((int) i);
			}
		}
#endif
	}
}

Animatable* XRefManager::GetXRefItem(const FUUri& uri, FCDEntity::Type type)
{
	fstring xrefFilePath = uri.GetAbsolutePath();
	for (size_t i = 0; i < xRefScenes.size(); ++i)
	{
		if (xRefScenes[i].first == xrefFilePath)
		{
			XRefNodeList& xRefNodes = xRefScenes[i].second;
			for (size_t j = 0; j < xRefNodes.size(); ++j)
			{
				if (type == FCDEntity::SCENE_NODE || type == FCDEntity::ENTITY)
				{
					if (IsEquivalent(xRefNodes[j]->nodeDaeId, uri.GetFragment())) 
					{
						// When dealing with nodes, just use them as templates and clone away.
						// This is necessary to deal with hierarchical references and 3dsMax' inability to
						// generate node instances properly.
						INodeTab originalNodes, clonedNodes;
						const TCHAR* namey = xRefNodes[j]->node->GetName(); namey;
						GetAllNodes(xRefNodes[j]->node, originalNodes);
						Point3 offset(0, 0, 0);
						GetCOREInterface()->CloneNodes(originalNodes, offset, true, NODE_INSTANCE, NULL, &clonedNodes);
						return clonedNodes[0];
					}
				}
				if (type == FCDEntity::GEOMETRY || type == FCDEntity::ENTITY)
				{
					if (IsEquivalent(xRefNodes[j]->objectDaeId, uri.GetFragment()))
					{
						return xRefNodes[j]->node->GetObjectRef();
					}
				}
				if (type == FCDEntity::MATERIAL || type == FCDEntity::ENTITY)
				{
					if (IsEquivalent(xRefNodes[j]->materialDaeId, uri.GetFragment()))
					{
						return xRefNodes[j]->node->GetMtl();
					}
				}
			}
		}
	}

	return NULL;
}

FCDGeometry*  XRefManager::CreateGeometry()
{
	FCDGeometry* geom = xprtDocument->GetGeometryLibrary()->AddEntity();
	return geom;
}


void XRefManager::ApplyXRefMaterials(ReferenceTarget* x)
{
	if (x == NULL) return;
#if USE_MAX_8
	IObjXRefManager8* mgr = IObjXRefManager8::GetInstance();
	Tab< ReferenceTarget* > targetTab;
	targetTab.Append(1,& x);
	mgr->ApplyXRefMaterialsToXRefObjects(targetTab);
#elif USE_MAX_7
	// NOT SUPPORTED
#endif // USE_MAX_8
}

Mtl* XRefManager::RetrieveXRefMaterial(const FUUri& materialURI, FUFileManager* fmgr)
{
#if USE_MAX_8
	
	if (materialURI.GetPath().empty() || fmgr == NULL)
		return NULL;

	FUUri uri(fmgr->GetCurrentUri().MakeAbsolute(materialURI.GetPath()));

	size_t count = uriList.size();
	for (size_t i = 0; i < count; i++)
	{
		bool isMaterial = uriList[i].second->isMaterial;
		if (!isMaterial) continue;

		if (IsEquivalent(uri.GetPath(), uriList[i].first.GetPath())&& 
			IsEquivalent(uri.GetFragment(), uriList[i].first.GetFragment()))
		{
			return uriList[i].second->material;
		}
	}

	return NULL;

#elif USE_MAX_7
	(void)fmgr;
	(void)materialURI;
	return NULL;
#endif
}

void XRefManager::FinalizeImport()
{
	Interface* maxInterface = GetCOREInterface();

	for (size_t i = 0; i < xRefScenes.size(); i++)
	{
		XRefNodeList& nodes = xRefScenes[i].second;
		for (XRefNode** it = nodes.begin(); it != nodes.end(); ++it)
		{
			maxInterface->DeleteNode((*it)->node, FALSE, FALSE);
			SAFE_DELETE(*it);
		}
	}

	xRefScenes.clear();

/*	size_t c = uriList.size();
	for (size_t i = 0; i < c; i++)
	{
		INode* n = uriList[i].second->inode;

		// check "assigned" state
		if (!uriList[i].second->assigned && 
			n != NULL)
		{
			FUError::Error(FUError::WARNING_LEVEL, FUError::WARNING_XREF_UNASSIGNED, 0);
		}
		
		// delete the node
		if (n != NULL && !(uriList[i].second->nodeAssigned))
		{
			n->Delete(TIME_INITIAL_POSE, FALSE);
		}
		
		// delete the struct
		delete uriList[i].second;
		uriList[i].second = NULL;
	}

	// clear the list
	uriList.clear();

	// clear the record list
	XRefRecordList::iterator it = recordList.begin();
	while (it != recordList.end()){ delete* it; it++; }
	recordList.clear();

#if USE_MAX_8
	// force an update at the last moment
	IObjXRefManager8* mgr = IObjXRefManager8::GetInstance();
	mgr->UpdateAllRecords();
#endif //USE_MAX_8

	// After the import is finalized, no need to hang around
	ReleaseInstance(); */
}
