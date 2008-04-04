/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_ENTITY_MANAGER__
#define __DAE_ENTITY_MANAGER__

#ifndef __DAE_BASE_LIBRARY_INCLUDED__
#include "DaeBaseLibrary.h"
#endif // __DAE_BASE_LIBRARY_INCLUDED__

class FCDEntity;
class FCDENode;
class DaeEntity;
typedef fm::pvector<DaeEntity> DaeEntityList;

class DaeEntityManager : public DaeBaseLibrary
{
public:
	DaeEntityManager(DaeDoc* doc);
	~DaeEntityManager();

	// Manage the list of document entities
	void Clear();
	void RegisterEntity(DaeEntity* entity);
	void ReleaseEntity(DaeEntity* entity);
	uint GetEntityIndex(DaeEntity* entity);
	size_t GetEntityCount() { return entities.size(); }
	DaeEntity* GetEntity(uint index) { FUAssert(index < GetEntityCount(), return NULL); return entities.at(index); }
	void ReleaseNodes();

	// Export/Import per-entity user-defined information
	// TODO: move these functions to the DaeEntity class.
	void ExportEntity(const MObject& obj, FCDEntity* entity);
	void ImportEntity(MObject& obj, DaeEntity* entity);

private:
	void ExportEntityAttribute(MPlug& parameterPlug, FCDENode* parentNode);
	MObject ImportEntityAttribute(MObject& parentAttribute, FCDENode* parameterNode);
	void ImportEntityAttributeValues(MPlug& parameterPlug, FCDENode* parameterNode);

	DaeEntityList entities;
};

#endif // __DAE_ENTITY_MANAGER__

