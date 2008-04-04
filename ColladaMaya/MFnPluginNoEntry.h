/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/** Use this header if you plan to include MFnPlugin.h in more than one
	include file.*/

// Maya 6.0 and 6.5 don't have MNoPluginEntry macro check
#undef NT_PLUGIN
// tells maya to omit the plugin entry definition
#define MNoPluginEntry
// tells maya to omit the version string definition
#define MNoVersionString
// finally, include the MFnPlugin.h header
#include <maya/MFnPlugin.h>
#define NT_PLUGIN
