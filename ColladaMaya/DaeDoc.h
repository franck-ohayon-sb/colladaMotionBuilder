/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_SCENE_INCLUDED__
#define __DAE_SCENE_INCLUDED__

#include <maya/MPxFileTranslator.h>
#include <maya/MItDag.h>
#include <maya/MSelectionList.h>
#include <maya/MFnTransform.h>
#include <maya/MDagPathArray.h>
#include <maya/MTransformationMatrix.h>
#include "TranslatorHelpers/HIAnimCache.h"

class FCDEntity;

class DaeAgeiaPhysicsScene;
class DaeAnimationLibrary;
class DaeAnimClipLibrary;
class DaeCameraLibrary;
class DaeControllerLibrary;
class DaeEmitterLibrary;
class DaeEntityManager;
class DaeForceFieldLibrary;
class DaeGeometryLibrary;
class DaeLightLibrary;
class DaeMaterialLibrary;
class DaeNativePhysicsScene;
class DaePhysicsMaterialLibrary;
class DaePhysicsModelLibrary;
class DaePhysicsSceneLibrary;
class DaeSceneGraph;
class DaeTextureLibrary;
class FCDocument;
class DaeDocNode;

class DaeDoc
{
public:
	DaeDoc(DaeDocNode* colladaNode, const MString& documentFilename, uint logicalIndex);
	~DaeDoc();

	// Retrieves the document filename
	inline DaeDocNode* GetCOLLADANode() { return colladaNode; }
	inline const MString& GetFilename() const { return filename; }
	inline uint GetLogicalIndex() const { return logicalIndex; }

	// Import/Export a COLLADA document
	void Import(bool isCOLLADAReference);
	void Export(bool selectionOnly);

	// Are we import or exporting COLLADA?
	inline bool IsImport() const { return isImport; }
	inline bool IsExport() const { return !isImport; }

	// Access the scene libraries
	inline DaeGeometryLibrary* GetGeometryLibrary() { return geometryLibrary; }
	inline DaeCameraLibrary* GetCameraLibrary() { return cameraLibrary; }
	inline DaeControllerLibrary* GetControllerLibrary() { return controllerLibrary; }
	inline DaeAnimationLibrary* GetAnimationLibrary() { return animationLibrary; }
	inline DaeEmitterLibrary* GetEmitterLibrary() { return emitterLibrary; }
	inline DaeEntityManager* GetEntityManager() { return entityManager; }
	inline DaeForceFieldLibrary* GetForceFieldLibrary() { return forceFieldLibrary; }
	inline DaeMaterialLibrary* GetMaterialLibrary() { return materialLibrary; }
	inline DaeTextureLibrary* GetTextureLibrary() { return textureLibrary; }
	inline DaeLightLibrary* GetLightLibrary() { return lightLibrary; }
	inline DaePhysicsMaterialLibrary* GetPhysicsMaterialLibrary() { return physicsMaterialLibrary; }
	inline DaePhysicsModelLibrary* GetPhysicsModelLibrary() { return physicsModelLibrary; }
	inline DaePhysicsSceneLibrary* GetPhysicsSceneLibrary() { return physicsSceneLibrary; }
	inline DaeNativePhysicsScene* GetNativePhysicsScene() { return nativePhysicsScene; }
	inline const DaeNativePhysicsScene* GetNativePhysicsScene() const { return nativePhysicsScene; }
	inline DaeAgeiaPhysicsScene* GetAgeiaPhysicsScene() { return ageiaPhysicsScene; }
	inline const DaeAgeiaPhysicsScene* GetAgeiaPhysicsScene() const { return ageiaPhysicsScene; }
	inline DaeSceneGraph* GetSceneGraph() { return sceneGraph; }
	inline DaeAnimClipLibrary* GetAnimClipLibrary() { return animClipLibrary; }
	inline CAnimCache* GetAnimationCache() { return animCache; } 
	inline float GetUIUnitFactor() { return uiUnitFactor; }

	// Create COLLADA names and ids
	MString MayaNameToColladaName(const MString& str, bool removeNamespace=true);
	MString DagPathToColladaId(const MDagPath& dagPath);
	MString DagPathToColladaName(const MDagPath& dagPath);
	MString ColladaNameToDagName(const MString& name);

	// Retrieve the COLLADA base document
	FCDocument* GetCOLLADADocument() { return colladaDocument; }

	// Import whatever this entity is.
	DaeEntity* Import(FCDEntity* colladaEntity);

private:
	// Import/export the document asset information
	void ImportAsset();
	void ExportAsset();

	// Manage the import/export libraries
	void CreateLibraries();
	void ReleaseLibraries();

	// Basic elements
	CAnimCache* animCache;
	DaeAgeiaPhysicsScene* ageiaPhysicsScene;
	DaeAnimationLibrary* animationLibrary;
	DaeAnimClipLibrary* animClipLibrary;
	DaeCameraLibrary* cameraLibrary;
	DaeControllerLibrary* controllerLibrary;
	DaeEmitterLibrary* emitterLibrary;
	DaeForceFieldLibrary* forceFieldLibrary;
	DaeEntityManager* entityManager;
	DaeGeometryLibrary*	geometryLibrary;
	DaeLightLibrary* lightLibrary;
	DaeMaterialLibrary* materialLibrary;
	DaeNativePhysicsScene* nativePhysicsScene;
	DaeTextureLibrary* textureLibrary;
	DaePhysicsMaterialLibrary* physicsMaterialLibrary;
	DaePhysicsModelLibrary* physicsModelLibrary;
	DaePhysicsSceneLibrary* physicsSceneLibrary;

	DaeSceneGraph* sceneGraph;

	// Length Conversion Factor
	float uiUnitFactor;

	// FCollada Document
	FCDocument* colladaDocument;
	bool isImport;

	// COLLADA document DG node
	DaeDocNode* colladaNode;
	MString filename;
	uint logicalIndex;
};

#define ANIM doc->GetAnimationLibrary()
#define CDOC doc->GetCOLLADADocument()
#define NODE doc->GetCOLLADANode()

#endif /* __DAE_MAYA_EXPORTER__ */
