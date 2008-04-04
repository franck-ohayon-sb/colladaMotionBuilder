/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/*
	Dependency Graph Node: CFXShaderNode
*/

#ifndef _colladafxShaderNodeNode
#define _colladafxShaderNodeNode

class CFXParameter;
class CFXRenderState;
struct _CGprogram;

#ifndef _MPxHwShaderNode
#include <maya/MPxHwShaderNode.h>
#endif // _MPxHwShaderNode

typedef fm::pvector<CFXParameter> CFXParameterList;
typedef fm::pvector<CFXRenderState> CFXRenderStateList;

class CFXShaderNode : public MPxHwShaderNode
{
public:
	CFXShaderNode();
	virtual	~CFXShaderNode(); 

	virtual MStatus		compute(const MPlug& plug, MDataBlock& data);

	static const char* DEFAULT_FRAGMENT_ENTRY;
	static const char* DEFAULT_VERTEX_ENTRY;
	static const char* DEFAULT_FRAGMENT_EFFECT_ENTRY;
	static const char* DEFAULT_VERTEX_EFFECT_ENTRY;
	static const char* DEFAULT_TECHNIQUE_NAME;
	static const char* DEFAULT_PASS_NAME;

	static void			CgErrorCallback();
	static void*		creator();
	static MStatus		initialize();
		
	static	MTypeId	id;

	static  MObject	aVertexShader;
	static	MObject aVertexShaderEntry;
	static  MObject	aFragmentShader;
	static  MObject	aFragmentShaderEntry;

	static  MObject	aColorSet0;
	static  MObject	aColorSet1;
	static  MObject	aTex0;
	static  MObject	aTex1;
	static  MObject	aTex2;
	static  MObject	aTex3;
	static  MObject	aTex4;
	static  MObject	aTex5;
	static  MObject	aTex6;
	static  MObject	aTex7;
	static	MObject aVertParam;
	static	MObject aFragParam;
	static  MObject	aTechniques;
	static	MObject aEffectPassDec;

	static	MObject	aTechTarget;
	static	MObject	aPassTarget;

	// the editable render states on the current CgFX shader, sorted alphabetically
	static	MObject aRenderStates;
	static	MObject aDynRenderStates;

	// a simple, boolean dirty attribute
	static	MObject	aDirtyAttrib;

	// facilities to create effects and programs from file names
	static CGeffect CreateEffectFromFile(CGcontext ctx, const char* filename);
	static CGprogram CreateProgramFromFile(CGcontext ctx, const char* filename, CGprofile profile, const char* entry);

public:
	// MPxHardwareShader node interface
	virtual bool setInternalValueInContext(const MPlug&, const MDataHandle&, MDGContext&);
	virtual bool getInternalValueInContext(const MPlug&, MDataHandle&, MDGContext&);

	virtual MStatus	glBind(const MDagPath& shapePath);
	virtual MStatus	glUnbind(const MDagPath& shapePath);
	virtual MStatus glGeometry(const MDagPath& shapePath, int prim, unsigned int writable, int indexCount, const unsigned int * indexArray, int vertexCount, const int * vertexIDs, const float * vertexArray, int normalCount, const float ** normalArrays, int colorCount, const float ** colorArrays, int texCoordCount, const float ** texCoordArrays);
#if MAYA_API_VERSION == 700 || MAYA_API_VERSION > 800
	virtual MStatus renderSwatchImage(MImage& image);
#endif	
	virtual int  texCoordsPerVertex ();
	virtual int  normalsPerVertex () { return 3; }
	virtual int  colorsPerVertex (); 

	virtual MStatus setDependentsDirty(const MPlug &, MPlugArray&);

#if MAYA_API_VERSION >= 800
	virtual bool supportsBatching() const { return true; }
#endif

	// Accessors
	bool isCgFX();
	inline bool IsLoaded() { return isLoaded; }
	const char* getVertexEntry();
	const char* getVertexProfileName(){ return cgGetProfileString(vertexProfile); }
	const char* getFragmentEntry();
	const char* getFragmentProfileName(){ return cgGetProfileString(fragmentProfile); }
	inline const fchar* getVertexProgramFilename() { return MConvert::ToFChar(vpfile); }
	inline const fchar* getFragmentProgramFilename() { return MConvert::ToFChar(fpfile); }
	inline bool isBlending() { return blendFlag; }

	const fchar* getTechniqueName(){ return MConvert::ToFChar(mTechniqueName); }
	const fchar* getPassName(){ return MConvert::ToFChar(mPassName); }

	CFXParameterList& GetParameters() { return effectParameters; }
	const CFXParameterList& GetParameters() const { return effectParameters; }
	CFXRenderStateList& GetRenderStates() { return renderStates; }
	CFXParameter* FindEffectParameter(const char* name);
	CFXParameter* FindEffectParameterBySemantic(const char* semantic);
	CFXRenderState* FindRenderState(uint32 type);

	// Mutators
	void setVertexProgramFilename(const char* filename, const char* entry);
	void setFragmentProgramFilename(const char* filename, const char* entry);
	void setEffectTargets(uint techniqueTarget,uint passTarget);
	void setEffectFilename(const char* filename, bool reloadRenderStates = true);
	void setBlendFlag(bool blend) { blendFlag = blend; }
	void forceLoad();

	// Texture/Color channels
	// 9 channels: 0-7 are TEXCOORDi and 8 is COLOR0.
	static const int channelCount = 9;
	inline const fchar* getChannelName(int id) { return (id >= 0 && id < channelCount) ? MConvert::ToFChar(channelNames[id]) : emptyFString.c_str(); }
	inline void setChannelName(int id, const fchar* name) { if (id >= 0 && id < channelCount) channelNames[id] = name; }

	// UI parameters
	// Not all the parameters are considered available for the UI
	// These functions allow you to iterate through them.
	CFXParameterList collectUIParameters();
	uint getUIParameterCount();
	MStringArray getUIParameterInfo(uint index); // three strings: name, type, semantic.
	void editParamMinMax(const fchar* paramName, double min, double max);

	// render states
	// Get the available (not xrefed to INVALID) CgFX render states.
	MStringArray getAvailableRenderStates();
	MStringArray getAvailableIndexedRenderStates();
	// add the given render state to this pass, and get the ui information back
	bool addRenderState(FUDaePassState::State stateType, int index = -1);
	bool addRenderState(const char* cgStateName, int index = -1);
	bool removeRenderState(const char* cgStateName, int index = -1);
	MString getRenderStateUI(const char* colladaStateName, int layout);

private:
	// Memory Initialization
	void initCGShader();
	void loadParameters();
	void loadRenderStates();
	CGprogram compileProgram(const char* filename, const char* entry, CGprofile profile);
	void connectParameters(CGprogram prog, bool isFrag);

	/** Call this method to refresh the swatch image after some dynamic attributes have changed.*/
	void refresh();

	// Memory Release
	void clearVertexProgram();
	void clearFragmentProgram();
	void clearEffect();

	// batching methods
	void BindUVandColorSets(const MDagPath& shapePath);
	void BindGLStates();
	void BindCgStates();
	void BindRenderStates();
	void UnbindGLStates();
	void UnbindRenderStates();
	void UnbindCgStates();

private :	
	uint mTechTarget, mPassTarget;
	MString vpfile, vpEntry, fpfile, fpEntry;
	MString mTechniqueName, mPassName;

	// bug 351: RenderStates added by binary attributes in setInternalValueInContext()
	// were deleted in loadRenderStates().
	bool mForcedOnce;

	int m_uvSetId;
	
	MString channelNames[channelCount];
	int channelIndexes[channelCount];
	int channelTypes[channelCount];

	bool isLoaded;
	GLuint textureid;
	int glutDummyId;

	CGprogram vertexProgram, fragmentProgram;

	CGcontext context;
	CGprofile vertexProfile, fragmentProfile;
	CFXParameterList effectParameters;
	CFXRenderStateList renderStates;
	CGeffect effect;
	bool blendFlag;
	// batching related
	bool mCgBound;
};

#endif
