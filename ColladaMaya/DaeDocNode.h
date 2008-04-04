/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COLLADA_DOCUMENT_NODE
#define _COLLADA_DOCUMENT_NODE

#include <maya/MPxNode.h>

class DaeDoc;
class DaeEntity;
class FCDocument;
typedef fm::pvector<DaeDoc> DaeDocList;

class DaeDocNode : public MPxNode
{
private:
	DaeDocList documents;

public:
	DaeDocNode();
	virtual ~DaeDocNode();

	// Plug-in interface
	static void* creator();
	static MStatus initialize();
	static MTypeId id;

	// DG attributes
	static MObject rootAttribute;
	static MObject filenameAttribute;
	static MObject entityIdAttribute;
	static MObject entityLinkAttribute;

	// MPxNode interface.
	virtual Type type() const { return MPxNode::kDependNode; }

	// Document Management
	inline size_t GetDocumentCount() { return documents.size(); }
	inline DaeDoc* GetDocument(size_t index) { FUAssert(index < documents.size(), return NULL); return documents.at(index); }
	DaeDoc* FindDocument(uint logicalIndex);
	DaeDoc* FindDocument(const MString& filename);
	DaeDoc* FindDocument(FCDocument* colladaDocument);
	DaeDoc* NewDocument(const MString& filename);
	void ReleaseDocument(DaeDoc* document);
	void ReleaseAllEntityNodes();
	uint Synchronize();

	// Entites
	void LinkEntity(DaeDoc* document, DaeEntity* entity, MObject& node);
	DaeEntity* FindEntity(const MObject& node, DaeDoc** document=NULL);
};

#endif // _COLLADA_DOCUMENT_NODE

