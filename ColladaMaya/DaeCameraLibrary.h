/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_CAMERA_LIBRARY_INCLUDED__
#define __DAE_CAMERA_LIBRARY_INCLUDED__

#include "DaeBaseLibrary.h"

class DaeDoc;
class DaeEntity;
class FCDCamera;
class FCDEntity;
typedef fm::map<MDagPath, MString> DagPathIdMap;
typedef fm::pvector<DaeEntity> DaeEntityList;

class DaeCameraLibrary : public DaeBaseLibrary
{
public:
	DaeCameraLibrary(DaeDoc* doc);
	~DaeCameraLibrary();

	// Attempt to find & create an element from this library
	void Import();
	DaeEntity* ImportCamera(FCDCamera* colladaCamera);
	void InstantiateCamera(DaeEntity* sceneNode, FCDCamera* colladaCamera);

	// Add a camera to this library and return the export Id
	DaeEntity* ExportCamera(const MObject& cameraNode);

	void ExportCamera(FCDSceneNode* sceneNode, const MDagPath& dagPath, bool isLocal);
};





#endif 
