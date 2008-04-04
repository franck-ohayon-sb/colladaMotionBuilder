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

#include "StdAfx.h"
#include "DocumentExporter.h"
#include "AnimationExporter.h"
#include "CameraExporter.h"
#include "ControllerExporter.h"
#include "EmitterExporter.h"
#include "EntityExporter.h"
#include "ColladaOptions.h"
#include "ForceExporter.h"
#include "GeometryExporter.h"
#include "LightExporter.h"
#include "MaterialExporter.h"
#include "NodeExporter.h"
#include "XRefManager.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAsset.h"
#include "FUtils/FUFileManager.h"

#include "common/ConversionFunctions.h"



DocumentExporter::DocumentExporter(DWORD opts)
{
	// Set-up FCollada.
	FCollada::SetDereferenceFlag(false);
	colladaDocument = FCollada::NewTopDocument(); // must be first

	// Members Initialization (in order)
	pMaxInterface		= GetCOREInterface();
	animationExporter	= new AnimationExporter(this);
	cameraExporter		= new CameraExporter(this);
	controllerExporter	= new ControllerExporter(this);
	entityExporter		= new EntityExporter(this);
	geometryExporter	= new GeometryExporter(this);
	lightExporter		= new LightExporter(this);
	materialExporter	= new MaterialExporter(this);
	nodeExporter		= new NodeExporter(this);
	options				= new ColladaOptions(pMaxInterface);

	// Process the options
	options->SetExportSelected((opts & SCENE_EXPORT_SELECTED) == SCENE_EXPORT_SELECTED);
}

DocumentExporter::~DocumentExporter() 
{
	SAFE_DELETE(animationExporter);
	SAFE_DELETE(cameraExporter);
	SAFE_DELETE(controllerExporter);
	SAFE_DELETE(entityExporter);
	SAFE_DELETE(geometryExporter);
	SAFE_DELETE(lightExporter);
	SAFE_DELETE(materialExporter);
	SAFE_DELETE(nodeExporter);

	SAFE_DELETE(colladaDocument); // must be last
	SAFE_DELETE(options);

	pMaxInterface = NULL;
}

bool DocumentExporter::ShowExportOptions(BOOL suppressPrompts)
{
	if (!suppressPrompts) 
	{
		// Prompt the user with our dialogbox, and get all the options.
		// The user may cancel the export at this point.
		if (!options->ShowDialog(TRUE)) return false;
	}
	else if (!options->SampleAnim())
	{
		Interval animRange = GetCOREInterface()->GetAnimRange();
		int sceneStart = animRange.Start();
		int sceneEnd = animRange.End();
		options->SetAnimBounds(sceneStart, sceneEnd);
	}

	// Set relative/absolute export
	colladaDocument->GetFileManager()->SetForceAbsoluteFlag(!options->ExportRelativePaths());

	return true;
}

void DocumentExporter::ExportCurrentMaxScene()
{
	// Set the document's start/end time as the animation timeline.
	colladaDocument->SetStartTime(FSConvert::MaxTickToSecs(options->AnimStart()));
	colladaDocument->SetEndTime(FSConvert::MaxTickToSecs(options->AnimEnd()));

	ExportAsset();
	nodeExporter->ExportScene();
	animationExporter->FinalizeExport();
}

void DocumentExporter::ExportAsset()
{
	FCDAsset* asset = colladaDocument->GetAsset();
	FCDAssetContributor* contributor = asset->AddContributor();

	// Set the author
	TCHAR* userName = getenv("USERNAME");
	if (userName == NULL || *userName == 0) userName = getenv("USER");
	if (userName != NULL && *userName != 0) contributor->SetAuthor(fstring(userName));

    // Set the authoring tool
	fchar authoringToolString[1024];
	fsnprintf(authoringToolString, 1024, FC("3dsMax %d - Feeling ColladaMax v%d.%02d%s."), MAX_VERSION_MAJOR, FCOLLADA_VERSION >> 16, FCOLLADA_VERSION & 0xFFFF, FCOLLADA_BUILDSTR);
	authoringToolString[1023] = 0;
	contributor->SetAuthoringTool(authoringToolString);

	const TSTR& filename = GetMaxInterface()->GetCurFilePath();
	contributor->SetSourceData(filename.data());

	// set *system* unit information
	int systemUnitType = UNITS_CENTIMETERS;
	float systemUnitScale = 1.0f;
	GetMasterUnitInfo(&systemUnitType, &systemUnitScale);

	switch (systemUnitType)
	{
	case UNITS_INCHES: asset->SetUnitName(FC("inch")); asset->SetUnitConversionFactor(systemUnitScale * 0.0254f); break;
	case UNITS_FEET: asset->SetUnitName(FC("feet")); asset->SetUnitConversionFactor(systemUnitScale * 0.3048f); break;
	case UNITS_MILES: asset->SetUnitName(FC("mile")); asset->SetUnitConversionFactor(systemUnitScale * 1609.344f); break;
	case UNITS_MILLIMETERS: asset->SetUnitName(FC("millimeter")); asset->SetUnitConversionFactor(systemUnitScale * 0.001f); break;
	case UNITS_CENTIMETERS: asset->SetUnitName(FC("centimeter")); asset->SetUnitConversionFactor(systemUnitScale * 0.01f); break;
	case UNITS_METERS: asset->SetUnitName(FC("meter")); asset->SetUnitConversionFactor(systemUnitScale); break;
	case UNITS_KILOMETERS: asset->SetUnitName(FC("kilometer")); asset->SetUnitConversionFactor(systemUnitScale * 1000.0f); break;
	default: break;
	}

	// Dump the export options into the comments section of the contributor
	FUStringBuilder optionsSz(FC("ColladaMax Export Options: "));

#define APPEND_OPTION(optionName) \
	optionsSz.append(#optionName "="); \
	optionsSz.append(options->optionName()); \
	optionsSz.append(';');

#define APPEND_OPTION_TIME(optionName) \
	optionsSz.append(#optionName "="); \
	optionsSz.append(FSConvert::MaxTickToSecs(options->optionName())); \
	optionsSz.append(';');

	APPEND_OPTION(ExportNormals);
	APPEND_OPTION(ExportEPolyAsTriangles);
	APPEND_OPTION(ExportXRefs);
	APPEND_OPTION(ExportSelected);
	APPEND_OPTION(ExportTangents);
	APPEND_OPTION(ExportAnimations);
	APPEND_OPTION(SampleAnim);
	APPEND_OPTION(ExportAnimClip);
	APPEND_OPTION(BakeMatrices);
	APPEND_OPTION(ExportRelativePaths);
	APPEND_OPTION_TIME(AnimStart);
	APPEND_OPTION_TIME(AnimEnd);

	contributor->SetComments(optionsSz.ToString());

#undef APPEND_OPTION
#undef APPEND_OPTION_TIME

	// Set the up-axis
	asset->SetUpAxis(FMVector3::ZAxis);
}
