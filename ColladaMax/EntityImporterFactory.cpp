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

// Entity Importer Factory Class - Creates entity importers
// Useful because both NodeImporter and the controller importers create sub-entities

#include "StdAfx.h"
#include "EntityImporterFactory.h"
#include "GeometryImporter.h"
#include "CameraImporter.h"
#include "LightImporter.h"
#include "NodeImporter.h"
#include "MorpherImporter.h"
#include "SkinImporter.h"
#include "EmitterImporter.h"
#include "XRefImporter.h"
#include "FCDocument/FCDEntity.h"
#include "FCDocument/FCDController.h"

EntityImporter* EntityImporterFactory::Create(FCDEntity* colladaEntity, DocumentImporter* document, NodeImporter* parentNode)
{
	EntityImporter* out = NULL;

	FCDEntity::Type type = colladaEntity->GetType();
	switch (type)
	{
	case FCDEntity::GEOMETRY: {
		// Create a static mesh object
		GeometryImporter* geometryImporter = new GeometryImporter(document, parentNode);
		Object* object = geometryImporter->ImportGeometry((FCDGeometry*) colladaEntity);
		if (object == NULL) { SAFE_DELETE(geometryImporter); break; }
		out = geometryImporter;
		break; }

	case FCDEntity::CONTROLLER: {
		// Create the correct controller.
		FCDController* colladaController = (FCDController*) colladaEntity;
		if (colladaController->IsSkin())
		{
			// Instantiate a skinned object
			SkinImporter* skinImporter = new SkinImporter(document, parentNode);
			Object* object = skinImporter->ImportSkinController(colladaController->GetSkinController());
			if (object == NULL) { SAFE_DELETE(skinImporter); break; }
			out = skinImporter;
		}
		else if (colladaController->IsMorph())
		{
			// Instantiate a morphed object
			MorpherImporter* morphImporter = new MorpherImporter(document, parentNode);
			Object* object = morphImporter->ImportMorphController(colladaController->GetMorphController());
			if (object == NULL) { SAFE_DELETE(morphImporter); break; }
			out = morphImporter;
		}
		break; }

	case FCDEntity::CAMERA: {
		// Create a camera object
		CameraImporter* cameraImporter = new CameraImporter(document, parentNode);
		Object* object = cameraImporter->ImportCamera((FCDCamera*) colladaEntity);
		if (object == NULL) { SAFE_DELETE(cameraImporter); break; }
		out = cameraImporter;
		break; }

	case FCDEntity::LIGHT: {
		// Create a light object
		LightImporter* lightImporter = new LightImporter(document, parentNode);
		Object* object = lightImporter->ImportLight((FCDLight*) colladaEntity);
		if (object == NULL) { SAFE_DELETE(lightImporter); break; }
		out = lightImporter;
		break; }

	case FCDEntity::SCENE_NODE: {
		// Create the child scene node
		NodeImporter* importer = new NodeImporter(document, parentNode);
		ImpNode* childNode = importer->ImportSceneNode((FCDSceneNode*) colladaEntity);
		if (childNode == NULL){ SAFE_DELETE(importer); break; }
		out = importer;
		break; }

	case FCDEntity::EMITTER: // Premium support only.
	case FCDEntity::MATERIAL:
		// This entity type should not be instantiated in the scene nodes/controllers
	default: break;
	}

	return out;
}
