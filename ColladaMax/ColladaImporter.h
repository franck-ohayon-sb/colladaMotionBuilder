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

#ifndef _COLLADA_IMPORTER_H_
#define _COLLADA_IMPORTER_H_

#define COLLADAIMPORTER_CLASS_ID	Class_ID(0x6f8128b3, 0x16ec0a83)

class COLLADAImporter : public SceneImport
{
public:
	COLLADAImporter();
	virtual	~COLLADAImporter();

	virtual int	ExtCount();
	virtual const TCHAR* Ext(int n);
	virtual const TCHAR* LongDesc();
	virtual const TCHAR* ShortDesc();
	virtual const TCHAR* AuthorName();		
	virtual const TCHAR* CopyrightMessage();
	virtual const TCHAR* OtherMessage1();
	virtual const TCHAR* OtherMessage2();
	virtual unsigned int Version();	
	virtual void ShowAbout(HWND hWnd);
	virtual int ZoomExtents();		

	// File import entry point
	virtual int	DoImport(const TCHAR *name, ImpInterface *ii, Interface *i, BOOL suppressPrompts=FALSE);
};

ClassDesc2* GetCOLLADAImporterDesc();

#endif // _COLLADA_IMPORTER_H_