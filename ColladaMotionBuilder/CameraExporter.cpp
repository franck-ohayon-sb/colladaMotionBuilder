/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "AnimationExporter.h"
#include "CameraExporter.h"
#include "ColladaExporter.h"
#include "NodeExporter.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDCamera.h"
#include "FCDocument/FCDLibrary.h"

//
// CameraExporter
//

CameraExporter::CameraExporter(ColladaExporter* base)
:	EntityExporter(base)
{
}

CameraExporter::~CameraExporter()
{
}

FCDEntity* CameraExporter::ExportCamera(FBCamera* camera)
{
	// Retrieve the camera type.
	bool isOrthographic;
	switch (camera->Type)
	{
	case kFBCameraTypePerspective: isOrthographic = false; break;
	case kFBCameraTypeOrthogonal: isOrthographic = true; break;
	default: return NULL;
	}

	// Create the FCollada camera.
	FCDCamera* colladaCamera = CDOC->GetCameraLibrary()->AddEntity();
	ExportEntity(colladaCamera, camera);
	if (isOrthographic)
	{
		// For orthographic cameras, retrieve the Motion Builder
		// orthogonal vertical size and the aspect ratio.
		colladaCamera->SetProjectionType(FCDCamera::ORTHOGRAPHIC);
		colladaCamera->SetMagY(camera->OrthogonalVerticalSize);
		ANIMATABLE(&camera->OrthogonalVerticalSize, colladaCamera, colladaCamera->GetMagY(), -1, NULL, false);
		colladaCamera->SetAspectRatio(camera->FilmAspectRatio);
		ANIMATABLE(&camera->FilmAspectRatio, colladaCamera, colladaCamera->GetAspectRatio(), -1, NULL, false);
	}
	else
	{
		colladaCamera->SetProjectionType(FCDCamera::PERSPECTIVE);

		// For perspective cameras, process the aperture mode first.
		// SDK does not give us support for "Focal Length" perspective cameras.
		switch (camera->ApertureMode)
		{
		case kFBApertureVertical:
			colladaCamera->SetFovY(camera->FieldOfView);
			ANIMATABLE(&camera->FieldOfView, colladaCamera, colladaCamera->GetFovY(), -1, NULL, false);
			colladaCamera->SetAspectRatio(camera->FilmAspectRatio);
			ANIMATABLE(&camera->FilmAspectRatio, colladaCamera, colladaCamera->GetAspectRatio(), -1, NULL, false);
			break;

		case kFBApertureHorizontal:
			colladaCamera->SetFovX(camera->FieldOfView);
			ANIMATABLE(&camera->FieldOfView, colladaCamera, colladaCamera->GetFovX(), -1, NULL, false);
			colladaCamera->SetAspectRatio(camera->FilmAspectRatio);
			ANIMATABLE(&camera->FilmAspectRatio, colladaCamera, colladaCamera->GetAspectRatio(), -1, NULL, false);
			break;

		case kFBApertureVertHoriz:
			colladaCamera->SetFovX(camera->FieldOfViewX);
			ANIMATABLE(&camera->FieldOfViewX, colladaCamera, colladaCamera->GetFovX(), -1, NULL, false);
			colladaCamera->SetFovY(camera->FieldOfViewY);
			ANIMATABLE(&camera->FieldOfViewY, colladaCamera, colladaCamera->GetFovY(), -1, NULL, false);
			break;

		default:
			// Focal Length?
			FUFail("");
		}
	}

	// Set the near/far-Z planes
	colladaCamera->SetNearZ(camera->NearPlaneDistance);
	ANIMATABLE(&camera->NearPlaneDistance, colladaCamera, colladaCamera->GetNearZ(), -1, NULL, false);
	colladaCamera->SetFarZ(camera->FarPlaneDistance);
	ANIMATABLE(&camera->FarPlaneDistance, colladaCamera, colladaCamera->GetFarZ(), -1, NULL, false);

	return colladaCamera;
}

void CameraExporter::ExportSystemCameras(FCDSceneNode* colladaScene)
{
	FBScene* scene = NODE->GetScene();

	int cameraCount = scene->Cameras.GetCount();
	for (int i = 0; i < cameraCount; ++i)
	{
		FBCamera* camera = scene->Cameras[i];
#if K_FILMBOX_POINT_RELEASE_VERSION >= 7050
		if (camera->SystemCamera)
#else
		if (strstr((const char*) camera->Name, "Producer ") != NULL)
#endif
		{
			NODE->ExportNode(colladaScene, camera); // This will call the ExportCamera above.
		}
	}
}
