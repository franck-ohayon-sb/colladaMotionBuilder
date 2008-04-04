/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COLLADAFX_EFFECT_SHADER_H_
#define _COLLADAFX_EFFECT_SHADER_H_

class ColladaEffectPass;
class ColladaEffectParameter;

struct ColladaEffectShaderInfo
{
	fstring filename;

	// Cg specific
	fstring entry;
	CGprogram cgProgram;
	bool isVertex;

	// CgFX specific
	bool isEffect;
	CGeffect cgEffect;
	uint32 techniqueIndex;
	fstring techniqueName; // transient
	uint32 passIndex;
	fstring passName; // transient
	CGprogram cgVertexProgram;
	CGprogram cgFragmentProgram;

	ColladaEffectShaderInfo();
	~ColladaEffectShaderInfo(){}
};

#define COLLADA_EFFECT_SHADER_ID Class_ID(0x2bfa0654, 0x74356b5)

class ColladaEffectShader : public ReferenceTarget
{
	ColladaEffectPass* mParent;
	ColladaEffectShaderInfo mInfo;

	fstring mErrorString;
	bool mIsValid;

private:
	bool compileCg();
	bool compileCgFX();

	bool loadCgParameters(CGprogram prog);
	bool loadCgFXParameters(CGeffect effect);
	void connectParameters(CGprogram prog, bool isFrag);

	// general purpose
	bool compile();
	bool loadParameters();

	// transient parameter list (recreated at load time)
	ColladaEffectParameterList mParameters;

public:
	ColladaEffectShader(BOOL isLoading, ColladaEffectPass* parentP = NULL);
	~ColladaEffectShader();
	virtual void DeleteThis() { delete this; }

	void prepare();
	bool isValid() const { return mIsValid; }

	// ReferenceTarget implementation
	Class_ID ClassID() { return COLLADA_EFFECT_SHADER_ID; }
	SClass_ID SuperClassID() { return REF_TARGET_CLASS_ID; }
	int NumRefs() { return 0; }
	RefTargetHandle GetReference(int) { return NULL; }
	void SetReference(int, RefTargetHandle){}
	RefResult NotifyRefChanged(Interval, RefTargetHandle, PartID&, RefMessage){return REF_SUCCEED;}

	ColladaEffectPass* getParent(){ return mParent; }
	void setParent(ColladaEffectPass* parent){ mParent = parent; }

	ColladaEffectShaderInfo& getInfo() { return mInfo; }
	const ColladaEffectShaderInfo& getInfo() const { return mInfo; }

	ColladaEffectParameterList& getParameters() { return mParameters; }

	const fstring& getErrorString() const { return mErrorString; }

	void setParamBlock(IParamBlock2* pb);

	/** Compiles the shader and registers the parameters on the parent pass.*/
	bool initialize();

	// overrides from ReferenceMaker
	IOResult Save(ISave *isave);
	IOResult Load(ILoad *iload);

public:
	// Creates a temporary Cg context to parse the given effect file and retrieve
	// its informations.
	static void GetEffectInfo(const fstring& fileName, StringList& techNames, UInt32List& passCounts, StringList& passNames, fstring& errorString);
};

extern ClassDesc2* GetColladaEffectShaderClassDesc();

#endif // _COLLADAFX_EFFECT_SHADER_H_
