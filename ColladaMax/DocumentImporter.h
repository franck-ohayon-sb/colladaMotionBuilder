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

#ifndef _SCENE_IMPORTER_H_
#define _SCENE_IMPORTER_H_

class ImpInterface;
class ImpNode;
class FCDocument;
class FCDGeometry;
class FCDLight;
class FCDMaterial;
class FCDSceneNode;
class AnimationImporter;
class NodeImporter;
class EntityImporter;
class XRefManager;
class MaterialImporter;
class COLLADADocumentContainer;
class ColladaEffectParameterNode;
class ColladaOptions;

typedef fm::pvector<NodeImporter> NodeImporterList;
typedef fm::pvector<FCDLight> LightList;
typedef fm::pvector<MaterialImporter> MaterialImporterList;
typedef fm::map<fm::string, EntityImporter*> IDEntityMap;
typedef fm::pvector<ColladaEffectParameterNode> EffectNodeParameterList;

class DocumentImporter
{
private:
	// Main 3dsMax import interface.
	// Contained once, always access it through this class.
	ImpInterface* importInterface;

	// COLLADA document base object
	// You should use this to look for animations
	FCDocument* colladaDocument;

	// Map of all the instances created
	IDEntityMap instances;

	// Scene graph tree
	NodeImporterList sceneNodes;

	// List of all the COLLADA lights that affect ambient lighting.
	// Last element loaded into 3dsMax
	LightList ambientLights;

	// One global animation importer.
	AnimationImporter* animationImporter;

	// One global and persistent COLLADA document container.
	COLLADADocumentContainer* documentContainer;

	// The options container
	ColladaOptions* options;

	XRefManager* xRefManager;

	// For ColladaMax backward compatibility
	bool isCMaxExported;
	float versionCMax;

public:
	DocumentImporter(ImpInterface* importInterface);
	~DocumentImporter();

	// Read in the import options
	BOOL ShowImportOptions(BOOL suppressPrompts);

	// Accessors
	inline void AddAmbientLight(FCDLight* ambientLight) { ambientLights.push_back(ambientLight); }
	inline FCDocument* GetCOLLADADocument() const { return colladaDocument; }
	inline ImpInterface* GetImportInterface() const { return importInterface; }

	// Create a max object, specified by the given SuperClassID and ClassID
	void* MaxCreate(SClass_ID sid, Class_ID clid);

	// Create a max object, specified by a given SuperClassID and ClassName
	void* MaxCreate(SClass_ID sid, TSTR name);
	
	// Add a new instance to our list
	void AddInstance(const fm::string& colladaId, EntityImporter* entity);

	// Search for an instance with the given COLLADA id
	// Use this method to find a node, material, geometry...
	EntityImporter* FindInstance(const fm::string& colladaId);

	// The total number of instances imported in this scene
	size_t NumInstances() { return instances.size(); }
	// Get the i'th instance
	EntityImporter* GetInstance(size_t i);

	
	// Class entry point
	// Creates the Max scene, given a COLLADA document.
	void ImportDocument();
	void ImportAsset();

	// Retrieve the managers
	inline XRefManager* GetXRefManager() { return xRefManager; }
	inline AnimationImporter* GetAnimationImporter() { return animationImporter; }
	inline ColladaOptions* GetColladaOptions() { return options; }
	COLLADADocumentContainer* GetDocumentContainer();

	inline bool IsDocumentColladaMaxExported() { return isCMaxExported; }
	inline float GetDocumentColladaMaxVersion() { return versionCMax; }

	// Scene Node access
	inline size_t GetSceneNodeCount() { return sceneNodes.size(); }
	inline NodeImporter* GetSceneNode(size_t idx) { return sceneNodes[idx]; }

	// Break down the import to allow more control by XRefManager.
	void InitializeImport();
	void ImportScene();
	void FinalizeImport();
};

// Macro to help retrieves important import modules
#define ANIM GetDocument()->GetAnimationImporter()
#define CDOC GetDocument()->GetCOLLADADocument()
#define OPTS GetDocument()->GetColladaOptions();

/** A singleton listener for POST_IMPORT notifications.*/
class PostImportListener
{
private:
	static PostImportListener mInstance;
	static void NotificationCallback(void* param, NotifyInfo* info);

private:
	PostImportListener();
	PostImportListener(const PostImportListener&){}
	
	void MatchNodeParameters();

public:
	~PostImportListener();

	// ColladaEffectParameterNode post-import registration
	void RegisterNodeParameter(ColladaEffectParameterNode* param);

public:
	static PostImportListener& GetInstance(){ return mInstance; }

private:
	// Keeps track of the ColladaEffectParameterNode instances during material import
	// and binds them to an imported node after the scene has been created
	EffectNodeParameterList effectNodeParams;
};

#endif // _SCENE_IMPORTER_H_