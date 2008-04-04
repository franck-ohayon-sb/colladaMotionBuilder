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

#include "StdAfx.h"
#include "ColladaExporter.h"
#include "ControllerExporter.h"
#include "DocumentExporter.h"
#include "FCDocument/FCDocument.h"
#include <locale.h>

//
// COLLADAExporter
//

COLLADAExporter::COLLADAExporter()
{
}

COLLADAExporter::~COLLADAExporter() 
{
}

// SceneExport methods
int COLLADAExporter::ExtCount()
{
	// Returns the number of file name extensions supported by the plug-in.
	return 1;
}

const TCHAR* COLLADAExporter::Ext(int)
{		
	// Return the 'i-th' file name extension (i.e. "dae").
	return _T("dae");
}

const TCHAR* COLLADAExporter::LongDesc()
{
	// Return long ASCII description (i.e. "Targa 2.0 Image File")
	return _T("COLLADA Document");
}
	
const TCHAR* COLLADAExporter::ShortDesc() 
{			
	// Return short ASCII description (i.e. "Targa")
	return _T("COLLADA");
}

const TCHAR* COLLADAExporter::AuthorName()
{			
	// Return ASCII Author name
	return _T("Feeling Software");
}

const TCHAR* COLLADAExporter::CopyrightMessage() 
{	
	// Return ASCII Copyright message
	return _T("Copyright 2006 Feeling Software. Based on Autodesk' 3dsMax COLLADA Tools.");
}

const TCHAR* COLLADAExporter::OtherMessage1() 
{		
	// Return Other message #1 if any
	return _T("");
}

const TCHAR* COLLADAExporter::OtherMessage2() 
{		
	// Return other message #2 in any
	return _T("");
}

unsigned int COLLADAExporter::Version()
{				
	// Return Version number * 100 (e.g. v3.01 = 301)
	uint32 version = (FCOLLADA_VERSION >> 16) * 100;
	version += FCOLLADA_VERSION % 100;
	return version;
}

void COLLADAExporter::ShowAbout(HWND)
{			
	// Optional
}

BOOL COLLADAExporter::SupportsOptions(int, DWORD)
{
	// TODO Decide which options to support.  Simply return
	// true for each option supported by each Extension 
	// the exporter supports.

	return TRUE;
}


// dummy function for progress bar
DWORD WINAPI fn(LPVOID)
{
	return 0;
}

// main export function
int	COLLADAExporter::DoExport(const TCHAR* name, ExpInterface* UNUSED(ei), Interface* i, BOOL suppressPrompts, DWORD options)
{
	BOOL success = FALSE;
	i->ProgressStart(_T("COLLADA export..."), TRUE, fn, NULL);

#if !defined(_DEBUG)
	try
	{
#endif
		// Export
		FUErrorSimpleHandler handler;
		DocumentExporter document(options);
		if (document.ShowExportOptions(suppressPrompts))
		{
			document.ExportCurrentMaxScene();
			//document.GetCOLLADADocument()->WriteToFile(TO_FSTRING(name));
			FCollada::SaveDocument(document.GetCOLLADADocument(), name);
			success = handler.IsSuccessful();
		}
		else
		{
			// Set to TRUE although nothing happened in other to avoid the "generic failure" dialog.
			success = TRUE; 
		}

		document.GetControllerExporter()->ReactivateSkins();

#if !defined(_DEBUG)
	} 
	catch (...)
	{
		// Add some check, here, for full UI-mode or batch-mode only.
		MessageBox(NULL, "Fatal Error: exception caught.", "ColladaMax", MB_OK);
	}
#endif

	// Restore the locale
	setlocale(LC_NUMERIC, "C");
	i->ProgressEnd();

	return success;
}

//
// ColladaExporterClassDesc
//

class ColladaExporterClassDesc : public ClassDesc2 
{
public:
	int 			IsPublic() { return TRUE; }
	void *			Create(BOOL isLoading = FALSE) { isLoading; return new COLLADAExporter(); }
	const TCHAR *	ClassName() { return GetString(IDS_CLASS_NAME_E); }
	SClass_ID		SuperClassID() { return SCENE_EXPORT_CLASS_ID; }
	Class_ID		ClassID() { return COLLADAEXPORTER_CLASS_ID; }
	const TCHAR* 	Category() { return GetString(IDS_CATEGORY_E); }

	const TCHAR*	InternalName() { return _T("COLLADAExporter"); }	// returns fixed parsable name (scripter-visible name)
	HINSTANCE		HInstance() { return hInstance; }				// returns owning module handle

};

ClassDesc2* GetCOLLADAExporterDesc()
{
	static ColladaExporterClassDesc description;
	return &description;
}
