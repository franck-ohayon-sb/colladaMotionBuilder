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

// Camera Importer Class

#ifndef _CAMERA_IMPORTER_H_
#define _CAMERA_IMPORTER_H_

#include "EntityImporter.h"
#include "AnimationImporter.h"

class FCDCamera;
class FCDENode;
class DocumentImporter;

class CameraImporter : public EntityImporter
{
private:
	FCDCamera* colladaCamera;
	CameraObject* cameraObject;

public:
	CameraImporter(DocumentImporter* document, NodeImporter* parent);
	~CameraImporter();

	// EntityImporter overrides
	virtual Type GetType() { return CAMERA; }
	virtual Object* GetObject() { return cameraObject; }

	// Main entry point for the class.
	// Create a Max camera from a COLLADA camera.
	CameraObject* ImportCamera(FCDCamera* colladaCamera);

private:
	IMultiPassCameraEffect *CreateMultiPassEffect(Class_ID id);
	void ImportExtraParameter(FCDENode *parentNode, Animatable *animObj, char *colladaID, int blockIndex, int paramID);
};

#endif // _CAMERA_IMPORTER_H_