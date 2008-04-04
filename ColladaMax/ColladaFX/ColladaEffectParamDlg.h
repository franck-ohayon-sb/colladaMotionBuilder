/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COLLADAFX_COLLADA_EFFECT_PARAMDLG_H_
#define _COLLADAFX_COLLADA_EFFECT_PARAMDLG_H_

/**
A class to handle Collada Effect UI.
*/
class ColladaEffectParamDlg : public ParamDlg
{
private:
	ColladaEffect* mColladaEffect;

	TimeValue mCurrentTime;
	HWND mMtlEditorHandle;
	IMtlParams* mMtlParams;

	HWND mPanelTechniques;
	HWND mPanelPasses;
	HWND mPanelRenderStates;
	HWND mPanelShaders;
	HWND mPanelParameters;

private:
	void UpdateTechniqueNames(bool descend = true);
	void UpdatePassNames(bool descend = true);
	void UpdateShaders();
	void FillRenderStates();
	void UpdateErrorMessage(const fstring& errMsg);

	void EnablePassPanel(bool enable);
	void EnableShaderPanel(bool enable);
	void EnableRenderStatePanel(bool enable);

	void CreateErrorRollup(const fstring& errMsg);

public:

	ColladaEffectParamDlg(HWND hwMtlEdit, IMtlParams *imp, ColladaEffect *m);
	virtual ~ColladaEffectParamDlg();

	// the following methods are rollup specific
	// each time you add a new rollup, add a new case statement and its
	// corresponding Notify* method.
	void setPanel(int which, HWND pnl)
	{
		switch (which)
		{
		case 0: mPanelTechniques = pnl; break;
		case 1: mPanelPasses = pnl; break;
		case 2: mPanelRenderStates = pnl; break;
		case 3: mPanelShaders = pnl; break;
		case 4: mPanelParameters = pnl; break;
		default: FUFail(break);
		}
	}

	// MFC callbacks
	template<int _I>
	INT_PTR PanelProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (_I)
		{
		case 0: return NotifyTechniques(hDlg,message,wParam,lParam);
		case 1: return NotifyPasses(hDlg,message,wParam,lParam);
		case 2: return NotifyRenderStates(hDlg,message,wParam,lParam);
		case 3: return NotifyShaders(hDlg,message,wParam,lParam);
		case 4: return NotifyErrors(hDlg,message,wParam,lParam);
		default: FUFail(break);
		}
		return 0;
	}

	INT_PTR NotifyTechniques(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	INT_PTR NotifyPasses(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	INT_PTR NotifyRenderStates(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	INT_PTR NotifyShaders(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	INT_PTR NotifyErrors(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

	bool LoadShader(bool isVertex, TCHAR* fileName, TCHAR* entry = NULL);

	// stuff parameters into dialog
	void LoadDialog(BOOL draw);

	// from ParamDlg
	virtual Class_ID ClassID() { return COLLADA_EFFECT_ID; }
	virtual void SetThing(ReferenceTarget* m);
	virtual ReferenceTarget* GetThing(){ return static_cast<ReferenceTarget*>(mColladaEffect); }
	virtual void SetTime(TimeValue t);
	virtual void ReloadDialog();
	virtual void DeleteThis() { delete this; }
	virtual void ActivateDlg(BOOL UNUSED(onOff)){}

private:
	// effect targets query
	StringList mTechNames, mPassNames;
	UInt32List mPassCounts;
	fstring mErrorString;
	uint32 mTechTarget, mPassTarget;
	bool QueryEffectTargets(const fstring& effectFile);
	INT_PTR EffectTargetsDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK EffectTargetsDlgProcS(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);	
};

#endif // _COLLADAFX_COLLADA_EFFECT_PARAMDLG_H_
