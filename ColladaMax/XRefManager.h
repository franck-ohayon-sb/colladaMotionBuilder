/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef XREFMANAGER_H
#define XREFMANAGER_H

#ifndef _FU_URI_H_
#include "FUtils/FUUri.h"
#endif // _FU_URI_H_

#ifndef _FCD_ENTITY_H_
#include "FCDocument/FCDEntity.h"
#endif // _FCD_ENTITY_H_

// forward declarations
class XRefImporter;

// API independent XRefRecord
struct XRefURIMatch;
class XRefRecord;
class FCDGeometry;
class FUFileManager;

class XRefManager
{
private:
	static XRefManager*  m_sInstance;

private:
	struct XRefNode
	{
		INode* node;
		int32 refCount; // reserved for future usage.
		fm::string nodeDaeId;
		fm::string objectDaeId;
		fm::string materialDaeId;
	};

	typedef fm::pair<FUUri, XRefURIMatch*> XRefURIMatchPair;
	typedef fm::vector<XRefURIMatchPair> XRefURIMatchList;
	typedef fm::pvector<XRefRecord> XRefRecordList;
	typedef fm::pair<fm::string, fm::string> MaxNameMaterialXRefPair;
	typedef fm::vector<MaxNameMaterialXRefPair> MaxNameMaterialXRefList;

	typedef fm::pvector<XRefNode> XRefNodeList;
	typedef fm::pair<fm::string, XRefNodeList> XRefScene;
	typedef fm::vector<XRefScene> XRefSceneList;

	XRefSceneList xRefScenes;

	XRefURIMatchList uriList;
	XRefRecordList recordList;
	bool isUIEnabled;

	/** Current Document being imported.*/
	FCDocument* document,* xprtDocument;

public:
	XRefManager();
	XRefManager(XRefManager&) {}
	~XRefManager();

	// methods (export specific)
	void BeginExport();
	void FinalizeExport();

	// methods (import specific)
	void InitImport(const FCDocument* colladaDocument);
	void FinalizeImport();

	Animatable* GetXRefItem(const FUUri& uri, FCDEntity::Type type);

	FCDGeometry*  CreateGeometry();
	void ApplyXRefMaterials(ReferenceTarget* x);
	Mtl* RetrieveXRefMaterial(const FUUri& materialURI, FUFileManager* fmgr);

private:
	void ClearExportDocument();
};

#endif // XREFMANAGER_H
