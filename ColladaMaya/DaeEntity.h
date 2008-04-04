/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_SCENE_ELEMENT__
#define __DAE_SCENE_ELEMENT__

// This class should be the base class for all the FCollada 'user-handle' structures.

class DaeDoc;
class DaeTransform;
class FCDEntity;

class DaeEntity
{
private:
	MObject node; // transient information
	MDagPath nodePath; // persistent information for DAG nodes
	MString nodeName; // persistent information for DG nodes.

public:
	DaeEntity(DaeDoc* doc, FCDEntity* entity, const MObject& node);
	virtual ~DaeEntity();

	virtual void Instantiate(DaeEntity* sceneNode);
	MObject GetNode() const;
	void SetNode(const MObject& node);
	virtual void ReleaseNode();

public:
	FUTrackedPtr<FCDEntity> entity;
	bool isOriginal; // Export-only: implies that the FCDEntity* was not cloned from a previously imported FCDEntity.
	uint instanceCount; // Import-only?
	DaeTransform* transform; // Should be moved to scene nodes only.
};

#endif // __DAE_SCENE_ELEMENT__
