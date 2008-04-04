/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/
/*
	Based on the 3dsMax COLLADA Tools:
	Copyright (C) 2005-2006 Autodesk Media Entertainment
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "ColladaOptions.h"

#pragma warning(disable:4100)
#include <CS/BipedApi.h>
#pragma warning(default:4100)

//
// Macros
//

static void EnableDlgControl(HWND window, uint32 controlId, BOOL enabled)
{
	HWND control = GetDlgItem(window, controlId);
	if (control != NULL) EnableWindow(control, enabled);
}

//
// ColladaOptions
//
ColladaOptions::ColladaOptions(Interface* _maxInterface)
{
	maxInterface = _maxInterface;

	// Set the option values to their default
	normals = true;
	triangulate = true;
	xrefs = true;
	tangents = false;
	animations = true;
	sampleAnim = false;
	createClip = false;
	bakeMatrices = false;
	relativePaths = true;
	importUnits = true;
	importUpAxis = true;

	animStart = TIME_INITIAL_POSE;
	animEnd = TIME_PosInfinity;

	// Load the export options from the configuration file
	LoadOptions();
}

ColladaOptions::~ColladaOptions()
{
	maxInterface = NULL;
}

// Displays the exporter options dialog to allow the user to change the options.
bool ColladaOptions::ShowDialog(bool exporter)
{
	// Prompt the user with our dialogbox, and get all the options.
	if (exporter)
	{
		BOOL doExport = DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_EXPORT_OPTIONS), maxInterface->GetMAXHWnd(), ExportOptionsDlgProcS, (LPARAM)this) != FALSE;
		if (!doExport) return false;
	}
	else
	{
		BOOL doImport = DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_IMPORT_OPTIONS), maxInterface->GetMAXHWnd(), ImportOptionsDlgProcS, (LPARAM)this) != FALSE;
		if (!doImport) return false;
	}

	// Save the export options to the configuration file
	SaveOptions();

	return true;
}

// optins dialog message handler
INT_PTR ColladaOptions::ExportOptionsDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM)
{
	ISpinnerControl* spin;
//	int ticksPerFrame = GetTicksPerFrame();
	Interval animRange = maxInterface->GetAnimRange();
	int sceneStart = animRange.Start();
	int sceneEnd = animRange.End();

	switch (message)
	{
		case WM_INITDIALOG: {
			CenterWindow(hWnd, GetParent(hWnd));  

			// grab scene timing details & clip anim start & end if needed
			if (animStart < sceneStart) animStart = sceneStart;
			if (animStart > sceneEnd) animStart = sceneEnd;
			if (animEnd < sceneStart) animEnd = sceneStart;
			if (animEnd > sceneEnd) animEnd = sceneEnd;

			// setup checkboxes
			CheckDlgButton(hWnd, IDC_GEOM_NORMALS, normals);
			CheckDlgButton(hWnd, IDC_GEOM_TRIANGLES, triangulate);
			CheckDlgButton(hWnd, IDC_GEOM_XREFS, xrefs);
			CheckDlgButton(hWnd, IDC_GEOM_TANGENTS, tangents);
			CheckDlgButton(hWnd, IDC_ANIM_ENABLE, animations);
			CheckDlgButton(hWnd, IDC_ANIM_SAMPLE, sampleAnim);
			CheckDlgButton(hWnd, IDC_ANIM_CLIP, createClip);
			CheckDlgButton(hWnd, IDC_BAKE_MATRICES, bakeMatrices);
			CheckDlgButton(hWnd, IDC_RELATIVE_PATHS, relativePaths);

			// Animation checkboxes depend on the enable button.
			EnableDlgControl(hWnd, IDC_ANIM_SAMPLE, animations);
			EnableDlgControl(hWnd, IDC_ANIM_CLIP, animations);

			// setup spinners
			spin = GetISpinner(GetDlgItem(hWnd, IDC_ANIM_START_SPIN)); 
			spin->LinkToEdit(GetDlgItem(hWnd,IDC_ANIM_START), EDITTYPE_TIME);
			spin->SetLimits(sceneStart, sceneEnd, TRUE); 
			spin->SetScale(1.0f);
			spin->SetValue(animStart, FALSE);
			spin->Enable(sampleAnim && animations);
			EnableWindow(GetDlgItem(hWnd, IDC_START_LABEL), sampleAnim && animations);
			ReleaseISpinner(spin);
						
			spin = GetISpinner(GetDlgItem(hWnd, IDC_ANIM_END_SPIN)); 
			spin->LinkToEdit(GetDlgItem(hWnd,IDC_ANIM_END), EDITTYPE_TIME); 
			spin->SetLimits(sceneStart, sceneEnd, TRUE); 
			spin->SetScale(1.0f);
			spin->SetValue(animEnd, FALSE);
			spin->Enable(sampleAnim && animations);
			EnableWindow(GetDlgItem(hWnd, IDC_END_LABEL), sampleAnim && animations);
			ReleaseISpinner(spin);

			return TRUE;
		}

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDC_ANIM_ENABLE:
					animations = IsDlgButtonChecked(hWnd, IDC_ANIM_ENABLE) == BST_CHECKED;
					EnableDlgControl(hWnd, IDC_ANIM_SAMPLE, animations);
					EnableDlgControl(hWnd, IDC_ANIM_CLIP, animations);

					/*spin = GetISpinner(GetDlgItem(hWnd, IDC_ANIM_START_SPIN)); 
					//spin->LinkToEdit(GetDlgItem(hWnd,IDC_ANIM_START), EDITTYPE_INT);
					spin->Enable(sampleAnim && animations);
					EnableWindow(GetDlgItem(hWnd, IDC_START_LABEL), sampleAnim && animations);
					ReleaseISpinner(spin);

					spin = GetISpinner(GetDlgItem(hWnd, IDC_ANIM_END_SPIN)); 
					//spin->LinkToEdit(GetDlgItem(hWnd,IDC_ANIM_END), EDITTYPE_INT);
					spin->Enable(sampleAnim && animations);
					EnableWindow(GetDlgItem(hWnd, IDC_END_LABEL), sampleAnim && animations);
					ReleaseISpinner(spin);
					break; */

				case IDC_ANIM_SAMPLE:
					sampleAnim = IsDlgButtonChecked(hWnd, IDC_ANIM_SAMPLE) == BST_CHECKED;
					
					spin = GetISpinner(GetDlgItem(hWnd, IDC_ANIM_START_SPIN)); 
					spin->Enable(sampleAnim && animations);
					EnableWindow(GetDlgItem(hWnd, IDC_START_LABEL), sampleAnim && animations);
					ReleaseISpinner(spin);
					
					spin = GetISpinner(GetDlgItem(hWnd, IDC_ANIM_END_SPIN)); 
					spin->Enable(sampleAnim && animations);
					EnableWindow(GetDlgItem(hWnd, IDC_END_LABEL), sampleAnim && animations);
					ReleaseISpinner(spin);
					break;
				
				case IDOK:
					// hit OK, pick up control values & end dialog
					bakeMatrices = IsDlgButtonChecked(hWnd, IDC_BAKE_MATRICES) == BST_CHECKED;
					relativePaths = IsDlgButtonChecked(hWnd, IDC_RELATIVE_PATHS) == BST_CHECKED;
					animations = IsDlgButtonChecked(hWnd, IDC_ANIM_ENABLE) == BST_CHECKED;
					sampleAnim = IsDlgButtonChecked(hWnd, IDC_ANIM_SAMPLE) == BST_CHECKED;
					createClip = IsDlgButtonChecked(hWnd, IDC_ANIM_CLIP) == BST_CHECKED;
					normals = IsDlgButtonChecked(hWnd, IDC_GEOM_NORMALS) == BST_CHECKED;
					triangulate = IsDlgButtonChecked(hWnd, IDC_GEOM_TRIANGLES) == BST_CHECKED;
					xrefs = IsDlgButtonChecked(hWnd, IDC_GEOM_XREFS) == BST_CHECKED;
					tangents = IsDlgButtonChecked(hWnd, IDC_GEOM_TANGENTS) == BST_CHECKED;

					spin = GetISpinner(GetDlgItem(hWnd, IDC_ANIM_START_SPIN)); 
					animStart = sampleAnim ? spin->GetIVal() : sceneStart;
					ReleaseISpinner(spin);
					spin = GetISpinner(GetDlgItem(hWnd, IDC_ANIM_END_SPIN)); 
					animEnd = sampleAnim ? spin->GetIVal() : sceneEnd; 
					ReleaseISpinner(spin);

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

INT_PTR ColladaOptions::ImportOptionsDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM)
{
	switch (message)
	{
		case WM_INITDIALOG: {
			CenterWindow(hWnd, GetParent(hWnd));  

			// setup checkboxes
			CheckDlgButton(hWnd, IDC_IMPORT_UNITS, importUnits);
			CheckDlgButton(hWnd, IDC_IMPORT_UPAXIS, importUpAxis);
			return TRUE;
		}

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					// hit OK, pick up control values & end dialog
					importUnits = IsDlgButtonChecked(hWnd, IDC_IMPORT_UNITS) == BST_CHECKED;
					importUpAxis = IsDlgButtonChecked(hWnd, IDC_IMPORT_UPAXIS) == BST_CHECKED;
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

// options dialog proc
INT_PTR CALLBACK ColladaOptions::ExportOptionsDlgProcS(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) 
{
	ColladaOptions* exp;
	if (message == WM_INITDIALOG)
	{
		// record exp instance pointer for subsequent callbacks
		exp = (ColladaOptions*) lParam;
		SetWindowLongPtr(hWnd, GWLP_USERDATA,lParam); 
	}
	else
	{
		exp = (ColladaOptions*)(size_t) GetWindowLongPtr(hWnd, GWLP_USERDATA);
	}
	// hand off to message-hanlder method
	return exp ? exp->ExportOptionsDlgProc(hWnd, message, wParam, lParam) : FALSE;
}

// options dialog proc
INT_PTR CALLBACK ColladaOptions::ImportOptionsDlgProcS(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) 
{
	ColladaOptions* exp;
	if (message == WM_INITDIALOG)
	{
		// record exp instance pointer for subsequent callbacks
		exp = (ColladaOptions*) lParam;
		SetWindowLongPtr(hWnd, GWLP_USERDATA,lParam); 
	}
	else
	{
		exp = (ColladaOptions*)(size_t) GetWindowLongPtr(hWnd, GWLP_USERDATA);
	}
	// hand off to message-hanlder method
	return exp ? exp->ImportOptionsDlgProc(hWnd, message, wParam, lParam) : FALSE;
}

// Save the export options to a XML file
void ColladaOptions::SaveOptions()
{
	TSTR configurationPath = maxInterface->GetDir(APP_PLUGCFG_DIR);
	TSTR filename = configurationPath + TSTR("\\ColladaMax.ini");
	FILE* file = fopen(filename.data(), _T("wb"));
	if (file == NULL) return;

	// Write down a standard INI header to allow MaxScript edition.
	CStr headerString("[ColladaMax]\r\n");
	fwrite(headerString.data(), headerString.length(), 1, file);

	// Write down the options, one by one, in the form: 'option=X\n'
	CStr optionString;
#define WRITE_OPTION(optionValue) \
	optionString.printf(#optionValue "=%d\r\n", optionValue); \
	fwrite(optionString.data(), optionString.length(), 1, file)

	WRITE_OPTION(normals);
	WRITE_OPTION(triangulate);
	WRITE_OPTION(xrefs);
	WRITE_OPTION(tangents);
	WRITE_OPTION(animations);
	WRITE_OPTION(sampleAnim);
	WRITE_OPTION(createClip);
	WRITE_OPTION(bakeMatrices);
	WRITE_OPTION(relativePaths);
	WRITE_OPTION(animStart);
	WRITE_OPTION(animEnd);
	WRITE_OPTION(importUnits);
	WRITE_OPTION(importUpAxis);
#undef WRITE_OPTION

	fclose(file);
}

void ColladaOptions::LoadOptions()
{
	TSTR configurationPath = maxInterface->GetDir(APP_PLUGCFG_DIR);
	TSTR filename = configurationPath + TSTR("\\ColladaMax.ini");
	FILE* file = fopen(filename.data(), _T("rb"));
	if (file == NULL) return;

	// Read the whole configuration file
	fseek(file, 0, SEEK_END);
	int fileLength = ftell(file);
	fseek(file, 0, SEEK_SET);
	char* wholeFile = new char[fileLength + 1];
	fread(wholeFile, 1, fileLength, file);
	fclose(file);
	wholeFile[fileLength] = 0;
    
	// Read in the options, one by one, in the form: 'option=X\n'
	char* token = strtok(wholeFile, "\n");
	while (token != NULL)
	{
		// Skip whitespaces
		while (*token != 0 && (*token == ' ' || *token == '\r' || *token == '\t')) ++token;
		if (*token != 0)
		{
			if (*token == '[') {} // Standard INI header. Just skip the line for now.

			// Read in the Property=Value lines.
			char* value = strchr(token, '=');
			if (value != NULL)
			{
				*(value++) = 0;
				while (*value != 0 && (*value == ' ' || *value == '\r' || *value == '\t')) ++value;
				if (*value != 0) // skip empty lines.
				{
					// Look for/read in the ColladaMax options.
#define READ_OPTION(optionCast, optionValue) \
					if (strcmp(token, #optionValue) == 0) { \
						optionValue = (optionCast) atoi(value); } else
#define READ_OPTION_BOOL(optionValue) \
					if (strcmp(token, #optionValue) == 0) { \
						optionValue = *value != 'f' && *value != 'F' && *value != '0'; } else

					READ_OPTION_BOOL(normals);
					READ_OPTION_BOOL(triangulate);
					READ_OPTION_BOOL(xrefs);
					READ_OPTION_BOOL(tangents);
					READ_OPTION_BOOL(sampleAnim);
					READ_OPTION_BOOL(animations);
					READ_OPTION_BOOL(createClip);
					READ_OPTION_BOOL(bakeMatrices);
					READ_OPTION_BOOL(relativePaths);
					READ_OPTION_BOOL(importUnits);
					READ_OPTION_BOOL(importUpAxis);
					READ_OPTION(int, animStart);
					READ_OPTION(int, animEnd);
#undef READ_OPTION
#undef READ_OPTION_BOOL
					{
						// Handle unknown option here.
					}
				}
			}
		}

		token = strtok(NULL, "\n");
	}

	SAFE_DELETE_ARRAY(wholeFile);
}

// Function searches ctrl and all subanims for presence of a Constraint.
// This is bit of a cheap fix, a slightly better option may be to include
// parent offset calculation in rotation/position sampling (like Matrix sampling).
bool FindConstraint(Animatable* ctrl)
{
	if (ctrl->SuperClassID() == CTRL_FLOAT_CLASS_ID) return false;
	Class_ID clid = ctrl->ClassID();
	if (clid.PartB() == 0)
	{
		unsigned long pa = clid.PartA();
		if (pa == POSITION_CONSTRAINT_CLASS_ID ||
			pa == ORIENTATION_CONSTRAINT_CLASS_ID ||
			pa == LOOKAT_CONSTRAINT_CLASS_ID)
		{
			return true;
		}
	}

	int nSubs = ctrl->NumSubs();
	for (int i = 0; i < nSubs; i++)
	{
		Animatable* sub = ctrl->SubAnim(i);
		if (sub != NULL)
		{
			if (FindConstraint(sub)) return true;
		}
	}
	return false;
}

// Bug 233. Sampling fails if IK is involved.
bool ColladaOptions::DoSampleMatrices(INode* maxNode)
{
	bool sampleMatrices = BakeMatrices();
	// even of option is not checked, non-standard transforms need to be sampled too
	if (!sampleMatrices && maxNode != NULL)
	{
		Control* maxCtrl = maxNode->GetTMController();
		// The -only- class we can hope to export without sampling is the PRS controller
		sampleMatrices = (maxCtrl->ClassID() != Class_ID(PRS_CONTROL_CLASS_ID, 0) || FindConstraint(maxCtrl)) != 0;
	}
	return sampleMatrices;
}

// This function is used to determine if the node should be sampled with GetObjTMAfterWSM or GetNodeTM
// The difference is that GetNodeTM does NOT include the Obj Offset matrix.  When using GetObjTMAfterWSM
// then objTM is included, however this can cause problems with skinning etc, where the referenced matrix
// is actually the GetNodeTM matrix.
// If this function returns TRUE: 
//		Export Node with GetNodeTM
//		Export Pivot using GetObjTMAfterWSM * Inv(GetNodeTM)
// Else
//		Export Node with GetNodeTM
//		Export Pivot using GetPivotTM
// This should happen if the node in question is:
// being sampled AND
// NOT a BONE
// NOT a Biped object.
bool ColladaOptions::IncludeObjOffset(INode* node)
{
	return DoSampleMatrices(node) && !node->GetBoneNodeOnOff() && (GetBipMasterInterface(node->GetTMController()) == NULL);
}