/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
// this is the only place where we explicitly include MFnPlugin.h
// if you wish to use the MFnPlugin class in another object, please use
// MFnPluginNoEntry.h
#include <maya/MFnPlugin.h>
#include <maya/MFileIO.h>
#include "DaeTranslator.h"
#include "CExportOptions.h"
#include "CReferenceManager.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "ColladaFX/CFXPasses.h"
#include "ColladaFX/CFXShaderNode.h"
#include "ColladaFX/CFXShaderCmd.h"
#include "ColladaFX/CFXBehavior.h"
#include "DaeDocNode.h"
#if MAYA_API_VERSION >= 700
#include <maya/MHWShaderSwatchGenerator.h>
#endif
#include <maya/MItDependencyNodes.h>

#define TRANSLATOR_VENDOR					"Feeling Software"
#define TRANSLATOR_MAYA_API_VERSION			"6.0.1"

//
// ColladaMaya Translator entry point
//

#ifdef _WINDOWS
_declspec(dllexport)
#endif
// Register the exporter when the plug-in is loaded
MStatus initializePlugin(MObject obj)
{
	FCollada::Initialize();

	uint32 minorVersion = FCOLLADA_VERSION & 0x0000FFFF;
	FUSStringBuilder version;
	version.append(FCOLLADA_VERSION >> 16); version.append('.');
	version.append(minorVersion / 10); version.append(minorVersion % 10);
	version.append(FCOLLADA_BUILDSTR);
    MFnPlugin plugin(obj, TRANSLATOR_VENDOR, version, TRANSLATOR_MAYA_API_VERSION);
    MStatus status;
    
	// Register the COLLADA document node
	// This node is needed by the translator: register it before the translation plug-in.
	status = plugin.registerNode("colladaDocument", DaeDocNode::id,
		DaeDocNode::creator, DaeDocNode::initialize, MPxNode::kDependNode);
	if (status != MStatus::kSuccess)
	{
		status.perror("registerNode");
		return status;
	}

	// Register the import and the export translation plug-ins.
	status = plugin.registerFileTranslator("COLLADA exporter", "", DaeTranslator::createExporter, 
										 "colladaTranslatorOpts", NULL, false);
    if (!status)
	{
		status.perror("registerFileTranslator");
		MGlobal::displayError(MString("Unable to register COLLADA exporter: ") + status);
		return status;
	}

	status = plugin.registerFileTranslator("COLLADA importer", "", DaeTranslator::createImporter, 
										 "colladaImporterOpts", NULL, false);
    if (!status)
	{
		status.perror("registerFileTranslator");
		MGlobal::displayError(MString("Unable to register COLLADA importer: ") + status);
	}
	
	MString UserClassify("shader/surface/utility");
	
#if MAYA_API_VERSION >= 700
	// Don't initialize swatches in batch mode
	if (MGlobal::mayaState() != MGlobal::kBatch)
	{
		const MString& swatchName =	MHWShaderSwatchGenerator::initialize();
		UserClassify = MString("shader/surface/utility/:swatch/"+swatchName);
	}
#endif // MAYA_API_VERSION >= 700
	
	status = plugin.registerNode("colladafxShader", CFXShaderNode::id, CFXShaderNode::creator,
								  CFXShaderNode::initialize, MPxNode::kHwShaderNode,
									&UserClassify);
	if (!status) {
		status.perror("registerNode");
		return status;
	}

	status = plugin.registerNode("colladafxPasses", CFXPasses::id, CFXPasses::creator,
								  CFXPasses::initialize, MPxNode::kHwShaderNode,
									&UserClassify);

	if (!status) {
		status.perror("registerNode");
		return status;
	}
	
	plugin.registerCommand("colladafxShaderCmd", CFXShaderCommand::creator, CFXShaderCommand::newSyntax);

	plugin.registerDragAndDropBehavior("CFXBehavior", 
									   CFXBehavior::creator);

	MGlobal::executeCommand("if (`pluginInfo -query -loaded cgfxShader.mll`){ print(\"cgfxShader.mll must be unloaded to use ColladaFX shader plug-in.\");unloadPlugin cgfxShader.mll; }");
	MGlobal::executeCommand("source AEcolladafxShaderTemplate.mel");
	MGlobal::executeCommand("source AEcolladafxPassesTemplate.mel");
	
	// needed in CFXParameter::loadDefaultTexture()
	MGlobal::executeCommand("source AEfileTemplate.mel");

	return status;
}


#ifdef _WINDOWS
_declspec(dllexport)
#endif
// Deregister the exporter when the plug-in is unloaded
MStatus uninitializePlugin(MObject obj)
{
    MFnPlugin plugin(obj);

	MStatus status;
	status = plugin.deregisterFileTranslator("COLLADA exporter");
	if (!status)
	{
		status.perror("deregisterFileTranslator");
		MGlobal::displayError(MString("Unable to deregister COLLADA exporter: ") + status);
		return status;
	}

	status = plugin.deregisterFileTranslator("COLLADA importer");
	if (!status)
	{
		status.perror("deregisterFileTranslator");
		MGlobal::displayError(MString("Unable to deregister COLLADA importer: ") + status);
		return status;
	}
	
	status = plugin.deregisterNode(CFXShaderNode::id);
	if (!status) {
		status.perror("deregisterNode");
		return status;
	}

	status = plugin.deregisterNode(CFXPasses::id);
	if (!status) {
		status.perror("deregisterNode");
		return status;
	}
	
	plugin.deregisterCommand("CFXShaderCommand");
	plugin.deregisterDragAndDropBehavior("CFXBehavior");
	plugin.deregisterNode(DaeDocNode::id);

#if MAYA_API_VERSION >= 800
	// Disable the shared-reference node options.
	MGlobal::executeCommand("optionVar -iv \"referenceOptionsSharedReference\" 0;");
	MGlobal::executeCommand("optionVar -iv \"referenceOptionsShareDisplayLayers\" 0;");
	MGlobal::executeCommand("optionVar -iv \"referenceOptionsShareShaders\" 0;");
#endif // MAYA 8.0 and 8.5

	FCollada::Release();
	return status;
}

uint DaeTranslator::readDepth = 0;

DaeTranslator::DaeTranslator(bool _isImporter)
{
	isImporter = _isImporter;
}

DaeTranslator::~DaeTranslator()
{
}

void DaeTranslator::retrieveColladaNode()
{
	// Find/Create the global ColladaNode
	MFnDependencyNode colladaNodeFn;
	MObject colladaNodeObject = CDagHelper::GetNode("colladaDocuments");
	if (!colladaNodeObject.isNull())
	{
		colladaNodeFn.setObject(colladaNodeObject);
		if (colladaNodeFn.userNode() == NULL)
		{
			// We didn't create this node! Reset it.
			MGlobal::executeCommand("delete colladaDocuments");
			colladaNodeObject = MObject::kNullObj;
			colladaNodeFn.setObject(MObject::kNullObj);
		}
	}

	if (colladaNodeObject.isNull())
	{
		// Create a new node.
		colladaNodeFn.create(DaeDocNode::id, "colladaDocuments");
	}

	// Retrieve our node structure.
	colladaNode = (DaeDocNode*) colladaNodeFn.userNode();
	FUAssert(colladaNode != NULL, return);
}

// Create a new instance of the translator
// These two methods are registered to Maya's plugin module in the above initializePlugin function.
void* DaeTranslator::createExporter()
{
	return new DaeTranslator(false);
}
void* DaeTranslator::createImporter()
{
	return new DaeTranslator(true);
}

// Entry point for the file import
MStatus DaeTranslator::reader(const MFileObject &fileObject, const MString &optionsString, MPxFileTranslator::FileAccessMode mode)
{
#if MAYA_API_VERSION >= 800
	if (mode == MPxFileTranslator::kReferenceAccessMode)
	{
		int optionValue;
		MGlobal::executeCommand("optionVar -q \"referenceOptionsSharedReference\";", optionValue);
		if (optionValue != 0)
		{
#ifdef WIN32
			MessageBox(NULL, FC("Maya may now hang. Do disable the reference option named: \"Shared Reference Nodes\"."), FC("POSSIBLE HANG"), MB_OK);
#endif
		}
	}
#endif // Maya 8.0 and 8.5

	++readDepth;

	MStatus status = MStatus::kFailure;
	retrieveColladaNode();

	bool ownsReferenceManager = false;
	if (CReferenceManager::singleton == NULL)
	{
		CReferenceManager::singleton = new CReferenceManager();
		ownsReferenceManager = true;
	}

#ifndef _DEBUG
	try {
#endif
		// Extract the filename
		//
		MString filename = fileObject.fullName(); 

		// Look for a document re-opening
		DaeDoc* oldDocument = colladaNode->FindDocument(filename);
		if (oldDocument != NULL)
		{
			colladaNode->ReleaseDocument(oldDocument);
		}

		// Process the import options
		//
		CImportOptions::Set(optionsString, mode);

		// Now import the file
		//
		status = Import(filename);

#ifndef _DEBUG
	}
	catch (...)
	{
		MGlobal::displayError("ColladaMaya has thrown an exception!");
	}
#endif

	if (ownsReferenceManager)
	{
		SAFE_DELETE(CReferenceManager::singleton);
	}

	CAnimationHelper::samplingTimes.clear();

	--readDepth;

	return status;
}


// Entry point for the file export
MStatus DaeTranslator::writer(const MFileObject &fileObject, const MString &optionsString, MPxFileTranslator::FileAccessMode mode)
{
	MStatus status = MStatus::kFailure;
	retrieveColladaNode();

	bool ownsReferenceManager = false;
	if (CReferenceManager::singleton == NULL)
	{
		CReferenceManager::singleton = new CReferenceManager();
		ownsReferenceManager = true;
	}

#ifndef _DEBUG
	try {
#endif

		// Extract the filename
		MString filename = fileObject.fullName();

		// Maya forces the write of all the references, on export.
		// Intentionally skip known reference file paths.
		for (MItDependencyNodes it(MFn::kReference); !it.isDone(); it.next())
		{
			MObject refNode = it.item(); MString refNodeName = MFnDependencyNode(refNode).name();
			MString refNodeFilename;
			MGlobal::executeCommand(MString("reference -rfn \"") + refNodeName + MString("\" -q -filename"), refNodeFilename);
			if (refNodeFilename == filename) return MStatus::kSuccess;
		}

		// Parse the export options
		CExportOptions::Set(optionsString);
		exportSelection = mode == MPxFileTranslator::kExportActiveAccessMode;

		// Do the actual export now
		status = Export(filename);

#ifndef _DEBUG
	} catch (...) {
		MGlobal::displayError("ColladaMaya has thrown an exception!");
	}
#endif

	if (ownsReferenceManager)
	{
		SAFE_DELETE(CReferenceManager::singleton);
	}

	CAnimationHelper::samplingTimes.clear();
	return status;
}

// Return the default extension for COLLADA files
MString DaeTranslator::defaultExtension() const
{
    return "dae";
}

MString DaeTranslator::filter() const
{
	return "*.dae;*.xml";
}

// Check the given file to see if it is COLLADA data
MPxFileTranslator::MFileKind DaeTranslator::identifyFile(const MFileObject &fileObject, const char *buffer, short size) const
{
	// Just check for the proper extension for now
	MFileKind rval = kNotMyFileType;
	int extLocation = fileObject.name().rindex('.');
		
	if (extLocation > 0)
	{
		MString ext = fileObject.name().substring(extLocation + 1, fileObject.name().length()-1).toLowerCase();
		if (ext == "dae" || ext == "xml") 
		{
			rval = kIsMyFileType;
		}
	}

	return rval;
}

// Parses the COLLADA document and generates the Maya structures
MStatus DaeTranslator::Import(const MString& filename)
{
	MStatus status = MS::kSuccess;

	// Importing our scene
	DaeDoc* document = colladaNode->NewDocument(filename);
	document->Import(CImportOptions::IsReferenceMode() && readDepth > 1);

	if (CImportOptions::HasError()) status = MStatus::kFailure;
	return status;
}


// Parses the Maya DAG/DG graphs and writes out a COLLADA document
MStatus DaeTranslator::Export(const MString& filename)
{
	MStatus status = MS::kSuccess;

	// Actually export the document
	DaeDoc* document = colladaNode->NewDocument(filename);
	document->Export(exportSelection);
	colladaNode->ReleaseDocument(document); // don't keep export documents

	// Display some closing information.
	FUStringBuilder tmpstr;
	tmpstr.append(FC("ColladaMaya export finished:  \""));
	tmpstr.append(MConvert::ToFChar(filename.asChar()));
	tmpstr.append((fchar) '\"');
	MGlobal::displayInfo(tmpstr.ToCharPtr());

	return status;
}
