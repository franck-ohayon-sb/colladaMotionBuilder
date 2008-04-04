/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_CONTROLLER_LIBRARY_INCLUDED__
#define __DAE_CONTROLLER_LIBRARY_INCLUDED__

#ifndef __DAE_SCENE_ELEMENT__
#include "DaeEntity.h"
#endif //__DAE_SCENE_ELEMENT__
#ifndef _FCD_SCENE_NODE_
#include "FCDocument/FCDSceneNode.h"
#endif //_FCD_SCENE_NODE_

class DaeController;
class DaeDoc;
class FCDController;
class FCDControllerInstance;
class FCDGeometry;
class FCDGeometryMesh;
class FCDGeometryInstance;
class FCDMorphController;
class FCDSkinController;
class MFnSkinCluster;

typedef fm::pvector<FCDControllerInstance> FCDControllerInstanceList;
typedef fm::pvector<DaeController> DaeControllerList;

class DaeController : public DaeEntity
{
public:
	DaeEntity* target; // import-only
	MDagPathArray influences; // export-only
	FMMatrix44List bindPoses; // export-only
	FCDControllerInstanceList instances; // export-only;

	DaeController(DaeDoc* doc, FCDController* entity, const MObject& node)
		: DaeEntity(doc, (FCDEntity*) entity, node), target(NULL) {}
	virtual ~DaeController() {}
};

class DaeControllerLibrary
{
public:
	DaeControllerLibrary(DaeDoc* doc);
	~DaeControllerLibrary();

	// Create the requested controller instance
	// Note: this function returns whether the id is a controller, not success
	void Import();
	DaeEntity* ImportController(FCDController* colladaController);
	void InstantiateController(DaeEntity* sceneNode, FCDGeometryInstance* colladaInstance, FCDController* colladaController);
	void AttachControllers();
	MObject AttachMorphController(DaeController* controller, FCDGeometry* baseMesh);
	bool ObjectMatchesInstance(const DaeEntityList& influences, const MDagPathArray& paths);
	void LinkSkinControllerInstance(FCDControllerInstance* instance, FCDSkinController* skinController, DaeController* controller);
	MObject AttachSkinController(DaeController* controller);
	MObject AttachSkinController(DaeController* controller, MString meshName);
	void ImportMorphTarget(MPlug& vertexListPlug, MPlug& componentListPlug, FCDGeometry* baseMesh, FCDGeometryMesh* targetMesh);

	// After import: release the unnecessary memory hogging data
	void LeanMemory();
	void LeanSkin(FCDSkinController* colladaSkin);

	// If the given 'node' is of a supported shape type and has a controller,
	// return true and set 'controllerNode'. Otherwise, return false.
	bool GetMorphController(const MObject& node, MObjectArray& controllerNodes);

	// Export any controller associated with the given plug.
	void AddForceNodes(const MDagPath& dagPath);
	void ExportJoint(FCDSceneNode* sceneNode, const MDagPath& dagPath, bool isLocal);
	DaeEntity* ExportController(FCDSceneNode* sceneNode, const MDagPath& dagPath, bool isSkin, 
			bool instantiate=true);
	DaeEntity* ExportController(FCDSceneNode* sceneNode, const MObject& node);
	DaeEntity* ExportMorphController(FCDSceneNode* sceneNode, MObjectArray& controllerNodes, DaeEntity* target);
	DaeEntity* ExportSkinController(MObject controllerNode, MDagPath outputShape, DaeEntity* target);
	FCDGeometry* ExportMorphTarget(MPlug& vertexListPlug, MPlug& targetComponentListPlug, uint targetIndex, FCDGeometry* baseMesh);

	void CompleteInstanceExport(FCDControllerInstance* instance, FCDController* entity);
	void CompleteControllerExport();

	// Returns true if 'node' is of a supported shape type and it has a controller.
	bool HasController(const MObject& node);
	bool HasSkinController(const MObject& node);
	bool HasMorphController(const MObject& node);

private:
	DaeDoc* doc;

	// A lookup table of elements we've already processed
	DaeControllerList importedMorphControllers;
	DaeControllerList skinControllers;
	unsigned long boneCounter; // ensure unique joint names

	// Support for Joint clusters pipeline
	void GetJointClusterInfluences(const MObject& controllerNode, MDagPathArray& influences, MObjectArray& weightFilters, uint clusterIndex);
};

#endif
