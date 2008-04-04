/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COLLADAFX_COLLADAEFFECTTECHNIQUE_H_
#define _COLLADAFX_COLLADAEFFECTTECHNIQUE_H_

class ColladaEffect;
class ColladaEffectPass;

#define COLLADA_EFFECT_TECHNIQUE_ID Class_ID(0x366a133f, 0x7cf5b23)

class ColladaEffectTechnique : public ReferenceTarget
{
public:
	enum BlockIDs
	{
		parameters
	};

	enum ParamIDs
	{
		param_parent,
		param_name,
		param_passes,
		param_edited_pass,
	};

private:
	IParamBlock2* mParamBlock;

private:
	void insertPass(ColladaEffectPass* pass);

public:
	ColladaEffectTechnique(BOOL isLoading = FALSE);
	ColladaEffectTechnique(ColladaEffect* parentP, const fstring& name);
	~ColladaEffectTechnique();
	virtual void DeleteThis() { delete this; }

	ColladaEffect* getParent();
	void setParent(ColladaEffect* p);

	// ReferenceTarget implementation
	Class_ID ClassID() { return COLLADA_EFFECT_TECHNIQUE_ID; }
	SClass_ID SuperClassID() { return REF_TARGET_CLASS_ID; }
	int NumRefs() { return 1; }
	RefTargetHandle GetReference(int i) { FUAssert(i == 0,return NULL); return mParamBlock; }
	void SetReference(int i, RefTargetHandle rtarg) { FUAssert(i == 0,return); mParamBlock = (IParamBlock2*)rtarg; }
	RefResult NotifyRefChanged(Interval valid, RefTargetHandle r, PartID& partID, RefMessage msg);

	fstring getName();
	bool setName(const fstring& name);

	bool isNameUnique(const fstring& name);
	size_t getPassCount();
	ColladaEffectPass* getPass(size_t idx);
	ColladaEffectPass* addPass(const fstring& name);
	ColladaEffectPass* removePass(size_t index);

	ColladaEffectPass* getEditedPass();
	ColladaEffectPass* setEditedPass(size_t index);

	bool isValid();
};

extern ClassDesc2* GetColladaEffectTechniqueClassDesc();

#endif // _COLLADAFX_COLLADAEFFECTTECHNIQUE_H_
