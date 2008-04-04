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

// Main Importer Class

#include "StdAfx.h"
#include "COLLADAImporter.h"
#include "DocumentImporter.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDocumentTools.h"

COLLADAImporter::COLLADAImporter()
{ 
}

COLLADAImporter::~COLLADAImporter()
{
}

int COLLADAImporter::ExtCount()
{
	// Number of extemsions supported
	return 2;
}

const TCHAR* COLLADAImporter::Ext(int n)
{
	// Extension #n (i.e. "3DS")
	switch (n)
	{
	case 0: return _T("DAE");
	case 1: return _T("XML");
	default: return _T("");
	}
}

const TCHAR* COLLADAImporter::LongDesc()
{
	// Long ASCII description (i.e. "Autodesk 3D Studio File")
	return _T("COLLADA Document");
}

const TCHAR* COLLADAImporter::ShortDesc()
{
	// Short ASCII description (i.e. "3D Studio")
	return _T("COLLADA");
}

const TCHAR* COLLADAImporter::AuthorName()
{
	// ASCII Author name
	return _T("Feeling Software");
}
const TCHAR* COLLADAImporter::CopyrightMessage()
{
	// ASCII Copyright message
	return _T("Copyright 2006 Feeling Software. Based on Autodesk' 3dsMax COLLADA Tools.");
}

const TCHAR* COLLADAImporter::OtherMessage1()
{
	// Other message #1
	return _T("");
}

const TCHAR* COLLADAImporter::OtherMessage2()
{
	// Other message #2
	return _T("");
}

unsigned int COLLADAImporter::Version()
{
	// Return Version number * 100 (e.g. v3.01 = 301)
	uint32 version = (FCOLLADA_VERSION >> 16) * 100;
	version += FCOLLADA_VERSION % 100;
	return version;
}

void COLLADAImporter::ShowAbout(HWND)
{
	// Show DLL's "About..." box [Optional]
}

int COLLADAImporter::ZoomExtents()
{
	// Override this for zoom extents control
	return ZOOMEXT_NOT_IMPLEMENTED;
}

// File import entry point
int COLLADAImporter::DoImport(const TCHAR* name, ImpInterface* ii, Interface*, BOOL suppressPrompts)
{
	int errorCode = IMPEXP_SUCCESS;

#if !defined(_DEBUG)
	try {
#endif

	DocumentImporter documentImporter(ii);
	if (documentImporter.ShowImportOptions(suppressPrompts))
	{
		// Import file
		FUErrorSimpleHandler handler;
		FCollada::LoadDocumentFromFile(documentImporter.GetCOLLADADocument(), name);
		bool success = handler.IsSuccessful();
		if (success) documentImporter.ImportDocument();
		else errorCode = IMPEXP_FAIL;

#if defined(_DEBUG)
		success = false;
#endif

		if (!suppressPrompts && !success)
		{
			MessageBox(NULL, handler.GetErrorString(), "COLLADA Import Errors", MB_OK | MB_ICONWARNING | MB_TOPMOST | MB_APPLMODAL);
		}
	}
	else errorCode = IMPEXP_CANCEL;

#if !defined(_DEBUG)
	}
	catch (...)
	{
		// Add some check, here, for full UI-mode or batch-mode only.
		if (!suppressPrompts)
		{
			MessageBox(NULL, "Fatal Error: exception caught.", "C++ COLLADA Importer", MB_OK | MB_ICONWARNING | MB_TOPMOST | MB_APPLMODAL);
			errorCode = IMPEXP_FAIL;
		}
	}
#endif

	return errorCode;
}

//
// COLLADAImporterClassDesc
//

class COLLADAImporterClassDesc : public ClassDesc2 
{
public:
	int 			IsPublic() { return TRUE; }
	void*			Create(BOOL isLoading = FALSE) { isLoading; return new COLLADAImporter(); }
	const TCHAR*	ClassName() { return GetString(IDS_CLASS_NAME_I); }
	SClass_ID		SuperClassID() { return SCENE_IMPORT_CLASS_ID; }
	Class_ID		ClassID() { return COLLADAIMPORTER_CLASS_ID; }
	const TCHAR* 	Category() { return GetString(IDS_CATEGORY_I); }

	const TCHAR*	InternalName() { return _T("COLLADAImporter"); }	// returns fixed parsable name (scripter-visible name)
	HINSTANCE		HInstance() { return hInstance; }				// returns owning module handle

};

ClassDesc2* GetCOLLADAImporterDesc()
{
	static COLLADAImporterClassDesc description;
	return &description;
}
