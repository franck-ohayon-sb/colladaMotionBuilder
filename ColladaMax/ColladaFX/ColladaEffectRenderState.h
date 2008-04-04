/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COLLADAFX_COLLADA_EFFECT_RENDER_STATE_H_
#define _COLLADAFX_COLLADA_EFFECT_RENDER_STATE_H_

#ifndef _FU_DAE_ENUM_H_
#include "FUtils/FUDaeEnum.h"
#endif // _FU_DAE_ENUM_H_

class DlgTemplateBuilder;

class ColladaEffectRenderState
{
public:
	/** Static cross reference data between Collada names and Cg names.*/
	static const char* CG_RENDER_STATE_NAMES[];
	static const FUDaePassState::State CG_RENDER_STATES_XREF[];

private:
	ColladaEffectPass* mParent; /**< The pass holding the render state.*/
	FCDEffectPassState* mColladaState; /**< State data holder.*/
	int mIndex; /**< Used if indexed state only.*/

public:
	ColladaEffectRenderState(ColladaEffectPass* parent);
	~ColladaEffectRenderState();

	void Initialize(FCDEffectPassState* renderState);
	bool Initialize(_CGstateassignment* effectState);
	bool Initialize(FUDaePassState::State stateType);

	inline const FCDEffectPassState* GetData() const { return colladaState; }
	FUDaePassState::State GetType() const;
	
	void Synchronize();
	void Use();
	
	const char* GetColladaStateName() const;
	const char* GetCgStateName() const;

	bool IsIndexed() const;
	int GetIndex() const { return mIndex; }
	TSTR GetIndexedName();
	void SetIndex(int i) { mIndex = i; }

	/**	Builds a Max template for this state.*/
	void GenerateUICommand(DlgTemplateBuilder& builder) const;

public:
	static uint32 getTotalRenderStateCount();
	static FUDaePassState::State CgToColladaTranslation(const char* cgName);
	static const char* ColladaToCgTranslation(FUDaePassState::State stateType);
	static bool IsStateIndexed(FUDaePassState::State stateType);
	static FUDaePassState::State TranslateState(_CGstate* state);
	/** Splits the name into a Collada render state name and an index.
		@param (in/out) In: indexed name, Out: name without index suffix.
		@return The index.*/
	static uint GetIndexFromIndexedName(fm::string& colladaName);
};

#endif // _COLLADAFX_COLLADA_EFFECT_RENDER_STATE_H_
