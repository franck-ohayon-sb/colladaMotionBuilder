/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COLLADAFX_COLLADAEFFECTL_H_
#define _COLLADAFX_COLLADAEFFECTL_H_

#ifndef _COLLADAFX_STDMAT2_ADAPTER_H_
#include "StdMat2Adapter.h"
#endif // _COLLADAFX_STDMAT2_ADAPTER_H_

#include "ColladaEffectTechnique.h"

// generated using /3dsMax9/maxsdk/help/gencid.exe
#define COLLADA_EFFECT_ID Class_ID(0x29ab0360, 0x4a54492e)

// forward declarations
class ColladaEffectRenderer;
class ColladaEffectParamDlg;
static void ShaderUpdate(void *param, NotifyInfo *info);

/**
An hardware material plugin. This class sets Max internals properly in order to
take over the complete rendering of the geometries (Mesh, MNMesh) it affects.

It contains a ColladaEffectRenderer responsible for the actual DirectX 9.0 rendering.
*/
class ColladaEffect : 
	public StdMat2Adapter,
	public IDX9DataBridge
{
public:
	enum BlockIDs
	{
		parameters
	};

	enum ParamIDs
	{
		param_techniques,
		param_chosen_technique,
	};

private:
	IParamBlock2* mParamBlock2;
	Interval mValidInterval;

	ColladaEffectRenderer* mDxRenderer;
	ColladaEffectParamDlg* mParamDlg;
	CustAttrib* mUniformParameters; // pointer to a parameter collection

private:
	void insertTechnique(ColladaEffectTechnique* tech);

public:
	ColladaEffect(BOOL isLoading = FALSE);
	virtual ~ColladaEffect();

	fstring getName();

	void setParamDlg(ColladaEffectParamDlg* dlg){ mParamDlg = dlg; }

	bool isNameUnique(const fstring& name);
	void sortTechniques();
	size_t getTechniqueCount();
	ColladaEffectTechnique* getTechnique(size_t idx);
	ColladaEffectTechnique* addTechnique(const fstring& name);
	ColladaEffectTechnique* removeTechnique(size_t index);

	ColladaEffectTechnique* getCurrentTechnique();
	ColladaEffectTechnique* setCurrentTechnique(size_t index);

	void releaseTextureParameters();
	void recreateTextureParameters(IDirect3DDevice9* pNewDevice);

	void reloadUniforms(ColladaEffectPass* selectedPass);

	// from Animatable
	// ===============
	SClass_ID SuperClassID(){ return MATERIAL_CLASS_ID; }
	Class_ID ClassID(){ return COLLADA_EFFECT_ID;}

	// used to propagate notifications to our ColladaEffectRenderer.
	BaseInterface *GetInterface(Interface_ID id);

	// used to put the type name in the Material Editor
	void GetClassName(CStr& s){ s = GetString(IDS_COLLADA_EFFECT_NAME); }
	void DeleteThis(){ delete this; }
	// parameter editing: called when the use "may" edit the item's parameters
	void BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev = NULL);
	void EndEditParams(IObjParam* ip, ULONG flags, Animatable* next = NULL);

	// these are not abstract, but we MUST override them or else automatic UI crashes
	int	NumParamBlocks() { return 1; }
	IParamBlock2* GetParamBlock(int /*i*/) { return mParamBlock2; }
	IParamBlock2* GetParamBlockByID(BlockID id) { return (mParamBlock2->ID() == id) ? mParamBlock2 : NULL; }

	int NumSubs() {return 1;}
	Animatable* SubAnim(int i);
	TSTR SubAnimName(int i);
	int SubNumToRefNum(int subNum) {return subNum;}

	// used for linking the auxiliary files to the scene
	void EnumAuxFiles(NameEnumCallback& nameEnum, DWORD flags);

	// from ReferenceMaker
	// ===================
	int NumRefs() { return 1; }
	RefTargetHandle GetReference(int i);
	void SetReference(int i, RefTargetHandle rtarg);

#if (MAX_VERSION_MAJOR >= 9)
	RefTargetHandle Clone(RemapDir &remap = DefaultRemapDir());
#else
	RefTargetHandle Clone(RemapDir &remap = NoRemap());
#endif // MAX_VERSION
	RefResult NotifyRefChanged(Interval changeInt, RefTargetHandle hTarget, 
	   PartID& partID, RefMessage message);
	void NotifyChanged();

	// from MtlBase
	// ============
	void Reset();
	Interval Validity(TimeValue t);
	void Update(TimeValue t, Interval& valid);

	// UI methods
	BOOL SetDlgThing(ParamDlg* /*dlg*/) { return FALSE; }
	ParamDlg* CreateParamDlg(HWND hWindow, IMtlParams* params);

	// SubMaterial access methods (we have none)
	int NumSubMtls() {return 0;}
	Mtl* GetSubMtl(int /*i*/) { return NULL; }
	void SetSubMtl(int /*i*/, Mtl* /*m*/) {}
	TSTR GetSubMtlSlotName(int /*i*/) { return _T(""); }
	TSTR GetSubMtlTVName(int i) { return GetSubMtlSlotName(i); }

	// SubTexmap access methods (none)
	int NumSubTexmaps() {return 0;}
	Texmap* GetSubTexmap(int /*i*/) { return NULL; }
	void SetSubTexmap(int /*i*/, Texmap* /*m*/) {}
	TSTR GetSubTexmapSlotName(int /*i*/) { return _T(""); }
	TSTR GetSubTexmapTVName(int i) { return GetSubMtlSlotName(i); }

	// Mandatory override to get Max notifying us when rendering geometry
	ULONG Requirements(int subMtlNum);

	// from Mtl
	// ========
	virtual void Shade(ShadeContext& sc);
	float EvalDisplacement(ShadeContext& sc);
	Interval DisplacementValidity(TimeValue t);

	// IO (todo)
	// =========
	IOResult Save(ISave *isave);
	IOResult Load(ILoad *iload);

	// from IDX9DataBridge
	// ===================
	ParamDlg * CreateEffectDlg(HWND /*hWnd*/, IMtlParams* /*imp*/) {return NULL;}
	void DisableUI(){};
	void SetDXData(IHardwareMaterial * pHWMtl, Mtl * pMtl);
	TCHAR * GetName(){return _T("DxColladaEffect");}
	float GetDXVersion(){return 9.0f;}
};

// prototype forward declaration
ClassDesc2* GetCOLLADAEffectDesc();

#endif // _COLLADAFX_COLLADAEFFECTL_H_
