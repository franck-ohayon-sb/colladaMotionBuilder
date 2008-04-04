/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "ColladaAttrib.h"
#include "ColladaDocumentContainer.h"
#include "ColladaExporter.h"
#include "ColladaImporter.h"
#include "SimpleAttrib.h"
#include "ColladaFX/ColladaEffect.h"
#include "ColladaFX/ColladaEffectTechnique.h"
#include "ColladaFX/ColladaEffectPass.h"
#include "ColladaFX/ColladaEffectShader.h"
#include "ColladaFX/ColladaEffectParameter.h"
#include "FColladaPlugin.h"
#include "Controller/ColladaController.h"


// Uncomment the line below to run the visual leak detector: you'll also need to
// add the correct folder to the linker includes.
//#include "../External/VisualLeakDetector/vld.h"

HINSTANCE hInstance;
int controlsInit = FALSE;
 
BOOL WINAPI DllMain(HINSTANCE hinstDLL, ULONG fdwReason, LPVOID UNUSED(lpvReserved))
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		hInstance = hinstDLL;
#if MAX_VERSION_MAJOR < 10
		InitCustomControls(hInstance);
#endif // pre-Max 2008 only.
		FCollada::Initialize();
		break;

	case DLL_PROCESS_DETACH:
		FCollada::Release();
		break;
	}
	return TRUE;
}

__declspec(dllexport) const TCHAR* LibDescription()
{
	return GetString(IDS_LIBDESCRIPTION);
}

__declspec(dllexport) int LibNumberClasses()
{
	return 10;
}

__declspec(dllexport) ClassDesc* LibClassDesc(int i)
{
	switch (i)
	{
		case 0: return GetCOLLADAExporterDesc();
		case 1: return GetCOLLADAImporterDesc();
		case 2: return GetSimpleAttribDesc();
		case 3: return GetCOLLADAAttribDesc();
		case 4: return GetCOLLADADocumentContainerDesc();
		// ColladaFX
		case 5: return GetCOLLADAEffectDesc();
		case 6: return GetColladaEffectTechniqueClassDesc();
		case 7: return GetColladaEffectPassClassDesc();
		case 8: return GetColladaEffectShaderClassDesc();
		case 9: return GetColladaEffectParameterCollectionClassDesc();

		//case 6: return GetCOLLADAControllerDesc();

		default: return NULL;
	}
}

__declspec(dllexport) ULONG LibVersion()
{
	return VERSION_3DSMAX;
}

// Let the plug-in register itself for deferred loading
__declspec(dllexport) ULONG CanAutoDefer()
{
	return 1;
}

TCHAR* GetString(int id)
{
	static TCHAR buf[1024];
	return (hInstance != NULL && LoadString(hInstance, id, buf, sizeof(buf))) ? buf : NULL;
}
