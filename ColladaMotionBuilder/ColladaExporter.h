/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COLLADA_EXPORTER_H_
#define _COLLADA_EXPORTER_H_

class FCDocument;
class AnimationExporter;
class CameraExporter;
class ControllerExporter;
class GeometryExporter;
class LightExporter;
class MaterialExporter;
class NodeExporter;

class ColladaExporter
{
private:
	FCDocument* colladaDocument;
	fstring exportFilename;

	AnimationExporter* animationExporter;
	CameraExporter* cameraExporter;
	ControllerExporter* controllerExporter;
	GeometryExporter* geometryExporter;
	LightExporter* lightExporter;
	MaterialExporter* materialExporter;
	NodeExporter* nodeExporter;

public:
	ColladaExporter();
	~ColladaExporter();

	// Accessors
	FCDocument* GetCOLLADADocument() { return colladaDocument; }
	AnimationExporter* GetAnimationExporter() { return animationExporter; }
	CameraExporter* GetCameraExporter() { return cameraExporter; }
	ControllerExporter* GetControllerExporter() { return controllerExporter; }
	GeometryExporter* GetGeometryExporter() { return geometryExporter; }
	LightExporter* GetLightExporter() { return lightExporter; }
	MaterialExporter* GetMaterialExporter() { return materialExporter; }
	NodeExporter* GetNodeExporter() { return nodeExporter; }

	// Main entry points of the exporter.
	void Export(const fchar* filename);
	void Complete();

	// Document-level asset export.
	void ExportAsset();

	//bone list to export
	fm::vector<const char *> boneNameExported;
};

#define CDOC GetBaseExporter()->GetCOLLADADocument()
#define ANIM GetBaseExporter()->GetAnimationExporter()
#define CAMR GetBaseExporter()->GetCameraExporter()
#define CTRL GetBaseExporter()->GetControllerExporter()
#define GEOM GetBaseExporter()->GetGeometryExporter()
#define LGHT GetBaseExporter()->GetLightExporter()
#define MATL GetBaseExporter()->GetMaterialExporter()
#define NODE GetBaseExporter()->GetNodeExporter()

#endif // _COLLADA_EXPORTER_H_