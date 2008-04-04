/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "ColladaEffectShader.h"
#include "ColladaEffectParameter.h"
#include "ColladaEffect.h"
#include "ColladaEffectPass.h"
#include "ColladaEffectTechnique.h"
#include "../DlgTemplateBuilder.h"

void _DebugCG(const char* file, int line)
{
#ifdef _DEBUG
	static char sz[1024];
	HRESULT d3derr = cgD3D9GetLastError();
	if (d3derr != D3D_OK)
	{
		snprintf(sz,1024,"%s:%d\nCg D3D9 error. D3D error code = %d, desc: %s\n",file,line,d3derr, cgD3D9TranslateHRESULT(d3derr));
		OutputDebugString(sz);
#		if (CG_FAIL_ON_ERROR)
		FUFail(return);
#		endif
	}
	CGerror err = cgGetError();
	if (err != CG_NO_ERROR)
	{
		const char* errorString = cgGetErrorString(err);
		snprintf(sz,1024,"%s:%d\nCg error %d: %s\n",file,line,err,errorString);
		OutputDebugString(sz);
#		if (CG_FAIL_ON_ERROR)
		FUFail(return);
#		endif
	}
#else
	line = (int) file[0];
#endif // _DEBUG
}

void _DebugCGC(CGcontext context, const char* file, int line)
{
#ifdef _DEBUG
	const char* szListing = cgGetLastListing(context);
	if (szListing != NULL)
	{
		static char sz[512];
		snprintf(sz,1024,"%s:%d\nCg compiler listing:\n", file, line);
		OutputDebugString(sz);
		OutputDebugString(szListing); // this string may be very long
		OutputDebugString("\n");
	}
#else 
	line = (int) file[0];
	context;
#endif // _DEBUG
}

///////////////////////////////////////////////////////////////////////////////
// CLASS DESCRIPTION
///////////////////////////////////////////////////////////////////////////////

class ColladaEffectPassClassDesc : public ClassDesc2
{
public:
	int				IsPublic() {return FALSE; }
	void*			Create(BOOL isLoading)
	{
		return new ColladaEffectPass(isLoading);
	}

	const TCHAR*	ClassName() {return _T("ColladaEffectPass");}
	SClass_ID		SuperClassID() {return REF_TARGET_CLASS_ID;}
	Class_ID 		ClassID() {return COLLADA_EFFECT_PASS_ID;}
	const TCHAR* 	Category() {return _T("");}
	const TCHAR*	InternalName() { return _T("ColladaEffectPass"); }
	HINSTANCE		HInstance() { return hInstance; }
};

static ColladaEffectPassClassDesc gColladaEffectPassCD;
ClassDesc2* GetColladaEffectPassClassDesc()
{
	return &gColladaEffectPassCD;
}

///////////////////////////////////////////////////////////////////////////////
// PARAMETER BLOCKS
///////////////////////////////////////////////////////////////////////////////

static ParamBlockDesc2 gColladaEffectPassPBDesc(
	ColladaEffectPass::parameters, _T("parameters"),  0, &gColladaEffectPassCD, P_AUTO_CONSTRUCT, 0,

	// params
	// ParamID, internal_name, ParamType, [tab size], flags, local_name_res_ID, <Param Tags>

	ColladaEffectPass::param_parent, _T("parent"), TYPE_REFTARG, P_NO_REF, 0,
		end,

	ColladaEffectPass::param_name, _T("name"), TYPE_STRING, 0, 0,
		end,

	ColladaEffectPass::param_vs, _T("vertex_shader"), TYPE_REFTARG, 0, 0,
		end,

	ColladaEffectPass::param_fs, _T("fragment_shader"), TYPE_REFTARG, 0, 0,
		end,

	ColladaEffectPass::param_effect, _T("effect_shader"), TYPE_REFTARG, 0, 0,
		end,

	ColladaEffectPass::param_parameters, _T("effect_params"), TYPE_REFTARG, 0, 0,
		end,
	
	end
);

///////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION
///////////////////////////////////////////////////////////////////////////////

ColladaEffectPass::ColladaEffectPass(BOOL isLoading, ColladaEffectTechnique* parentP, const fstring& nameP)
:	mParamBlock(NULL)
,	mCgContext(NULL)
,	mIsValid(false)
,	mErrorString("")
,	mCollection(NULL)
,	mDXWindow(NULL)
,	mINode(NULL)
{
	gColladaEffectPassCD.MakeAutoParamBlocks(this);

	mCgContext = cgCreateContext();
	cgD3D9RegisterStates(mCgContext);
	DebugCG();

	if (!isLoading)
	{
		setParent(parentP);
		setName(nameP);

		// create empty parameter collection
		setParameterCollection(new ColladaEffectParameterCollection());
	}
	// at load time, the parameter collection is fetched after the param block initialization
}

ColladaEffectPass::~ColladaEffectPass()
{
	cgDestroyContext(mCgContext);
	DebugCG();
}

uint16 ColladaEffectPass::GetNextDialogID()
{
	return mCollection->GetNextDialogID();
}

ColladaEffectShader* ColladaEffectPass::getShader(ParamID which)
{
	ReferenceTarget* ref = NULL;
	Interval valid;
	mParamBlock->GetValue(which,0,ref,valid);
	ColladaEffectShader* s = (ColladaEffectShader*)ref;
	return s;
}

bool ColladaEffectPass::isEffect()
{
	return (getShader(ColladaEffectPass::param_effect) != NULL);
}

void ColladaEffectPass::destroyShader(ParamID which)
{
	// ColladaEffectShader* s = getShader(which);
	mParamBlock->SetValue(which,0,(ReferenceTarget*)NULL);
	// SAFE_DELETE(s); // shader is in a managed parameter block
}

void ColladaEffectPass::setShader(ParamID which, ColladaEffectShader* shader)
{
	destroyShader(which);
	mParamBlock->SetValue(which,0,(ReferenceTarget*)shader);
}

ColladaEffectTechnique* ColladaEffectPass::getParent()
{
	ReferenceTarget* p = NULL;
	Interval valid;
	mParamBlock->GetValue(ColladaEffectPass::param_parent, 0, p, valid);
	return (ColladaEffectTechnique*)p;
}

void ColladaEffectPass::setParent(ColladaEffectTechnique* parent)
{
	mParamBlock->SetValue(ColladaEffectPass::param_parent, 0, (ReferenceTarget*)parent);
}

fstring ColladaEffectPass::getName()
{
	TCHAR* n = NULL;
	Interval valid;
	mParamBlock->GetValue(ColladaEffectPass::param_name, 0, n, valid);
	return FS(n);
}

bool ColladaEffectPass::setName(const fstring& name)
{
	ColladaEffectTechnique* parent = getParent();
	FUAssert(parent != NULL, return false);

	if (!parent->isNameUnique(name)) return false;
	TSTR s = name.c_str();
	mParamBlock->SetValue(ColladaEffectPass::param_name, 0, (TCHAR*)s.data());
	return true;
}

void ColladaEffectPass::newParameterCollection()
{
	// remove the ParameterCollection reference in the ColladaEffect
	// CustAttribContainer since loading a shader deletes the pass parameter collection
	//FUAssert(this->getParent()->getParent() != NULL, return); // the effect must exist
	//this->getParent()->getParent()->reloadUniforms(NULL);

	// recreate the collection (we lose any change applied to the other shader)
	ColladaEffectParameterCollection *collection = new ColladaEffectParameterCollection();
	setParameterCollection(collection);
	mCollection = collection;
}

bool ColladaEffectPass::loadShader(bool isVertex, const fstring& fileName, const fstring& entry)
{
	// reset validity
	mIsValid = true;
	mErrorString = "";

	// no mix between shaders and effects
	destroyShader(ColladaEffectPass::param_effect);

	newParameterCollection();

	ColladaEffectShader* shader = new ColladaEffectShader(FALSE,this);
	ColladaEffectShader* otherShader = NULL;
	if (isVertex)
	{
		setShader(ColladaEffectPass::param_vs, shader);
		otherShader = getShader(ColladaEffectPass::param_fs);
	}
	else
	{
		setShader(ColladaEffectPass::param_fs, shader);
		otherShader = getShader(ColladaEffectPass::param_vs);
	}

	if (otherShader != NULL)
	{
		if (!otherShader->initialize())
		{
			mIsValid = false;
			mErrorString = otherShader->getErrorString();
		}
	}

	// setup shader info
	ColladaEffectShaderInfo& info = shader->getInfo();
	info.filename = fileName;
	info.entry = (entry.empty()) ? FC("main") : entry;
	info.isVertex = isVertex;
	info.isEffect = false;
	info.techniqueIndex = 0;
	info.passIndex = 0;

	// fix the collection
	if (!shader->initialize())
	{
		mIsValid = false;
		mErrorString += "\n";
		mErrorString += shader->getErrorString();
	}

	if (mIsValid)
	{
		IParamBlock2* pb = mCollection->CreateParamBlock();
		shader->setParamBlock(pb);
		if (otherShader) otherShader->setParamBlock(pb);
	}
	return mIsValid;
}

bool ColladaEffectPass::loadEffect(const fstring& fileName, uint32 techTarget, uint32 passTarget)
{
	// reset validity
	mIsValid = true;
	mErrorString = "";

	// no mix between shaders and effects
	destroyShader(ColladaEffectPass::param_vs);
	destroyShader(ColladaEffectPass::param_fs);

	newParameterCollection();

	// load Effect shader
	ColladaEffectShader* shader = new ColladaEffectShader(FALSE,this);
	shader->getInfo().filename = fileName;
	shader->getInfo().isEffect = true;
	shader->getInfo().techniqueIndex = techTarget;
	shader->getInfo().passIndex = passTarget;
	setShader(ColladaEffectPass::param_effect, shader);

	// fix the collection
	if (!shader->initialize())
	{
		mIsValid = false;
		mErrorString = shader->getErrorString();
	}

	if (mIsValid)
	{
		IParamBlock2* pb = mCollection->CreateParamBlock();
		shader->setParamBlock(pb);
		
	}
	return mIsValid;
}

bool ColladaEffectPass::addParameter(ColladaEffectParameter& paramP)
{
	return mCollection->registerParameter(paramP);
}


ColladaEffectParameterCollection* ColladaEffectPass::getParameterCollection()
{
	Interval dummy;
	ReferenceTarget *ref = NULL;
	mParamBlock->GetValue(ColladaEffectPass::param_parameters, 0, ref, dummy);
	mCollection = (ColladaEffectParameterCollection*)ref;
	return mCollection;
}

void ColladaEffectPass::setParameterCollection(ColladaEffectParameterCollection *collection)
{
	ReferenceTarget* ref = (ReferenceTarget*)collection;
	mParamBlock->SetValue(ColladaEffectPass::param_parameters, 0, ref);
	// SAFE_DELETE(mCollection); // by putting the collection in a parameter block, the reference has already been deleted
	mCollection = collection;
}

void ColladaEffectPass::releaseTextureParameters()
{
	ColladaEffectParameterList& params = mCollection->getParameters();
	for (size_t i = 0; i < params.size(); ++i)
	{
		params[i]->releaseTexture();
	}
}

void ColladaEffectPass::recreateTextureParameters(IDirect3DDevice9* pNewDevice)
{
	ColladaEffectParameterList& params = mCollection->getParameters();
	for (size_t i = 0; i < params.size(); ++i)
	{
		params[i]->rebuildTexture(pNewDevice);
	}
}

void ColladaEffectPass::begin(ID3D9GraphicsWindow *pWindow, INode* pINode)
{
	mDXWindow = pWindow;
	mINode = pINode;

	ColladaEffectShader* effect = getShader(ColladaEffectPass::param_effect);
	if (effect != NULL)
	{
		effect->prepare();
	}
	else
	{
		ColladaEffectShader* vs = getShader(ColladaEffectPass::param_vs);
		if (vs != NULL) vs->prepare();
		ColladaEffectShader* fs = getShader(ColladaEffectPass::param_fs);
		if (vs != NULL) fs->prepare();
	}
}

void ColladaEffectPass::proc(ILoad* UNUSED(iload))
{
	// at this point the shaders should have been created
	// and they should be present in the ParamBlock2.\

	mCollection = getParameterCollection();
	FUAssert(mCollection != NULL, return);

	// the ColladaEffectParameterCollection is loaded, so we need to compare
	// the new parameters with existing ones
	bool success = true;
	bool hasVertex = false, hasFrag = false, hasEffect = false;

	// first pass, try to match everything

	// VERTEX
	ColladaEffectShader* s = getShader(ColladaEffectPass::param_vs);
	if (success && s)
	{
		s->setParent(this);
		success &= s->initialize();
		if (success) hasVertex = true;
	}

	// FRAGMENT
	s = getShader(ColladaEffectPass::param_fs);
	if (success && s)
	{
		s->setParent(this);
		success &= s->initialize();
		if (success) hasFrag = true;
	}

	// EFFECT
	s = getShader(ColladaEffectPass::param_effect);
	if (success && s)
	{
		s->setParent(this);
		success &= s->initialize();
		if (success) hasEffect = true;
	}

	if (!success)
	{
		// on failure, clear the parameter collection, and retry to initialize the shaders
		ColladaEffectParameterCollection* collect = getParameterCollection();
		FUAssert(collect != NULL, return);
		collect->ReleaseColladaFXBlockDesc();
		success = true;

		// VERTEX
		ColladaEffectShader* s = getShader(ColladaEffectPass::param_vs);
		if (success && s)
		{
			s->setParent(this);
			success &= s->initialize();
			if (success) hasVertex = true;
		}

		// FRAGMENT
		s = getShader(ColladaEffectPass::param_fs);
		if (success && s)
		{
			s->setParent(this);
			success &= s->initialize();
			if (success) hasFrag = true;
		}

		// EFFECT
		s = getShader(ColladaEffectPass::param_effect);
		if (success && s)
		{
			s->setParent(this);
			success &= s->initialize();
			if (success) hasEffect = true;
		}
	}

	if (success && (hasEffect || (hasVertex && hasFrag)))
	{
		mIsValid = true;
		mErrorString = "";
	}
	else
	{
		mIsValid = false;
		mErrorString = "Unable to re-initialize shaders.";
	}
}

IOResult ColladaEffectPass::Load(ILoad* iload)
{
	iload->RegisterPostLoadCallback(this);
	return ReferenceTarget::Load(iload);
}

///////////////////////////////////////////////////////////////////////////////
// ColladaEffectParameterCollection class implementation
///////////////////////////////////////////////////////////////////////////////

// static members
const uint16 ColladaEffectParameterCollection::FIRST_DIALOG_ID = 256;

ColladaEffectParameterCollection::ColladaEffectParameterCollection()
:	mParamBlock2(NULL)
,	mParamBlockDesc2(NULL)
,	mBlockID(0)
,	mDialogName("Uniform Parameters")
,	mDialogTemplate(NULL)
,	mExtraReferences()
,	mNextDialogID(FIRST_DIALOG_ID)
{
}

ColladaEffectParameterCollection::~ColladaEffectParameterCollection()
{
	SAFE_DELETE_ARRAY(mDialogTemplate);
	mExtraReferences.clear();
	mParameters.clear();
	
	DeleteAllRefs();
	ReleaseColladaFXBlockDesc();
}

ParamBlockDesc2* ColladaEffectParameterCollection::GetColladaFXBlockDesc()
{
	if (mParamBlockDesc2 == NULL)
	{
		TSTR temp = mDialogName.c_str();
		ColladaEffectParameterCollectionClassDesc* cd = (ColladaEffectParameterCollectionClassDesc*)GetColladaEffectParameterCollectionClassDesc();
		mParamBlockDesc2 = new ParamBlockDesc2(cd->GetFreeParamBlockId(), temp.data(), 0, cd, P_TEMPLATE_UI, end);
	}
	return mParamBlockDesc2;
}
	
void ColladaEffectParameterCollection::ReleaseColladaFXBlockDesc()
{
	if (mParamBlockDesc2 != NULL)
	{
		ClassDesc2* cd = mParamBlockDesc2->cd;

		// Remove the pbDescriptor from our classDesc
		int pbCount = cd->NumParamBlockDescs();
		ParamBlockDesc2** allPblockDescs = new ParamBlockDesc2*[pbCount];
		for (int i = 0; i < pbCount; i++)
		{
			ParamBlockDesc2* aDesc = cd->GetParamBlockDesc(i);
			if (aDesc != mParamBlockDesc2) allPblockDescs[i] = aDesc;
			else allPblockDescs[i] = NULL;
		}

		// Now, reset our class descriptors parameter blocks
		cd->ClearParamBlockDescs();
		for (int i = 0; i < pbCount; i++)
		{
			if (allPblockDescs[i] != NULL) cd->AddParamBlockDesc(allPblockDescs[i]);
		}

		// Release the descriptor entirely
		SAFE_DELETE(mParamBlockDesc2);
#ifdef _DEBUG
		FUAssert(pbCount != cd->NumParamBlockDescs(), return);
#endif
		mNextDialogID = FIRST_DIALOG_ID;
	}
}

IParamBlock2* ColladaEffectParameterCollection::CreateParamBlock()
{
	if (mParamBlockDesc2 != NULL)
	{
		IParamBlock2* paramBlock = CreateParameterBlock2(mParamBlockDesc2, this);
		ReplaceReference(ref_pblock, paramBlock);
		SetBlockID(mParamBlockDesc2->ID);
		return paramBlock;
	}
	return NULL;
}

ReferenceTarget* ColladaEffectParameterCollection::Clone(RemapDir &remap)
{
	ColladaEffectParameterCollection* pnew = new ColladaEffectParameterCollection;
	pnew->ReplaceReference(0, remap.CloneRef(mParamBlock2));
	BaseClone(this, pnew, remap);
	return pnew;
}

void ColladaEffectParameterCollection::DeleteThis()
{
	DeleteAllRefs();
	delete this;
}

int	ColladaEffectParameterCollection::NumParamBlocks()
{
	return 1;
}

IParamBlock2* ColladaEffectParameterCollection::GetParamBlock(int i)
{
	return (i == ref_pblock) ? mParamBlock2 : NULL;
}

IParamBlock2* ColladaEffectParameterCollection::GetParamBlockByID(BlockID id)
{
	return (mBlockID == (uint32) id) ? mParamBlock2 : NULL;
}

int ColladaEffectParameterCollection::NumRefs()
{
	// see SimpleAttrib.cpp for an explanation about this
	return num_static_refs + (int) mExtraReferences.size() + 1;
}

RefTargetHandle ColladaEffectParameterCollection::GetReference(int i)
{
	if (i == ref_pblock) return mParamBlock2;
	else if (i - num_static_refs < (int) mExtraReferences.size()) return mExtraReferences.at(i - num_static_refs);
	else return NULL;
}

void ColladaEffectParameterCollection::SetReference(int i, RefTargetHandle rtarg)
{
	if (i == ref_pblock) 
	{
		mParamBlock2 = (IParamBlock2*) rtarg;
		if (mParamBlock2 != NULL)
		{
			mParamBlockDesc2 = mParamBlock2->GetDesc();
		}
	}
	else 
	{
		if (i - num_static_refs >= (int) mExtraReferences.size()) mExtraReferences.resize(i);
		mExtraReferences.at(i - num_static_refs) = rtarg;
	}
}

int ColladaEffectParameterCollection::NumSubs()
{
	return 1;
}

Animatable* ColladaEffectParameterCollection::SubAnim(int)
{
	return mParamBlock2;
}

TSTR ColladaEffectParameterCollection::SubAnimName(int)
{
	return mDialogName.c_str();
}

DLGTEMPLATE* ColladaEffectParameterCollection::GetDialogTemplate()
{
	if (!isFixed()) return NULL;
	if (!mDialogTemplate)
		GenerateDialogTemplate();
	return mDialogTemplate;
}

ParamDlg* ColladaEffectParameterCollection::CreateParamDlg(HWND hwMtlEdit, IMtlParams*  imp)
{
	if (mDialogTemplate == NULL)
		if (!GenerateDialogTemplate())
			return NULL;

	IAutoMParamDlg* dlgMaker = CreateAutoMParamDlg(
		0,
		hwMtlEdit, imp, this, 
		mParamBlock2, GetColladaEffectParameterCollectionClassDesc(),
		hInstance, mDialogTemplate, GetName(), 0);

	return dlgMaker;
}

// Dialog Template UI
static const uint16 lineHeight = 11;
static const uint16 lineWidth = 216;
static const uint16 leftSectionLeft = 2;
static const uint16 leftSectionRight = 118;
static const uint16 rightSectionLeft = 124;
static const uint16 rightSectionRight = 213;
static const uint16 rightSectionWidth = rightSectionRight - rightSectionLeft;

/** Ugly? Yes.*/
class Hack
{
public:
	ColladaEffectParameter* p;
};

static int sortParameters(const void* p1v, const void* p2v)
{
	ColladaEffectParameter* p1 = ((Hack*)p1v)->p;
	ColladaEffectParameter* p2 = ((Hack*)p2v)->p;
	return fstrcmp(p1->getUIName().c_str(), p2->getUIName().c_str());
}

bool ColladaEffectParameterCollection::registerParameter(ColladaEffectParameter& paramP)
{
	ParamBlockDesc2* desc = GetColladaFXBlockDesc(); // lazy init
	TCHAR* name = (TCHAR*)paramP.getName().c_str();

	// maybe it already exists (loading)
	for (uint16 i = 0; i < desc->count; ++i)
	{
		ParamDef& pdef = desc->paramdefs[i];
		if (IsEquivalent(FS(name), FS(pdef.int_name)))
		{
			paramP.setParamID(pdef.ID);
			if (mParamBlock2 != NULL) paramP.setParamBlock(mParamBlock2);
			mParameters.push_back(&paramP);
			return true;
		}
	}

	// we can't add more parameters if the block has been constructed.
	if (isFixed())
		return false;

	// add it to our block description
	paramP.addToDescription(desc);
	mParameters.push_back(&paramP);
	return true;
}

ColladaEffectParameter* ColladaEffectParameterCollection::findParameter(const fstring& name)
{
	for (size_t i = 0; i < mParameters.size(); ++i)
	{
		ColladaEffectParameter* param = mParameters[i];
		if (param == NULL) continue;
		if (IsEquivalent(param->getName(),name))
		{
			return param;
		}
	}
	return NULL;
}

bool ColladaEffectParameterCollection::GenerateDialogTemplate()
{
	SAFE_DELETE_ARRAY(mDialogTemplate);
	if (mParamBlock2 == NULL) return false;

	size_t paramCount = mParamBlock2->NumParams();
	FUAssert(paramCount == mParameters.size(), return false);

	// create the builder with a large enough buffer
	DlgTemplateBuilder tb(16*1024);

	// Generate the dialog template.
	DLGTEMPLATE& dialog = tb.Append<DLGTEMPLATE>();
	dialog.style = WS_CHILD | WS_VISIBLE | DS_SETFONT;
	dialog.cx = lineWidth;

	UNUSED(uint16& menuIdentifier =) tb.Append<uint16>(); // Skip the menu identifier.
	UNUSED(uint16& windowClassName =) tb.Append<uint16>(); // Skip the window class name.
	UNUSED(uint16& windowTitleName =) tb.Append<uint16>(); // Skip the window title name.

	// [DS_SETFONT]
	tb.Append((uint16) 8); // font size
	tb.AppendWString("MS Sans Serif"); // font typename

	// sort the parameter vector using their UI names
	qsort(mParameters.begin(), mParameters.size(), sizeof(ColladaEffectParameter*), sortParameters);

	// Process the param block items into dialog controls
	WORD lineCount = 0;
	for (size_t p = 0; p < paramCount; ++p)
	{
		mParameters[p]->buildTemplateItem(tb,dialog.cdit,lineCount);
	}

	// adjust the height of the dialog based on the resulting UI
	dialog.cy = lineCount * lineHeight + 4;

	// Scale down the buffer in order to save memory.
	mDialogTemplate = tb.buildTemplate();

	return true;
}


///////////////////////////////////////////////////////////////////////////////
// ColladaEffectParameterCollectionClassDesc class implementation
///////////////////////////////////////////////////////////////////////////////

ClassDesc2* GetColladaEffectParameterCollectionClassDesc()
{
	static ColladaEffectParameterCollectionClassDesc description;
	return &description;
}

#define PB_CHUNK			0x1101
#define PB_HEADER_CHUNK		0x1102
#define PB_PARAMS_CHUNK		0x1103
#define PB_PARAM_DESC_CHUNK	0x1104

const size_t bufferSize = 1024;
#define CHECK_IOR(command) { IOResult ior = command; FUAssert(ior == IO_OK, return ior); }
#define WRITE_STR(str) { \
	uint32 size = min((uint32) (bufferSize - 1), (uint32) strlen(str)); \
	CHECK_IOR(iSave->Write(&size, sizeof(uint32), &dummy)); \
	CHECK_IOR(iSave->Write(str, size, &dummy)); \
	CHECK_IOR(iSave->Write(&(size = 0), sizeof(uint8), &dummy)); }

#define READ_STR(str) { \
	uint32 size; CHECK_IOR(iLoad->Read(&size, sizeof(uint32), &dummy)); \
	FUAssert(size < bufferSize, break) \
	str = new char[size + 1]; \
	CHECK_IOR(iLoad->Read(str, size + 1, &dummy)); }

// Serialization
IOResult ColladaEffectParameterCollectionClassDesc::Save(ISave* iSave)
{
	ULONG dummy;
	// Write out the attribute info.
	
	int numDesc = NumParamBlockDescs();
	for (int i = 0; i < numDesc; i++)
	{
		ParamBlockDesc2* pbDesc = GetParamBlockDesc(i);
		if (pbDesc == NULL) continue;

		// Begin parameter descriptor chunk
		iSave->BeginChunk(PB_CHUNK);

		/***/
		// begin pb header chunk
		iSave->BeginChunk(PB_HEADER_CHUNK);

		// Write out some relevant information about the block.
		BlockID blockID = pbDesc->ID;
		CHECK_IOR(iSave->Write(&blockID, sizeof(BlockID), &dummy));

		uint32 flags = pbDesc->flags;
		CHECK_IOR(iSave->Write(&flags, sizeof(uint32), &dummy));

		USHORT paramCount = pbDesc->Count();
		CHECK_IOR(iSave->Write(&paramCount, sizeof(USHORT), &dummy));

		// Write out name in its own chunk
		WRITE_STR(pbDesc->int_name);
		
		iSave->EndChunk(); // pb header
		/***/

		// Write out each parameter from its description, within its own chunk.
		for (uint32 p = 0; p < paramCount; ++p)
		{
			ParamDef& definition = pbDesc->GetParamDef(p);
			iSave->BeginChunk(PB_PARAM_DESC_CHUNK);

			// Write out the parameter details
			ParamType2 ptype = definition.type;
			CHECK_IOR(iSave->Write(&ptype, sizeof(ParamType2), &dummy));

			ControlType2 ctype = definition.ctrl_type;
			CHECK_IOR(iSave->Write(&ctype, sizeof(ControlType2), &dummy));

			EditSpinnerType stype = definition.spin_type;
			CHECK_IOR(iSave->Write(&stype, sizeof(EditSpinnerType), &dummy));

			int flags = definition.flags;
			CHECK_IOR(iSave->Write(&flags, sizeof(int), &dummy));

			short ctrl_count = definition.ctrl_count;
			CHECK_IOR(iSave->Write(&ctrl_count, sizeof(short), &dummy));
			CHECK_IOR(iSave->Write(definition.ctrl_IDs, sizeof(int) * ctrl_count, &dummy));

			WRITE_STR(definition.int_name);
			
			iSave->EndChunk();
		}
		iSave->EndChunk();
	}
	return IO_OK;
}

IOResult ColladaEffectParameterCollectionClassDesc::Load(ILoad* iLoad)
{
	IOResult r, result = IO_OK;
	while (IO_OK == iLoad->OpenChunk())
	{
		if (iLoad->CurChunkID() == PB_CHUNK)
		{
			if ((r = LoadPBlockDesc(iLoad)) != IO_OK) result = r;
		}
		else FUFail(;);
		CHECK_IOR(iLoad->CloseChunk());
	}
	return result;
}

IOResult ColladaEffectParameterCollectionClassDesc::LoadPBlockDesc(ILoad* iLoad)
{
	ULONG dummy;
	ParamBlockDesc2* pbDesc = NULL;
	USHORT paramCount = 0;

	while (IO_OK == iLoad->OpenChunk())
	{
		switch (iLoad->CurChunkID())
		{
		case PB_HEADER_CHUNK: {
			FUAssert(pbDesc == NULL, return IO_ERROR);

			// it is very important reads are in the same order as above.
			BlockID id;
			uint32 flags;
			char* name = NULL;
			CHECK_IOR(iLoad->Read(&id, sizeof(BlockID), &dummy));
			CHECK_IOR(iLoad->Read(&flags, sizeof(uint32), &dummy));
			CHECK_IOR(iLoad->Read(&paramCount, sizeof(USHORT), &dummy));
			READ_STR(name);

			pbDesc = new ParamBlockDesc2(id, name, 0, this, flags, end);
			SetTakenId(id);
			break; }

		case PB_PARAM_DESC_CHUNK: {
			FUAssert(pbDesc != NULL && paramCount > 0 && pbDesc->count < paramCount,);

			ParamType2 ptype;
			ControlType2 ctrlType;
			EditSpinnerType spinType;
			int flags;
			short ctrlCount;
			int* ctrlIds = NULL;


			CHECK_IOR(iLoad->Read(&ptype, sizeof(ParamType2), &dummy));
			CHECK_IOR(iLoad->Read(&ctrlType, sizeof(ControlType2), &dummy));	
			CHECK_IOR(iLoad->Read(&spinType, sizeof(EditSpinnerType), &dummy));
			CHECK_IOR(iLoad->Read(&flags, sizeof(int), &dummy));
			CHECK_IOR(iLoad->Read(&ctrlCount, sizeof(short), &dummy));

			if (ctrlCount > 0)
			{
				ctrlIds = new int[ctrlCount];
				CHECK_IOR(iLoad->Read(ctrlIds, sizeof(int) * ctrlCount, &dummy));
			}

			char* name = NULL;
			READ_STR(name);

			// Reset the ctrl ids?
			// This looks stupid, but we have to let max assign its own
			// arrays.  Otherwise, we run the risk of memory contamination
			// when max free's its arrays.
			if (ctrlCount == 0)
			{
				pbDesc->AddParam(pbDesc->count, name, ptype, flags, 0, end);
			}
			else if (ctrlCount == 1)
			{
				pbDesc->AddParam(pbDesc->count, name, ptype, flags, 0, p_ui, ctrlType, *ctrlIds, end);
			}
			else if (ctrlCount == 2)
			{
				pbDesc->AddParam(pbDesc->count, name, ptype, flags, 0, p_ui, ctrlType, spinType, ctrlIds[0], ctrlIds[1], SPIN_AUTOSCALE, end);
			}

			break; }
		}
		CHECK_IOR(iLoad->CloseChunk());
	}
	FUAssert(pbDesc != NULL && paramCount == pbDesc->count, return IO_ERROR);

	return IO_OK;
}
