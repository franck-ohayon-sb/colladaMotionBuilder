/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "SimpleAttrib.h"
#include "DlgTemplateBuilder.h"
#include <custcont.h>
#include <stdlib.h>

//
// SimpleAttrib
//

static SimpleAttribClassDesc description;

SimpleAttrib::SimpleAttrib()
:	pblock(NULL), dialogTemplate(NULL), pnames(NULL)
,	pBlockDescId(0), name("Empty")
{
}

SimpleAttrib::~SimpleAttrib()
{
//	[GLaforte 26-09-2006] Leak this for now: seems to avoid crashing 3dsMax on close.
	pnames = NULL;
	SAFE_DELETE_ARRAY(dialogTemplate);
	extraReferences.clear();
	
	if (pBlockDescId != 0)
	{
		ParamBlockDesc2* desc = description.GetParamBlockDescByID(pBlockDescId);

		// Reduce the reference count on the param block descriptor.
		if (desc != NULL)
		{
			desc->ref_no--;
			if (desc->ref_no == 0)
			{
				// Remove the pbDescriptor from our classDesc
				int pbCount = description.NumParamBlockDescs();
				fm::pvector<ParamBlockDesc2> allPblockDescs;
				allPblockDescs.reserve(pbCount);
				for (int i = 0; i < pbCount; ++i)
				{
					ParamBlockDesc2* pd = description.GetParamBlockDesc(i);
					if (pd != desc) allPblockDescs.push_back(pd);
				}
				FUAssert(allPblockDescs.size() == (size_t)(pbCount - 1), return);

				// Now, reset our class descriptors parameter blocks
				description.ClearParamBlockDescs();
				for (ParamBlockDesc2** it = allPblockDescs.begin(); it != allPblockDescs.end(); ++it)
				{
					description.AddParamBlockDesc(*it);
				}

				// Release the descriptor entirely
				SAFE_DELETE(desc);
			}
		}
	}

	DeleteAllRefs();
}

void SimpleAttrib::DeleteThis()
{
	delete this;
}

IOResult SimpleAttrib::Save(ISave *)
{
	// If we are saving, register so that the ClassDesc saves our parameter block desc
	FUAssert(pBlockDescId != 0, return IO_ERROR);
	ParamBlockDesc2* classDesc = description.GetParamBlockDescByID(pBlockDescId);
	description.RegisterSaveDesc(classDesc);
	
	return IO_OK;
}

ParamBlockDesc2* SimpleAttrib::GetColladaBlockDesc(TCHAR* name)
{
	ParamBlockDesc2* pbDesc;
	if (pBlockDescId != 0) pbDesc = description.GetParamBlockDescByID(pBlockDescId);
	else
	{
		pBlockDescId = description.GetFreeParamBlockId();
		pbDesc = new ParamBlockDesc2(pBlockDescId, name, 0, &description, P_TEMPLATE_UI, end);
		pbDesc->ref_no = 1;
	}

	return pbDesc;
}

void SimpleAttrib::CreateParamBlock()
{
	if (pBlockDescId != 0)
	{
		ParamBlockDesc2* pbDesc = description.GetParamBlockDescByID(pBlockDescId);
		IParamBlock2* paramBlock = CreateParameterBlock2(pbDesc, this);
		ReplaceReference(ref_pblock, paramBlock);
	}
}

#pragma warning (disable : 4018) // signed/unsigned mismatch
#pragma warning (disable : 4244) // possible loss of data in implicit type conversion

// Dialog Template UI
static uint32 lineHeight = 11;
static uint32 lineWidth = 108;
static uint32 leftSectionLeft = 2;
static uint32 leftSectionRight = 59;
static uint32 rightSectionLeft = 62;
static uint32 rightSectionRight = 105;
static uint32 rightSectionWidth = rightSectionRight - rightSectionLeft;

void SimpleAttrib::GenerateDialogTemplate(uint32 baseWidth)
{
	SAFE_DELETE_ARRAY(dialogTemplate);

	// Scale the width of the dialog template to create.
	lineWidth = baseWidth;
	leftSectionRight = baseWidth * 59 / 108;
	rightSectionLeft = baseWidth * 62 / 108;
	rightSectionRight = baseWidth - 3;
	rightSectionWidth = rightSectionRight - rightSectionLeft;

	// Create a huge memory buffer to generate the dialog template that 3dsMax craves.
	// Once we have generated the dialog template, we will relocate it to a small enough buffer.
	DlgTemplateBuilder tb(16*1024);

	// Generate the dialog template.
	DLGTEMPLATE& dialog = tb.Append<DLGTEMPLATE>();
	dialog.style = WS_CHILD | WS_VISIBLE | DS_SETFONT;
	dialog.cx = lineWidth;
	int paramCount = pblock->NumParams();
	dialog.cy = paramCount * lineHeight + 4;

	UNUSED(uint16& menuIdentifier =) tb.Append<uint16>(); // Skip the menu identifier.
	UNUSED(uint16& windowClassName =) tb.Append<uint16>(); // Skip the window class name.
	UNUSED(uint16& windowTitleName =) tb.Append<uint16>(); // Skip the window title name.

	// [DS_SETFONT]
	tb.Append((uint16) 8); // font size
	tb.AppendWString("MS Sans Serif"); // font typename

	// Process the param block items into dialog controls
	for (int p = 0; p < paramCount; ++p)
	{
		ParamID pid = pblock->IndextoID(p);
		ParamDef& definition = pblock->GetParamDef(pid);
		GenerateDialogItemTemplate(tb, dialog.cdit, definition, pid);
	}

	// Scale down the buffer in order to save memory.
	dialogTemplate = tb.buildTemplate();
}

void SimpleAttrib::GenerateDialogItemTemplate(DlgTemplateBuilder& tb, WORD& controlCount, const ParamDef& definition, uint32 parameterIndex)
{
	// Create a label for the parameter control
	tb.Align();
	DLGITEMTEMPLATE& label = tb.Append<DLGITEMTEMPLATE>();
	controlCount++;
	label.x = leftSectionLeft;
	label.y = parameterIndex * lineHeight + 4;
	label.id = 1024 + parameterIndex;
	label.cx = leftSectionRight - leftSectionLeft;
	label.cy = lineHeight - 2;
	label.style = WS_CHILD | WS_VISIBLE;
	label.dwExtendedStyle = WS_EX_RIGHT;

	tb.Append((uint32) 0x0082FFFF); // STATIC window class
	tb.AppendWString(definition.int_name); // Window text
	tb.Append((uint16) 0); // extra bytes count

	// Create the correct control for this parameter, according to its type.
	// IMPORTANT: The control IDs are set in the EntityImporter class, and we need to match this exactly!
	switch ((int) definition.type)
	{
	case TYPE_FLOAT: {
	case TYPE_INT:
		FUAssert(definition.ctrl_count == 2, break);
		uint32 breakX = rightSectionRight - 8;

		// CUSTEDITWINDOWCLASS
		tb.Align();
		DLGITEMTEMPLATE& custEdit = tb.Append<DLGITEMTEMPLATE>();
		controlCount++;
		custEdit.x = rightSectionLeft;
		custEdit.y = label.y;
		custEdit.id = definition.ctrl_IDs[0];
		custEdit.cx = breakX - rightSectionLeft;
		custEdit.cy = lineHeight - 1;
		custEdit.style = WS_CHILD | WS_VISIBLE;
		custEdit.dwExtendedStyle = WS_EX_RIGHT;

		tb.AppendWString(CUSTEDITWINDOWCLASS); // Window class
		tb.AppendWString(""); // Window text
		tb.Append((uint16) 0); // extra bytes count

		// SPINNERWINDOWCLASS
		tb.Align();
		DLGITEMTEMPLATE& spinner = tb.Append<DLGITEMTEMPLATE>();
		controlCount++;
		spinner.x = breakX + 1;
		spinner.y = label.y;
		spinner.id = definition.ctrl_IDs[1];
		spinner.cx = rightSectionRight - breakX - 1;
		spinner.cy = lineHeight - 1;
		spinner.style = WS_CHILD | WS_VISIBLE;
		spinner.dwExtendedStyle = WS_EX_RIGHT;

		tb.AppendWString(SPINNERWINDOWCLASS); // Window class
		tb.AppendWString(""); // Window text
		tb.Append((uint16) 0); // extra bytes count

		break; }

	case TYPE_POINT3:
	case TYPE_FRGBA:
	case TYPE_RGBA: {
		FUAssert(definition.ctrl_count == 1, break);

		// COLORSWATCHWINDOWCLASS
		tb.Align();
		DLGITEMTEMPLATE& colorSwatch = tb.Append<DLGITEMTEMPLATE>();
		controlCount++;
		colorSwatch.x = rightSectionLeft + 1;
		colorSwatch.y = label.y;
		colorSwatch.id = definition.ctrl_IDs[0];
		colorSwatch.cx = rightSectionWidth - 10;
		colorSwatch.cy = lineHeight - 1;
		colorSwatch.style = WS_CHILD | WS_VISIBLE;
		colorSwatch.dwExtendedStyle = WS_EX_RIGHT;

		tb.AppendWString(COLORSWATCHWINDOWCLASS); // Window class
		tb.AppendWString(""); // Window text
		tb.Append((uint16) 0); // extra bytes count

		break; }

	case TYPE_BOOL: {
		FUAssert(definition.ctrl_count == 1, break);

		// CUSTBUTTONWINDOWCLASS
		tb.Align();
		DLGITEMTEMPLATE& colorSwatch = tb.Append<DLGITEMTEMPLATE>();
		controlCount++;
		colorSwatch.x = rightSectionLeft + 1;
		colorSwatch.y = label.y + 1;
		colorSwatch.id = definition.ctrl_IDs[0];
		colorSwatch.cx = lineHeight - 3;
		colorSwatch.cy = lineHeight - 3;
		colorSwatch.style = WS_CHILD | WS_VISIBLE | BS_CHECKBOX;
		colorSwatch.dwExtendedStyle = WS_EX_RIGHT;

		tb.AppendWString(CUSTBUTTONWINDOWCLASS); // Window class
		tb.AppendWString(""); // Window text
		tb.Append((uint16) 0); // extra bytes count

		break; }

	case TYPE_STRING: {
		FUAssert(definition.ctrl_count == 1, break);

		// CUSTEDITWINDOWCLASS
		tb.Align();
		DLGITEMTEMPLATE& colorSwatch = tb.Append<DLGITEMTEMPLATE>();
		controlCount++;
		colorSwatch.x = rightSectionLeft;
		colorSwatch.y = label.y;
		colorSwatch.id = definition.ctrl_IDs[0];
		colorSwatch.cx = rightSectionWidth;
		colorSwatch.cy = lineHeight - 1;
		colorSwatch.style = WS_CHILD | WS_VISIBLE;
		colorSwatch.dwExtendedStyle = WS_EX_LEFT;

		// For the string type: set the initial text correctly.
		TCHAR* paramValue = NULL; Interval dummy;
		pblock->GetValue(parameterIndex, TIME_INITIAL_POSE, paramValue, dummy);
		tb.AppendWString(CUSTEDITWINDOWCLASS); // Window class
		tb.AppendWString(paramValue != NULL ? paramValue : ""); // Window text
		tb.Append((uint16) 0); // extra bytes count
		break; }

	case TYPE_MATRIX3: break; // no ui...
	}
}

void SimpleAttrib::InvalidateUI()
{
	// Clean-up the current dialog template to force a re-creation of it when next used.
	SAFE_DELETE_ARRAY(dialogTemplate);
}

// UI creation/destruction interface

// Create a parameter map for the material editor.
ParamDlg* SimpleAttrib::CreateParamDlg(HWND hwMtlEdit, IMtlParams *imp)
{
	if (dialogTemplate == NULL) GenerateDialogTemplate(216);

	IAutoMParamDlg* dlgMaker = CreateAutoMParamDlg(0, hwMtlEdit, imp, this, pblock, GetSimpleAttribDesc(), hInstance, dialogTemplate, GetName(), 0);
	return dlgMaker;
}

// Create a parameter map for the object modification tab.
void SimpleAttrib::BeginEditParams(IObjParam* ip, ULONG flags, Animatable* UNUSED(prev))
{
	if (dialogTemplate == NULL) GenerateDialogTemplate(108);
	flags;

	CreateCPParamMap2(0, pblock, ip, hInstance, dialogTemplate, GetName(), 0, NULL);
}

// Destroy a parameter map of the object modification tab.
void SimpleAttrib::EndEditParams(IObjParam* UNUSED(ip), ULONG UNUSED(flags), Animatable* UNUSED(next))
{
	IParamMap2* pmap = pblock->GetMap(0);
	if (pmap != NULL)
	{
		DestroyCPParamMap2(pmap);
	}
}

// Clone it!
ReferenceTarget* SimpleAttrib::Clone(RemapDir &remap)
{
	SimpleAttrib* pnew = new SimpleAttrib;
	pnew->ReplaceReference(0, remap.CloneRef(pblock));
	BaseClone(this, pnew, remap);
	return pnew;
}

int	SimpleAttrib::NumParamBlocks()
{
	return 1;
}

IParamBlock2* SimpleAttrib::GetParamBlock(int i)
{
	return (i == ref_pblock) ? pblock : NULL;
}

IParamBlock2* SimpleAttrib::GetParamBlockByID(BlockID id)
{
	return (pBlockDescId == id) ? pblock : NULL;
}

int SimpleAttrib::NumRefs()
{
	// Always return 1 more than we actually reference.  When
	// setting references Max first checks that we have enough
	// 'reference slots'. (when setting extra references, 
	// we need to have the extra slots visible to max before they
	// are set).
	return num_static_refs + (int) extraReferences.size() + 1;
}

RefTargetHandle SimpleAttrib::GetReference(int i)
{
	if (i == ref_pblock) return pblock;
	else if (i - num_static_refs < extraReferences.size()) return extraReferences.at(i - num_static_refs);
	else return NULL;
}

void SimpleAttrib::SetReference(int i, RefTargetHandle rtarg)
{ 
	if (i == ref_pblock) 
	{
		pblock = (IParamBlock2*) rtarg;
		if (pblock != NULL)
		{
			// Verify the integrity of the ClassDesc.
			if (pBlockDescId == 0) // issed by a load?
			{
				short descId = pblock->GetDesc()->ID;
				if (description.GetParamBlockDescByID(descId) == pblock->GetDesc())
				{
					pBlockDescId = descId;
					pblock->GetDesc()->ref_no++;
				}
				else
				{
					// REFUSE the reference.
					// This implies broken ref-counting or some other programmer error.
					pblock = NULL; 
				}
			}
			else
			{
				FUAssert(pBlockDescId == pblock->GetDesc()->ID, "");
			}
		}
	}
	else 
	{
		if (i - num_static_refs >= extraReferences.size()) extraReferences.resize(i);
		extraReferences.at(i - num_static_refs) = rtarg;
	}
}

int SimpleAttrib::NumSubs()
{
	return 1; 
}

Animatable* SimpleAttrib::SubAnim(int i)
{
	i = i;
	return pblock; 
}

TSTR SimpleAttrib::SubAnimName(int UNUSED(i))
{
	return "SimpleAttrib";
}

//
// SimpleAttribClassDesc
//

ClassDesc2* GetSimpleAttribDesc()
{
	return &description; 
}

SimpleAttribClassDesc::SimpleAttribClassDesc() 
: ClassDesc2()
, freeBlockId(1), isXRef(false)
{
	RegisterNotification(&SimpleAttribClassDesc::PostSaveNotify, this, NOTIFY_FILE_POST_SAVE);
}

SimpleAttribClassDesc::~SimpleAttribClassDesc()
{
	UnRegisterNotification(&SimpleAttribClassDesc::PostSaveNotify, this, NOTIFY_FILE_POST_SAVE);
}

void SimpleAttribClassDesc::PostSaveNotify(void* param, NotifyInfo* UNUSED(info))
{
	FUAssert(param != NULL, return);
	((SimpleAttribClassDesc*)param)->ClearAllSaveDesc();
	((SimpleAttribClassDesc*)param)->IsXRefSave(false);
}


#define PB_CHUNK			0x1101
#define PB_HEADER_CHUNK		0x1102
//#define PB_INT_NAME			0x1103
#define PB_PARAMS_CHUNK		0x1103
#define PB_PARAM_DESC_CHUNK	0x1104

const size_t bufferSize = 1024;
#define CHECK_IOR(command) { IOResult ior = command; FUAssert(ior == IO_OK, return ior); }
#define WRITE_STR(str) { \
	uint32 size = min((uint32) (bufferSize - 1), (uint32) strlen(str)); \
	CHECK_IOR(iSave->Write(&size, sizeof(uint32), &dummy)); \
	CHECK_IOR(iSave->Write(str, size, &dummy)); \
	CHECK_IOR(iSave->Write(&(size = 0), sizeof(uint8), &dummy)); }
/*
#define READ_STR(str) { \
	uint32 size; CHECK_IOR(iLoad->Read(&size, sizeof(uint32), &dummy)); \
	if (size >= bufferSize) { \
		CHECK_IOR(iLoad->Read(str, bufferSize - 1, &dummy)); \
		uint32 leftOver = size - bufferSize + 2; \
		char* dummyB = new char[leftOver]; \
		CHECK_IOR(iLoad->Read(dummyB, leftOver, &dummy)); \
		str[bufferSize - 1] = 0; } \
	else { CHECK_IOR(iLoad->Read(str, size + 1, &dummy)); } }
*/

#define READ_STR(str) { \
	uint32 size; CHECK_IOR(iLoad->Read(&size, sizeof(uint32), &dummy)); \
	FUAssert(size < bufferSize, break) \
	str = new char[size + 1]; \
	CHECK_IOR(iLoad->Read(str, size + 1, &dummy)); }

// Serialization
IOResult SimpleAttribClassDesc::Save(ISave* iSave)
{
	ULONG dummy;
	// Write out the attribute info.
	
	size_t numDesc = 0;

	// It is necessary to know if we are saving for an XRef
	// import here (see XREfManager::InitImport).
	if (isXRef) numDesc = saveBlocks.size();
	else numDesc = NumParamBlockDescs();

	for (size_t i = 0; i < numDesc; i++)
	{
		ParamBlockDesc2* pbDesc = NULL;
		if (!isXRef) pbDesc = GetParamBlockDesc((int)i);
		else pbDesc = saveBlocks[i];
		FUAssert(pbDesc != NULL, continue);

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
		//iSave->BeginChunk(PB_INT_NAME);
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

IOResult SimpleAttribClassDesc::Load(ILoad* iLoad)
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

IOResult SimpleAttribClassDesc::LoadPBlockDesc(ILoad* iLoad)
{
	ULONG dummy;
	ParamBlockDesc2* pbDesc = NULL;
	USHORT paramCount = 0;

	bool skipThis = false;

	while (IO_OK == iLoad->OpenChunk())
	{
		switch (iLoad->CurChunkID())
		{
		case PB_HEADER_CHUNK: 
			{
				FUAssert(pbDesc == NULL, return IO_ERROR);

				// it is very important reads are in the same order as above.
				BlockID id;
				uint32 flags;
				char* name = NULL;
				CHECK_IOR(iLoad->Read(&id, sizeof(BlockID), &dummy));
				CHECK_IOR(iLoad->Read(&flags, sizeof(uint32), &dummy));
				CHECK_IOR(iLoad->Read(&paramCount, sizeof(USHORT), &dummy));
				READ_STR(name);

				// It is possible that this id is already taken.  If this is the 
				// case, then we have to bump over the existing ones.
				// We need the id loaded here to be maintained.
				ParamBlockDesc2* bumpDesc = GetParamBlockDescByID(id);
				if (bumpDesc != NULL) 
				{
					// These ID's are supposed to be 'permanent'.
					// This will allow the scene to load, but it will
					// probably not save & reload correctltly
					bumpDesc->ID = GetFreeParamBlockId();
				
					// new plan - dissallow duplicates.  When loading a saved XRef, 
					// the main file has already saved all the pbdescs - but when
					// loading the xref link, the descs specific to that file
					// are loaded again.  The chances of collisions are now very low 
					skipThis = true;
				}
				else
				{
					skipThis = false;
					pbDesc = new ParamBlockDesc2(id, name, 0, this, flags, end);
					pbDesc->ref_no = 0;
					SetTakenId(id);
				}	
				break; 
			}

		case PB_PARAM_DESC_CHUNK: 
			{
				if (skipThis) break;

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

				//pbDesc->AddParam(pbDesc->count, name, ptype, flags, 0, end);
				
				//pbDesc->paramdefs[pbDesc->count-1].ctrl_count = ctrlCount;
				//pbDesc->paramdefs[pbDesc->count-1].ctrl_IDs = ctrlIds;
				//pbDesc->paramdefs[pbDesc->count-1].ctrl_type = ctrlType;

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

				break; 
			}
		}
		CHECK_IOR(iLoad->CloseChunk());
	}
	FUAssert(skipThis || (pbDesc != NULL && paramCount == pbDesc->count), return IO_ERROR);

	return IO_OK;
}

#pragma warning (default : 4018)
#pragma warning (default : 4244)
