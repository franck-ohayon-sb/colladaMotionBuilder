/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __REFERENCE_MANAGER__
#define __REFERENCE_MANAGER__

#include <maya/MTransformationMatrix.h>
#include "FUtils/FUUri.h"

class CReference;
class FCDocument;
class FCDPlaceHolder;

class CReferenceRoot
{
public:
	MDagPath reroot;
	uint index;
};
typedef fm::vector<CReferenceRoot> CReferenceRootList;

class CReferenceFile
{
public:
	MString filename;
	MString prefix;
	uint usageCount;

	MStringArray rootNames;
	fm::vector<MTransformationMatrix> originalTransformations;

	CReferenceFile() { usageCount = 0; }
};
typedef fm::pvector<CReferenceFile> CReferenceFileList;

class CReference
{
public:
	CReferenceFile* file;
	MObject referenceNode;

	// NOTE: Only root transform modifications are currently supported
	// So, "paths" only contains root transform paths
	//
	MDagPathArray paths;
	CReferenceRootList reroots;

public:
	CReference() { file = NULL; }
};
typedef fm::pvector<CReference> CReferenceList;

class CReferenceManager
{
public:
	CReferenceManager();
	~CReferenceManager();

	// Grab all the referencing information from the current scene. Used on export.
	void Synchronize();
	void ProcessReference(const MObject& reference);
	CReferenceFile* ProcessReferenceFile(const MString& filename);

	// Find the reference information for a given dag path
	CReference* FindReference(const MDagPath& path, uint* referenceIndex=NULL);
	CReference* FindReference(CReferenceFile* file);
	CReference* FindRerootedReference(const MDagPath& path, uint* referenceIndex=NULL);

	// Create a new reference
	CReference* CreateReference(const MDagPath& transform, const FUUri& url);
	CReference* CreateReference(const FUUri& url);
	CReference* CreateReference(CReferenceFile* file);

	// Retrieves the filename of a referenced node
	static MString GetReferenceFilename(const MDagPath& path);
	static MString GetReferenceFilename(const MObject& dgNode);

	// Retrieves the placeholder of a referenced node
	static FCDPlaceHolder* GetPlaceHolder(FCDocument* colladaDocument, const MString& filename);
	static FCDPlaceHolder* GetPlaceHolder(FCDocument* colladaDocument, const MDagPath& dagPath) { return GetPlaceHolder(colladaDocument, GetReferenceFilename(dagPath)); }
	static FCDPlaceHolder* GetPlaceHolder(FCDocument* colladaDocument, const MObject& dgNode) { return GetPlaceHolder(colladaDocument, GetReferenceFilename(dgNode)); }

	// Returns the reference node of a given filename
	static MObject GetReferenceNode(const MString& filename);

private:
	// Returns a list of the root transforms attached to a reference, given by its filename
	void GetRootObjects(const MObject& referenceNode, MDagPathArray& rootPaths, MObjectArray& subReferences);
	bool IsRootTransform(const MDagPathArray& allPaths, const MDagPath& testPath);

	// Returns the terminating index of a node. This is the increasing index used by Maya for renaming.
	uint GetEndIndex(const MString& name);

	// Handles reference namespace prefixing
	MString GetReferencePrefix(const MString& filename);
	void RemoveReferencePrefix(MString& name, CReference* reference);

	// set a reference root as used and apply the transformation on it.
	void UseReference(CReference* reference, uint index, const MDagPath& transform);
	void ApplyTransformation(const MDagPath& transform, const MTransformationMatrix& rootTransformation);

	// Find various information in the reference file lists
	int FindRootName(CReferenceFile* file, const MString& rootName);

	// Retrieve the name of the last reference created
	MString GetLastReferenceFilename(const MString& referenceResult);

	MString GetMayaFriendlyFilename(MString filename);

private:
	CReferenceList references;
	CReferenceFileList files;

public:
	static CReferenceManager* singleton;
};

inline CReferenceManager* GetReferenceManager() { return CReferenceManager::singleton; }

#endif // __REFERENCE_MANAGER__
