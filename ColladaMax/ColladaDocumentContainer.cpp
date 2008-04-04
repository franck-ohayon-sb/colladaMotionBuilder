/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "ColladaDocumentContainer.h"

//
// COLLADADocumentContainer
//

COLLADADocumentContainer::COLLADADocumentContainer()
:	ReferenceTarget()
,	colladaDocument(NULL), filename("")
,	referenceCount(0)
{
}

COLLADADocumentContainer::~COLLADADocumentContainer()
{
	FUAssert(referenceCount == 0,);
}

void COLLADADocumentContainer::DeleteThis()
{
	delete this;
}

IOResult COLLADADocumentContainer::Save(ISave* UNUSED(iSave))
{
	return IO_OK;
}

IOResult COLLADADocumentContainer::Load(ILoad* UNUSED(iLoad))
{
	return IO_OK;
}

RefResult COLLADADocumentContainer::NotifyRefChanged(Interval UNUSED(changeInt), RefTargetHandle UNUSED(hTarget), PartID& UNUSED(partID),  RefMessage UNUSED(message))
{
	return REF_SUCCEED;
}

//
// COLLADADocumentContainerDesc
//

class COLLADADocumentContainerDesc : public ClassDesc2
{
public:
	int 			IsPublic() { return 1; }
	void *			Create(BOOL isLoading = FALSE) { isLoading; return new COLLADADocumentContainer(); }
	const TCHAR *	ClassName() { return FC("ColladaDocumentContainer"); }
	SClass_ID		SuperClassID() {return SYSTEM_CLASS_ID;}
	Class_ID 		ClassID() {return COLLADADOCUMENTCONTAINER_CLASS_ID;}
	const TCHAR* 	Category() {return _T("");}
	const TCHAR*	InternalName() { return _T("ColladaDocumentContainer"); }	// returns fixed parsable name (scripter-visible name)
	HINSTANCE		HInstance() { return hInstance; }			// returns owning module handle
};

ClassDesc2* GetCOLLADADocumentContainerDesc()
{
	static COLLADADocumentContainerDesc description;
	return &description; 
}
