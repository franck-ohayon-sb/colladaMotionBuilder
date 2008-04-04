/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef	__SIMPLE_ATTRIB_H__
#define __SIMPLE_ATTRIB_H__

#define SIMPLE_ATTRIB_CLASS_ID Class_ID(0x5b621f96, 0x4ed628b1)

class SimpleAttribClassDesc;
class DlgTemplateBuilder;

class SimpleAttrib : public CustAttrib
{
private:
	// The name of the dialog UI
	fstring name;

	// Dynamic parameters handling
	IParamBlock2* pblock;
	short pBlockDescId;
	FStringList* pnames;
	DLGTEMPLATE* dialogTemplate;

	// References Management
	PointerList extraReferences;

public:
	enum ref_ids
	{
		ref_pblock,
		num_static_refs,
		ref_extra = num_static_refs
	};

public:
	SimpleAttrib();
	virtual ~SimpleAttrib();

	ParamBlockDesc2* GetColladaBlockDesc(TCHAR* name);
	void CreateParamBlock();

	inline void SetName(const fstring& _name) { name = _name; }
	inline void SetParamNames(FStringList* names) { pnames = names; }

	// Retrieves the identification information for this plug-in class.
	SClass_ID SuperClassID() { return CUST_ATTRIB_CLASS_ID; }
	Class_ID ClassID() { return SIMPLE_ATTRIB_CLASS_ID; }
	virtual TCHAR* GetName() { return const_cast<TCHAR*>(name.c_str()); }

	// Max Interfaces
	virtual RefResult NotifyRefChanged(Interval UNUSED(changeInt), RefTargetHandle UNUSED(hTarget), PartID& UNUSED(partID),  RefMessage UNUSED(message)) { return REF_SUCCEED; }
	virtual ReferenceTarget* Clone(RemapDir &remap);
	virtual bool CheckCopyAttribTo(ICustAttribContainer* UNUSED(to)) { return true; }
	virtual void DeleteThis();

	virtual IOResult Save(ISave *save);

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

	// UI interface.
	virtual ParamDlg *CreateParamDlg(HWND hwMtlEdit, IMtlParams *imp);
	virtual void BeginEditParams(IObjParam *ip,ULONG flags,Animatable *prev);
	virtual void EndEditParams(IObjParam *ip, ULONG flags, Animatable *next);
	virtual void InvalidateUI(); // dirty the UI dialog

private:
	void GenerateDialogTemplate(uint32 baseWidth);
	void GenerateDialogItemTemplate(DlgTemplateBuilder& tb, WORD& controlCount, const ParamDef& definition, uint32 parameterIndex);
};

//
// SimpleAttribClassDesc
//

class SimpleAttribClassDesc : public ClassDesc2
{
private:
	uint16 freeBlockId;
	fm::pvector<ParamBlockDesc2> saveBlocks;
	bool isXRef;
public:
	SimpleAttribClassDesc();
	virtual ~SimpleAttribClassDesc();

	int 			IsPublic() {return 1;}
	void *			Create(BOOL isLoading = FALSE) { isLoading; return new SimpleAttrib(); }
	const TCHAR *	ClassName() { return FC("ColladaSimpleAttrib"); }
	SClass_ID		SuperClassID() {return CUST_ATTRIB_CLASS_ID;}
	Class_ID 		ClassID() {return SIMPLE_ATTRIB_CLASS_ID;}
	const TCHAR* 	Category() {return _T("");}
	const TCHAR*	InternalName() { return _T("SimpleAttrib"); }	// returns fixed parsable name (scripter-visible name)
	HINSTANCE		HInstance() { return hInstance; }			// returns owning module handle

	// Load/Save our pbdescriptions, since they are scene-specific
	virtual BOOL NeedsToSave() { return TRUE; }
	virtual IOResult Save(ISave *is);
	virtual IOResult Load(ILoad *il);

	// Get the next available pb id
	uint16 GetFreeParamBlockId() { return freeBlockId++; }
	// Set an id that is taken (on file loading)
	void SetTakenId(uint16 id) { if (freeBlockId <= id) freeBlockId = id+1; }

	// Only registered pbdescs will save
	// Saving is based on the assumption that our classes will call this.
	void IsXRefSave(bool b) {isXRef = b; }
	void RegisterSaveDesc(ParamBlockDesc2* pbDesc) { FUAssert(!saveBlocks.contains(pbDesc), return); saveBlocks.push_back(pbDesc); };
	void ClearAllSaveDesc() { saveBlocks.clear(); }
	
	// Register post save so we can clear our save lists after serialization.
	static void PostSaveNotify(void *param, NotifyInfo *info);
private:
	IOResult LoadPBlockDesc(ILoad* iLoad);
};

ClassDesc2* GetSimpleAttribDesc();

#endif // __SIMPLE_ATTRIB_H__
