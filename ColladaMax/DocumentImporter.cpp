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

// Document Importer Class

#include "StdAfx.h"
#include "AnimationImporter.h"
#include "ColladaAttrib.h"
#include "ColladaDocumentContainer.h"
#include "ColladaOptions.h"
#include "DocumentImporter.h"
#include "MaterialImporter.h"
#include "NodeImporter.h"
#include "XRefManager.h"
#include "ColladaFX/ColladaEffectParameter.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDocumentTools.h"
#include "FCDocument/FCDAsset.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDLight.h"
#include "FCDocument/FCDMaterial.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDSceneNodeIterator.h"
#include "FCDocument/FCDGeometryInstance.h"
#include "FCDocument/FCDMaterialInstance.h"

DocumentImporter::DocumentImporter(ImpInterface* _importInterface)
:	importInterface(_importInterface)
,	colladaDocument(NULL), animationImporter(NULL), documentContainer(NULL), xRefManager(NULL), options(NULL)
,	isCMaxExported(false), versionCMax(0.0f)
{
	// Since external references are treated separately (imported in separate Max sessions)
	// we set the dereference flag to false to avoid stack overflow in the event of an XRef
	// dependency cycle.
	FCollada::SetDereferenceFlag(false);
	colladaDocument = FCollada::NewTopDocument();
	options = new ColladaOptions(GetCOREInterface());
}

DocumentImporter::~DocumentImporter()
{
	SAFE_DELETE(animationImporter);
	SAFE_DELETE(xRefManager);
	importInterface = NULL;
	SAFE_DELETE(colladaDocument);
	SAFE_DELETE(options);
	documentContainer = NULL; // Released once all references are gone.

	CLEAR_POINTER_STD_PAIR_CONT(IDEntityMap, instances);
	ambientLights.clear();
	sceneNodes.clear();
}

BOOL DocumentImporter::ShowImportOptions(BOOL suppressPrompts)
{
	if (!suppressPrompts) 
	{
		// Prompt the user with our dialogbox, and get all the options.
		// The user may cancel the import at this point.
		if (!options->ShowDialog(FALSE)) return false;
	}

	return true;
}

// Create a max object
void* DocumentImporter::MaxCreate(SClass_ID sid, Class_ID clid)
{
	// Attempt to create
	FUAssert(importInterface != NULL, return NULL);

	void* c = importInterface->Create(sid, clid);

	if (c == NULL)
	{
		// This can be caused if the object referred to is in a deffered plugin.
		DllDir* dllDir = GetCOREInterface()->GetDllDirectory();
		ClassDirectory& classDir = dllDir->ClassDir();
		ClassEntry* classEntry = classDir.FindClassEntry(sid, clid);
		FUAssert(classEntry != NULL, return NULL);

		// This will force the loading of the specified plugin
		classEntry->FullCD();

		FUAssert(classEntry != NULL && classEntry->IsLoaded() == TRUE, return NULL);

		c = importInterface->Create(sid, clid);
	}
	return c;
}

// Create a max object, based on SuperClass and ClassName
void* DocumentImporter::MaxCreate(SClass_ID sid, TSTR name)
{
	// Attempt to create
	FUAssert(importInterface != NULL, return NULL);

	void* c = NULL;

	// Find our mystery object through the class listings
	DllDir* dllDir = GetCOREInterface()->GetDllDirectory();
	ClassDirectory& classDir = dllDir->ClassDir();
	SubClassList* classList = classDir.GetClassList(sid);
	FUAssert(classList != NULL, return NULL);

	int idx = classList->FindClass(name);
	if (idx > 0)
	{
		ClassEntry& subClass = (*classList)[idx];
		//FUAssert(name == subClass.ClassName(), return NULL);
		
		// This will force the loading of the specified plugin
		subClass.FullCD();
		// Create the object (whatever it is)
		c = importInterface->Create(sid, subClass.ClassID());
	}

	/*
	int nClass = classList->Count();
	for (int i = 0; i < nClass; i++)
	{
		ClassEntry& subClass = (*classList)[i];
		if (name == subClass.ClassName())
		{
			// This will force the loading of the specified plugin
			subClass.FullCD();
			// Create the object (whatever it is)
			c = importInterface->Create(sid, subClass.ClassID());
			break;
		}
	}
	*/
	return c;
}


// Add a new instance to the list
void DocumentImporter::AddInstance(const fm::string& colladaId, EntityImporter* entity)
{
	instances[colladaId] = entity;
}

// Search for an instance with the given id
EntityImporter* DocumentImporter::FindInstance(const fm::string& colladaId)
{
	IDEntityMap::iterator it = instances.find(colladaId);
	return (it != instances.end()) ? (*it).second : NULL;
}

EntityImporter* DocumentImporter::GetInstance(size_t i)
{
	if (i < NumInstances())
	{
		IDEntityMap::iterator it = instances.begin();
		for (size_t n = 0; n < i; n++) ++it;
		return it->second;
	}
	return NULL;
}

void DocumentImporter::InitializeImport()
{
	SAFE_DELETE(animationImporter);
	animationImporter = new AnimationImporter(this);

	SAFE_DELETE(xRefManager);
	xRefManager = new XRefManager();
}

void DocumentImporter::ImportScene()
{
	FCDSceneNode* colladaSceneRoot = colladaDocument->GetVisualSceneInstance();
	if (colladaSceneRoot == NULL) return;

	for (FCDSceneNodeIterator it(colladaSceneRoot); !it.IsDone(); it.Next())
	{
		FCDSceneNode* sceneNode = it.GetNode();
		size_t instanceCount = sceneNode->GetInstanceCount();
		for (size_t a = 0; a < instanceCount; ++a)
		{
			FCDEntityInstance* instance = sceneNode->GetInstance(a);
			if (instance->HasType(FCDGeometryInstance::GetClassType()))
			{
				FCDGeometryInstance* geometryInstance = (FCDGeometryInstance*) instance;
				size_t materialInstanceCount = geometryInstance->GetMaterialInstanceCount();
				for (size_t b = 0; b < materialInstanceCount; ++b)
				{
					FCDMaterialInstance* materialInstance = geometryInstance->GetMaterialInstance(b);
					FCDMaterial* colladaMaterial = materialInstance->GetMaterial();
					if (colladaMaterial == NULL) continue;
					MaterialImporter* importer = new MaterialImporter(this);

					Mtl* material = importer->ImportMaterial(colladaMaterial, materialInstance, geometryInstance);
					if (material == NULL) { SAFE_DELETE(importer); }
					else
					{
						AddInstance(colladaMaterial->GetDaeId(), importer);
						importer->Finalize(colladaMaterial, material);
					}
				}
			}
		}
	}

	// Import the COLLADA document's scene nodes
	
	if (colladaSceneRoot != NULL)
	{
		if (colladaSceneRoot->GetInstanceCount() > 0 || colladaSceneRoot->GetTransformCount() > 0)
		{
			NodeImporter* importer = new NodeImporter(this, NULL);
			importer->ImportSceneNode(colladaSceneRoot);
			sceneNodes.push_back(importer);

			// Instantiate the node
			AddInstance(colladaSceneRoot->GetDaeId(), importer);
		}
		else
		{
			// Create the scene nodes child directly in the world,
			// in order to avoid recreating an unnecessary top node
			size_t childNodeCount = colladaSceneRoot->GetChildrenCount();
			for (size_t c = 0; c < childNodeCount; ++c)
			{
				FCDSceneNode* childNode = colladaSceneRoot->GetChild(c);
				const fm::string& colladaNodeId = childNode->GetDaeId();
				EntityImporter* instanceImporter = FindInstance(colladaNodeId);
				if (instanceImporter == NULL)
				{
					// Create this new Max node
					NodeImporter* importer = new NodeImporter(this, NULL);
					importer->ImportSceneNode(childNode);
					sceneNodes.push_back(importer);
					AddInstance(childNode->GetDaeId(), importer);
				}
				else
				{
					// This is a second instance of this node
					// TODO: Figure out how to attach a node to both the scene's base and another node.
					sceneNodes.push_back((NodeImporter*) instanceImporter);
				}
			}
		}
	}
}


void DocumentImporter::FinalizeImport()
{
#define FOR_ALL_NODES(fn) \
	for (NodeImporterList::iterator it = sceneNodes.begin(); it != sceneNodes.end(); ++it) (*it)->fn();

	// Current scene creation heuristic:
	// Perform XRef import

	xRefManager->InitImport(GetCOLLADADocument());
	FOR_ALL_NODES(ImportInstances);
	FOR_ALL_NODES(CreateDummies);
	FOR_ALL_NODES(Finalize);
	xRefManager->FinalizeImport();

#undef FOR_ALL_NODES

	// Handle the ambient lighting.
	// Note that ambient color can be animated in 3dsMax and COLLADA.
	Point3 totalAmbientColor = Point3::Origin;
	for (LightList::iterator it = ambientLights.begin(); it != ambientLights.end(); ++it)
	{
		totalAmbientColor += ToPoint3((*it)->GetColor() * (*it)->GetIntensity());
	}
	importInterface->SetAmbient(0, totalAmbientColor);
}

// Entry point: imports into Max the whole document
void DocumentImporter::ImportDocument()
{
	InitializeImport();
	ImportAsset();
	ImportScene();
	FinalizeImport();
}

COLLADADocumentContainer* DocumentImporter::GetDocumentContainer()
{
	if (documentContainer == NULL)
	{
		ClassDesc2* cd = GetCOLLADADocumentContainerDesc();
		documentContainer = (COLLADADocumentContainer*) importInterface->Create(cd->SuperClassID(), cd->ClassID());
	}
	return documentContainer;
}

void DocumentImporter::ImportAsset()
{
	FMVector3 upAxis = FMVector3::Zero;
	if (options->ImportUpAxis())
	{
		upAxis = FMVector3::ZAxis;
	}

	float systemUnitScale = 0.0f;
	if (options->ImportUnits())
	{
		systemUnitScale = 1.0f;

		// Retrieve the system unit information
		int systemUnitType = UNITS_CENTIMETERS;
		GetMasterUnitInfo(&systemUnitType, &systemUnitScale);

		switch (systemUnitType)
		{
		case UNITS_INCHES: systemUnitScale *= 0.0254f; break;
		case UNITS_FEET: systemUnitScale *= 0.3048f; break;
		case UNITS_MILES: systemUnitScale *= 1609.344f; break;
		case UNITS_MILLIMETERS: systemUnitScale *= 0.001f; break;
		case UNITS_CENTIMETERS: systemUnitScale *= 0.01f; break;
		case UNITS_METERS: break;
		case UNITS_KILOMETERS: systemUnitScale *= 1000.0f; break;
		default: break;
		}
	}

	// Standardize the data. Even if the user doesn't want to match the document's up-axis/units to 3dsMax'
	// we still standardize in order to remove any differences of up-axis/units within the document's own asset tags.
	FCDocumentTools::StandardizeUpAxisAndLength(colladaDocument, upAxis, systemUnitScale, true);

	// Verify whether this document was exported using ColladaMax and parse the version of ColladaMax used.
	FCDAsset* asset = colladaDocument->GetAsset();
	if (asset != NULL && asset->GetContributorCount() > 0)
	{
		FCDAssetContributor* contributor = asset->GetContributor(asset->GetContributorCount() - 1);
		fstring authoringTool = contributor->GetAuthoringTool();
		const fchar* tool = authoringTool.c_str();
		const fchar* versionInfo = strstr(tool, FC("Feeling ColladaMax v"));
		if (versionInfo != NULL)
		{
			isCMaxExported = true;
			versionInfo += fstrlen(FC("Feeling ColladaMax v"));
			versionCMax = FUStringConversion::ToFloat(versionInfo);
		}
	}
}

//
// PostImportListener class
//

PostImportListener PostImportListener::mInstance;

PostImportListener::PostImportListener()
:	effectNodeParams()
{
	RegisterNotification(NotificationCallback, this, NOTIFY_POST_IMPORT);
}

PostImportListener::~PostImportListener()
{
	UnRegisterNotification(NotificationCallback, this, NOTIFY_POST_IMPORT);
}

void PostImportListener::RegisterNodeParameter(ColladaEffectParameterNode* param)
{
	if (param == NULL) return;
	effectNodeParams.push_back(param);
}

void PostImportListener::MatchNodeParameters()
{
	for (size_t i = 0; i < effectNodeParams.size(); ++i)
	{
		ColladaEffectParameterNode* param = effectNodeParams[i];
		if (param == NULL) continue;
		
		// after import, all imported nodes are selected
		int nodeCount = GetCOREInterface()->GetSelNodeCount();
		for (int j = 0; j < nodeCount; ++j)
		{
			INode* node = GetCOREInterface()->GetSelNode(j);
			if (node == NULL) continue;

			// the node must match the superclass id
			// currently, only a limited amount of super class ids are set in the
			// ColladaEffectParameterFactory
			if (param->getSuperClassID() != 0 && // filter has been set
				node->GetObjectRef()->SuperClassID() != param->getSuperClassID())
				continue;
			
			// Get the node Collada custom attribute
			ICustAttribContainer* custCont = node->GetCustAttribContainer();
			bool found = false;
			if (custCont != NULL)
			{
				for (int k = 0; k < custCont->GetNumCustAttribs(); ++k)
				{
					CustAttrib* attrib = custCont->GetCustAttrib(k);
					if (attrib == NULL) continue;
					if (attrib->ClassID() == COLLADA_ATTRIB_CLASS_ID)
					{
						ColladaAttrib* ca = (ColladaAttrib*)attrib;
						fm::string id = TO_STRING(ca->GetDaeId());
						if (IsEquivalent(id, param->getNodeId()))
						{
							// we have a match
							IParamBlock2* pblock = param->getParamBlock();
							if (pblock != NULL)
								pblock->SetValue(param->getParamID(), TIME_INITIAL_POSE, node);
							found = true;
							break;
						}
					}
				}
			}
			if (found) break;
		} // for each imported node
	} // for each ColladaEffectParameterNode

	// clear the list after matching
	effectNodeParams.clear();
}

void PostImportListener::NotificationCallback(void* param, NotifyInfo* UNUSED(info))
{
	FUAssert(param == &mInstance, return);
	mInstance.MatchNodeParameters();
}
