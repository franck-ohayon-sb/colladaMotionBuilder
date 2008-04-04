/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/*
	COLLADA Export options
	All options are stored statically to allow easy retrieval
	from any class. [claforte] At some point we may want to 
	modify this class to use a singleton pattern...
*/

#include "StdAfx.h"
#include "CExportOptions.h"
#include "TranslatorHelpers/CSetHelper.h"

// Static Members
bool CExportOptions::bakeTransforms = false;
bool CExportOptions::bakeLighting = false;
bool CExportOptions::relativePaths = true;
bool CExportOptions::exportPolygonMeshes = true;
bool CExportOptions::exportLights  = true;
bool CExportOptions::exportCameras = true; 
bool CExportOptions::exportJointsAndSkin = true;
bool CExportOptions::exportAnimations = true;
bool CExportOptions::removeStaticCurves = true;
bool CExportOptions::exportInvisibleNodes = false;
bool CExportOptions::exportDefaultCameras = false;
bool CExportOptions::exportNormals = true;
bool CExportOptions::exportTexCoords = true;
bool CExportOptions::exportVertexColors = true;
bool CExportOptions::exportVertexColorAnimations = false;
bool CExportOptions::exportTangents = false;
bool CExportOptions::exportTexTangents = false;
bool CExportOptions::exportMaterialsOnly = false;
bool CExportOptions::exportCameraAsLookat = true;
bool CExportOptions::exportTriangles = false;
bool CExportOptions::exportXRefs = true;
bool CExportOptions::dereferenceXRefs = false;
bool CExportOptions::cameraXFov = false;
bool CExportOptions::cameraYFov = true;
bool CExportOptions::exportConstraints = true;
bool CExportOptions::exportPhysics = true;
int CExportOptions::exclusionSetMode = CSetHelper::kExcluding;
MStringArray CExportOptions::exclusionSets;

bool CExportOptions::isSampling = false;
bool CExportOptions::curveConstrainSampling = false;
MFloatArray CExportOptions::samplingFunction;

bool CImportOptions::isOpenCall = false;
bool CImportOptions::isReferenceCall = false;
bool CImportOptions::fileLoadDeferRefOptionVar = false;
bool CImportOptions::hasError = false;

bool CImportOptions::importUpAxis = true;
bool CImportOptions::importUnits = true;
bool CImportOptions::importNormals = false;

// Parse the options String
//
void CExportOptions::Set(const MString& optionsString)
{
	// Reset everything to the default value
	bakeTransforms = false;
	bakeLighting = false;
	relativePaths = true;
	isSampling = false;
	curveConstrainSampling = false;
	removeStaticCurves = true;
	samplingFunction.clear();
	exportCameraAsLookat = false;
	exportTriangles = false;
	
	exportPolygonMeshes = true;
	exportLights = true;
	exportCameras = true;
	exportJointsAndSkin = true;
	exportAnimations = true;
	exportInvisibleNodes = false;
	exportDefaultCameras = false;
	exportNormals = true;
	exportTexCoords = true;
	exportVertexColors = true;
	exportVertexColorAnimations = false;
	exportTangents = false;
	exportTexTangents = false;
	exportMaterialsOnly = false;

	exportXRefs = false;
	dereferenceXRefs = true;
	
	cameraXFov = false;
	cameraYFov = true;

	// Parse option string
	if (optionsString.length() > 0) 
	{
		MStringArray optionList;
		optionsString.split(';', optionList);
		uint optionCount = optionList.length();
		for (uint i = 0; i < optionCount; ++i) 
		{
			MString& currentOption = optionList[i]; 

			// Process option name and values.
			MStringArray decomposedOption;
			currentOption.split('=', decomposedOption);
			MString& optionName = decomposedOption[0];

			// For boolean values, the value is assumed to be true
			// if omitted.
			bool value = true;
			if (decomposedOption.length() > 1 &&
				decomposedOption[1] != "true" &&
				decomposedOption[1] != "1")
				value = false;

			// Process options.
			if (optionName == "bakeTransforms") bakeTransforms = value;
			else if (optionName == "bakeLighting") bakeLighting = value;
			else if (optionName == "relativePaths") relativePaths = value;
			else if (optionName == "exportPolygonMeshes") exportPolygonMeshes = value;
			else if (optionName == "exportLights") exportLights = value;
			else if (optionName == "exportCameras") exportCameras = value;
			else if (optionName == "exportJointsAndSkin") exportJointsAndSkin = value;
			else if (optionName == "exportAnimations") exportAnimations = value;
			else if (optionName == "exportInvisibleNodes") exportInvisibleNodes = value;
			else if (optionName == "exportDefaultCameras") exportDefaultCameras = value;
			else if (optionName == "exportNormals") exportNormals = value;
			else if (optionName == "exportTexCoords") exportTexCoords = value;
			else if (optionName == "exportVertexColors") exportVertexColors = value;
			else if (optionName == "exportVertexColorAnimations") exportVertexColorAnimations = value;
			else if (optionName == "exportTangents") exportTangents = value;
			else if (optionName == "exportTexTangents") exportTexTangents = value;
			else if (optionName == "exportMaterialsOnly") exportMaterialsOnly = value;
			else if (optionName == "exportCameraAsLookat") exportCameraAsLookat = value;
			else if (optionName == "exportTriangles") exportTriangles = value;
			else if (optionName == "cameraXFov") cameraXFov = value;
			else if (optionName == "cameraYFov") cameraYFov = value;
			else if (optionName == "isSampling") isSampling = value;
			else if (optionName == "curveConstrainSampling") curveConstrainSampling = value;
			else if (optionName == "removeStaticCurves") removeStaticCurves = value;
            else if (optionName == "exportConstraints") exportConstraints = value;
            else if (optionName == "exportPhysics") exportPhysics = value;
			else if (optionName == "exportXRefs") exportXRefs = value;
			else if (optionName == "dereferenceXRefs") dereferenceXRefs = value;
            else if (optionName == "exclusionSetMode") exclusionSetMode = value;
            else if (optionName == "exclusionSets")
			{
				exclusionSets.clear();
				MStringArray sets;
				decomposedOption[1].split(',', sets);
				uint setsCount = sets.length();
				for (uint j = 0; j < setsCount; ++j) exclusionSets.append(sets[j]);
			}
			else if (optionName == "samplingFunction")
			{
				samplingFunction.clear();
				MStringArray floats;
				decomposedOption[1].split(',', floats);
				uint floatsCount = floats.length();
				for (uint j = 0; j < floatsCount; ++j) samplingFunction.append(floats[j].asFloat());
			}
		}
	}

	// CAnimationHelper handles the sampling, tell it the function.
	// It returns whether the function is valid.
	if (isSampling && !CAnimationHelper::SetSamplingFunction(samplingFunction))
	{
		MGlobal::displayWarning("Given sampling function is invalid.");
		isSampling = false;
		samplingFunction.clear();
	}

	if (!isSampling)
	{
		CAnimationHelper::GenerateSamplingFunction();
	}

    // CSetHelper provides an interface for querying set membership
    // We want to set this every time, even when no sets were passed in, since this function also
    // clears out any sets that might have stuck around from last time
    CSetHelper::setSets(exclusionSets, (CSetHelper::SetModes) exclusionSetMode);	
}

//
// Parse the options String
//
void CImportOptions::Set(const MString& optionsString, MPxFileTranslator::FileAccessMode mode)
{
	// Default option values
	importUpAxis = true;
	importUnits = true;
	importNormals = false;

	hasError = false;

	switch (mode)
	{
	case MPxFileTranslator::kOpenAccessMode: isOpenCall = true; isReferenceCall = false; break;
#if MAYA_API_VERSION >= 650
	case MPxFileTranslator::kReferenceAccessMode: isOpenCall = false; isReferenceCall = true; break;
#endif
	case MPxFileTranslator::kImportAccessMode: isOpenCall = false; isReferenceCall = false; break;
	default: isOpenCall = false; break;
	}

	// Parse option string
	if (optionsString.length() > 0) 
	{
		MStringArray optionList;
		optionsString.split(';', optionList);
		uint optionCount = optionList.length();
		for (uint i = 0; i < optionCount; ++i) 
		{
			MString& currentOption = optionList[i]; 

			// Process option name and values.
			MStringArray decomposedOption;
			currentOption.split('=', decomposedOption);
			MString& optionName = decomposedOption[0];

			// For boolean values, the value is assumed to be true
			// if omitted.
			bool value = true;
			if (decomposedOption.length() > 1 && decomposedOption[1] != "true" && decomposedOption[1] != "1")
			{
				value = false;
			}

			// Process options.
			if (optionName == "importUpAxis") importUpAxis = value;
			else if (optionName == "importUnits") importUnits = value;
			else if (optionName == "importNormals") importNormals = value;
		}
	}

	int optionValue;
	MGlobal::executeCommand("optionVar -q \"fileLoadDeferRef\";", optionValue);
	fileLoadDeferRefOptionVar = optionValue != 0;
}
