/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _colladaPasses_h
#define _colladaPasses_h

#ifndef _MPxHwShaderNode
#include <maya/MPxHwShaderNode.h>
#endif // _MPxHwShaderNode

class CFXShaderNode;
typedef fm::pvector<CFXShaderNode> CFXShaderNodeList;

class CFXPasses : public MPxHwShaderNode
{
public:
						CFXPasses();
	virtual				~CFXPasses(); 

	virtual MStatus		compute(const MPlug& plug, MDataBlock& data);

	static  void*		creator();
	static  MStatus		initialize();

public:

	// To deal with multiple techniques, each technique owns n passes
	static MObject aTechniqueNames; // Available techniques, MStringArray.
	// each technique has its own storable attribute generated
	static MObject aChosenTechniqueName;

	// The typeid is a unique 32bit indentifier that describes this node.
	// It is used to save and retrieve nodes of this type from the binary
	// file format.  If it is not unique, it will cause file IO problems.
	//
	static MTypeId id;
	
public:
	virtual MStatus	glBind(const MDagPath& shapePath);
	virtual MStatus	glUnbind(const MDagPath& shapePath);
	virtual MStatus glGeometry(const MDagPath& shapePath, int prim, unsigned int writable, int indexCount, const unsigned int * indexArray, int vertexCount, const int * vertexIDs, const float * vertexArray, int normalCount, const float ** normalArrays, int colorCount, const float ** colorArrays, int texCoordCount, const float ** texCoordArrays);
	
#if MAYA_API_VERSION == 700 || MAYA_API_VERSION > 800
	virtual MStatus renderSwatchImage(MImage & image);
#endif

	virtual int  normalsPerVertex ();
	virtual int  colorsPerVertex ();
	virtual int  texCoordsPerVertex ();

	// Accessors
	uint getTechniqueCount();
	void getTechniqueNames(MStringArray& output);
	uint getPassCount(const MString& techName);
	uint getPassCount(uint techIndex);
	uint getChosenTechniquePassCount();

	MObject getPass(const MString& techName, uint index);
	CFXShaderNode* getPass(uint techIndex, uint passIndex);
	MString getPassName(const MString& techName, uint index);
	MObject getChosenTechniquePass(uint index);
	void getTechniquePasses(const MString& techName, CFXShaderNodeList& shaders);
	void getChosenTechniquePasses(CFXShaderNodeList& shaders);

	// Add/remove techniques
	MStatus addTechnique(const MString& techName);
	MStatus removeCurrentTechnique();

	// Add/remove passes (to chosen technique)
	MStatus addPass(const MString& passName);
	MStatus removeLastPass();

	// Populate techniques and passes from CgFX file
	MStatus reloadFromCgFX(const char* fxFileName);

private:
	MStatus getCurrentTechnique(MPlug& result);
	MStatus getTechnique(const MString& techName, MPlug& result);
};

#endif
