/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/*
* COLLADA Export options
*/

#ifndef __DAE_EXPORT_OPTIONS_INCLUDED__
#define __DAE_EXPORT_OPTIONS_INCLUDED__

#include <maya/MPxFileTranslator.h>

class CExportOptions
{
public:
	// Reset all options to their default values.
	static void Set(const MString &optionsString);

	// Should the transforms be baked into a single matrix,
	// as opposed to decomposed into primitive collada
	// transforms (e.g. translate, rotate, scale)?
	// Default: FALSE
	static bool BakeTransforms() { return bakeTransforms; }
	static bool ExportPolygonMeshes() { return exportPolygonMeshes; }
	static bool BakeLighting() { return bakeLighting; }
	static bool IsSampling() { return isSampling; }
	static bool CurveConstrainSampling() { return curveConstrainSampling; }
	static bool RemoveStaticCurves() { return removeStaticCurves; }
	static bool ExportCameraAsLookat() { return exportCameraAsLookat; }
	static bool RelativePaths() { return relativePaths; }

	// Export filters
	static bool ExportLights() { return exportLights; }
	static bool ExportCameras() { return exportCameras; }
	static bool ExportJointsAndSkin() { return exportJointsAndSkin; }
	static bool ExportAnimations() { return exportAnimations; }
	static bool ExportTriangles() { return exportTriangles; }
	static bool ExportInvisibleNodes() { return exportInvisibleNodes; }
	static bool ExportDefaultCameras() { return exportDefaultCameras; }
	static bool ExportNormals() { return exportNormals; }
	static bool ExportTexCoords() { return exportTexCoords; }
	static bool ExportVertexColors() { return exportVertexColors; }
	static bool ExportVertexColorAnimations() { return exportVertexColorAnimations; }
	static bool ExportTangents() { return exportTangents; }
	static bool ExportTexTangents() { return exportTexTangents; }

	// DF Export Filters
	static bool ExportConstraints() { return exportConstraints; }
    static bool ExportPhysics() { return exportPhysics; }
	static MStringArray GetExclusionSets() { return exclusionSets; }	

	// XRef options
	static bool ExportXRefs() { return exportXRefs; }
	static bool DereferenceXRefs() { return dereferenceXRefs; }

	// Camera options
	static bool CameraXFov() { return cameraXFov; }
	static bool CameraYFov() { return cameraYFov; }

private:
	static bool bakeTransforms;
	static bool bakeLighting;
	static bool relativePaths;
	static bool exportPolygonMeshes;
	static bool exportLights;
	static bool exportCameras;
	static bool exportJointsAndSkin;
	static bool exportAnimations;
	static bool exportCameraAsLookat;
	static bool exportTriangles;
	static bool exportInvisibleNodes;
	static bool exportDefaultCameras;
	static bool exportNormals;
	static bool exportTexCoords;
	static bool exportVertexColors;
	static bool exportVertexColorAnimations;
	static bool exportTangents;
	static bool exportTexTangents;
	static bool exportMaterialsOnly;
    static bool exportConstraints;
	static bool removeStaticCurves;
    static bool exportPhysics;
	static bool exportXRefs;
	static bool dereferenceXRefs;
	static bool cameraXFov;
	static bool cameraYFov;

	static int exclusionSetMode;
    static MStringArray exclusionSets;

	static bool isSampling;
	static bool curveConstrainSampling;
	static MFloatArray samplingFunction;
};

class CImportOptions
{
public:
	static void Set(const MString &optionsString, MPxFileTranslator::FileAccessMode mode);

	static bool IsOpenMode() { return isOpenCall; }
	static bool IsReferenceMode() { return isReferenceCall; }
	static bool FileLoadDeferReferencesOption() { return fileLoadDeferRefOptionVar; }
	static bool HasError() { return hasError; }

	static bool ImportUpAxis() { return importUpAxis; }
	static bool ImportUnits() { return importUnits; }
	static bool ImportNormals() { return importNormals; }

	static void SetErrorFlag() { hasError = true; }

private:
	static bool isOpenCall;
	static bool isReferenceCall;
	static bool fileLoadDeferRefOptionVar;
	static bool hasError;

	static bool importUpAxis;
	static bool importUnits;
	static bool importNormals;
};

#endif // __DAE_EXPORT_OPTIONS_INCLUDED__
