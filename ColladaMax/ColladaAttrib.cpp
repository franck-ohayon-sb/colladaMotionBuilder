/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/*
	Based off a sample by Neil Hazzard and Nikolai Sander.
	Copyright (C) Autodesk 2000, All Rights Reserved.
*/

#include "StdAfx.h"
#include "istdplug.h"
#include "ColladaAttrib.h"
#include "ColladaDocumentContainer.h"

//
// ColladaAttrib
//

const TCHAR* daeIdParameterName = _T("COLLADA_id");

ColladaAttrib::ColladaAttrib(BOOL isLoading)
:	SimpleAttrib()
{
	daeIdParameterId = 0;
	if (!isLoading)
	{
		ParamBlockDesc2* description = GetColladaBlockDesc(COLLADA_ATTRIB_NAME);
		description->AddParam(daeIdParameterId, daeIdParameterName, TYPE_STRING, 0, 0, p_ui, TYPE_EDITBOX, 554, end);
		SetName("COLLADA Attributes");
		CreateParamBlock();
	}
}

ColladaAttrib::~ColladaAttrib()
{
}

void ColladaAttrib::DeleteThis()
{
	delete this;
}

TCHAR* ColladaAttrib::GetName()
{
	return COLLADA_ATTRIB_NAME;
}

ReferenceTarget* ColladaAttrib::Clone(RemapDir &remap)
{
	ColladaAttrib* pnew = new ColladaAttrib();
	pnew->ReplaceReference(0, remap.CloneRef(GetParamBlock(0)));
	BaseClone(this, pnew, remap);
	return pnew;
}

void ColladaAttrib::SetDaeId(const fm::string& id)
{
	IParamBlock2* paramBlock = GetParamBlock(0);
	Interval dummy;
	paramBlock->SetValue(daeIdParameterId, 0, (TCHAR*) id.c_str());
}

const TCHAR* ColladaAttrib::GetDaeId()
{
	IParamBlock2* paramBlock = GetParamBlock(0);
	Interval dummy;
	return paramBlock->GetStr(daeIdParameterId, 0);
}

void ColladaAttrib::SetDocumentContainer(COLLADADocumentContainer* container)
{
	ReplaceReference(1, container);
}

COLLADADocumentContainer* ColladaAttrib::GetDocumentContainer()
{
	ReferenceTarget* target = GetReference(1);
	FUAssert(target == NULL || (target->SuperClassID() == SYSTEM_CLASS_ID && target->ClassID() == COLLADADOCUMENTCONTAINER_CLASS_ID), return NULL);
	return (COLLADADocumentContainer*) target;
}

//
// ColladaAttribClassDesc
//

class ColladaAttribClassDesc : public SimpleAttribClassDesc
{
public:
	int 			IsPublic() { return 0; }
	void *			Create(BOOL isLoading = FALSE) { return new ColladaAttrib(isLoading); }
	const TCHAR *	ClassName() { return GetString(IDS_CLASS_NAME_A); }
	SClass_ID		SuperClassID() {return CUST_ATTRIB_CLASS_ID;}
	Class_ID 		ClassID() {return COLLADA_ATTRIB_CLASS_ID;}
	const TCHAR* 	Category() {return _T("");}
	const TCHAR*	InternalName() { return _T("ColladaAttrib"); }	// returns fixed parsable name (scripter-visible name)
	HINSTANCE		HInstance() { return hInstance; }			// returns owning module handle
};

ClassDesc2* GetCOLLADAAttribDesc()
{
	static ColladaAttribClassDesc description;
	return &description;
}
