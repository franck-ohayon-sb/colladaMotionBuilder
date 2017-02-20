/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "FCDocument/FCDAsset.h"
#include "FCDocument/FCDocument.h"
#include "AnimationExporter.h"
#include "CameraExporter.h"
#include "ColladaExporter.h"
#include "ControllerExporter.h"
#include "GeometryExporter.h"
#include "LightExporter.h"
#include "MaterialExporter.h"
#include "NodeExporter.h"

//
// ColladaExporter
//

ColladaExporter::ColladaExporter()
:	colladaDocument(NULL), animationExporter(NULL)
,	cameraExporter(NULL), controllerExporter(NULL), geometryExporter(NULL)
,	lightExporter(NULL), materialExporter(NULL), nodeExporter(NULL)
{
}

ColladaExporter::~ColladaExporter()
{
	SAFE_RELEASE(colladaDocument);
	SAFE_DELETE(animationExporter);
	SAFE_DELETE(cameraExporter);
	SAFE_DELETE(controllerExporter);
	SAFE_DELETE(geometryExporter);
	SAFE_DELETE(lightExporter);
	SAFE_DELETE(materialExporter);
	SAFE_DELETE(nodeExporter);
}

void ColladaExporter::Export(const fchar* filename)
{
	exportFilename = filename;
	FUAssert(colladaDocument == NULL, colladaDocument->Release());

	// Create the COLLADA document to fill in.
	colladaDocument = FCollada::NewTopDocument();
	colladaDocument->SetFileUrl(exportFilename);

	// Export the global document-level information
	ExportAsset();

	// Create the sub-exporters
	animationExporter = new AnimationExporter(this);
	cameraExporter = new CameraExporter(this);
	controllerExporter = new ControllerExporter(this);
	geometryExporter = new GeometryExporter(this);
	lightExporter = new LightExporter(this);
	materialExporter = new MaterialExporter(this);
	nodeExporter = new NodeExporter(this);

	nodeExporter->SetBoneListToExport(&boneNameExported);

	// Export the library entities.
	FBSystem global; // think of FBSystem as a function set, rather than an object.
	FBScene* globalScene = global.Scene;

	// Export the scene graph.
	nodeExporter->PreprocessScene(globalScene);
	nodeExporter->ExportScene(globalScene);
	nodeExporter->PostprocessScene(globalScene);
}

void ColladaExporter::Complete()
{
	// Write out the file.
	FCollada::SaveDocument(colladaDocument, exportFilename.c_str());
	SAFE_RELEASE(colladaDocument);

	// Clean-up the sub-exporters.
	SAFE_DELETE(animationExporter);
	SAFE_DELETE(cameraExporter);
	SAFE_DELETE(controllerExporter);
	SAFE_DELETE(geometryExporter);
	SAFE_DELETE(lightExporter);
	SAFE_DELETE(materialExporter);
	SAFE_DELETE(nodeExporter);
}

void ColladaExporter::ExportAsset()
{
	FCDAsset* asset = colladaDocument->GetAsset();
	FCDAssetContributor* contributor = asset->AddContributor();

	// Authoring tool.
	FUStringBuilder builder(FC("Motion Builder v"));
	builder.append(K_FILMBOX_MAIN_VERSION_AS_STRING);
	builder.append(FC(" | FCollada v"));
	uint32 version = FCOLLADA_VERSION & 0x0000FFFF;
	builder.append(FCOLLADA_VERSION >> 16);
    builder.append((fchar) '.');
	builder.append(version / 10); builder.append(version % 10);
    builder.append(FCOLLADA_BUILDSTR);
	fm::string authoringTool = builder.ToString();
	contributor->SetAuthoringTool(TO_FSTRING(authoringTool));

	// Author string.
	const char* userName = getenv("USER");
	if (userName == NULL) userName = getenv("USERNAME");
	if (userName != NULL) contributor->SetAuthor(TO_FSTRING(userName));

	// Comments
	contributor->SetComments(FC("No export options."));

	// Source Data
	FBApplication motionBuilder; // think of FBApplication as a function set, rather than an object.
	fstring sourceData = motionBuilder.FBXFileName.AsString();
	if (sourceData.length() > 0) contributor->SetSourceData(sourceData);

	// Not sure that MotionBuilder has the asset units/up-axis information.
	//-----

	// System units
	// asset->SetUnitConversionFactor(0.01f);

	// System up-axis
	// asset->SetUpAxis(FMVector3::YAxis);

	// FCollada extra asset information
	//---------------------------------------------
	FBPlayerControl player;
	colladaDocument->SetStartTime(ToSeconds(player.LoopStart));
	colladaDocument->SetEndTime(ToSeconds(player.LoopStop));
}
