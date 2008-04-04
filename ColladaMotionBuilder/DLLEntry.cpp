/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/
/**
	This plug-in's source is based on the Motion Builder 7.5
	orimpexpgames sample plug-in.
*/

#include "StdAfx.h"

//
// Plug-in main entry point
//

FBLibraryDeclare( ColladaMotionBuilder )
{
	FBLibraryRegister( ExportOptionsTool );
}
FBLibraryDeclareEnd;

// The options container is a global member..
static ColladaOptions* g_options = NULL;
ColladaOptions* GetOptions() { return g_options; }

bool FBLibrary::LibInit()	{ FCollada::Initialize(); g_options = new ColladaOptions(); return true; }
bool FBLibrary::LibOpen()	{ return true; }
bool FBLibrary::LibReady()	{ return true; }
bool FBLibrary::LibClose()	{ return true; }
bool FBLibrary::LibRelease(){ FCollada::Release(); SAFE_DELETE(g_options); return true; }

