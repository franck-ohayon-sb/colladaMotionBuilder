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

// Skin Controller Importer Class

#ifndef _SKIN_IMPORTER_H_
#define _SKIN_IMPORTER_H_

#include "EntityImporter.h"

class DocumentImporter;
class NodeImporter;
class GeometryImporter;
class FCDSkinController;
class FCDControllerInstance;
class IDerivedObject;
class Modifier;

class SkinImporter : public EntityImporter
{
private:
	EntityImporter* target;
	FCDSkinController* colladaSkin;
	IDerivedObject* derivedObject;
	Modifier* skinModifier;

public:
	SkinImporter(DocumentImporter* document, NodeImporter* parent);
	virtual ~SkinImporter();

	// EntityImporter's overrides: RTTI-like type information
	virtual Type GetType() { return SKIN; }
	virtual Object* GetObject();
	virtual void AddParent(NodeImporter* parent);
	
	bool SetTargetCurrentImport(INode* currImport);

	// Main entry point for the class.
	// Create a Max mesh-derived object, with the skin modifier from a COLLADA skin controller.
	Object* ImportSkinController(FCDSkinController* colladaSkin);
	
	bool BonesAreCorrect(FCDControllerInstance* inst, Modifier* skinModifier);
	// Check to make sure the instantiated skin has the
	// same bones as this instance, otherwise, make something
	// unique and set the correct bones in.
	bool MaybeRebuild(FCDEntityInstance* instance);

	// Finalize the import of the skin modifier. Call after the bones have been created
	virtual void Finalize(FCDEntityInstance* instance);
};

#endif // _SKIN_IMPORTER_H_