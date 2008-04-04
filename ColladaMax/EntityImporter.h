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

// Entity Importer Class - Useful for instantiation

#ifndef _ENTITY_IMPORTER_H_
#define _ENTITY_IMPORTER_H_

class DocumentImporter;
class NodeImporter;
class FCDEntity;
class FCDEntityInstance;
class FCDENode;

typedef fm::pvector<NodeImporter> NodeImporterList;

class EntityImporter
{
private:
	DocumentImporter* document;
	NodeImporterList parents;
	bool finalized;

public:
	EntityImporter(DocumentImporter* document, NodeImporter* parent);
	virtual ~EntityImporter();

	enum Type
	{ NODE, GEOMETRY, SKIN, MORPHER, CAMERA, LIGHT, MATERIAL, PARTICLE_SYSTEM, XREF };

	// Retrieves the base document importer.
	DocumentImporter* GetDocument() { return document; }

	// Access/Modify the parent information
	inline NodeImporter* GetParent(int i=0) { return i < (int) parents.size() ? parents[i] : NULL; }
	inline int GetParentCount() { return (int) parents.size(); }
	virtual void AddParent(NodeImporter* parent) { parents.push_back(parent); }

	// RTTI-like
	virtual Type GetType() = 0;

	// Returns the entity object for this node.
	// Only specific entities override this function: geometries, cameras, lights..
	virtual Object* GetObject() { return NULL; }

	// Finalize the entity. Called after everything is created, including the scene nodes
	// Also imports the dynamic attributes from the COLLADA entity.
	inline bool IsFinalized() { return finalized; }
	virtual void Finalize(FCDEntityInstance* colladaInstance);
	virtual void Finalize(FCDEntity* colladaEntity, Animatable* object); // It is preferable to call the above-function instead... It calls this function.


private:
	// The following functions are used to create custom attribute and their animations.
	void AttachEntity(FCDEntity* entity, Animatable* object);
	void ImportEntityAttribute(ParamBlockDesc2* parameterBlock, FCDENode* parameterNode, uint32& controlId, FStringList& pnames);
	void ImportEntityAttributeValue(IParamBlock2* parameterBlock, FCDENode* parameterNode, short& parameterIndex, FStringList& pnames);
};

#endif // _ENTITY_IMPORTER_H_