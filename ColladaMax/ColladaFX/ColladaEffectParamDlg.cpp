/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/


#include "StdAfx.h"

#if (MAX_VERSION_MAJOR >= 9)
#	pragma warning(disable:4245)
#	include <3dsmaxport.h>
#	pragma warning(default:4245)
#else

template<typename Type>
inline Type DLSetWindowLongPtr(HWND hWnd, Type ptr, int n = GWLP_USERDATA)
{
#	if !defined(_WIN64)
   // SetWindowLongPtr() maps to SetWindowLong() in 32 bit land; react accordingly to keep
   // the compiler happy, even with /Wp64.
   return (Type)(static_cast<LONG_PTR>(::SetWindowLongPtr(hWnd, n, (LONG)((LONG_PTR)(ptr)))));
#	else
   return (Type)(static_cast<LONG_PTR>(::SetWindowLongPtr(hWnd, n, (LONG_PTR)(ptr))));
#	endif // _WIN64
}

template<typename DataPtr>
DataPtr DLGetWindowLongPtr(HWND hWnd, int n = GWLP_USERDATA, DataPtr = NULL)
{
   return (DataPtr)(static_cast<LONG_PTR>(::GetWindowLongPtr(hWnd, n)));
}

#endif // MAX_VERSION

#include "ColladaEffect.h"
#include "ColladaEffectPass.h"
#include "ColladaEffectShader.h"
#include "ColladaEffectParameter.h"
#include "ColladaEffectParamDlg.h"

#include "FUtils/FUFileManager.h"

///////////////////////////////////////////////////////////////////////////////
// CALLBACK PROCEDURES
///////////////////////////////////////////////////////////////////////////////

template<int _I>
static INT_PTR CALLBACK PanelDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ColladaEffectParamDlg *theDlg;
	if (msg == WM_INITDIALOG)
	{
		theDlg = (ColladaEffectParamDlg*)lParam;
		theDlg->setPanel(_I,hwndDlg);
		DLSetWindowLongPtr(hwndDlg, lParam);
	}
	else {
		if ((theDlg = DLGetWindowLongPtr<ColladaEffectParamDlg*>(hwndDlg)) == NULL)
			return FALSE;
	}

	int res = theDlg->PanelProc<_I>(hwndDlg,msg,wParam,lParam);
	return res;
}

///////////////////////////////////////////////////////////////////////////////
// ColladaEffectParamDlg class implementation
///////////////////////////////////////////////////////////////////////////////

ColladaEffectParamDlg::ColladaEffectParamDlg(HWND hwMtlEdit, IMtlParams *imp, ColladaEffect *m)
:	mPanelParameters(NULL)
{
	mCurrentTime = imp->GetTime();
	mMtlEditorHandle = hwMtlEdit;
	mMtlParams = imp;
	mColladaEffect = m;

	CreateErrorRollup(FS("No shaders are currently assigned."));

	// create the technique rollup page
	mPanelTechniques = mMtlParams->AddRollupPage(
      hInstance,
      MAKEINTRESOURCE(IDD_COLLADA_EFFECT_TECHNIQUES),
      PanelDlgProc<0>,
      GetString(IDS_CE_TECHNIQUES_PARAMS),
      (LPARAM)this);

	// create the passes rollup page
	mPanelPasses = mMtlParams->AddRollupPage(
      hInstance,
      MAKEINTRESOURCE(IDD_COLLADA_EFFECT_PASSES),
      PanelDlgProc<1>,
      GetString(IDS_CE_PASSES_PARAMS),
      (LPARAM)this);

	// create the shader rollup page
	mPanelShaders = mMtlParams->AddRollupPage(
      hInstance,
      MAKEINTRESOURCE(IDD_COLLADA_EFFECT_SHADERS),
      PanelDlgProc<3>,
      GetString(IDS_CE_SHADERS_PARAMS),
      (LPARAM)this);
}

ColladaEffectParamDlg::~ColladaEffectParamDlg()
{
	mColladaEffect->setParamDlg(NULL);

	DLSetWindowLongPtr(mPanelTechniques, NULL);
	mPanelTechniques =  NULL;

	DLSetWindowLongPtr(mPanelPasses, NULL);
	mPanelPasses =  NULL;

	DLSetWindowLongPtr(mPanelRenderStates, NULL);
	mPanelRenderStates =  NULL;

	//for (int i=0; i<NSUBMTLS; i++) {
	//ReleaseICustButton(iBut[i]);
	//ReleaseICustEdit(iName[i]);
	//ReleaseICustEdit(iIndex[i]);
	//iBut[i] = NULL; 
	//iName[i] = NULL;
	//iIndex[i] = NULL;
	//}
	//DeleteObject(hFont);
	//}
}

void ColladaEffectParamDlg::CreateErrorRollup(const fstring& UNUSED(errMsg))
{
	if (mPanelParameters == NULL)
	{
		// not created yet, ADD the rollup
		mPanelParameters = mMtlParams->AddRollupPage(
			hInstance,
			MAKEINTRESOURCE(IDD_COLLADA_EFFECT_ERROR),
			PanelDlgProc<4>,
			_T("Messages"),
			(LPARAM)this);
	}
}

static bool IsValidName(TCHAR* name)
{
	char c = char((*name) & 0x7F);
	// test for empty string
	if (!c) return false;
	
	// begins with a letter
	if (c < 'A' || (c > 'Z' && c < 'a') || c > 'z') return false;
	++ name;

	// remaining is alpha-numeric
	while(*name)
	{
		c = char((*name) & 0x7F);
		if (c < '0' || (c > '9' && c < 'A') || (c > 'Z' && c < 'a') || c > 'z') return false;
		++ name;
	}
	return true;
}

void ColladaEffectParamDlg::UpdateTechniqueNames(bool descend)
{
	HWND hListBox = GetDlgItem(mPanelTechniques,IDC_LIST_TECHNIQUES);
	SendMessage(hListBox, LB_RESETCONTENT, NULL, NULL);

	size_t techCount = mColladaEffect->getTechniqueCount();
	for (size_t i = 0; i < techCount; ++i)
	{
		if (mColladaEffect->getTechnique(i) == NULL) continue;
		SendMessage(hListBox, LB_ADDSTRING, NULL, (LPARAM)_T(mColladaEffect->getTechnique(i)->getName().c_str()));
		if (mColladaEffect->getTechnique(i) == mColladaEffect->getCurrentTechnique())
			SendMessage(hListBox,LB_SETCURSEL,(WPARAM)i,NULL);
	}

	if (descend)
	{
		EnablePassPanel(mColladaEffect->getCurrentTechnique() != NULL);
		UpdatePassNames();
	}
}

void ColladaEffectParamDlg::UpdatePassNames(bool descend)
{
	HWND hListBox = GetDlgItem(mPanelPasses,IDC_LIST_PASSES);
	SendMessage(hListBox, LB_RESETCONTENT, NULL, NULL);

	ColladaEffectTechnique* tech = mColladaEffect->getCurrentTechnique();
	if (tech == NULL) return;

	size_t passCount = tech->getPassCount();
	for (size_t i = 0; i < passCount; ++i)
	{
		SendMessage(hListBox, LB_ADDSTRING, NULL, (LPARAM)_T(tech->getPass(i)->getName().c_str()));
		if (tech->getPass(i) == tech->getEditedPass())
			SendMessage(hListBox,LB_SETCURSEL,(WPARAM)i,NULL);
	}

	if (descend)
	{
		EnableShaderPanel(tech->getEditedPass() != NULL);
		EnableRenderStatePanel(tech->getEditedPass() != NULL);
		UpdateShaders();
	}
}

void ColladaEffectParamDlg::UpdateShaders()
{
	// clear everything
	HWND vEdit = GetDlgItem(mPanelShaders, IDC_EDIT_S_VERTEX);
	HWND fEdit = GetDlgItem(mPanelShaders, IDC_EDIT_S_FRAGMENT);
	SendMessage(vEdit,WM_SETTEXT, NULL, (LPARAM)_T(""));
	SendMessage(fEdit,WM_SETTEXT, NULL, (LPARAM)_T(""));
	HWND hvEntryEdit = GetDlgItem(mPanelShaders, IDC_EDIT_VENTRY);
	HWND hfEntryEdit = GetDlgItem(mPanelShaders, IDC_EDIT_FENTRY);
	SendMessage(hvEntryEdit, EM_SETREADONLY, TRUE, 0);
	SendMessage(hfEntryEdit, EM_SETREADONLY, TRUE, 0);
	ICustEdit* vEntryEdit = GetICustEdit(hvEntryEdit);
	ICustEdit* fEntryEdit = GetICustEdit(hfEntryEdit);
	vEntryEdit->SetText(_T(""));
	vEntryEdit->Disable();
	fEntryEdit->SetText(_T(""));
	fEntryEdit->Disable();
	HWND fxTech = GetDlgItem(mPanelShaders, IDC_STATIC_CGFXTECH);
	HWND fxPass = GetDlgItem(mPanelShaders, IDC_STATIC_CGFXPASS);
	SendMessage(fxTech,WM_SETTEXT, NULL, (LPARAM)_T("--"));
	SendMessage(fxPass,WM_SETTEXT, NULL, (LPARAM)_T("--"));

	// update if needed
	ColladaEffectTechnique* tech = mColladaEffect->getCurrentTechnique();
	if (tech)
	{
		ColladaEffectPass* pass = tech->getEditedPass();
		if (pass)
		{
			if (pass->isEffect())
			{
				ColladaEffectShader* shader = pass->getShader(ColladaEffectPass::param_effect);
				if (shader)
				{
					SendMessage(vEdit,WM_SETTEXT, NULL, (LPARAM)(TCHAR*)shader->getInfo().filename.c_str());
					SendMessage(fEdit,WM_SETTEXT, NULL, (LPARAM)(TCHAR*)shader->getInfo().filename.c_str());
					vEntryEdit->SetText(_T("VertexProgram"));
					fEntryEdit->SetText(_T("FragmentProgram"));
					SendMessage(fxTech,WM_SETTEXT, NULL, (LPARAM)(TCHAR*)shader->getInfo().techniqueName.c_str());
					SendMessage(fxPass,WM_SETTEXT, NULL, (LPARAM)(TCHAR*)shader->getInfo().passName.c_str());
				}
			}
			else
			{
				ColladaEffectShader* shader = pass->getShader(ColladaEffectPass::param_vs);
				if (shader)
				{
					SendMessage(vEdit,WM_SETTEXT, NULL, (LPARAM)(TCHAR*)shader->getInfo().filename.c_str());
					vEntryEdit->SetText((TCHAR*)shader->getInfo().entry.c_str());
					SendMessage(hvEntryEdit, EM_SETREADONLY, FALSE, 0);
					vEntryEdit->Enable();
				}
				shader = pass->getShader(ColladaEffectPass::param_fs);
				if (shader)
				{
					SendMessage(fEdit,WM_SETTEXT, NULL, (LPARAM)(TCHAR*)shader->getInfo().filename.c_str());
					fEntryEdit->SetText((TCHAR*)shader->getInfo().entry.c_str());
					SendMessage(hfEntryEdit, EM_SETREADONLY, FALSE, 0);
					fEntryEdit->Enable();
				}
			}

			if (!pass->isValid())
			{
				UpdateErrorMessage(FS("Invalid shaders in this pass.\n") + pass->getErrorString());
			}
		}
		else
			UpdateErrorMessage(FS("No pass available."));
	}
	else
		UpdateErrorMessage(FS("No technique available."));

	// release Max custom controls
	ReleaseICustEdit(vEntryEdit);
	ReleaseICustEdit(fEntryEdit);
}

void ColladaEffectParamDlg::FillRenderStates()
{
	// TODO.
}

static void EnableDlgControl(HWND window, uint32 controlId, BOOL enabled)
{
	HWND control = GetDlgItem(window, controlId);
	if (control != NULL) EnableWindow(control, enabled);
}

void ColladaEffectParamDlg::EnablePassPanel(bool enable)
{
	EnableDlgControl(mPanelPasses, IDC_LIST_PASSES, enable);
	EnableDlgControl(mPanelPasses, IDC_BUTTON_P_ADD, enable);
	EnableDlgControl(mPanelPasses, IDC_BUTTON_P_REP, enable);
	EnableDlgControl(mPanelPasses, IDC_BUTTON_P_REM, enable);
	EnableDlgControl(mPanelPasses, IDC_EDIT_P_NAME, enable);
}

void ColladaEffectParamDlg::EnableShaderPanel(bool enable)
{
	EnableDlgControl(mPanelShaders, IDC_BUTTON_S_LOADVERTEX, enable);
	EnableDlgControl(mPanelShaders, IDC_BUTTON_S_LOADFRAGMENT, enable);
	EnableDlgControl(mPanelShaders, IDC_BUTTON_VRELOAD, enable);
	EnableDlgControl(mPanelShaders, IDC_BUTTON_FRELOAD, enable);
}

void ColladaEffectParamDlg::EnableRenderStatePanel(bool enable)
{
	EnableDlgControl(mPanelPasses, IDC_LIST_RS, enable);
	EnableDlgControl(mPanelPasses, IDC_BUTTON_RS_ADD, enable);
	EnableDlgControl(mPanelPasses, IDC_BUTTON_RS_REM, enable);
}

static void DisplayInvalidNameError(HWND owner)
{
	MessageBox(owner, 
		_T("Names must be non-empty and unique.\nThey must start with a letter and contain alpha-numeric characters only.\nE.g.: \"technique0\" or \"pass0\"."), 
		_T("Invalid Name"),
		MB_OK | MB_ICONERROR);
}

INT_PTR ColladaEffectParamDlg::NotifyTechniques(HWND UNUSED(hDlg), UINT message, WPARAM wParam, LPARAM UNUSED(lParam))
{
	int id = LOWORD(wParam);
	int code = HIWORD(wParam);
	const int maxSize = 128;
	TCHAR buffer[maxSize];
	HWND hListBox = GetDlgItem(mPanelTechniques,IDC_LIST_TECHNIQUES);

	ICustEdit* nameCEdit = GetICustEdit(GetDlgItem(mPanelTechniques,IDC_EDIT_T_NAME));
	if (nameCEdit != NULL) nameCEdit->GetText(buffer,maxSize);
	else buffer[0] = 0;

	switch (message)
	{
	case WM_INITDIALOG:
		{
			UpdateTechniqueNames(false);
		}
		return TRUE;

	case WM_COMMAND:  
		switch (id) {
		case IDC_BUTTON_T_ADD:
			{
				if (IsValidName(buffer) && mColladaEffect->addTechnique(FS(buffer)) != NULL)
				{
					UpdateTechniqueNames();
				}
				else
				{
					DisplayInvalidNameError(mPanelTechniques);
				}
			} break;
		case IDC_BUTTON_T_REM:
			{
				HRESULT idx = SendMessage(hListBox, LB_GETCURSEL, NULL, NULL);
				if (idx != LB_ERR)
				{
					mColladaEffect->removeTechnique((size_t)idx);
					UpdateTechniqueNames();
				}
			} break;
		case IDC_BUTTON_T_REP:
			{
				if (IsValidName(buffer))
				{
					HRESULT idx = SendMessage(hListBox, LB_GETCURSEL, NULL, NULL);
					if (idx != LB_ERR)
					{
						ColladaEffectTechnique* tech = mColladaEffect->getTechnique((size_t)idx);
						if (tech != NULL)
						{
							if (!tech->setName(FS(buffer)))
								DisplayInvalidNameError(mPanelTechniques);
							UpdateTechniqueNames();
						}
					}
				}
				else
				{
					DisplayInvalidNameError(mPanelTechniques);
				}
			} break;
		case IDC_LIST_TECHNIQUES:
			{
				switch (code)
				{
				case LBN_SELCHANGE:
					{
						HRESULT idx = SendMessage(hListBox, LB_GETCURSEL, NULL, NULL);
						mColladaEffect->setCurrentTechnique((size_t)idx);
						UpdateTechniqueNames();

						if (mColladaEffect->getCurrentTechnique() != NULL)
							mColladaEffect->reloadUniforms(mColladaEffect->getCurrentTechnique()->getEditedPass());
						else
							mColladaEffect->reloadUniforms(NULL);

						GetCOREInterface()->RedrawViews(GetCOREInterface()->GetTime());
					} break;
				}
			} break;
		}
		break;
	case WM_CLOSE: break;
	case WM_DESTROY: break;
	}

	ReleaseICustEdit(nameCEdit);
   return FALSE;
}

INT_PTR ColladaEffectParamDlg::NotifyPasses(HWND UNUSED(hDlg), UINT message, WPARAM wParam, LPARAM UNUSED(lParam))
{
	int id = LOWORD(wParam);
	int code = HIWORD(wParam);
	const int maxSize = 128;
	TCHAR buffer[maxSize];
	HWND hListBox = GetDlgItem(mPanelPasses,IDC_LIST_PASSES);

	ColladaEffectTechnique* curTech = mColladaEffect->getCurrentTechnique();

	ICustEdit* nameCEdit = GetICustEdit(GetDlgItem(mPanelPasses,IDC_EDIT_P_NAME));
	if (nameCEdit != NULL) nameCEdit->GetText(buffer,maxSize);
	else buffer[0] = 0;

	switch (message)
	{
    case WM_INITDIALOG:
		{
			UpdatePassNames(false);
			EnablePassPanel(mColladaEffect->getCurrentTechnique() != NULL);
		}
		return TRUE;
	case WM_COMMAND:
		switch (id) {
		case IDC_BUTTON_P_ADD:
			{
				if (IsValidName(buffer) && curTech != NULL && curTech->addPass(FS(buffer)) != NULL)
				{
					UpdatePassNames();
				}
				else
				{
					DisplayInvalidNameError(mPanelPasses);
				}
			} break;
	 case IDC_BUTTON_P_REM:
			{
				HRESULT idx = SendMessage(hListBox, LB_GETCURSEL, NULL, NULL);
				if (idx != LB_ERR && curTech != NULL)
				{
					curTech->removePass((size_t)idx);
					UpdatePassNames();
				}
			} break;
	 case IDC_BUTTON_P_REP:
			{
				if (IsValidName(buffer))
				{
					HRESULT idx = SendMessage(hListBox, LB_GETCURSEL, NULL, NULL);
					if (idx != LB_ERR && curTech != NULL)
					{
						ColladaEffectPass* pass = curTech->getPass((size_t)idx);
						if (pass != NULL)
						{
							if (!pass->setName(FS(buffer)))
								DisplayInvalidNameError(mPanelPasses);
							UpdatePassNames();
						}
					}
				}
				else
				{
					DisplayInvalidNameError(mPanelPasses);
				}
			} break;
	 case IDC_LIST_PASSES:
			{
				switch (code)
				{
				case LBN_SELCHANGE:
					{
						HRESULT idx = SendMessage(hListBox, LB_GETCURSEL, NULL, NULL);
						curTech->setEditedPass((size_t)idx);
						UpdatePassNames();
						mColladaEffect->reloadUniforms(curTech->getEditedPass());
					} break;
				}
			} break;
		}
		break;
	case WM_CLOSE: break;
	case WM_DESTROY: break;
	}
	ReleaseICustEdit(nameCEdit);
   return FALSE;
}

INT_PTR ColladaEffectParamDlg::NotifyRenderStates(HWND UNUSED(hDlg), UINT UNUSED(message), WPARAM UNUSED(wParam), LPARAM UNUSED(lParam))
{
	//int id = LOWORD(wParam);
	//int code = HIWORD(wParam);

	//switch (message)    {
 //     case WM_INITDIALOG:   {    
 //        }
 //        return TRUE;
 //        
 //     case WM_COMMAND:  
 //         switch (id) {

 //           }
 //        break;
 //     case WM_CLOSE:
 //        break;       
 //     case WM_DESTROY: 
 //        break;      
 //     }
   return FALSE;
}

INT_PTR ColladaEffectParamDlg::NotifyShaders(HWND UNUSED(hDlg), UINT message, WPARAM wParam, LPARAM UNUSED(lParam))
{
	int id = LOWORD(wParam);
	int code = HIWORD(wParam);

	switch (message)
	{
    case WM_INITDIALOG:
		{
			UpdateShaders();
			bool enabled = (mColladaEffect->getCurrentTechnique() && mColladaEffect->getCurrentTechnique()->getEditedPass());
			EnableShaderPanel(enabled);
		}
		return TRUE;
	case WM_COMMAND:
		{
			switch (id)
			{
			case IDC_BUTTON_S_LOADVERTEX:
			case IDC_BUTTON_S_LOADFRAGMENT:
				{
					if (code == BN_CLICKED)
					{
						TCHAR szFileName[MAX_PATH] = {0};

						static TCHAR* szFilter =
							TEXT("Cg/CgFX files (*.cg, *.cgfx)\0")
							TEXT("*.cg;*.cgfx\0")
							TEXT("All files\0")
							TEXT("*\0");
						static TCHAR* szTitle = _T("Load Cg Shader");

						OPENFILENAME ofn;
						ZeroMemory(&ofn,sizeof(OPENFILENAME));

						ofn.lStructSize = sizeof(OPENFILENAME);
						ofn.hwndOwner = mPanelShaders;
						ofn.lpstrFilter = szFilter;
						ofn.nFilterIndex = 1;
						ofn.lpstrFile = szFileName;
						ofn.nMaxFile = MAX_PATH;
						ofn.lpstrTitle = szTitle;
						ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

						if (GetOpenFileName(&ofn) != 0)
						{
							LoadShader(id == IDC_BUTTON_S_LOADVERTEX, szFileName);
						}
					}
				} break;
			case IDC_BUTTON_VRELOAD:
			case IDC_BUTTON_FRELOAD:
				{
					if (code == BN_CLICKED)
					{
						bool isVertex = (id == IDC_BUTTON_VRELOAD);
						TCHAR entry[256];
						ICustEdit* ce = GetICustEdit(GetDlgItem(mPanelShaders, isVertex ? IDC_EDIT_VENTRY : IDC_EDIT_FENTRY));
						ce->GetText(entry, 256);
						ReleaseICustEdit(ce);
						HWND hfilename = GetDlgItem(mPanelShaders, isVertex ? IDC_EDIT_S_VERTEX : IDC_EDIT_S_FRAGMENT);
						TCHAR filename[MAX_PATH]; SendMessage(hfilename, WM_GETTEXT, (WPARAM)MAX_PATH, (LPARAM)(TCHAR*)filename);
						LoadShader(isVertex, filename, entry);
					}
				} break;
			}
		}
		return TRUE;
	default: break;
	}
	return FALSE;
}

INT_PTR ColladaEffectParamDlg::NotifyErrors(HWND UNUSED(hDlg), UINT message, WPARAM UNUSED(wParam), LPARAM UNUSED(lParam))
{
	UNUSED(int id = LOWORD(wParam));
	UNUSED(int code = HIWORD(wParam));

	switch (message)
	{
    case WM_INITDIALOG:
		{
			UpdateErrorMessage(FS("No errors."));
		}
		return TRUE;
	case WM_COMMAND:
		{
		}
		return TRUE;
	default: break;
	}
	return FALSE;
}

void ColladaEffectParamDlg::UpdateErrorMessage(const fstring& errMsg)
{
	TCHAR* msg = (TCHAR*)errMsg.c_str();
	SetDlgItemText(mPanelParameters, IDC_EDIT_CE_ERROR, msg);
}

bool ColladaEffectParamDlg::LoadShader(bool isVertex, TCHAR* fileName, TCHAR* entry)
{
	FUAssert(mColladaEffect->getCurrentTechnique(), return false);
	ColladaEffectPass* pass = mColladaEffect->getCurrentTechnique()->getEditedPass();
	FUAssert(pass, return false);

	fstring file = FS(fileName);
	bool isCgFX = fstricmp(FUFileManager::GetFileExtension(file),"cgfx") == 0;
	bool isSuccess = false;

	if (isCgFX)
	{
		// choose from which technique/pass
		if (QueryEffectTargets(file))
		{
			isSuccess = pass->loadEffect(file, mTechTarget, mPassTarget);
		}
	}
	else
	{
		// load Cg Shader
		isSuccess = pass->loadShader(isVertex,file, fstring(entry));
	}

	UpdateShaders();

	// load-specific error messages
	if (isSuccess && pass->isValid())
	{
		UpdateErrorMessage(file + FS(" loaded successfully!"));
	}
	else
	{
		UpdateErrorMessage(FS("Errors while loading ") + file + FS("\n") + pass->getErrorString());
	}

	if (isSuccess)
	{
		mColladaEffect->reloadUniforms(pass);	
	}
	else
	{
		mColladaEffect->reloadUniforms(NULL);
	}
	// at this point, you may have been deleted...
	// By changing the content of the custom attrib container, you invalidate the UI.

	GetCOREInterface()->RedrawViews(GetCOREInterface()->GetTime());

	return isSuccess;
}


void ColladaEffectParamDlg::SetThing(ReferenceTarget* m)
{
	FUAssert(m != NULL, return);
	FUAssert(m->ClassID() == COLLADA_EFFECT_ID, return);
	FUAssert(m->SuperClassID() == MATERIAL_CLASS_ID, return);

	if (mColladaEffect != NULL) mColladaEffect->setParamDlg(NULL);
	mColladaEffect = static_cast<ColladaEffect*>(m);
	if (mColladaEffect != NULL) mColladaEffect->setParamDlg(this);
	LoadDialog(TRUE);
}

void ColladaEffectParamDlg::SetTime(TimeValue t)
{
	if (t != mCurrentTime) {
		Interval valid;
		mCurrentTime = t;
		mColladaEffect->Update(mCurrentTime,valid);
		// Since nothing is time varying, can skip this
		//InvalidateRect(hPanelBasic,NULL,0);
	}
}

void ColladaEffectParamDlg::ReloadDialog()
{
	Interval valid;
	mColladaEffect->Update(mCurrentTime,valid);
	LoadDialog(FALSE);
}

void ColladaEffectParamDlg::LoadDialog(BOOL UNUSED(draw))
{
	if (mColladaEffect != NULL)
	{
		Interval valid;
		mColladaEffect->Update(mCurrentTime,valid);
		//UpdateSubMtlNames
	}
}

bool ColladaEffectParamDlg::QueryEffectTargets(const fstring& effectFile)
{
	mTechNames.clear();
	mPassCounts.clear();
	mPassNames.clear();
	mErrorString.clear();
	mTechTarget = 0;
	mPassTarget = 0;

	ColladaEffectShader::GetEffectInfo(effectFile, mTechNames, mPassCounts, mPassNames, mErrorString);
	if (mTechNames.size() == 1 && mPassNames.size() == 1)
	{
		// keep 0,0
		return true;
	}
	else if (mErrorString.empty() && !mTechNames.empty())
	{
		// let the dialog handle the query
		BOOL ok = DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_COLLADA_EFFECT_TARGETS), GetCOREInterface()->GetMAXHWnd(), EffectTargetsDlgProcS, (LPARAM)this) != FALSE;
		return (ok == TRUE);
	}
	else
	{
		// error
		return false;
	}
}

INT_PTR ColladaEffectParamDlg::EffectTargetsDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM UNUSED(lParam))
{
	HWND hTechListBox = GetDlgItem(hWnd,IDC_LIST_TECHNIQUES);
	HWND hPassListBox = GetDlgItem(hWnd,IDC_LIST_PASSES);

	switch (message)
	{
		case WM_INITDIALOG: {
			CenterWindow(hWnd, GetParent(hWnd));  

			// fill the technique names
			for (size_t i = 0; i < mTechNames.size(); ++i)
			{
				SendMessage(hTechListBox, LB_ADDSTRING, NULL, (LPARAM)_T(mTechNames[i].c_str()));
			}
			// select the first technique by default
			SendMessage(hTechListBox,LB_SETCURSEL,(WPARAM)0,NULL);

			// fill the first technique pass names
			uint32 pCount = mPassCounts[0];
			for (uint32 i = 0; i < pCount; ++i)
			{
				SendMessage(hPassListBox, LB_ADDSTRING, NULL, (LPARAM)_T(mPassNames[i].c_str()));
			}
			// select the first pass
			SendMessage(hPassListBox,LB_SETCURSEL,(WPARAM)0,NULL);

			return TRUE;
		}

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDC_LIST_TECHNIQUES:
					switch (HIWORD(wParam))
					{
					case LBN_SELCHANGE:
						{
							size_t curTech = (size_t)SendMessage(hTechListBox, LB_GETCURSEL, NULL, NULL);
							if ((uint32)curTech == mTechTarget) break; // no changes, early exit

							// clear the pass box
							SendMessage(hPassListBox, LB_RESETCONTENT, NULL, NULL);

							size_t passOffset = 0;
							for (size_t i = 0; i < curTech; ++i) passOffset += mPassCounts[i];
							
							// update the pass list box
							for (size_t i = 0; i < (size_t)mPassCounts[curTech]; ++i)
							{
								SendMessage(hPassListBox, LB_ADDSTRING, NULL, (LPARAM)_T(mPassNames[passOffset + i].c_str()));
							}
							// select the first one
							SendMessage(hPassListBox,LB_SETCURSEL,(WPARAM)0,NULL);

							// update targets
							mTechTarget = (uint32)curTech;
							mPassTarget = 0;
						} break;
					}
					break;
				case IDC_LIST_PASSES:
					switch (HIWORD(wParam))
					{
					case LBN_SELCHANGE:
						{
							// update target
							mPassTarget = (uint32)SendMessage(hPassListBox, LB_GETCURSEL, NULL, NULL);
						} break;
					}
					break;
				case IDOK:
					EndDialog(hWnd, 1);
					break;

				case IDCANCEL:
					EndDialog(hWnd, 0);
					break;
			}
		default:
			return FALSE;
	}
	return TRUE;
}

INT_PTR CALLBACK ColladaEffectParamDlg::EffectTargetsDlgProcS(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	ColladaEffectParamDlg* pdlg;
	if (message == WM_INITDIALOG)
	{
		// record exp instance pointer for subsequent callbacks
		pdlg = (ColladaEffectParamDlg*) lParam;
		SetWindowLongPtr(hWnd, GWLP_USERDATA,lParam); 
	}
	else
	{
		pdlg = (ColladaEffectParamDlg*)(size_t) GetWindowLongPtr(hWnd, GWLP_USERDATA);
	}
	// hand off to message-hanlder method
	return pdlg ? pdlg->EffectTargetsDlgProc(hWnd, message, wParam, lParam) : FALSE;
}
