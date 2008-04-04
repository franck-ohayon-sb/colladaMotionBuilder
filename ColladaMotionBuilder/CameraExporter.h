/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _CAMERA_EXPORTER_H_
#define _CAMERA_EXPORTER_H_

class FCDEntity;
class FCDCamera;
class FCDSceneNode;

#ifndef _ENTITY_EXPORTER_H_
#include "EntityExporter.h"
#endif // _ENTITY_EXPORTER_H_

class CameraExporter : public EntityExporter
{
private:

public:
	CameraExporter(ColladaExporter* base);
	virtual ~CameraExporter();

	// Entity export.
	FCDEntity* ExportCamera(FBCamera* camera);
	void ExportSystemCameras(FCDSceneNode* colladaScene);
};

#endif // _CAMERA_EXPORTER_H_