/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "Options.h"
#include "FUtils/FUFileManager.h"

//
// ColladaOptions
//

ColladaOptions::ColladaOptions()
{
	FBConfigFile file("ColladaMotionBuilder.ini", false);
	FBString out = file.Get("Global", "LastFilePath");
	lastFilePath = (fchar*) out;
	out = file.Get("Global", "ExportTriangleStrips", "False");
	exportTriangleStrips = out == "True";
	out = file.Get("Global", "Export3dData", "True");
	export3dData = out != "False";
	out = file.Get("Global", "ExportSystemCameras", "False");
	exportSystemCameras = out == "True";
	out = file.Get("Global", "ForceSampling", "False");
	forceSampling = out == "True";
	out = file.Get("Global", "SamplingStart", "");
	hasSamplingInterval = out != "";
	samplingStart = FUStringConversion::ToInt32((char*) out);
	out = file.Get("Global", "SamplingEnd", "");
	hasSamplingInterval &= out != "";
	samplingEnd = FUStringConversion::ToInt32((char*) out);
}

ColladaOptions::~ColladaOptions()
{
	FBConfigFile file("ColladaMotionBuilder.ini", true);
	file.Set("Global", "LastFilePath", const_cast<char*>(lastFilePath.c_str()));
	file.Set("Global", "ExportTriangleStrips", exportTriangleStrips ? "True" : "False");
	file.Set("Global", "Export3dData", export3dData ? "True" : "False");
	file.Set("Global", "ForceSampling", forceSampling ? "True" : "False");
	if (hasSamplingInterval)
	{
		file.Set("Global", "SamplingStart", const_cast<char*>(TO_STRING(samplingStart).c_str()));
		file.Set("Global", "SamplingEnd", const_cast<char*>(TO_STRING(samplingEnd).c_str()));
	}
}

void ColladaOptions::SetLastFilename(const fchar* filename)
{
	// Strip the filename..
	lastFilePath = FUFileManager::StripFileFromPath(filename);
}
