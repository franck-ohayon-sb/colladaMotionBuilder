/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "Options.h"
#include "AnimationExporter.h"
#include "ColladaExporter.h"

//
// ExportOptionsTool
//

#define	ExportOptionsTool__CLASSNAME	ExportOptionsTool
#define ExportOptionsTool__CLASSSTR	"ExportOptionsTool"

class ExportOptionsTool : public FBTool
{
private:
	FBToolDeclare(ExportOptionsTool, FBTool);

	// The UI checkboxes
	FBButton data3dCheckBox;
	FBButton stripsCheckBox;
	FBButton systemCamerasCheckBox;
	FBButton forceSamplingCheckBox;
	FBLabel samplingIntervalLabel;
	FBEdit samplingStartEdit;
	FBEdit samplingEndEdit;
	FBLabel statusLabel;

	// The UI button.
	FBButton exportButton;

	// Sampling support.
	ColladaExporter* exporter;
	float currentSampleTime;
	float sampleIntervalEnd;
	float samplePeriod;

public:
	virtual bool FBCreate()
	{	
		exporter = NULL;

		exportButton.Caption = "Export";
		exportButton.OnClick.Add(this, (FBCallback) &ExportOptionsTool::OnExportClicked);
		stripsCheckBox.Caption = "Geometry: triangle-strip meshes.";
		stripsCheckBox.Style = kFB2States;
		stripsCheckBox.State = GetOptions()->ExportTriangleStrips() ? 1 : 0;
		data3dCheckBox.Caption = "Geometry: drop to 3D positions.";
		data3dCheckBox.Style = kFB2States;
		data3dCheckBox.State = GetOptions()->Export3dData() ? 1 : 0;
		systemCamerasCheckBox.Caption = "Camera: export system cameras.";
		systemCamerasCheckBox.Style = kFB2States;
		systemCamerasCheckBox.State = GetOptions()->ExportSystemCameras() ? 1 : 0;
		forceSamplingCheckBox.Caption = "Animation: force sampling.";
		forceSamplingCheckBox.Style = kFB2States;
		forceSamplingCheckBox.State = GetOptions()->ExportSystemCameras() ? 1 : 0;
		samplingIntervalLabel.Caption = "Animation: sampling interval.";
		statusLabel.Caption = "";
		statusLabel.Border.Style = kFBStandardBorder;
		statusLabel.Border.Width = 1;

		const uint32 lineHeight = 22;
		const uint32 width = 320;
		const uint32 height = 185;

		AddRegion("StripOption", "StripOption", 3, kFBAttachLeft, "", 1.0f, 3, kFBAttachTop, "", 1.0f, -3, kFBAttachRight, "", 1.0f, lineHeight + 3, kFBAttachTop, "", 1.0f);
		AddRegion("3dData", "3dData", 3, kFBAttachLeft, "", 1.0f, 1, kFBAttachBottom, "StripOption", 1.0f, -3, kFBAttachRight, "", 1.0f, lineHeight + 3, kFBAttachBottom, "StripOption", 1.0f);
		AddRegion("SystemCameras", "SystemCameras", 3, kFBAttachLeft, "", 1.0f, 1, kFBAttachBottom, "3dData", 1.0f, -3, kFBAttachRight, "", 1.0f, lineHeight + 3, kFBAttachBottom, "3dData", 1.0f);
		AddRegion("ForceSampling", "ForceSampling", 3, kFBAttachLeft, "", 1.0f, 1, kFBAttachBottom, "SystemCameras", 1.0f, -3, kFBAttachRight, "", 1.0f, lineHeight + 3, kFBAttachBottom, "SystemCameras", 1.0f);
		AddRegion("SamplingStart", "SamplingStart", 3, kFBAttachLeft, "", 1.0f, 1, kFBAttachBottom, "ForceSampling", 1.0f, 70, kFBAttachLeft, "", 1.0f, lineHeight + 3, kFBAttachBottom, "ForceSampling", 1.0f);
		AddRegion("SamplingEnd", "SamplingEnd", 2, kFBAttachRight, "SamplingStart", 1.0f, 1, kFBAttachBottom, "ForceSampling", 1.0f, 68, kFBAttachRight, "SamplingStart", 1.0f, lineHeight + 3, kFBAttachBottom, "ForceSampling", 1.0f);
		AddRegion("SamplingInterval", "SamplingInterval", 5, kFBAttachRight, "SamplingEnd", 1.0f, 1, kFBAttachBottom, "ForceSampling", 1.0f, -3, kFBAttachRight, "", 1.0f, lineHeight + 3, kFBAttachBottom, "ForceSampling", 1.0f);
		AddRegion("ExportButton", "ExportButton", 60, kFBAttachLeft, "", 1.0f, 3, kFBAttachBottom, "SamplingEnd", 1.0f, -60, kFBAttachRight, "", 1.0f, - (int)lineHeight - 5, kFBAttachBottom, "", 1.0f);
		AddRegion("StatusLabel", "StatusLabel", 5, kFBAttachLeft, "", 1.0f, 2, kFBAttachBottom, "ExportButton", 1.0f, -3, kFBAttachRight, "", 1.0f, lineHeight + 3, kFBAttachBottom, "ExportButton", 1.0f);
		
		SetControl("ExportButton", exportButton);
		SetControl("3dData", data3dCheckBox);
		SetControl("StripOption", stripsCheckBox);
		SetControl("SystemCameras", systemCamerasCheckBox);
		SetControl("ForceSampling", forceSamplingCheckBox);
		SetControl("SamplingInterval", samplingIntervalLabel);
		SetControl("SamplingStart", samplingStartEdit);
		SetControl("SamplingEnd", samplingEndEdit);
		SetControl("StatusLabel", statusLabel);

		MinSize[0] = width + 10; MaxSize[0] = width + 10; StartSize[0] = width + 10; 
		MinSize[1] = height + 20; MaxSize[1] = height + 20; StartSize[1] = height + 20;
		return true;
	}

	void OnExportClicked(HISender pSender, HKEvent pEvent)
	{
		// Retrieve the frame time.
		FBPlayerControl controller;
#if K_FILMBOX_POINT_RELEASE_VERSION >= 7050
		samplePeriod = 1.0f / controller.GetTransportFpsValue();
#else
		samplePeriod = 1.0f / 30.0f; // Always use 30 FPS, since I have no way to determine the user's value.
#endif

		FBFilePopup fileChooser;
		fileChooser.Caption = "COLLADA Export...";
		fileChooser.Style = kFBFilePopupSave;
		fileChooser.Path = const_cast<char*>(GetOptions()->GetLastFilePath().c_str());
		fileChooser.Filter = "*.dae";
		
		// Get filename & export if ok was clicked
		if (fileChooser.Execute())
		{
			// Recover the export options.
			GetOptions()->useBoneList = false;
			GetOptions()->exportOnlyAnimAndScene = false;
			GetOptions()->exportBakedMatrix = false;
			GetOptions()->exportTriangleStrips = (stripsCheckBox.State == 1);
			GetOptions()->export3dData = (data3dCheckBox.State == 1);
			GetOptions()->exportSystemCameras = (systemCamerasCheckBox.State == 1);
			GetOptions()->forceSampling = (forceSamplingCheckBox.State == 1);
			GetOptions()->hasSamplingInterval = ((FBString) samplingStartEdit.Text != "" || (FBString) samplingEndEdit.Text != "");
			if ((FBString) samplingStartEdit.Text == "") GetOptions()->samplingStart = ToSeconds(controller.LoopStart) / samplePeriod;
			else GetOptions()->samplingStart = FUStringConversion::ToInt32((char*) samplingStartEdit.Text.AsString());
			if ((FBString) samplingEndEdit.Text == "") GetOptions()->samplingEnd = ToSeconds(controller.LoopStop) / samplePeriod;
			else GetOptions()->samplingEnd = FUStringConversion::ToInt32((char*) samplingEndEdit.Text.AsString());

			// Generate and export the document.
			statusLabel.Caption = "Starting Export Process...";
			statusLabel.Refresh(true);

			exporter = new ColladaExporter();
			exporter->Export(fileChooser.FullFilename);
			GetOptions()->SetLastFilename(fileChooser.FullFilename);

			// Check for sampling obligations.
			if (exporter->GetAnimationExporter()->HasSampling())
			{
				FBSystem system;
				float samplingStart = GetOptions()->samplingStart * samplePeriod;
				currentSampleTime = samplingStart;
				sampleIntervalEnd = GetOptions()->samplingEnd * samplePeriod;

				FBTime t; t.SetSecondDouble(currentSampleTime);
				controller.Goto(t);

				while (true)
				{
					FUSStringBuilder percentString("Sampling: ");
					percentString.append((uint32) ((currentSampleTime - samplingStart) / (sampleIntervalEnd - samplingStart) * 100.0f));
					statusLabel.Caption = (char*) percentString.ToString().c_str();
					statusLabel.Refresh(true);

					// Sample once.
					FBSystem system;
					system.Scene->Evaluate();
					exporter->GetAnimationExporter()->DoSampling();
					float currentTime = ToSeconds(system.LocalTime);
					if (currentTime > sampleIntervalEnd)
					{
						exporter->GetAnimationExporter()->TerminateSampling();
						break;
					}
					else
					{
						// Advance the time: we'll sample once more.
						FBPlayerControl controller;
						float newTime = currentTime + samplePeriod;
						float padder = (float)(int)newTime + 0.5f;
						if (IsEquivalent(newTime, padder)) newTime = padder;
						FBTime t; t.SetSecondDouble(newTime);
						controller.Goto(t);
					}
				}
			}

			// The export process is now complete.
			exporter->Complete();
			statusLabel.Caption = "Export Completed.";
			SAFE_DELETE(exporter);
		}
	}
};

FBToolImplementation(ExportOptionsTool);
FBRegisterTool(ExportOptionsTool, "COLLADA Export", "FCollada COLLADA Export.", FB_DEFAULT_SDK_ICON);
