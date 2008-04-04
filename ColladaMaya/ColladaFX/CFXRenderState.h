/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __CFX_RENDER_STATE_H_
#define __CFX_RENDER_STATE_H_

#ifndef _FU_DAE_ENUM_H_
#include "FUtils/FUDaeEnum.h"
#endif // _FU_DAE_ENUM_H_

class FCDEffectPassState;
struct _CGstate;
struct _CGstateassignment;
class CFXShaderNode;

class CFXRenderState
{
public:
	static const char* CG_RENDER_STATE_NAMES[];
	static const FUDaePassState::State CG_RENDER_STATES_XREF[];

private:
	CFXShaderNode* parent;
	FCDEffectPassState* colladaState; // This state is just used to hold the data: don't attach it to a FCDocument object.

	fm::string attributeName; // Persistent mostly for debugging purposes.
	MObject attribute;
	bool isDirty, isDead;

	// for indexed render states such as TextureEnable, LightEnable, ClipPaneEnable, etc.
	int index;

public:
	CFXRenderState(CFXShaderNode* parent);
	~CFXRenderState();

	void Initialize(FCDEffectPassState* renderState);
	bool Initialize(_CGstateassignment* effectState);
	bool Initialize(FUDaePassState::State stateType);

	inline const FCDEffectPassState* GetData() const { return colladaState; }
	inline const MObject& GetAttribute() const { return attribute; }
	FUDaePassState::State GetType() const;
	MPlug GetPlug();
	inline void SetDirtyFlag() { isDirty = true; }
	inline void ResetDirty() { isDirty = false; }
	inline bool IsDirty() const { return isDirty; }
	inline void SetDeadFlag() { isDead = true; }
	inline void ResetDeadFlag() { isDead = false; }
	inline bool IsDead() const { return isDead; }
	void Synchronize();
	void Use();
	void Reset();
	/** Call this method when removing a render state from a shader node.*/
	void Restore();

	const char* GetColladaStateName() const;
	// based on the fact that there's a 1-1 mapping between Cg and ColladaFx render state types (as it should be).
	const char* GetCgStateName() const;

	bool IsIndexed() const;
	int GetIndex() const { return index; }
	MString GetIndexedName();
	void SetIndex(int i) { index = i; }

	/** Generate a MEL command for UI generation.
		Use the eval(string) MEL function to generate the populated string builder.
		@param shader The parent ColladaFX shader node.
		@param command (out) The UI command.*/
	void GenerateUICommand(const CFXShaderNode* shader, FUStringBuilder& command) const;

private:
	void CreateAttribute();
	void AddAttribute();

public:
	static uint32 getTotalRenderStateCount();
	static FUDaePassState::State CgToColladaTranslation(const char* cgName);
	static const char* ColladaToCgTranslation(FUDaePassState::State stateType);
	static bool IsStateIndexed(FUDaePassState::State stateType);
	static FUDaePassState::State TranslateState(_CGstate* state);
	/** Splits the name into a ColladaFx render state name and an index.
		@param (in/out) In: indexed name, Out: name without index suffix.
		@return The index.*/
	static uint GetIndexFromIndexedName(fm::string& colladaName);
};

#endif // __CFX_RENDER_STATE_H_

