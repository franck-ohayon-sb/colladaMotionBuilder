/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COLLADA_DOCUMENT_CONTAINER_H_
#define _COLLADA_DOCUMENT_CONTAINER_H_

#define COLLADADOCUMENTCONTAINER_CLASS_ID Class_ID(0x5a3f0833, 0x382c4ab8)

class FCDocument;

class COLLADADocumentContainer : public ReferenceTarget
{
private:
	FCDocument* colladaDocument;
	fm::string filename;

	size_t referenceCount;

public:
	COLLADADocumentContainer();
	virtual ~COLLADADocumentContainer();
	virtual void DeleteThis();
	virtual SClass_ID SuperClassID() { return SYSTEM_CLASS_ID; }
	virtual Class_ID ClassID() { return COLLADADOCUMENTCONTAINER_CLASS_ID; }

	// Serialization
	virtual IOResult Save(ISave* iSave);
	virtual IOResult Load(ILoad* iLoad);

	// Reference Management
	virtual RefResult NotifyRefChanged(Interval changeInt, RefTargetHandle hTarget, PartID& partID,  RefMessage message);
};

ClassDesc2* GetCOLLADADocumentContainerDesc();

#endif // _COLLADA_DOCUMENT_CONTAINER_H_