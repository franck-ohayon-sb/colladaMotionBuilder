/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COLLADA_MB_OPTIONS_H_
#define _COLLADA_MB_OPTIONS_H_

// Only one of these should be contained at the FBToolLibrary level..
class ColladaOptions
{
private:
	// Configuration settings.
	fstring lastFilePath;
	bool exportTriangleStrips;
	bool export3dData;
	bool exportSystemCameras;
	bool forceSampling;
	bool hasSamplingInterval;
	int samplingStart;
	int samplingEnd;

	bool useBoneList;
	bool exportOnlyAnimAndScene;
	bool exportBakedMatrix;

public:
	ColladaOptions(); // Reads in from the INI file.
	~ColladaOptions(); // Writes out the INI file.

	// The last file-path to be used by the exporter/importer.
	inline const fstring& GetLastFilePath() { return lastFilePath; }

	// Whether to export triangle strips as opposed to a list of polygons.
	inline bool ExportTriangleStrips() { return exportTriangleStrips; }
	
	// Whether the vertex positions should be exported as the standard 3D position or Motion Builder's 4D positions.
	inline bool Export3dData() { return export3dData; }

	// Whether to export system cameras or only user-defined cameras
	inline bool ExportSystemCameras() { return exportSystemCameras; }

	// Whether to sample all animatable properties.
	inline bool ForceSampling() { return forceSampling; }

	// Constrains sampling.
	inline bool HasSamplingInterval() { return hasSamplingInterval; }
	inline int SamplingStart() { return samplingStart; }
	inline int SamplingEnd() { return samplingEnd; }

	inline void setForceSampling(bool val){ forceSampling = val; }
	inline void setSamplingStart(bool val){ samplingStart = val; }
	inline void setSamplingEnd(bool val){ samplingEnd = val; }

	inline bool isUsingBoneList(){ return useBoneList; }
	inline bool isExportingOnlyAnimAndScene(){ return exportOnlyAnimAndScene; }
	inline bool isExportingBakedMatrix(){ return exportBakedMatrix; }

	inline void setBoneList(bool val){ useBoneList = val; }
	inline void setExportingOnlyAnimAndScene(bool val){ exportOnlyAnimAndScene = val; }
	inline void setExportingBakedMatrix(bool val){ exportBakedMatrix = val; }


private:
	// Only the tool should modify this singleton.
	friend class ExportOptionsTool;
	void SetLastFilename(const fchar* filename);
};

#endif // _COLLADA_MB_OPTIONS_H_
