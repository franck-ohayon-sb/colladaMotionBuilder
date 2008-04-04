/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_LIGHT_LIBRARY_INCLUDED__
#define __DAE_LIGHT_LIBRARY_INCLUDED__

#include "DaeBaseLibrary.h"

class DaeDoc;
class DaeEntity;
class FCDLight;

typedef fm::pvector<DaeEntity> DaeEntityList;

class DaeLightLibrary : public DaeBaseLibrary
{
public:
	DaeLightLibrary(DaeDoc* doc);
	~DaeLightLibrary();

	// Create a Maya light from the given COLLADA light entity
	void Import();
	DaeEntity* ImportLight(FCDLight* colladaLight);
	void InstantiateLight(DaeEntity* sceneNode, FCDLight* colladaLight);

	// Add an element to this library and return the export Id
	DaeEntity* ExportLight(const MObject& lightNode);

	void ExportLight(FCDSceneNode* sceneNode, const MDagPath& dagPath, bool isLocal);
};

#endif 
