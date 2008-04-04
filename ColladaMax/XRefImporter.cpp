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

// External References Importer Class

#include "StdAfx.h"
#include "DocumentImporter.h"
#include "XRefImporter.h"
#include "GeometryImporter.h"
#include "MaterialImporter.h"
#include "NodeImporter.h"
#include "XRefManager.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDEntityInstance.h"
#include "FCDocument/FCDEntityReference.h"
#include "FCDocument/FCDGeometryInstance.h"
#include "FCDocument/FCDMaterial.h"
#include "FCDocument/FCDMaterialInstance.h"
#include "FCDocument/FCDPlaceHolder.h"
#include "FUtils/FUFileManager.h"

XRefImporter::XRefImporter(DocumentImporter* document, NodeImporter* parent)
:	EntityImporter(document, parent)
{
	//xRefObject = NULL;
	//xRefNode = NULL;
	//xRefNode = node;
	xRefEntity = NULL;
}

XRefImporter::~XRefImporter()
{
	//xRefObject = NULL;
	//xRefNode = NULL;
}

bool XRefImporter::FindXRef(const FUUri& uri, FCDEntity::Type type)
{
	xRefEntity = GetDocument()->GetXRefManager()->GetXRefItem(uri, type);
	return (xRefEntity != NULL);
}

void XRefImporter::Finalize(FCDEntityInstance* instance)
{
	if (xRefEntity == NULL) return;
	if (instance == NULL || instance->GetType() != FCDEntityInstance::GEOMETRY) return;
		
	FCDGeometryInstance* geomInst = (FCDGeometryInstance*) instance;

	// External references can only have ONE material applied
	// applying multiple materials to an XRef forces its merge in the scene
	FCDMaterialInstance* matInst = NULL;
	if (geomInst->GetMaterialInstanceCount() > 0) matInst = geomInst->GetMaterialInstance(0);
	
	if (matInst == NULL)
	{
		// no material applied... leave empty and applied a random color material.
		return;
	}
	else if (matInst->IsExternalReference())
	{
		// An XRef with a different XRef material
		FCDEntityReference* xref = matInst->GetEntityReference();
		FUUri uri = xref->GetUri();
		Mtl* mat = (Mtl*) GetDocument()->GetXRefManager()->GetXRefItem(uri, FCDEntity::MATERIAL);
		if (mat == NULL) return;

		// this creates a new material in the Editor, assign it a name
		GetParent()->GetImportNode()->GetINode()->SetMtl(mat);
	}
	else
	{
		// An XRef with a different local material
		FCDMaterial* colladaMaterial = matInst->GetMaterial();
		
		if (colladaMaterial == NULL) return;

		EntityImporter* entityImporter = GetDocument()->FindInstance(colladaMaterial->GetDaeId());
		if (entityImporter == NULL || entityImporter->GetType() != EntityImporter::MATERIAL) return;

		MaterialImporter* matImp = (MaterialImporter*) entityImporter;
		Mtl* mat = matImp->GetMaterial();
		GetParent()->GetImportNode()->GetINode()->SetMtl(mat);
	}

	EntityImporter::Finalize(instance);
}
