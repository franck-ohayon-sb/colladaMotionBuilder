/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COLLADAFX_COLLADAEFFECTPASS_H_
#define _COLLADAFX_COLLADAEFFECTPASS_H_

// forward declarations
class ColladaEffectParameterCollection;
class ColladaEffectParameterCollectionClassDesc;
class DlgTemplateBuilder;
class ColladaEffectTechnique;
class ColladaEffectShader;

#define COLLADA_EFFECT_PASS_ID Class_ID(0x2b1f0f02, 0x7221419f)

// handy methods and macros when it comes to debugging Cg calls
#define CG_FAIL_ON_ERROR 1
extern void _DebugCG(const char* file, int line);
extern void _DebugCGC(CGcontext context, const char* file, int line);
#define DebugCG() _DebugCG(__FILE__,__LINE__)
#define DebugCGC(ctx) _DebugCGC(ctx,__FILE__,__LINE__);


/**	Manages fragment and vertex shaders, as well as render states.
	It currently supports nVidia Cg only.
*/
class ColladaEffectPass : public ReferenceTarget, public PostLoadCallback
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
		param_vs,
		param_fs,
		param_effect,
		param_parameters,
	};

private:
	IParamBlock2* mParamBlock;

	bool mIsValid;
	fstring mErrorString;

	// Cg specific
	CGcontext mCgContext;

	ColladaEffectParameterCollection *mCollection;

	// used in begin() / end()
	ID3D9GraphicsWindow *mDXWindow;
	INode *mINode;

private:
	void destroyShader(ParamID which);
	void setShader(ParamID which, ColladaEffectShader* shader);
	void newParameterCollection();

public:
	ColladaEffectPass(BOOL isLoading, ColladaEffectTechnique* parentP = NULL, const fstring& nameP = FS(""));
	~ColladaEffectPass();
	virtual void DeleteThis() { delete this; }

	ColladaEffectTechnique* getParent();
	void setParent(ColladaEffectTechnique* parent);

	fstring getName();
	bool setName(const fstring& nameP);

	bool isEffect();
	ColladaEffectShader* getShader(ParamID which);

	bool isValid() const { return mIsValid; }
	const fstring& getErrorString() const { return mErrorString; }

	CGcontext getCgContext(){ return mCgContext; }

	bool loadShader(bool isVertex, const fstring& fileName, const fstring& entry);
	bool loadEffect(const fstring& fileName, uint32 techTarget, uint32 passTarget);
	bool addParameter(ColladaEffectParameter& paramP);

	void begin(ID3D9GraphicsWindow *pWindow, INode* pINode);
	ID3D9GraphicsWindow* getGraphicsWindow(){ return mDXWindow; }
	INode* getObjectNode(){ return mINode; }
	void end(){ mDXWindow = NULL; mINode = NULL; }

	// retrieve the dynamic parameter collection
	ColladaEffectParameterCollection* getParameterCollection();
	void setParameterCollection(ColladaEffectParameterCollection *collection);

	// called when D3D device changes
	void releaseTextureParameters();
	void recreateTextureParameters(IDirect3DDevice9* pNewDevice);

	uint16 GetNextDialogID();

	// ReferenceTarget implementation
	Class_ID ClassID() { return COLLADA_EFFECT_PASS_ID; }
	SClass_ID SuperClassID() { return REF_TARGET_CLASS_ID; }
	int NumRefs() { return 1; }
	RefTargetHandle GetReference(int i) { FUAssert(i == 0,return NULL); return mParamBlock; }
	void SetReference(int i, RefTargetHandle rtarg) { FUAssert(i == 0,return); mParamBlock = (IParamBlock2*)rtarg; }
	RefResult NotifyRefChanged(Interval, RefTargetHandle, PartID&, RefMessage){return REF_SUCCEED;}

	// PostLoadCallback implementation
	void proc(ILoad* iload);

	// override from ReferenceMaker
	IOResult Load(ILoad* iload);
};

ClassDesc2* GetColladaEffectPassClassDesc();

#define COLLADA_EFFECT_PARAMETER_COLLECTION_ID Class_ID(0x64d47b11, 0x785d4ddc)

/**
An helper class that takes care of creating dynamic parameter blocks to hold
the collection of parameters held in each ColladaEffectPass.
*/
class ColladaEffectParameterCollection : public CustAttrib
{
private:
	IParamBlock2* mParamBlock2;
	ParamBlockDesc2* mParamBlockDesc2;
	uint32 mBlockID;
	fstring mDialogName;
	DLGTEMPLATE* mDialogTemplate;

	// References Management
	PointerList mExtraReferences;

	uint16 mNextDialogID;
	static const uint16 FIRST_DIALOG_ID;

	// transient parameter list (recreated at load time)
	ColladaEffectParameterList mParameters;

public:
	enum ref_ids
	{
		ref_pblock,
		num_static_refs,
		ref_extra = num_static_refs
	};

	enum RegistrationCode
	{
		INVALID_CALL,		// an unknown parameter was trying to get registered on a fixed collection
		PARAM_ADDED,		// the new parameter has been added
		PARAM_FOUND,		// the parameter already had an equivalent in the collection
	};

public:
	ColladaEffectParameterCollection();
	virtual ~ColladaEffectParameterCollection();

	inline uint16 GetNextDialogID(){ return mNextDialogID++; }

	// dynamic parameter blocks
	ParamBlockDesc2* GetColladaFXBlockDesc();
	void ReleaseColladaFXBlockDesc();
	IParamBlock2* CreateParamBlock();
	inline void SetBlockID(uint32 blockID) { mBlockID = blockID; }

	bool isFixed() const { return mParamBlock2 != NULL; }
	bool registerParameter(ColladaEffectParameter& paramP);

	ColladaEffectParameterList& getParameters(){ return mParameters; }
	ColladaEffectParameter* findParameter(const fstring& name);

	// dialog template creation
	inline void SetName(const fstring& name) { mDialogName = name; }
	DLGTEMPLATE* GetDialogTemplate();

	// main CustAttrib method
	virtual ParamDlg* CreateParamDlg(HWND hwMtlEdit, IMtlParams*  imp);

	// Retrieves the identification information for this plug-in class.
	SClass_ID SuperClassID() { return CUST_ATTRIB_CLASS_ID; }
	Class_ID ClassID() { return COLLADA_EFFECT_PARAMETER_COLLECTION_ID; }
	virtual TCHAR* GetName() { return const_cast<TCHAR*>(mDialogName.c_str()); }

	// from ReferenceMaker
	virtual RefResult NotifyRefChanged(Interval UNUSED(changeInt), RefTargetHandle UNUSED(hTarget), PartID& UNUSED(partID),  RefMessage UNUSED(message)) { return REF_SUCCEED; }
	virtual ReferenceTarget* Clone(RemapDir &remap);
	virtual void DeleteThis();

	// ReferenceTarget interface implementation: give access to one reference: a paramblock.
	virtual int	NumParamBlocks();
	virtual IParamBlock2* GetParamBlock(int i);
	virtual IParamBlock2* GetParamBlockByID(BlockID id);
	virtual int NumRefs();
	virtual RefTargetHandle GetReference(int i);
	virtual void SetReference(int i, RefTargetHandle rtarg);
	virtual	int NumSubs();
	virtual	Animatable* SubAnim(int i);
	virtual TSTR SubAnimName(int i);

private:
	bool GenerateDialogTemplate();
	void GenerateDialogItemTemplate(DlgTemplateBuilder& tb, WORD& controlCount, const ParamDef& definition, uint32 parameterIndex);
};

class ColladaEffectParameterCollectionClassDesc : public ClassDesc2
{
private:
	uint16 freeBlockId;

public:
	ColladaEffectParameterCollectionClassDesc() : freeBlockId(0) {}
	virtual ~ColladaEffectParameterCollectionClassDesc() {};

	int 			IsPublic() {return TRUE;}
	void *			Create(BOOL UNUSED(isLoading = FALSE)) { return new ColladaEffectParameterCollection(); }
	const TCHAR *	ClassName() { return FC("ColladaEffectParameterCollection"); }
	SClass_ID		SuperClassID() {return CUST_ATTRIB_CLASS_ID;}
	Class_ID 		ClassID() {return COLLADA_EFFECT_PARAMETER_COLLECTION_ID;}
	const TCHAR* 	Category() {return _T("");}
	const TCHAR*	InternalName() { return _T("ColladaEffectParameterCollection"); }
	HINSTANCE		HInstance() { return hInstance; }

	// Load/Save our pbdescriptions, since they are scene-specific
	virtual BOOL NeedsToSave() { return TRUE; }
	virtual IOResult Save(ISave *is);
	virtual IOResult Load(ILoad *il);

	// Get the next available pb id
	uint16 GetFreeParamBlockId() { return freeBlockId++; }
	// Set an id that is taken (on file loading)
	void SetTakenId(uint16 id) { if (freeBlockId <= id) freeBlockId = id+1; }
private:
	IOResult LoadPBlockDesc(ILoad* iLoad);
};

ClassDesc2* GetColladaEffectParameterCollectionClassDesc();

#endif // _COLLADAFX_COLLADAEFFECTPASS_H_
