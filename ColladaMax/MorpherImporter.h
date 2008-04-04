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

// Morpher Modifier Importer Class

#ifndef _MORPHER_IMPORTER_H_
#define _MORPHER_IMPORTER_H_

class DocumentImporter;
class GeometryImporter;
class NodeImporter;
class FCDGeometryMesh;
class FCDMorphController;
class IDerivedObject;
class MorphR3;
class morphChannel;
class FCDGeometry;

#include "EntityImporter.h"

class MorpherImporter : public EntityImporter
{
private:
	FCDMorphController* colladaMorph;
	IDerivedObject* derivedObject;
	MorphR3* morpherModifier;
	EntityImporter* target;

public:
	MorpherImporter(DocumentImporter* document, NodeImporter* parent);
	~MorpherImporter();

	// EntityImporter overrides
	virtual Type GetType() { return MORPHER; }
	virtual Object* GetObject();
	virtual void AddParent(NodeImporter* parent);

	bool SetTargetCurrentImport(INode* currImport);

	// Main entry point for the class.
	// Create a Max mesh-derived object, with the morpher modifier from a COLLADA morph controller.
	Object* ImportMorphController(FCDMorphController* colladaMorph);
	
	// Finalize the import of the morpher modifier. Call after the scene graph has been created
	virtual void Finalize(FCDEntityInstance* instance);

private:
	// COLLADA doesn't require that a geometry be instantiated to be a morph target:
	// Manually fill in the channel with the COLLADA geometry data.
	void InitializeChannelGeometry(morphChannel* channel, const FCDGeometry* geometry);
};

#endif // _MORPHER_IMPORTER_H_
