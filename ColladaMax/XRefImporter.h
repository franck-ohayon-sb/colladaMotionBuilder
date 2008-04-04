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

#ifndef _XREF_IMPORTER_H_
#define _XREF_IMPORTER_H_

#include "XRefIncludes.h"
#include "EntityImporter.h"

#ifndef _FCD_ENTITY_H_
#include "FCDocument/FCDEntity.h"
#endif // _FCD_ENTITY_H_

//class IXRefObject;
class FUUri;
struct XRefURIMatch;
class DocumentImporter;
class NodeImporter;
class FCDExternalReference;

class XRefImporter : public EntityImporter
{
private:
	//INode *xRefNode;
	//Object *xRefObject;

	//XRefURIMatch* xRefMatch;
	Animatable* xRefEntity;

public:
	XRefImporter(DocumentImporter* document, NodeImporter* parent);
	virtual ~XRefImporter();

	virtual Type GetType() { return XREF; }

	virtual Object* GetObject() { return (Object*)xRefEntity; }

	bool FindXRef(const FUUri& uri, FCDEntity::Type type);

	// Import a COLLADA external reference as a Max XRef
	void ImportExternalReference(FCDExternalReference* colladaExternalReference);

	// override from EntityImporter
	virtual void Finalize(FCDEntityInstance* instance);
};

#endif // _XREF_IMPORTER_H_
