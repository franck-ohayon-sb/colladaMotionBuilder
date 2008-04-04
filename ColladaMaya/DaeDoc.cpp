/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"

#include <time.h>
#include <maya/MAnimControl.h>
#include <maya/MAnimUtil.h>
#include <maya/MEulerRotation.h>
#include <maya/MFileIO.h>
#include <maya/MFnCamera.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MQuaternion.h>
#include <maya/MSelectionList.h>
#include <maya/MFnStringArrayData.h>

#include "DaeDocNode.h"
#include "DaeTransform.h"
#include "DaeTranslator.h"
#include "CExportOptions.h"
#include "CReferenceManager.h"
#include "DaeAnimClipLibrary.h"
#include "MFnPluginNoEntry.h"
#include "TranslatorHelpers/CAnimationHelper.h"
#include "TranslatorHelpers/CShaderHelper.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "TranslatorHelpers/HIAnimCache.h"

#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDocumentTools.h"
#include "FCDocument/FCDAsset.h"
#include "FCDocument/FCDCamera.h"
#include "FCDocument/FCDController.h"
#include "FCDocument/FCDEmitter.h"
#include "FCDocument/FCDForceField.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDLight.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDPhysicsScene.h"
#include "FUtils/FUFileManager.h"

#ifdef _WIN32
#pragma warning(disable: 4355)
#endif

// Constructor / Destructor
DaeDoc::DaeDoc(DaeDocNode* _colladaNode, const MString& documentFilename, uint _logicalIndex)
:	animCache(NULL), ageiaPhysicsScene(NULL), animationLibrary(NULL), animClipLibrary(NULL), cameraLibrary(NULL)
,	controllerLibrary(NULL), emitterLibrary(NULL), forceFieldLibrary(NULL)
,	geometryLibrary(NULL), lightLibrary(NULL), materialLibrary(NULL), nativePhysicsScene(NULL), textureLibrary(NULL)
,	physicsMaterialLibrary(NULL), physicsModelLibrary(NULL)
,	physicsSceneLibrary(NULL), sceneGraph(NULL), uiUnitFactor(1.0f), isImport(true)
,	colladaNode(_colladaNode), filename(documentFilename), logicalIndex(_logicalIndex)
{
	FCollada::SetDereferenceFlag(false);
	colladaDocument = FCollada::NewTopDocument();
	colladaDocument->SetFileUrl(MConvert::ToFChar(documentFilename));
	
	entityManager = new DaeEntityManager(this);
}

DaeDoc::~DaeDoc()
{
	ReleaseLibraries(); // The libraries should already have been released
	SAFE_DELETE(entityManager);
	SAFE_DELETE(colladaDocument); // We want to release the document after the libraries, in case they point to FCollada objects.
}

// Create the parsing libraries: we want access to the libraries only during import/export time.
void DaeDoc::CreateLibraries()
{
	ReleaseLibraries();

	// Create the basic elements
	animCache = new CAnimCache(this);
	sceneGraph = new DaeSceneGraph(this);
	geometryLibrary = new DaeGeometryLibrary(this);
	controllerLibrary = new DaeControllerLibrary(this);
	animationLibrary = new DaeAnimationLibrary(this, isImport);
	materialLibrary = new DaeMaterialLibrary(this);
	textureLibrary = new DaeTextureLibrary(this);
	lightLibrary = new DaeLightLibrary(this);
	cameraLibrary = new DaeCameraLibrary(this);
	physicsMaterialLibrary = new DaePhysicsMaterialLibrary(this);
	physicsModelLibrary = new DaePhysicsModelLibrary(this);
	physicsSceneLibrary = new DaePhysicsSceneLibrary(this);
	nativePhysicsScene = new DaeNativePhysicsScene(this);
	ageiaPhysicsScene = new DaeAgeiaPhysicsScene(this);
	animClipLibrary = new DaeAnimClipLibrary(this);
	emitterLibrary = new DaeEmitterLibrary(this);
	forceFieldLibrary = new DaeForceFieldLibrary(this);
}

void DaeDoc::ReleaseLibraries()
{
	SAFE_DELETE(animCache);
	SAFE_DELETE(geometryLibrary);
	SAFE_DELETE(controllerLibrary);
	SAFE_DELETE(animationLibrary);
	SAFE_DELETE(materialLibrary);
	SAFE_DELETE(textureLibrary);
	SAFE_DELETE(lightLibrary);
	SAFE_DELETE(cameraLibrary);
	SAFE_DELETE(physicsMaterialLibrary);
	SAFE_DELETE(physicsModelLibrary);
	SAFE_DELETE(physicsSceneLibrary);
	SAFE_DELETE(nativePhysicsScene);
	SAFE_DELETE(ageiaPhysicsScene);
	SAFE_DELETE(sceneGraph);
	SAFE_DELETE(animClipLibrary);
	SAFE_DELETE(emitterLibrary);
	SAFE_DELETE(forceFieldLibrary);
}

// Import a COLLADA document
//
void DaeDoc::Import(bool isCOLLADAReference)
{
	// Create the import/export library helpers
	entityManager->Clear();
	isImport = true;
	CreateLibraries();

	// Parse in the COLLADA document
	FUErrorSimpleHandler* errorHandler = NULL;
	if (!isCOLLADAReference) errorHandler = new FUErrorSimpleHandler();
	FCollada::LoadDocumentFromFile(colladaDocument, MConvert::ToFChar(filename));
	if (errorHandler != NULL && !errorHandler->IsSuccessful())
	{
		MGlobal::displayInfo(errorHandler->GetErrorString());
		CImportOptions::SetErrorFlag();
		return;
	}

	// Read in the document asset information
	ImportAsset();

	// Import the DAG entity libraries
	materialLibrary->Import();
	cameraLibrary->Import();
	lightLibrary->Import();
	geometryLibrary->Import();
	controllerLibrary->Import();
	sceneGraph->Import();

	// Instantiate the main visual scene when importing/opening a document
    FCDSceneNode* visualScene = colladaDocument->GetVisualSceneInstance();
	if (!isCOLLADAReference && visualScene != NULL)
	{
		sceneGraph->InstantiateVisualScene(visualScene);
		
		// don't bother if no real physics... it will also show rigid bodies
		// from previous load if import create the solver.
		FCDPhysicsScene* pScene = colladaDocument->GetPhysicsSceneInstance();
		if (pScene && !pScene->GetPhysicsModelInstances().empty())
		{
			MObject nima = MFnPlugin::findPlugin("physx");
			if (nima != MObject::kNullObj)
			{
				physicsSceneLibrary->Import();
				sceneGraph->InstantiatePhysicsScene(pScene);
			}
			else
			{
				MGlobal::displayWarning("physX plugin not detected. Skipping Physics.");
			}
		}
	}

	// When everything is loaded and created, connect the driven key inputs
	animationLibrary->ConnectDrivers();

	// Since we are keeping all FCollada objects around: reduce the memory usage of
	// some well-known memory hogging objects.
	controllerLibrary->LeanMemory();
	geometryLibrary->LeanMemory();

	// Release the import/export library helpers
	ReleaseLibraries();
	colladaNode->ReleaseAllEntityNodes();

	SAFE_DELETE(errorHandler);
}

DaeEntity* DaeDoc::Import(FCDEntity* colladaEntity)
{
	// Import whatever this is.
	DaeEntity* e = (DaeEntity*) colladaEntity->GetUserHandle();
	if (e != NULL) return e;

	if (colladaEntity->GetObjectType().Includes(FCDSceneNode::GetClassType()))
	{
		return sceneGraph->ImportNode((FCDSceneNode*) colladaEntity);
	}
	else if (colladaEntity->GetObjectType().Includes(FCDLight::GetClassType()))
	{
		return lightLibrary->ImportLight((FCDLight*) colladaEntity);
	}
	else if (colladaEntity->GetObjectType().Includes(FCDCamera::GetClassType()))
	{
		return cameraLibrary->ImportCamera((FCDCamera*) colladaEntity);
	}
	else if (colladaEntity->GetObjectType().Includes(FCDGeometry::GetClassType()))
	{
		return geometryLibrary->ImportGeometry((FCDGeometry*) colladaEntity);
	}
	else if (colladaEntity->GetObjectType().Includes(FCDController::GetClassType()))
	{
		return controllerLibrary->ImportController((FCDController*) colladaEntity);
	}
	return NULL;
}

void DaeDoc::ImportAsset()
{
	// Up_axis
	if (MGlobal::mayaState() != MGlobal::kBatch)
	{
		if (CImportOptions::IsOpenMode() && CImportOptions::ImportUpAxis())
		{
			fchar upAxis = 'y';
			const FMVector3& colladaUpAxis = colladaDocument->GetAsset()->GetUpAxis();
			if (IsEquivalent(colladaUpAxis, FMVector3::YAxis)) upAxis = 'y';
			else if (IsEquivalent(colladaUpAxis, FMVector3::ZAxis)) upAxis = 'z';
			else if (IsEquivalent(colladaUpAxis, FMVector3::XAxis)) MGlobal::displayWarning("An up_axis of 'X' is not supported by Maya.");
			else MGlobal::displayWarning("Unknown up_axis value.");

			// Use the MEL commands to set the up_axis. Currently resets the view, if the axis must change..
			FUStringBuilder command(FC("string $currentAxis = `upAxis -q -ax`; if ($currentAxis != \""));
			command.append(upAxis); command.append(FC("\") { upAxis -ax \"")); command.append(upAxis);
			command.append(FC("\"; viewSet -home persp; }"));
			MGlobal::executeCommand(command.ToCharPtr());
		}
	}

	// Retrieve Maya's current up-axis.
	MString result;
	FMVector3 mayaUpAxis = FMVector3::Zero;
	if (CImportOptions::ImportUpAxis())
	{
		mayaUpAxis = FMVector3::YAxis;
		MGlobal::executeCommand(FC("upAxis -q -ax;"), result, false, false);
		if (IsEquivalent(MConvert::ToFChar(result), FC("z"))) mayaUpAxis = FMVector3::ZAxis;
	}

	float mayaUnit = 0.0f;
	if (CImportOptions::ImportUnits()) mayaUnit = 0.01f;

	// Standardize the COLLADA document on this up-axis and units (centimeters).
	FCDocumentTools::StandardizeUpAxisAndLength(colladaDocument, mayaUpAxis, mayaUnit);

	// Get the UI unit factor, for parts of Maya that don't handle variable lengths correctly
	MDistance testDistance(1.0f, MDistance::uiUnit());
	uiUnitFactor = (float) testDistance.as(MDistance::kCentimeters);
}

void DaeDoc::Export(bool selectionOnly)
{
	// Create the import/export library helpers.
	isImport = false;
	CreateLibraries();

	// Get the UI unit type (internal units are centimeter)
	MDistance testDistance(1.0f, MDistance::uiUnit());
	float conversionFactor = (float) testDistance.as(MDistance::kMeters);
	uiUnitFactor = 1.0f / conversionFactor;

	// Export the document asset information.
	ExportAsset();

	// Export all the non-XRef materials
	if (!selectionOnly) GetMaterialLibrary()->Export();

	// Find all the nodes to export, and recurse through the scene to export.
	sceneGraph->FindForcedNodes(selectionOnly);
	sceneGraph->Export(selectionOnly);

	// Export rigid constraints if required.
	if (CExportOptions::ExportPhysics())
	{
		// save non physics animation library in case want to use again
		DaeAnimationLibrary* nonPhysAnimationLibrary = animationLibrary;
		animationLibrary = new DaeAnimationLibrary(this, false);

		nativePhysicsScene->finalize();
		ageiaPhysicsScene->finalize();

//		animationLibrary->ExportAnimations();
		SAFE_DELETE(animationLibrary);
		animationLibrary = nonPhysAnimationLibrary;
	}

	// Retrieve the unit name and set the asset's unit information
	MString unitName;
	MGlobal::executeCommand("currentUnit -q -linear -fullName;", unitName);
	colladaDocument->GetAsset()->SetUnitName(MConvert::ToFChar(unitName));
	FCDocumentTools::StandardizeUpAxisAndLength(colladaDocument, FMVector3::Origin, conversionFactor);

	// Write out the FCollada document.
	colladaDocument->GetFileManager()->SetForceAbsoluteFlag(!CExportOptions::RelativePaths());
	FCollada::SaveDocument(colladaDocument, MConvert::ToFChar(filename));

	// Release the import/export library helpers
	ReleaseLibraries();
	colladaNode->ReleaseAllEntityNodes();
}

void DaeDoc::ExportAsset()
{
	FCDAsset* asset = colladaDocument->GetAsset();

	// Clone any previous document's asset.
	if (GetCOLLADANode()->GetDocumentCount() > 0)
	{
		DaeDoc* previousDocument = GetCOLLADANode()->GetDocument(0);
		if (previousDocument != NULL && previousDocument->GetCOLLADADocument() != NULL)
		{
			previousDocument->GetCOLLADADocument()->GetAsset()->Clone(asset);
		}
	}

	FCDAssetContributor* contributor = asset->AddContributor();

	const char* userName = getenv("USER");
	if (userName == NULL) userName = getenv("USERNAME");
	if (userName != NULL) contributor->SetAuthor(TO_FSTRING(userName));

	FUStringBuilder authoringTool(FC("Maya"));
	authoringTool.append(MConvert::ToFChar(MGlobal::mayaVersion()));
	authoringTool.append(FC(" | ColladaMaya v"));
	uint32 version = FCOLLADA_VERSION & 0x0000FFFF;
	authoringTool.append(FCOLLADA_VERSION >> 16); authoringTool.append((fchar) '.');
	authoringTool.append(version / 10); authoringTool.append(version % 10);
	authoringTool.append(TO_FSTRING(FCOLLADA_BUILDSTR));
	contributor->SetAuthoringTool(authoringTool.ToCharPtr());
	
	// comments
	MString optstr = MString("ColladaMaya export options: bakeTransforms=") + CExportOptions::BakeTransforms()
		+ ";exportPolygonMeshes=" + CExportOptions::ExportPolygonMeshes() + ";bakeLighting=" + CExportOptions::BakeLighting()
		+ ";isSampling=" + CExportOptions::IsSampling() + ";\ncurveConstrainSampling=" + CExportOptions::CurveConstrainSampling()
		+ ";removeStaticCurves=" + CExportOptions::RemoveStaticCurves() + ";exportCameraAsLookat=" + CExportOptions::ExportCameraAsLookat()
		+ ";\nexportLights=" + CExportOptions::ExportLights() + ";exportCameras=" + CExportOptions::ExportCameras()
		+ ";exportJointsAndSkin=" + CExportOptions::ExportJointsAndSkin() + ";\nexportAnimations=" + CExportOptions::ExportAnimations()
		+ ";exportTriangles=" + CExportOptions::ExportTriangles() + ";exportInvisibleNodes=" + CExportOptions::ExportInvisibleNodes()
		+ ";\nexportNormals=" + CExportOptions::ExportNormals() + ";exportTexCoords=" + CExportOptions::ExportTexCoords()
		+ ";\nexportVertexColors=" + CExportOptions::ExportVertexColors()
		+ ";exportVertexColorsAnimation=" + CExportOptions::ExportVertexColorAnimations()
		+ ";exportTangents=" + CExportOptions::ExportTangents()
		+ ";\nexportTexTangents=" + CExportOptions::ExportTexTangents() + ";exportConstraints=" + CExportOptions::ExportConstraints()
		+ ";exportPhysics=" + CExportOptions::ExportPhysics() + ";exportXRefs=" + CExportOptions::ExportXRefs()
		+ ";\ndereferenceXRefs=" + CExportOptions::DereferenceXRefs() + ";cameraXFov=" + CExportOptions::CameraXFov()
		+ ";cameraYFov=" + CExportOptions::CameraYFov();
	contributor->SetComments(MConvert::ToFChar(optstr));

	// source is the scene we have exported from
	MString currentScene = MFileIO::currentFile();
	if (currentScene.length() != 0)
	{
		// Intentionally not relative
		FUUri uri(colladaDocument->GetFileManager()->GetCurrentUri().MakeAbsolute(MConvert::ToFChar(currentScene)));
		fstring sourceDocumentPath = uri.GetAbsolutePath();
		contributor->SetSourceData(sourceDocumentPath);
	}

	// Leave this as centimeters, it will be modified later.
	asset->SetUnitConversionFactor(0.01f);

	// Up axis
	if (MGlobal::isYAxisUp()) asset->SetUpAxis(FMVector3::YAxis);
	else if (MGlobal::isZAxisUp()) asset->SetUpAxis(FMVector3::ZAxis);
}

// Replace characters that are supported in Maya,
// but not supported in collada names
MString DaeDoc::MayaNameToColladaName(const MString& str, bool removeNamespace)
{
    // NathanM: Strip off namespace prefixes
    // TODO: Should really be exposed as an option in the Exporter
	MString mayaName;
	if (removeNamespace)
	{
		int prefixIndex = str.rindex(':');
		mayaName = (prefixIndex < 0) ? str : str.substring(prefixIndex + 1, str.length());
	}
	else mayaName = str;

	uint length = mayaName.length();
	FUStringBuilder builder;
	builder.reserve(length);
	const fchar* c = MConvert::ToFChar(mayaName);

	// Replace offending characters by some
	// that are supported within xml:
	// ':', '|' are replaced by '_'.
	// [claforte] Ideally, these should be encoded
	// as '%3A' for ':', etc. and decoded at import time.
	//
	for (uint i = 0; i < length; i++)
	{
		fchar d = c[i];
		if (d == '|' || d == ':' || d == '/' || d == '\\' || d == '(' || d == ')' || d == '[' || d == ']') 
			d = '_';
		builder.append(d);
	}

	return builder.ToCharPtr();
}

//
// Make an unique COLLADA Id from a dagPath
// We are free to use anything we want for Ids. For now use
// a honking unique name for readability - but in future we
// could just use an incrementing integer
//
MString DaeDoc::DagPathToColladaId(const MDagPath& dagPath)
{
	return MayaNameToColladaName(dagPath.partialPathName(), false);
}


//
// Get a COLLADA suitable node name from a DAG path
// For import/export symmetry, this should be exactly the
// Maya node name. If we include any more of the path, we'll
// get viral node names after repeated import/export.
//
MString DaeDoc::DagPathToColladaName(const MDagPath& dagPath)
{
	MFnDependencyNode node(dagPath.node());
	return MayaNameToColladaName(node.name(), true);
}


//
// Make a COLLADA name suitable for a DAG name
//
MString DaeDoc::ColladaNameToDagName(const MString& dagPath)
{
	int length = dagPath.length();
	char* tmp = new char[length + 1];
	const char* c = dagPath.asChar();
	for (int i = 0; i <= length; i++)
	{
		if ((tmp[i] = c[i]) == '.') 
		{
			tmp[i] =  '_';
		}
	}

	MString rv(tmp, length);
	SAFE_DELETE_ARRAY(tmp);
	return rv;
}
