/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _DAE_TRANSLATOR_H_
#define _DAE_TRANSLATOR_H_

#ifndef _MPxFileTranslator
#include <maya/MPxFileTranslator.h>
#endif // _MPxFileTranslator

class DaeDocNode;

class DaeTranslator : public MPxFileTranslator
{
public:
	DaeTranslator(bool isImporter);
	~DaeTranslator();

	static void* createImporter();
	static void* createExporter();

	void retrieveColladaNode();
	virtual MStatus reader(const MFileObject &, const MString &optionsString, MPxFileTranslator::FileAccessMode mode);
	virtual MStatus writer(const MFileObject &, const MString &optionsString, MPxFileTranslator::FileAccessMode mode);
	
	virtual bool haveReadMethod() const	{ return isImporter; }
	virtual bool haveReferenceMethod() const { return isImporter; }
	virtual bool haveWriteMethod() const { return !isImporter; }
	virtual bool canBeOpen() const { return isImporter; }
	
	MString defaultExtension() const;
	MString filter() const;
	virtual MFileKind identifyFile(const MFileObject &, const char *, short) const;

private:
	MStatus Export(const MString& filename);
	MStatus Import(const MString& filename);

	MString sceneName;
	bool exportSelection;
	bool isImporter;
	static uint readDepth;

	DaeDocNode* colladaNode;
};

#endif /* _DAE_TRANSLATOR_H_ */
