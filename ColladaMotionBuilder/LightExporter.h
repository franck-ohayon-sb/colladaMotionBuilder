/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _LIGHT_EXPORTER_H_
#define _LIGHT_EXPORTER_H_

class FCDEntity;
class FCDLight;
class FCDSceneNode;

#ifndef _ENTITY_EXPORTER_H_
#include "EntityExporter.h"
#endif // _ENTITY_EXPORTER_H_

class LightExporter : public EntityExporter
{
private:
	//FBGlobalLight* ambientLight;

public:
	LightExporter(ColladaExporter* base);
	virtual ~LightExporter();

	// Entity export.
	FCDEntity* ExportLight(FBLight* light);
	FCDEntity* ExportAmbientLight(FCDSceneNode* colladaScene);
};

#endif // _CONTROLLER_EXPORTER_H_
