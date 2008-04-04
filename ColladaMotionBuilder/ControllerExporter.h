/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _CONTROLLER_EXPORTER_H_
#define _CONTROLLER_EXPORTER_H_

class FCDEntity;
class FCDController;
class FCDControllerInstance;

#ifndef _ENTITY_EXPORTER_H_
#include "EntityExporter.h"
#endif // _ENTITY_EXPORTER_H_

class ControllerExporter : public EntityExporter
{
private:

public:
	ControllerExporter(ColladaExporter* base);
	virtual ~ControllerExporter();

	// Entity export.
	FCDEntity* ExportController(FBModel* node, FBGeometry* geometry);

	// Instance export.
	void ExportControllerInstance(FBModel* node, FCDControllerInstance* colladaInstance, FCDEntity* colladaEntity);

private:
	// Typed entity export.
	FCDEntity* ExportSkinController(FBModel* node, FBCluster& cluster, FCDEntity* colladaTarget);
	FBPose* FindBindPose(FBModel* node);
};

#endif // _CONTROLLER_EXPORTER_H_
