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

#ifndef _DOCUMENT_EXPORTER_H_
#define _DOCUMENT_EXPORTER_H_

class FCDocument;
class ColladaOptions;
class AnimationExporter;
class CameraExporter;
class ControllerExporter;
class EntityExporter;
class GeometryExporter;
class LightExporter;
class NodeExporter;
class MaterialExporter;
class ForceExporter;
class EmitterExporter;
class XRefManager;

// Main exporter class
class DocumentExporter
{
private:
	Interface*		pMaxInterface;		// The main MAX UI interface
	ColladaOptions* options;			// The options container
	FCDocument*		colladaDocument;	// The FCollada document to fill in and write out.

	// Sub-library exporter(s)
	AnimationExporter* animationExporter;
	CameraExporter* cameraExporter;
	ControllerExporter* controllerExporter;
	EntityExporter* entityExporter;
	GeometryExporter* geometryExporter;
	LightExporter* lightExporter;
	MaterialExporter* materialExporter;
	NodeExporter* nodeExporter;
	XRefManager* xRefManager;
public:
	DocumentExporter(DWORD options);
	~DocumentExporter();

	// Accessors
	inline FCDocument* GetCOLLADADocument() { return colladaDocument; }
	inline Interface* GetMaxInterface() { return pMaxInterface; }
	inline ColladaOptions* GetColladaOptions() { return options; }

	inline AnimationExporter* GetAnimationExporter() { return animationExporter; }
	inline CameraExporter* GetCameraExporter() { return cameraExporter; }
	inline ControllerExporter* GetControllerExporter() { return controllerExporter; }
	inline EntityExporter* GetEntityExporter() { return entityExporter; }
	inline GeometryExporter* GetGeometryExporter() { return geometryExporter; }
	inline LightExporter* GetLightExporter() { return lightExporter; }
	inline MaterialExporter* GetMaterialExporter() { return materialExporter; }
	inline NodeExporter* GetNodeExporter() { return nodeExporter; }
	inline XRefManager* GetXRefManager() { return xRefManager; }
	// Export
	void ExportCurrentMaxScene();
	void ExportAsset();
	bool ShowExportOptions(BOOL suppressOptions);
};


#endif // _DOCUMENT_EXPORTER_H_
