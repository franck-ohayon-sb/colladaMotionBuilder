/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "ColladaEffect.h"
#include "ColladaEffectTechnique.h"
#include "ColladaEffectPass.h"

class ColladaEffectTechniqueClassDesc : public ClassDesc2
{
public:
	int				IsPublic() {return FALSE; }
	void*			Create(BOOL isLoading)
	{
		// is this ever called if I don't expose the class as a plugin?
		return new ColladaEffectTechnique(isLoading);
		//return NULL;
	}

	const TCHAR*	ClassName() {return _T("ColladaEffectTechnique");}
	SClass_ID		SuperClassID() {return REF_TARGET_CLASS_ID;}
	Class_ID 		ClassID() {return COLLADA_EFFECT_TECHNIQUE_ID;}
	const TCHAR* 	Category() {return _T("");}
	const TCHAR*	InternalName() { return _T("ColladaEffectTechnique"); }
	HINSTANCE		HInstance() { return hInstance; }
};
static ColladaEffectTechniqueClassDesc gColladaEffectTechniqueCD;

ClassDesc2* GetColladaEffectTechniqueClassDesc()
{
	return &gColladaEffectTechniqueCD;
}

///////////////////////////////////////////////////////////////////////////////
// PARAMETER BLOCKS
///////////////////////////////////////////////////////////////////////////////

class ColladaEffectTechniquePBAccessor : public PBAccessor
{
public:
	void Set(PB2Value &v, ReferenceMaker* owner, ParamID id, int tabIndex, TimeValue t)
	{
		PBAccessor::Set(v,owner,id,tabIndex,t);
	}

	void TabChanged(tab_changes changeCode, Tab<PB2Value>* tab, ReferenceMaker* owner, ParamID id, int tabIndex, int count)
	{
		PBAccessor::TabChanged(changeCode,tab,owner,id,tabIndex,count);
	}
};

static ColladaEffectTechniquePBAccessor gColladaEffectTechniquePBAccessor;

static ParamBlockDesc2 gColladaEffectTechniquePBDesc(
	ColladaEffectTechnique::parameters, _T("parameters"),  0, &gColladaEffectTechniqueCD, P_AUTO_CONSTRUCT, 0,

	// params
	// ParamID, internal_name, ParamType, [tab size], flags, local_name_res_ID, <Param Tags>

	ColladaEffectTechnique::param_parent, _T("parent"), TYPE_REFTARG, P_NO_REF, 0,
		p_accessor, &gColladaEffectTechniquePBAccessor,
		end,

	ColladaEffectTechnique::param_name, _T("name"), TYPE_STRING, 0, 0,
		p_accessor, &gColladaEffectTechniquePBAccessor,
		end,

	ColladaEffectTechnique::param_passes, _T("passes"), TYPE_REFTARG_TAB, 0, P_VARIABLE_SIZE, 0,
		p_accessor, &gColladaEffectTechniquePBAccessor,
		end,

	ColladaEffectTechnique::param_edited_pass, _T("edited_pass"), TYPE_REFTARG, 0, 0,
		p_accessor, &gColladaEffectTechniquePBAccessor,
		end,
	
	end
);

///////////////////////////////////////////////////////////////////////////////
// COLLADAEFFECTTECHNIQUE CLASS
///////////////////////////////////////////////////////////////////////////////

ColladaEffectTechnique::ColladaEffectTechnique(BOOL UNUSED(isLoading))
:	mParamBlock(NULL)
{
	gColladaEffectTechniqueCD.MakeAutoParamBlocks(this);
}

ColladaEffectTechnique::ColladaEffectTechnique(ColladaEffect* parentP, const fstring& nameP)
:	mParamBlock(NULL)
{
	gColladaEffectTechniqueCD.MakeAutoParamBlocks(this);
	setParent(parentP);
	setName(nameP);
}

ColladaEffectTechnique::~ColladaEffectTechnique()
{
	//CLEAR_POINTER_VECTOR(mPasses);
}

RefResult ColladaEffectTechnique::NotifyRefChanged(
	Interval UNUSED(valid), RefTargetHandle UNUSED(r), PartID& UNUSED(partID), RefMessage UNUSED(msg))
{
	return REF_SUCCEED;
}

ColladaEffect* ColladaEffectTechnique::getParent()
{
	ReferenceTarget* ref = NULL;
	Interval valid;
	mParamBlock->GetValue(ColladaEffectTechnique::param_parent, 0, ref, valid);
	return (ColladaEffect*)ref;
}

void ColladaEffectTechnique::setParent(ColladaEffect* p)
{
	mParamBlock->SetValue(ColladaEffectTechnique::param_parent, 0, (ReferenceTarget*)p);
}

fstring ColladaEffectTechnique::getName()
{
	TCHAR* val = NULL;
	Interval valid;
	mParamBlock->GetValue(ColladaEffectTechnique::param_name, 0, val, valid);
	return FS(val);
}

bool ColladaEffectTechnique::setName(const fstring& name)
{
	ColladaEffect* parent = getParent();
	if (parent == NULL) return false;
	if (!parent->isNameUnique(name)) return false;
	TSTR s = name.c_str();
	mParamBlock->SetValue(ColladaEffectTechnique::param_name, 0, (TCHAR*)s.data());
	parent->sortTechniques();
	return true;
}

size_t ColladaEffectTechnique::getPassCount()
{
	int count = mParamBlock->Count(ColladaEffectTechnique::param_passes);
	return (size_t)count;
}

ColladaEffectPass* ColladaEffectTechnique::getPass(size_t idx)
{
	if (idx >= getPassCount()) return NULL;
	ReferenceTarget* ref = NULL;
	Interval valid;
	mParamBlock->GetValue(ColladaEffectTechnique::param_passes, 0, ref, valid, (int)idx);
	return (ColladaEffectPass*)ref;
}

bool ColladaEffectTechnique::isNameUnique(const fstring& name)
{
	size_t count = getPassCount();
	for (size_t i = 0; i < count; ++i)
	{
		ColladaEffectPass* p = getPass(i);
		if (fstrcmp(p->getName().c_str(), name.c_str()) == 0) return false;
	}
	return true;
}

void ColladaEffectTechnique::insertPass(ColladaEffectPass* pass)
{
	ReferenceTarget* refArray[1] = {pass};
	mParamBlock->Append(ColladaEffectTechnique::param_passes, 1, refArray);
	mParamBlock->Shrink(ColladaEffectTechnique::param_passes);
}

ColladaEffectPass* ColladaEffectTechnique::addPass(const fstring& name)
{
	if (!isNameUnique(name)) return NULL;
	ColladaEffectPass* pass = new ColladaEffectPass(FALSE,this,name);
	insertPass(pass);
	return pass;
}

ColladaEffectPass* ColladaEffectTechnique::removePass(size_t index)
{
	if (index >= getPassCount())
		return NULL;

	ColladaEffectPass* pass = getPass(index);
	mParamBlock->Delete(ColladaEffectTechnique::param_passes, (int)index, 1);
	mParamBlock->Shrink(ColladaEffectTechnique::param_passes);
	return pass;
}

ColladaEffectPass* ColladaEffectTechnique::getEditedPass()
{
	ReferenceTarget* ref = NULL;
	Interval valid;
	mParamBlock->GetValue(ColladaEffectTechnique::param_edited_pass, 0, ref, valid);
	return (ColladaEffectPass*)ref;
}

ColladaEffectPass* ColladaEffectTechnique::setEditedPass(size_t index)
{
	ReferenceTarget* ref = (ReferenceTarget*)getPass(index);
	mParamBlock->SetValue(ColladaEffectTechnique::param_edited_pass, 0, ref);
	return (ColladaEffectPass*)ref;
}

bool ColladaEffectTechnique::isValid()
{
	size_t pc = getPassCount();
	if (pc == 0) return false;
	for (size_t i = 0; i < pc; ++i)
	{
		ColladaEffectPass* pass = getPass(i);
		if (pass == NULL || !pass->isValid()) return false;
	}
	return true;
}
