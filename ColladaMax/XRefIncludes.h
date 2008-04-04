/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef XREFINCLUDES_H_
#define XREFINCLUDES_H_

#define USE_MAX_8 (MAX_VERSION_MAJOR >= 8)

#if USE_MAX_8

#pragma warning(disable:4100)
#pragma warning(disable:4238)
#pragma warning(disable:4267)

#include <XRef/iXRefObjMgr8.h>
#include <XRef/iXRefMaterial.h>
#include <XRef/iXRefObj.h>

#pragma warning(default:4267)
#pragma warning(default:4238)
#pragma warning(default:4100)

#else
#define USE_MAX_7 1

#include <ixref.h>

// re-define a class took from the debug build
class XRefObject_REDEFINITION : public IXRefObject {
	public:
		TSTR xrefFile;
		TSTR xrefObj;
		TSTR xrefFileProxy;
		TSTR xrefObjProxy;
		Object *obj;
		DWORD flags;

		// don't care about the rest...
};

#endif // USE_MAX_8 else

class INode;
class Mtl;
class EntityImporter;

struct XRefURIMatch
{
	INode* inode;
	Mtl* material;
	fm::string materialID;
	EntityImporter* targetImporter;
	bool isMaterial;
	bool assigned;
	bool nodeAssigned;

	XRefURIMatch(bool _isMat, EntityImporter* _targetImporter=NULL) 
	:	inode(NULL), material(NULL), materialID("")
	,	assigned(false), nodeAssigned(false), isMaterial(_isMat), targetImporter(_targetImporter)
	{}
	~XRefURIMatch(){}
};

#endif //XREFINCLUDES_H_
