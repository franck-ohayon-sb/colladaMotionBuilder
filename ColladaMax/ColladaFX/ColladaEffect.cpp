/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "ColladaEffect.h"
#include "ColladaEffectPass.h"
#include "ColladaEffectRenderer.h"
#include "ColladaEffectParamDlg.h"

// Used in the ColladaEffectClassDesc class descriptor.
#define DXMATERIAL_DYNAMIC_UI Class_ID(0xef12512, 0x11351ed1)

/**
A class descriptor for the ColladaEffect class.
*/
class ColladaEffectClassDesc : public ClassDesc2
{
public:
	int				IsPublic() {return TRUE; }
	void*			Create(BOOL isLoading)
	{
		ColladaEffect*	effect = new ColladaEffect(isLoading);

		effect->SetMtlFlag(MTL_DISPLAY_ENABLE_FLAGS,true);
		effect->ClearMtlFlag(MTL_TEX_DISPLAY_ENABLED);

		// tell Max we'll use the hardware renderer
		effect->SetMtlFlag(MTL_HW_MAT_ENABLED, true);

		return effect;
	}

	const TCHAR*	ClassName() {return GetString(IDS_COLLADA_EFFECT_CLASS_NAME);}
	SClass_ID		SuperClassID() {return MATERIAL_CLASS_ID;}
	Class_ID 		ClassID() {return COLLADA_EFFECT_ID;}

	// This disables the DX Manager that hooks to any material in the editor.
	Class_ID		SubClassID() { return DXMATERIAL_DYNAMIC_UI; }

	const TCHAR* 	Category() {return _T("");}
	const TCHAR*	InternalName() { return _T("ColladaEffect"); }
	HINSTANCE		HInstance() { return hInstance; }
};

static ColladaEffectClassDesc gColladaEffectCD;
ClassDesc2* GetCOLLADAEffectDesc()
{
	return &gColladaEffectCD;
}

class ColladaEffectPBAccessor : public PBAccessor
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

static ColladaEffectPBAccessor gColladaEffectPBAccessor;

static ParamBlockDesc2 gColladaEffectPBDesc(
	ColladaEffect::parameters, _T("parameters"),  0, &gColladaEffectCD, P_AUTO_CONSTRUCT, 0,

	// params
	// ParamID, internal_name, ParamType, [tab size], flags, local_name_res_ID, <Param Tags>

	ColladaEffect::param_techniques, _T("techniques"), TYPE_REFTARG_TAB, 0, P_VARIABLE_SIZE, 0,
		p_accessor, &gColladaEffectPBAccessor,
		end,

	ColladaEffect::param_chosen_technique, _T("chosen_technique"), TYPE_REFTARG, 0, 0,
		p_accessor, &gColladaEffectPBAccessor,
		end,
	
	end
);

// callback function
static void ShaderUpdate(void *param, NotifyInfo *info) {

	MtlBase * mtl = (MtlBase*)param;

	if (info->intcode == NOTIFY_HW_TEXTURE_CHANGED && mtl == (MtlBase*)info->callParam)
	{
		GetCOREInterface()->ForceCompleteRedraw();
	}
}

// ColladaEffect class

ColladaEffect::ColladaEffect(BOOL isLoading)
:	mParamBlock2(NULL)
,	mValidInterval(FOREVER)
,	mDxRenderer(NULL)
,	mUniformParameters(NULL)
{
	gColladaEffectCD.MakeAutoParamBlocks(this);

	if (mDxRenderer != NULL)
	{
		delete mDxRenderer;
		mDxRenderer = NULL;
	}

	mDxRenderer = new ColladaEffectRenderer(this);
	RegisterNotification(ShaderUpdate,this,NOTIFY_HW_TEXTURE_CHANGED);

	if (!isLoading)
		Reset();
}

ColladaEffect::~ColladaEffect()
{
	SAFE_DELETE(mDxRenderer);
	UnRegisterNotification(ShaderUpdate,this,NOTIFY_HW_TEXTURE_CHANGED);
}

fstring ColladaEffect::getName()
{
	fstring name = FS(Mtl::GetName());
	return name;
}

size_t ColladaEffect::getTechniqueCount()
{
	int count = mParamBlock2->Count(ColladaEffect::param_techniques);
	return (size_t)count;
}

ColladaEffectTechnique* ColladaEffect::getTechnique(size_t idx)
{
	if (idx >= getTechniqueCount())
		return NULL;

	ReferenceTarget* ref = NULL;
	Interval valid;
	BOOL result = mParamBlock2->GetValue(ColladaEffect::param_techniques, 0, ref, valid, (int)idx);
	return (result == TRUE) ? (ColladaEffectTechnique*)ref : NULL;
}

void ColladaEffect::insertTechnique(ColladaEffectTechnique* tech)
{
	ReferenceTarget* refArray[1] = {tech};
	mParamBlock2->Append(ColladaEffect::param_techniques, 1, refArray);
	mParamBlock2->Shrink(ColladaEffect::param_techniques);
	sortTechniques();
}

static int __cdecl techSortPred(const void* t1v, const void* t2v)
{
	// No documentation about this, but the TYPE_REFTARG_TAB stores PB2Value*.
	ColladaEffectTechnique* t1 = (ColladaEffectTechnique*)((PB2Value*)t1v)->r;
	ColladaEffectTechnique* t2 = (ColladaEffectTechnique*)((PB2Value*)t2v)->r;
	return fstrcmp(t1->getName().c_str(), t2->getName().c_str());
}

void ColladaEffect::sortTechniques()
{
	mParamBlock2->Sort(ColladaEffect::param_techniques, techSortPred);
}

bool ColladaEffect::isNameUnique(const fstring& name)
{
	size_t tCount = getTechniqueCount();
	for (size_t i = 0; i < tCount; ++i)
	{
		ColladaEffectTechnique* t = getTechnique(i);
		if (fstrcmp(name.c_str(),t->getName().c_str()) == 0) return false;
	}
	return true;
}

ColladaEffectTechnique* ColladaEffect::addTechnique(const fstring& name)
{
	if (!isNameUnique(name)) return NULL;
	ColladaEffectTechnique* tech = new ColladaEffectTechnique(this,name);
	insertTechnique(tech);
	return tech;
}

ColladaEffectTechnique* ColladaEffect::removeTechnique(size_t index)
{
	if (index >= getTechniqueCount())
		return NULL;

	ColladaEffectTechnique* tech = getTechnique(index);
	mParamBlock2->Delete(ColladaEffect::param_techniques, (int)index, 1);
	mParamBlock2->Shrink(ColladaEffect::param_techniques);
	return tech;
}

ColladaEffectTechnique* ColladaEffect::getCurrentTechnique()
{
	ReferenceTarget* ref = NULL;
	Interval valid;
	mParamBlock2->GetValue(ColladaEffect::param_chosen_technique, 0, ref, valid);
	return (ColladaEffectTechnique*)ref;
}

ColladaEffectTechnique* ColladaEffect::setCurrentTechnique(size_t index)
{
	ReferenceTarget* ref = (ReferenceTarget*)getTechnique(index);
	mParamBlock2->SetValue(ColladaEffect::param_chosen_technique, 0, ref);
	return (ColladaEffectTechnique*)ref;
}

void ColladaEffect::releaseTextureParameters()
{
	size_t techCount = getTechniqueCount();
	for (size_t i = 0; i < techCount; ++i)
	{
		ColladaEffectTechnique* tech = getTechnique(i);
		size_t passCount = tech->getPassCount();
		for (size_t j = 0; j < passCount; ++j)
		{
			ColladaEffectPass* pass = tech->getPass(j);
			pass->releaseTextureParameters();
		}
	}
}

void ColladaEffect::recreateTextureParameters(IDirect3DDevice9* pNewDevice)
{
	size_t techCount = getTechniqueCount();
	for (size_t i = 0; i < techCount; ++i)
	{
		ColladaEffectTechnique* tech = getTechnique(i);
		size_t passCount = tech->getPassCount();
		for (size_t j = 0; j < passCount; ++j)
		{
			ColladaEffectPass* pass = tech->getPass(j);
			pass->recreateTextureParameters(pNewDevice);
		}
	}
}

void ColladaEffect::reloadUniforms(ColladaEffectPass* selectedPass)
{
	ICustAttribContainer* customAttributeContainer = this->GetCustAttribContainer();
	CustAttrib* newAttrib = (selectedPass == NULL) ? NULL : selectedPass->getParameterCollection();
	bool replaced = false;

	if (customAttributeContainer != NULL)
	{
		for (int i = 0; i < customAttributeContainer->GetNumCustAttribs(); ++i)
		{
			CustAttrib* ca = customAttributeContainer->GetCustAttrib(i);
			if (ca == NULL) continue;
			if (ca == mUniformParameters)
			{
				if (newAttrib != NULL)
					customAttributeContainer->SetCustAttrib(i, newAttrib);
				else
					customAttributeContainer->RemoveCustAttrib(i);
				replaced = true;
				break;
			}
		}
	}
	else
	{
		this->AllocCustAttribContainer();
		customAttributeContainer = this->GetCustAttribContainer();
	}

	if (!replaced && customAttributeContainer != NULL && newAttrib != NULL)
	{
		customAttributeContainer->AppendCustAttrib(newAttrib);
	}

	mUniformParameters = newAttrib;
}

///////////////////////////////////////////////////////////////////////////////
// ANIMATABLE

BaseInterface* ColladaEffect::GetInterface(Interface_ID id)
{
	if (id == VIEWPORT_SHADER_CLIENT_INTERFACE) {
		return static_cast<IDXDataBridge*>(this);
	}
	else if (id == VIEWPORT_SHADER9_CLIENT_INTERFACE) {
		return static_cast<IDXDataBridge*>(this);
	}
	else if (id == DX9_VERTEX_SHADER_INTERFACE_ID) {
		// Max requests this from Mesh::Render, D3DWindow::InitializeShaders
		return static_cast<IDX9VertexShader *>(mDxRenderer);
	}
	else {
		return StdMat2::GetInterface(id);
	}
}

Animatable* ColladaEffect::SubAnim(int i)
{
	switch (i) {
	case 0: return mParamBlock2;
	default: return NULL;
	}
}

TSTR ColladaEffect::SubAnimName(int i)
{
	switch (i) {
	case 0: return GetString(IDS_CE_NAME);
	default: return _T("");
	}
}

void ColladaEffect::BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev)
{
	StdMat2::BeginEditParams(ip,flags,prev);
}

void ColladaEffect::EndEditParams(IObjParam* ip, ULONG flags, Animatable* next)
{
	StdMat2::EndEditParams(ip,flags,next);
}

void ColladaEffect::EnumAuxFiles(NameEnumCallback& nameEnum, DWORD flags)
{
	if ((flags & FILE_ENUM_CHECK_AWORK1) && TestAFlag(A_WORK1)) return;

	// sample taken from the MAX SDK
	//if (GetCubeMapFile())
	//{
	//	if (flags & FILE_ENUM_ACCESSOR_INTERFACE)	{
	//		IEnumAuxAssetsCallback* callback = static_cast<IEnumAuxAssetsCallback*>(&nameEnum);
	//		callback->DeclareAsset(CubeMapFileAccessor(this));
	//	}
	//	else	{
	//		MaxSDK::Util::Path path(GetCubeMapFile());
	//		IPathConfigMgr::GetPathConfigMgr()->
	//			RecordInputAsset(path, nameEnum, flags);
	//	}
	//	
	//}
	
	//if (GetVertexShaderFile())
	//{
	//	if (flags & FILE_ENUM_ACCESSOR_INTERFACE)	{
	//		IEnumAuxAssetsCallback* callback = static_cast<IEnumAuxAssetsCallback*>(&nameEnum);
	//		callback->DeclareAsset(CubeMapVertexAccessor(this));
	//	}
	//	else	{
	//		MaxSDK::Util::Path path(GetVertexShaderFile());
	//		IPathConfigMgr::GetPathConfigMgr()->
	//			RecordInputAsset(path, nameEnum, flags);
	//	}
	//}
	
	ReferenceTarget::EnumAuxFiles(nameEnum, flags);
}

///////////////////////////////////////////////////////////////////////////////
// REFERENCEMAKER

RefTargetHandle ColladaEffect::GetReference(int i)
{
	if (i == 0)
	{
		return mParamBlock2;
	}
	else
	{
		return NULL;
	}
}

void ColladaEffect::SetReference(int i, RefTargetHandle rtarg)
{
	if (i == 0)
	{
		mParamBlock2 = (IParamBlock2*) rtarg;
	}
}

RefTargetHandle ColladaEffect::Clone(RemapDir &remap)
{
	// create new instance
	ColladaEffect *mnew = new ColladaEffect(FALSE);

	// copy superclass
	*((MtlBase*)mnew) = *((MtlBase*)this); 

	// clone parameter block
	mnew->ReplaceReference(0,remap.CloneRef(mParamBlock2));

	// clear valid interval
	mnew->mValidInterval.SetEmpty();

	// clone base
	BaseClone(this, mnew, remap);
	return (RefTargetHandle)mnew;
}

RefResult ColladaEffect::NotifyRefChanged(
	Interval UNUSED(changeInt),
	RefTargetHandle hTarget, 
	PartID& UNUSED(partID),
	RefMessage message)
{
		switch (message) {
		case REFMSG_CHANGE:
			mValidInterval.SetEmpty();
			if (hTarget == mParamBlock2){
				// see if this message came from a changing parameter in the pblock,
				// if so, limit rollout update to the changing item and update any active viewport texture
				//ParamID changing_param = mParamBlock2->LastNotifyParamID();
				//gColladaEffectPBDesc.InvalidateUI(changing_param);
			}

			return (REF_SUCCEED);
		}// end, switch

	return (REF_DONTCARE);
}

void ColladaEffect::NotifyChanged()
{
	NotifyDependents(FOREVER, (PartID)PART_ALL, REFMSG_CHANGE);
}

///////////////////////////////////////////////////////////////////////////////
// MTLBASE

void ColladaEffect::Reset()
{
	mValidInterval.SetEmpty();
	//gColladaEffectCD.MakeAutoParamBlocks(this);

	// reset parameters
	//mParamBlock2->ResetAll();
	//DeleteReference if you keep any
	NotifyDependents(FOREVER, (PartID)PART_ALL, REFMSG_CHANGE);
}

ULONG ColladaEffect::Requirements(int UNUSED(subMtlNum)) 
{
	// Requirements information provided by Jean-Francois Yelle, Autodesk.
	return (MTLREQ_TRANSP | MTLREQ_TRANSP_IN_VP);
}

Interval ColladaEffect::Validity(TimeValue UNUSED(t))
{
	return mValidInterval;
}

void ColladaEffect::Update(TimeValue t, Interval& valid)
{
	if (!mValidInterval.InInterval(t)) {

		mValidInterval.SetInfinite();
	}

	valid &= mValidInterval;
}

// create the template UI
ParamDlg* ColladaEffect::CreateParamDlg(HWND hWindow, IMtlParams* params)
{
	ColladaEffectParamDlg* dlg = new ColladaEffectParamDlg(hWindow,params,this);
	dlg->LoadDialog(TRUE);
	setParamDlg(dlg);
	return dlg;
}

///////////////////////////////////////////////////////////////////////////////
// MTL

void ColladaEffect::Shade(ShadeContext& UNUSED(sc))
{
	
}

float ColladaEffect::EvalDisplacement(ShadeContext& UNUSED(sc))
{
	return 0.0f;
}

Interval ColladaEffect::DisplacementValidity(TimeValue UNUSED(t))
{
	return FOREVER;
}

///////////////////////////////////////////////////////////////////////////////
// IO

#define MTL_HDR_CHUNK 0x4000

IOResult ColladaEffect::Save(ISave *isave)
{
	IOResult res;
	isave->BeginChunk(MTL_HDR_CHUNK);
	res = MtlBase::Save(isave);
	if (res!=IO_OK) return res;
	isave->EndChunk();

	return IO_OK;
}

IOResult ColladaEffect::Load(ILoad *iload)
{
	IOResult res;
	int id;
	while (IO_OK==(res=iload->OpenChunk())) {
		switch (id = iload->CurChunkID())  {
			case MTL_HDR_CHUNK:
				res = MtlBase::Load(iload);
				break;
			}
		iload->CloseChunk();
		if (res!=IO_OK) 
			return res;
		}

	return IO_OK;
}

///////////////////////////////////////////////////////////////////////////////
// IDX9DataBridge

void ColladaEffect::SetDXData(IHardwareMaterial * UNUSED(pHWMtl), Mtl* UNUSED(pMtl))
{
	// no op.
}

