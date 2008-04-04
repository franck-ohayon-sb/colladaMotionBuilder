/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/*
	Based off the a sample by Neil Hazzard and Nikolai Sander.
	Copyright (C) Autodesk 2000, All Rights Reserved.
*/

#ifndef	__COLLADA_ATTRIB_H__
#define __COLLADA_ATTRIB_H__

#ifndef __SIMPLE_ATTRIB_H__
#include "SimpleAttrib.h"
#endif // __SIMPLE_ATTRIB_H__

class COLLADADocumentContainer;

#define COLLADA_ATTRIB_CLASS_ID		Class_ID(0x5b621f96, 0x4ed628af)
#define COLLADA_ATTRIB_NAME			FC("COLLADA CA")

class ColladaAttrib : public SimpleAttrib
{
private:
	ParamID daeIdParameterId;

public:
	ColladaAttrib(BOOL isLoading=FALSE);
	virtual ~ColladaAttrib();
	virtual SClass_ID SuperClassID() { return CUST_ATTRIB_CLASS_ID; }
	virtual Class_ID ClassID() { return COLLADA_ATTRIB_CLASS_ID; }
	virtual void DeleteThis();

	// Max-Inherited functions
	virtual ReferenceTarget* Clone(RemapDir &remap);
	virtual TCHAR* GetName();
	virtual TSTR SubAnimName(int UNUSED(i)){ return "ColladaAttrib";} 
	
	//virtual IOResult Load(ILoad* iLoad);

	// COLLADA id
	void SetDaeId(const fm::string& id);
	const TCHAR* GetDaeId();

	// COLLADA document container
	void SetDocumentContainer(COLLADADocumentContainer* container);
	COLLADADocumentContainer* GetDocumentContainer();
};

ClassDesc2* GetCOLLADAAttribDesc();

#endif
