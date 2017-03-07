
#include "StdAfx.h"

#include "py_common.h"
#include <boost/python.hpp>
#include <string>

#include "ColladaExporter.h"

#include "Options.h"
#include "AnimationExporter.h"

namespace py = boost::python;




void MobuColladaExporter(const char* DAEName, int indexTake, py::list boneList)
{
	ColladaExporter* exporter;
	float currentSampleTime;
	float sampleIntervalEnd;
	float samplePeriod;
	
	FBSystem global; // think of FBSystem as a function set, rather than an object.
	FBScene* globalScene = global.Scene;

	//Get Current Take and put it back at the end of the process
	int previousIndexTake = 0;
	for (previousIndexTake = 0; previousIndexTake < globalScene->Takes.GetCount(); previousIndexTake++)
	{
		if ((global.CurrentTake)->Name == globalScene->Takes[previousIndexTake]->Name)
			break;
	}


	// Retrieve the frame time.
	FBPlayerControl controller;
	
	// Set Current Take to the index given by user
	// FBPlayerControl stuff will be used on currrentTake
	global.CurrentTake = globalScene->Takes[indexTake];


	float originalStart = ToSeconds(controller.LoopStart);

#if K_FILMBOX_POINT_RELEASE_VERSION >= 7050
	samplePeriod = 1.0f / controller.GetTransportFpsValue();
#else
	samplePeriod = 1.0f / 30.0f; // Always use 30 FPS, since I have no way to determine the user's value.
#endif
	
	GetOptions()->setBoneList(true);
	GetOptions()->setExportingOnlyAnimAndScene(true);
	GetOptions()->setExportingBakedMatrix(false);
	GetOptions()->setForceSampling(false);
	GetOptions()->setCharacterControlerToRetrieveIK(true);

	GetOptions()->setSamplingStart(ToSeconds(controller.LoopStart) / samplePeriod);
	GetOptions()->setSamplingEnd(ToSeconds(controller.LoopStop) / samplePeriod);
		
	exporter = new ColladaExporter();
	const char* filename = DAEName;

	for (int i = 0; i < len(boneList); i++)
	{
		exporter->boneNameExported.push_back(py::extract<char*>(boneList[i]));
	}

	exporter->Export(filename);

	// Check for sampling obligations.
	if (exporter->GetAnimationExporter()->HasSampling())
	{
		FBSystem system;
		float samplingStart = GetOptions()->SamplingStart() * samplePeriod;
		currentSampleTime = samplingStart;
		sampleIntervalEnd = GetOptions()->SamplingEnd() * samplePeriod;

		FBTime t; t.SetSecondDouble(currentSampleTime);
		controller.Goto(t);

		while (true)
		{
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

	//set original Take
		global.CurrentTake = globalScene->Takes[previousIndexTake];

	FBTime t; t.SetSecondDouble(originalStart);
	controller.Goto(t);

	SAFE_DELETE(exporter);
}


//--- Use the Macro BOOST_PYTHON_FUNCTION_OVERLOADS to help define function with default arguments.
//--- BOOST_PYTHON_FUNCTION_OVERLOADS(Shader__CreateShader_Wrapper_Overloads, ORCreateShader_Wrapper, "minimum number arguments", "maximum number arguments")

BOOST_PYTHON_FUNCTION_OVERLOADS(Wrapper_Overloads, MobuColladaExporter, 3, 3)


//--- Define non-member function used in Python
void MobuColladaExporterInit()
{    
	def("MobuColladaExporter",MobuColladaExporter,Wrapper_Overloads());
}