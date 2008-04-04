/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include <maya/MFileIO.h>
#include <maya/MCommandResult.h>
#include <maya/MStringArray.h>
#include "CExportOptions.h"
#include "CReferenceManager.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "FUtils/FUFileManager.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDExternalReferenceManager.h"

// Singleton variable
//
CReferenceManager* CReferenceManager::singleton = NULL;

// Singleton constructor / destructor
//
CReferenceManager::CReferenceManager()
{
}

CReferenceManager::~CReferenceManager()
{
	CLEAR_POINTER_VECTOR(references);
	CLEAR_POINTER_VECTOR(files);
}

// Synchronize the reference manager with the Maya references
// 
void CReferenceManager::Synchronize()
{
	CLEAR_POINTER_VECTOR(references);
	CLEAR_POINTER_VECTOR(files);

	if (!CExportOptions::ExportXRefs() || CExportOptions::DereferenceXRefs()) return;

#if MAYA_API_VERSION >= 600

	MStatus status;
	MStringArray referenceFilenames;
	MFileIO::getReferences(referenceFilenames);

	uint referenceCount = referenceFilenames.length();
	references.reserve(referenceCount);
	for (uint i = 0; i < referenceCount; ++i)
	{
		MString& filename = referenceFilenames[i];
		MObject referenceNode = GetReferenceNode(filename);
		if (referenceNode != MObject::kNullObj) ProcessReference(referenceNode);
	}
#endif
}

void CReferenceManager::ProcessReference(const MObject& referenceNode)
{
	MStatus status;
	MFnDependencyNode referenceNodeFn(referenceNode, &status);
	if (status != MStatus::kSuccess) return;

#if MAYA_API_VERSION >= 600
	CReference* reference = new CReference();
	references.push_back(reference);
	reference->referenceNode = referenceNode;
	MString referenceNodeName = MFnDependencyNode(reference->referenceNode).name();

	// Get the paths of the root transforms included in this reference
	MObjectArray subReferences;
	GetRootObjects(referenceNode, reference->paths, subReferences);
	uint pathCount = reference->paths.length();

	// Process the sub-references first
	uint subReferenceCount = subReferences.length();
	for (uint i = 0; i < subReferenceCount; ++i)
	{
		MObject& subReference = subReferences[i];
		if (subReference != MObject::kNullObj) ProcessReference(subReference);
	}

	// Retrieve the reference node's filename
	MString command = MString("reference -rfn \"") + referenceNodeFn.name() + MString("\" -q -filename;");
	MString filename;
	status = MGlobal::executeCommand(command, filename);
	if (status != MStatus::kSuccess || filename.length() == 0) return;

	// Strip the filename of the multiple file token
	int stripIndex = filename.index('{');
	if (stripIndex != -1) filename = filename.substring(0, stripIndex - 1);

	// Avoid transform look-ups on COLLADA references.
	int extLocation = filename.rindex('.');
	if (extLocation > 0)
	{
		MString ext = filename.substring(extLocation + 1, filename.length() - 1).toLowerCase();
		if (ext == "dae" || ext == "xml") return;
	}

	// Check for already existing file information
	// Otherwise create a new file information sheet with current node names
	for (CReferenceFileList::iterator it = files.begin(); it != files.end(); ++it)
	{
		if ((*it)->filename == filename) { reference->file = (*it); break; }
	}

	if (reference->file == NULL) reference->file = ProcessReferenceFile(filename);

	// Get the list of the root transform's first child's unreferenced parents.
	// This is a list of the imported nodes!
	for (uint j = 0; j < pathCount; ++j)
	{
		MDagPath path = reference->paths[j];
		if (path.childCount() > 0)
		{
			path.push(path.child(0));
			MFnDagNode childNode(path);
			if (!childNode.object().hasFn(MFn::kTransform)) continue;

			uint parentCount = childNode.parentCount();
			for (uint k = 0; k < parentCount; ++k)
			{
				MFnDagNode parentNode(childNode.parent(k));
				if (parentNode.object() == MObject::kNullObj || parentNode.isFromReferencedFile()) continue;

				MDagPath parentPath = MDagPath::getAPathTo(parentNode.object());
				if (parentPath.length() > 0)
				{
					CReferenceRootList::iterator it = reference->reroots.insert(reference->reroots.end(), CReferenceRoot());
					(*it).index = j;
					(*it).reroot = parentPath;
				}
			}
		}
	}
#endif
}

CReferenceFile* CReferenceManager::ProcessReferenceFile(const MString& filename)
{
	CReferenceFile* file = new CReferenceFile();
	file->filename = filename;
	files.push_back(file);

#if MAYA_API_VERSION >= 800
	return file; // Versions 8.00 and 8.50 of Maya don't allow us to create references inside a plug-in.

#elif MAYA_API_VERSION >= 600

	// Get the original transformations for this file.
	// 1. Create a new reference
	MString tempFilename;
	MObject tempReferenceNode;

	{
		MString command = MString("file -r -type \"COLLADA importer\" -namespace \"_TEMP_EXP_NAMESPACE\" \"") + filename + "\";";
		MGlobal::executeCommand(command, tempFilename);

		tempFilename = GetLastReferenceFilename(tempFilename);

		MObject tempReferenceNode = GetReferenceNode(tempFilename);
		MString tempNodeName = MFnDependencyNode(tempReferenceNode).name();
		command = MString("file -loadReference \"") + tempNodeName + "\" \"" + tempFilename + "\";";
		MGlobal::executeCommand(command);
	}

	// 2. Get the original transformations for the root transforms of the temporary reference object
	MDagPathArray tempRoots;
	MObjectArray subReferences;
	GetRootObjects(tempReferenceNode, tempRoots, subReferences);
	uint tempRootCount = tempRoots.length();
	for (uint j = 0; j < tempRootCount; ++j)
	{
		MFnTransform tempT(tempRoots[j]);
		file->originalTransformations.push_back(tempT.transformation());
	}

	// 3. Get the original node names. This will be used as the URL for export
	file->rootNames.setLength(tempRootCount);
	for (uint j = 0; j < tempRootCount; ++j)
	{
		MString& originalName = file->rootNames[j];
		originalName = tempRoots[j].partialPathName();
		originalName = originalName.substring(originalName.index(':') + 1, originalName.length());
	}

	// 4. Cleanup: remove this reference
	MString command = MString("file -rr \"") + tempFilename + "\";";
	MGlobal::executeCommand(command);

#endif // MAYA >= 600

	return file;
}

// Find the reference information for a given dag path
CReference*	CReferenceManager::FindReference(const MDagPath& path, uint* referenceIndex)
{
	for (CReferenceList::iterator it = references.begin(); it != references.end(); ++it)
	{
		uint pathCount = (*it)->paths.length();
		for (uint j = 0; j < pathCount; ++j)
		{
			if (path.node() == (*it)->paths[j].node())
			{
				if (referenceIndex != NULL) *referenceIndex = j;
				return (*it);
			}
		}
	}

	*referenceIndex = UINT_MAX;
	return NULL;
}

CReference*	CReferenceManager::FindReference(CReferenceFile* file)
{
	for (CReferenceList::iterator it = references.begin(); it != references.end(); ++it)
	{
		if ((*it)->file == file) return (*it);
	}
	return NULL;
}

CReference* CReferenceManager::FindRerootedReference(const MDagPath& path, uint* referenceIndex)
{
	for (CReferenceList::iterator itR = references.begin(); itR != references.end(); ++itR)
	{
		for (CReferenceRootList::iterator it = (*itR)->reroots.begin(); it != (*itR)->reroots.end(); ++it)
		{
			if ((*it).reroot.node() == path.node())
			{
				if (referenceIndex != NULL) *referenceIndex = (*it).index;
				return (*itR);
			}
		}
	}
	
	if (referenceIndex != NULL) *referenceIndex = UINT_MAX;
	return NULL;
}

// remove the prefix of a reference name
MString	CReferenceManager::GetReferencePrefix(const MString& filename)
{
	MString prefix = filename;
	int index = prefix.rindex('/');
	if (index != -1) prefix = prefix.substring(index + 1, prefix.length());
	index = prefix.rindex('\\');
	if (index != -1) prefix = prefix.substring(index + 1, prefix.length());
	index = prefix.index('.');
	if (index != -1) prefix = prefix.substring(0, index - 1);
	return prefix;
}

void CReferenceManager::RemoveReferencePrefix(MString& name, CReference* reference)
{
	int index = name.index(':');
	if (index >= 0)
	{
		// Namespace prefix
		name = name.substring(index + 1, name.length());
	}
	else
	{
		// Filename prefix
		MString prefix = GetReferencePrefix(reference->file->filename);
		name = name.substring(prefix.length() + 1, name.length());
	}
}

// For import, create a new reference, given a URL 
CReference* CReferenceManager::CreateReference(const FUUri& url)
{
	CReference* reference = NULL;

#if MAYA_API_VERSION >= 600
	MString prefix = GetReferencePrefix(url.GetAbsoluteUri().c_str());
	CReferenceFile* file = NULL;
	for (CReferenceFileList::iterator it = files.begin(); it != files.end(); ++it)
	{
		if ((*it)->prefix == prefix) { file = (*it); break; }
	}

	// If this is a new file, create a structure for it.
	// Otherwise, look for an unused entry
	if (file == NULL)
	{
		file = new CReferenceFile();
		file->filename = url.GetAbsolutePath().c_str();
		file->prefix = prefix;
		files.push_back(file);

		reference = CreateReference(file);
	}
	else
	{
		reference = FindReference(file);
	}

#endif

	return reference;
}

// For import, create a new reference, given a URL 
CReference* CReferenceManager::CreateReference(const MDagPath& transform, const FUUri& url)
{
	CReference* reference = CreateReference(url);

#if MAYA_API_VERSION >= 600

	// Use the correct reference(s) for non-COLLADA documents
	fstring _ext = FUFileManager::GetFileExtension(url.GetAbsoluteUri());
	MString extension = _ext.c_str();
	extension.toLowerCase();
	if (reference != NULL && extension != "dae" && extension != "xml")
	{
		if (url.GetFragment().length() == 0)
		{
			uint pathCount = reference->paths.length();
			for (uint i = 0; i < pathCount; ++i)
			{
				UseReference(reference, i, transform);
			}
		}
		else
		{
			int index = FindRootName(reference->file, url.GetFragment().c_str());
			if (index == -1) return NULL;
			UseReference(reference, (uint) index, transform);
		}
	}

#endif

	return reference;
}

MString CReferenceManager::GetMayaFriendlyFilename(MString filename)
{
	MString fixedFilename("");

	// replace "\" with "/"
	int slashIndex = filename.index('\\');
	while (slashIndex != -1)
	{
		if (slashIndex > 0)
		{
			fixedFilename += filename.substring(0, slashIndex - 1);
		}
		fixedFilename += "/";
		
		if ((slashIndex + 1) == filename.length())
		{
			filename.clear();
			break;
		}
		
		filename = filename.substring(slashIndex + 1, filename.length() - 1);
		slashIndex = filename.index('\\');
	}
	fixedFilename += filename;

	return fixedFilename;
}

CReference*	CReferenceManager::CreateReference(CReferenceFile* file)
{
	if (file == NULL) return NULL;

#if MAYA_API_VERSION < 650
	if (!CImportOptions::IsOpenMode())
	{
		MGlobal::displayError("Use File->Open instead. Maya versions 6.0 and below don't accept references during import!");
		CImportOptions::SetErrorFlag();
		return NULL;
	}
#endif
	if (CImportOptions::FileLoadDeferReferencesOption())
	{
		MGlobal::displayError("Load All References option interferes with the actual loading of the references. set this option to false: File->Open (Options)->Load All References.");
		CImportOptions::SetErrorFlag();
		return NULL;
	}
    
	MStatus status;

	// Generate a unique namespace for this reference
	MString prefix = file->prefix + ++(file->usageCount);

	// Create a new reference for the given filename
	CReference* reference = new CReference();
	reference->file = file;
	references.push_back(reference);

#if MAYA_API_VERSION >= 800
	{
		MString filename = GetMayaFriendlyFilename(file->filename);
		status = MFileIO::reference(filename, true);
		if (status != MStatus::kSuccess) { MGlobal::displayError(MString("Unable to create reference for file: ") + filename + ".\nMStatus:" + status.error()); return NULL; }

		MStringArray referenceNodeNames;
		MFileIO::getReferenceNodes(filename, referenceNodeNames);
		if (referenceNodeNames.length() == 0)
		{
			MGlobal::displayWarning(MString("Maya refused the reference for file. Attempting alternative path. ") + file->filename + ".");
			MString finalReferenceFilename = MFileIO::loadReference(filename, &status);
			if (status != MStatus::kSuccess) { MGlobal::displayError(MString("Maya refused to load the reference for file: ") + file->filename + ".\nMStatus:" + status.error()); return NULL; }
			reference->referenceNode = GetReferenceNode(finalReferenceFilename);
		}
		else
		{
			MString referenceNodeName = referenceNodeNames[referenceNodeNames.length() - 1];
			MFileIO::loadReferenceByNode(referenceNodeName, &status);
			if (status != MStatus::kSuccess) { MGlobal::displayError(MString("Maya refused to load the reference for file: ") + file->filename + ".\nMStatus:" + status.error()); return NULL; }
			reference->referenceNode = CDagHelper::GetNode(referenceNodeName);
		}
	
		if (reference->referenceNode == MObject::kNullObj) return NULL;
	}
#else
	{
		MString referenceFilename;
		MString command = MString("file -r -dr true -namespace \"") + prefix + "\" \"" + file->filename + "\";";
		status = MGlobal::executeCommand(command, referenceFilename);
		referenceFilename = GetLastReferenceFilename(referenceFilename);

		if (status != MStatus::kSuccess) { MGlobal::displayError(MString("Unable to create reference for file: ") + file->filename + ".\nMStatus:" + status.error()); return NULL; }

		command = MString("file -loadReference \"") + prefix + "\" \"" + referenceFilename + "\";";
		status = MGlobal::executeCommand(command);

		reference->referenceNode = GetReferenceNode(referenceFilename);
		if (reference->referenceNode == MObject::kNullObj) return NULL;
	}
#endif // MAYA 6.0, 6.5 and 7.0

	// Get the root transforms for the new reference object
	MObjectArray subReferences;
	GetRootObjects(reference->referenceNode, reference->paths, subReferences);
	uint rootPathCount = reference->paths.length();
	if (rootPathCount == 0) return NULL;

	// Fill in the file information's root names, if empty.
	if (file->rootNames.length() == 0)
	{
		for (uint i = 0; i < rootPathCount; ++i)
		{
			MString rootName = reference->paths[i].partialPathName();
			RemoveReferencePrefix(rootName, reference);
			file->rootNames.append(rootName);
		}
	}

	// set all the root paths invisible, reparenting will occur for each instantiation
	for (uint i = 0; i < rootPathCount; ++i)
	{
		// Hide every root transform and add them to the unused list
		CDagHelper::SetPlugValue(reference->paths[i].transform(), "visibility", false);
	}
	return reference;
}

// Returns whether the given path is a root transform in the given list of paths
bool CReferenceManager::IsRootTransform(const MDagPathArray& allPaths, const MDagPath& testPath)
{
	MStatus status;
	MFnTransform transform(testPath, &status);
	if (status != MStatus::kSuccess) return false;
	uint pathCount = allPaths.length();

	uint parentCount = transform.parentCount();
	for (uint k = 0; k < parentCount; ++k)
	{
		MFnDependencyNode parentNode(transform.parent(k));
		if (!parentNode.isFromReferencedFile()) continue;

		for (uint m = 0; m < pathCount; ++m)
		{
			if (allPaths[m].node() == parentNode.object()) return false;
		}
	}

	return true;
}

void CReferenceManager::GetRootObjects(const MObject& referenceNode, MDagPathArray& rootPaths, MObjectArray& subReferences)
{
	rootPaths.clear();
	subReferences.clear();

	MFnDependencyNode referenceNodeFn(referenceNode);

	// Get the paths of all the dag nodes included in this reference
	MStringArray nodeNames;
	MString command = MString("reference -rfn \"") + referenceNodeFn.name() + "\" -q -node -dp;";
	MGlobal::executeCommand(command, nodeNames);

	uint nodeNameCount = nodeNames.length();
	MDagPathArray nodePaths;
	for (uint j = 0; j < nodeNameCount; ++j)
	{
		MObject o = CDagHelper::GetNode(nodeNames[j]);
		MDagPath p = CDagHelper::GetShortestDagPath(o);
		if (p.length() > 0)
		{
			nodePaths.append(p);
		}
		else
		{
			if (o != MObject::kNullObj && o.apiType() == MFn::kReference
				&& strstr(nodeNames[j].asChar(), "_UNKNOWN_REF_NODE") == NULL)
			{
				subReferences.append(o);
			}
		}
	}

	// Keep only the root transform for the reference in our path arrays
	uint nodePathCount = nodePaths.length();
	for (uint j = 0; j < nodePathCount; ++j)
	{
		const MDagPath& p = nodePaths[j];
		if (!IsRootTransform(nodePaths, p)) continue;
		rootPaths.append(p);
	}
}

MObject	CReferenceManager::GetReferenceNode(const MString& filename)
{
	MString referenceNodeName;
	MString command = MString("file -q -rfn \"") + filename + "\"";
	MGlobal::executeCommand(command, referenceNodeName);
	return CDagHelper::GetNode(referenceNodeName);
}

uint CReferenceManager::GetEndIndex(const MString& name)
{
	if (name.length() == 0) return 0;

	const char* c = name.asChar(), * p;
	for (p = c + name.length() - 1; p != c; --p)
	{
		if (*p < '0' || *p > '9') break;
	}
	return (uint) atoi(p);
}

void CReferenceManager::UseReference(CReference* reference, uint index, const MDagPath& transform)
{
	const MDagPath& root = reference->paths[index];
	MFnTransform t(root);
	uint tChildCount = t.childCount();
	for (uint i = 0; i < tChildCount; ++i)
	{
		MObject o = t.child(i);
		if (o.hasFn(MFn::kDagNode))
		{
			// Multi-parent the root transform of the reference to be a child of the given transform.
			bool isTransform = o.hasFn(MFn::kTransform);
			MDagPath path = root; path.push(o);
			MString command = MString("parent -add ") + ((!isTransform) ? "-shape " : "") + path.fullPathName() + " " + transform.fullPathName() + ";";
			MGlobal::executeCommand(command);
		}
	}

	ApplyTransformation(transform, t.transformation());
}

void CReferenceManager::ApplyTransformation(const MDagPath& transform, const MTransformationMatrix& rootTransformation)
{
	// Order doesn't matter, as long as we apply all the differences
	MFnTransform t(transform.transform());
	MTransformationMatrix curMx = MFnTransform(transform.transform()).transformation();
	MTransformationMatrix newMx(rootTransformation.asMatrix() * curMx.asMatrix());
	t.set(newMx);

/*	// Do the commands TWICE. 
	// Once right away so that we can export the data correctly.
	// Once deferred so that the reference node can register it.
	#define INEQ_SETATTR(newFloat, curFloat, attrName) \
	if (!IsEquivalent((float)newFloat, (float)curFloat)) { \
		MString command = MString("setAttr ") + transform.fullPathName() + attrName + " " + newFloat + ";"; \
		MGlobal::executeCommand(command); \
		command = MString("evalDeferred (\"") + command + "\");"; \
		MGlobal::executeCommand(command); \
	}

	MVector newT = newMx.translation(MSpace::kTransform);
	MVector curT = curMx.translation(MSpace::kTransform);
	INEQ_SETATTR(newT.x, curT.x, ".tx");
	INEQ_SETATTR(newT.y, curT.y, ".ty");
	INEQ_SETATTR(newT.z, curT.z, ".tz");

	MTransformationMatrix::RotationOrder dummy;
	double newR[3], curR[3];
	newMx.getRotation(newR, dummy);
	curMx.getRotation(curR, dummy);
	INEQ_SETATTR(RadToDeg((float)newR[0]), RadToDeg((float)curR[0]), ".rx");
	INEQ_SETATTR(RadToDeg((float)newR[1]), RadToDeg((float)curR[1]), ".ry");
	INEQ_SETATTR(RadToDeg((float)newR[2]), RadToDeg((float)curR[2]), ".rz");

	double newS[3], curS[3];
	newMx.getScale(newS, MSpace::kTransform);
	curMx.getScale(curS, MSpace::kTransform);
	INEQ_SETATTR(newS[0], curS[0], ".sx");
	INEQ_SETATTR(newS[1], curS[1], ".sy");
	INEQ_SETATTR(newS[2], curS[2], ".sz");

	double newSH[3], curSH[3];
	newMx.getShear(newSH, MSpace::kTransform);
	curMx.getShear(curSH, MSpace::kTransform);
	INEQ_SETATTR(newSH[0], curSH[0], ".shxy");
	INEQ_SETATTR(newSH[1], curSH[1], ".shxz");
	INEQ_SETATTR(newSH[2], curSH[2], ".shyz");

	#undef INEQ_SETATTR*/
}

int CReferenceManager::FindRootName(CReferenceFile* file, const MString& rootName)
{
	uint rootNameCount = file->rootNames.length();
	for (uint i = 0; i < rootNameCount; ++i)
	{
		if (rootName == file->rootNames[i]) return (int) i;
	}
	return -1;
}

MString	CReferenceManager::GetLastReferenceFilename(const MString& referenceResult)
{
	#if MAYA_API_VERSION >= 650
	return referenceResult;
	#else
	MStringArray filenames;
	MGlobal::executeCommand("file -q -r;", filenames);
	if (CImportOptions::IsOpenMode())
	{
		return (filenames.length() > 0) ? filenames[filenames.length() - 1] : "";
	}
	else
	{
		return referenceResult;
	}
	#endif
}

MString CReferenceManager::GetReferenceFilename(const MDagPath& path)
{
	MString command = MString("reference -q -f ") + path.fullPathName();
	MString filename;
	MGlobal::executeCommand(command, filename);
	return filename;
}

MString CReferenceManager::GetReferenceFilename(const MObject& dgNode)
{
	MString command = MString("reference -q -f ") + MFnDependencyNode(dgNode).name();
	MString filename;
	MGlobal::executeCommand(command, filename);
	return filename;
}

FCDPlaceHolder* CReferenceManager::GetPlaceHolder(FCDocument* colladaDocument, const MString& filename)
{
	const fchar* _filename = MConvert::ToFChar(filename);
	FCDPlaceHolder* ph = colladaDocument->GetExternalReferenceManager()->FindPlaceHolder(_filename);
	if (ph == NULL) ph = colladaDocument->GetExternalReferenceManager()->AddPlaceHolder(_filename);
	return ph;
}
