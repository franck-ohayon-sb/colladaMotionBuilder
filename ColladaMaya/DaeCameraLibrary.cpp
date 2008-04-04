/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/*
* COLLADA Camera Library
*/

#include "StdAfx.h"
#include <maya/MFnCamera.h>
#include "CExportOptions.h"
#include "CReferenceManager.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDAnimationCurve.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDCamera.h"
#include "FCDocument/FCDExtra.h"
#include "FCDocument/FCDPlaceHolder.h"
#include "FCDocument/FCDEntityReference.h"
#include "DaeDocNode.h"

//
// Exporter converter functors
//

class FocalLengthConverter : public FCDConversionFunctor
{
public:
	float aperture;
	FocalLengthConverter(float _aperture) { aperture = _aperture; }
	virtual float operator() (float v) { return FMath::RadToDeg(2.0f * atan(25.4f * aperture / 2.0f / v)); }
};

//
// Import conversion functions
//

class FovConverter : public FCDConversionFunctor
{
public:
	float aperture;
	FovConverter(float _aperture) { aperture = _aperture; }
	virtual float operator() (float v)
	{
		// Note: Maya's aperture is in inches.
		v = FMath::DegToRad(v);
		float focalLength = 25.4f * aperture / tanf(v / 2.0f) / 2.0f;
		return focalLength;
	}
};

class OrthoWidthConverter : public FCDConversionScaleFunctor
{
public:
	OrthoWidthConverter(float scaleFactor) : FCDConversionScaleFunctor(scaleFactor * 2.0f) {}
	virtual float operator() (float v) { return fabsf(FCDConversionScaleFunctor::operator ()(v)); }
};

// Camera Library class Implementation

DaeCameraLibrary::DaeCameraLibrary(DaeDoc* doc)
:	DaeBaseLibrary(doc)
{
}

DaeCameraLibrary::~DaeCameraLibrary()
{
}

// Add a camera to this library and return the export Id
// 
DaeEntity* DaeCameraLibrary::ExportCamera(const MObject& cameraNode)
{
	MStatus status;
	MFnCamera cameraFn(cameraNode, &status); CHECK_MSTATUS(status);
	if (status != MStatus::kSuccess) return NULL;

	// Create the COLLADA camera object.
	FCDCamera* colladaCamera = CDOC->GetCameraLibrary()->AddEntity();
	DaeEntity* element = new DaeEntity(doc, colladaCamera, cameraNode);
	fstring cameraName = MConvert::ToFChar(doc->MayaNameToColladaName(cameraFn.name()));
	if (element->isOriginal) colladaCamera->SetDaeId(TO_STRING(cameraName));
	colladaCamera->SetName(cameraName);

	// Set the orthographic and perspective-specific parameters
	bool isOrthographic = cameraFn.isOrtho(&status); CHECK_MSTATUS(status);
	if (isOrthographic)
	{
		colladaCamera->SetProjectionType(FCDCamera::ORTHOGRAPHIC);

		double width = cameraFn.orthoWidth(&status); CHECK_MSTATUS(status);
		colladaCamera->SetMagX((float) width / 2.0f);
		ANIM->AddPlugAnimation(cameraFn.object(), "orthographicWidth", colladaCamera->GetMagX(), kSingle);
		colladaCamera->SetAspectRatio((float) cameraFn.aspectRatio(&status)); CHECK_MSTATUS(status);
	}
	else
	{
		colladaCamera->SetProjectionType(FCDCamera::PERSPECTIVE);

		if (CExportOptions::CameraYFov())
		{
			FocalLengthConverter* converter = new FocalLengthConverter((float) cameraFn.verticalFilmAperture());
			colladaCamera->SetFovY((*converter)((float) cameraFn.focalLength(&status))); CHECK_MSTATUS(status);
			ANIM->AddPlugAnimation(cameraFn.object(), "focalLength", colladaCamera->GetFovY(), kSingle, converter);
		}
		if (CExportOptions::CameraXFov() || !CExportOptions::CameraYFov())
		{
			FocalLengthConverter* converter = new FocalLengthConverter((float) (cameraFn.horizontalFilmAperture() * cameraFn.lensSqueezeRatio()));
			colladaCamera->SetFovX((*converter)((float) cameraFn.focalLength(&status))); CHECK_MSTATUS(status);
			ANIM->AddPlugAnimation(cameraFn.object(), "focalLength", colladaCamera->GetFovX(), kSingle, converter);
		}
		if (!CExportOptions::CameraXFov() || !CExportOptions::CameraYFov())
		{
			colladaCamera->SetAspectRatio((float) cameraFn.aspectRatio(&status)); CHECK_MSTATUS(status);
		}
	}

	// Add the camera common parameters.
	colladaCamera->SetNearZ((float)cameraFn.nearClippingPlane(&status)); CHECK_MSTATUS(status);
	ANIM->AddPlugAnimation(cameraFn.object(), "nearClipPlane", colladaCamera->GetNearZ(), kSingle | kLength);

	colladaCamera->SetFarZ((float)cameraFn.farClippingPlane(&status)); CHECK_MSTATUS(status);
	ANIM->AddPlugAnimation(cameraFn.object(), "farClipPlane", colladaCamera->GetFarZ(), kSingle | kLength);

	// Add the Maya-specific parameters
	FCDETechnique* colladaMayaTechnique = colladaCamera->GetExtra()->GetDefaultType()->AddTechnique(DAEMAYA_MAYA_PROFILE);

	float vAperture = (float) cameraFn.verticalFilmAperture(&status) * 2.54f; CHECK_MSTATUS(status);
	FCDENode* vApertureNode = colladaMayaTechnique->AddParameter(DAEMAYA_VAPERTURE_PARAMETER, vAperture);
	ANIM->AddPlugAnimation(cameraFn.object(), "verticalFilmAperture", vApertureNode->GetAnimated(), 
			kSingle | kLength, new FCDConversionScaleFunctor(2.54f));

	float hAperture = (float) cameraFn.horizontalFilmAperture(&status) * 2.54f; CHECK_MSTATUS(status);
	FCDENode* hApertureNode = colladaMayaTechnique->AddParameter(DAEMAYA_HAPERTURE_PARAMETER, hAperture);
	ANIM->AddPlugAnimation(cameraFn.object(), "horizontalFilmAperture", hApertureNode->GetAnimated(),
			kSingle | kLength, new FCDConversionScaleFunctor(2.54f));
	
	float lensSqueeze = (float) cameraFn.lensSqueezeRatio(&status); CHECK_MSTATUS(status);
	FCDENode* lensSqueezeNode = colladaMayaTechnique->AddParameter(DAEMAYA_LENSSQUEEZE_PARAMETER, lensSqueeze);
	ANIM->AddPlugAnimation(cameraFn.object(), "lensSqueezeRatio", lensSqueezeNode->GetAnimated(), kSingle);
	
	colladaMayaTechnique->AddParameter(DAEMAYA_CAMERA_FILMFIT, (int32) cameraFn.filmFit(&status)); CHECK_MSTATUS(status);

	FCDENode* filmFitOffset = colladaMayaTechnique->AddParameter(DAEMAYA_CAMERA_FILMFITOFFSET, (float) cameraFn.filmFitOffset());
	ANIM->AddPlugAnimation(cameraFn.object(), "filmFitOffset", filmFitOffset->GetAnimated(), kSingle);

	FCDENode* filmOffsetX = colladaMayaTechnique->AddParameter(DAEMAYA_CAMERA_FILMOFFSETX, (float) cameraFn.horizontalFilmOffset());
	ANIM->AddPlugAnimation(cameraFn.object(), "horizontalFilmOffset", filmOffsetX->GetAnimated(), kSingle);

	FCDENode* filmOffsetY = colladaMayaTechnique->AddParameter(DAEMAYA_CAMERA_FILMOFFSETY, (float) cameraFn.verticalFilmOffset());
	ANIM->AddPlugAnimation(cameraFn.object(), "verticalFilmOffset", filmOffsetY->GetAnimated(), kSingle);

	doc->GetEntityManager()->ExportEntity(cameraFn.object(), colladaCamera);
	return element;
}

void DaeCameraLibrary::ExportCamera(FCDSceneNode* sceneNode, const MDagPath& dagPath, bool isLocal)
{
	FUAssert(sceneNode != NULL, return);

	// Add this camera to our library
	if (isLocal)
	{
		DaeEntity* entity = ExportCamera(dagPath.node());
		if (entity != NULL)
		{
			doc->GetSceneGraph()->ExportInstance(sceneNode, entity->entity);
			doc->GetSceneGraph()->AddElement(entity);
		}
	}
	else
	{
		// Check for an entity linkage.
		DaeEntity* entity = NODE->FindEntity(dagPath.node());
		if (entity != NULL) doc->GetSceneGraph()->ExportInstance(sceneNode, entity->entity);
		else
		{
			// Generate the XRef URI.
			FCDPlaceHolder* placeHolder = CReferenceManager::GetPlaceHolder(CDOC, dagPath);
			MString name = doc->MayaNameToColladaName(MFnDagNode(dagPath).name(), true);
			FUUri uri(placeHolder->GetFileUrl());
			uri.SetFragment(MConvert::ToFChar(name));

			FCDEntityInstance* colladaInstance = sceneNode->AddInstance(FCDEntity::CAMERA);
			FCDEntityReference* xref = colladaInstance->GetEntityReference();
			xref->SetUri(uri);
		}
	}
}

void DaeCameraLibrary::Import()
{
	FCDCameraLibrary* library = CDOC->GetCameraLibrary();
	for (size_t i = 0; i < library->GetEntityCount(); ++i)
	{
		FCDCamera* camera = library->GetEntity(i);
		if (camera->GetUserHandle() == NULL) ImportCamera(camera);
	}
}

// Attempt to find & create an element from this library
DaeEntity* DaeCameraLibrary::ImportCamera(FCDCamera* colladaCamera)
{
	MStatus status;

	// Verify that we haven't imported this camera already
	if (colladaCamera->GetUserHandle() != NULL)
	{
		return (DaeEntity*) colladaCamera->GetUserHandle();
	}

	// Create the connection object for this camera
	DaeEntity* element = new DaeEntity(doc, colladaCamera, MObject::kNullObj);

	// Create a new Maya camera object
	MFnCamera cameraFn;
	MObject camera = cameraFn.create(&status); CHECK_MSTATUS(status);
	MString actualName = cameraFn.setName(colladaCamera->GetName().c_str(), &status); CHECK_MSTATUS(status);
	colladaCamera->SetName(MConvert::ToFChar(actualName));
	CDagHelper::SetPlugValue(camera, "visibility", false);
	element->SetNode(cameraFn.object());
	
	// Set the camera type and the default fit value
	CHECK_MSTATUS(cameraFn.setIsOrtho(colladaCamera->GetProjectionType() == FCDCamera::ORTHOGRAPHIC));
	CHECK_MSTATUS(cameraFn.setFilmFit(MFnCamera::kFillFilmFit));

	// Get the Maya specific camera parameters
	FCDETechnique* colladaMayaTechnique = 
			colladaCamera->GetExtra()->GetDefaultType()->FindTechnique(DAEMAYA_MAYA_PROFILE);
	if (colladaMayaTechnique != NULL)
	{
		// vertical aperture
		FCDENode* vApertureNode = colladaMayaTechnique->FindParameter(DAEMAYA_VAPERTURE_PARAMETER);
		if (vApertureNode != NULL)
		{
			float  vAperture = FUStringConversion::ToFloat(vApertureNode->GetContent());
			CHECK_MSTATUS(cameraFn.setVerticalFilmAperture(vAperture / 2.54f));
			ANIM->AddPlugAnimation(cameraFn.object(), "verticalFilmAperture", vApertureNode->GetAnimated(), 
					kSingle | kLength, new FCDConversionScaleFunctor(1.0f / 2.54f));
			SAFE_RELEASE(vApertureNode);
		}

		// horizontal aperture
		FCDENode* hApertureNode = colladaMayaTechnique->FindParameter(DAEMAYA_HAPERTURE_PARAMETER);
		if (hApertureNode != NULL)
		{
			float hAperture = FUStringConversion::ToFloat(hApertureNode->GetContent());
			CHECK_MSTATUS(cameraFn.setHorizontalFilmAperture(hAperture / 2.54f));
			ANIM->AddPlugAnimation(cameraFn.object(), "horizontalFilmAperture", hApertureNode->GetAnimated(), 
					kSingle | kLength, new FCDConversionScaleFunctor(1.0f / 2.54f));
			SAFE_RELEASE(hApertureNode);
		}

		// lens squeeze
		FCDENode* lensSqueezeNode = colladaMayaTechnique->FindParameter(DAEMAYA_LENSSQUEEZE_PARAMETER);
		if (lensSqueezeNode != NULL)
		{
			float lensSqueeze = FUStringConversion::ToFloat(lensSqueezeNode->GetContent());
			CHECK_MSTATUS(cameraFn.setLensSqueezeRatio(lensSqueeze));
			ANIM->AddPlugAnimation(cameraFn.object(), "lensSqueezeRatio", lensSqueezeNode->GetAnimated(), kSingle);
			SAFE_RELEASE(lensSqueezeNode);
		}
	}

	// Setup the perspective and orthographic parameters
	if (colladaCamera->GetProjectionType() == FCDCamera::PERSPECTIVE)
	{
		if (colladaCamera->HasVerticalFov())
		{
			status = cameraFn.setVerticalFieldOfView(colladaCamera->GetFovY());
			if (status != MStatus::kSuccess)
			{
				// Attempt using MEL to set the YFov.
				MGlobal::executeCommand(MString("camera -e -vfv ") + 
						colladaCamera->GetFovY() + MString(" ") + 
						cameraFn.name());
			}
			float filmAperture = (float) cameraFn.verticalFilmAperture();
			ANIM->AddPlugAnimation(cameraFn.object(), "focalLength", colladaCamera->GetFovY(), kSingle, new FovConverter(filmAperture));
		}
		if (colladaCamera->HasVerticalFov() ^ colladaCamera->HasHorizontalFov())
		{
			// If we are given an aspect ratio: set it here before the possible XFov,
			// because the MEL fallback depends on a valid aspect ratio.
			cameraFn.setAspectRatio(colladaCamera->GetAspectRatio());
		}
		if (colladaCamera->HasHorizontalFov())
		{
			status = cameraFn.setHorizontalFieldOfView(colladaCamera->GetFovX());
			if (status != MStatus::kSuccess)
			{
				// Attempt using MEL to set the YFov.
				MGlobal::executeCommand(MString("camera -e -hfv ") + 
						colladaCamera->GetFovX() + MString(" ") + 
						cameraFn.name());
			}
			if (!colladaCamera->HasVerticalFov())
			{
				float filmAperture = (float) (cameraFn.horizontalFilmAperture() * cameraFn.lensSqueezeRatio());
				ANIM->AddPlugAnimation(cameraFn.object(), "focalLength", colladaCamera->GetFovX(), kSingle, new FovConverter(filmAperture));
			}
		}
	}
	else if (colladaCamera->GetProjectionType() == FCDCamera::ORTHOGRAPHIC)
	{
		if (colladaCamera->HasHorizontalMag())
		{
			CHECK_MSTATUS(cameraFn.setOrthoWidth(colladaCamera->GetMagX() * 2.0f));
			ANIM->AddPlugAnimation(cameraFn.object(), "orthographicWidth", colladaCamera->GetMagX(), kSingle, new OrthoWidthConverter(1.0f));
		}
		else if (colladaCamera->HasVerticalMag())
		{
			CHECK_MSTATUS(cameraFn.setOrthoWidth(colladaCamera->GetMagY() * 2.0f));
			ANIM->AddPlugAnimation(cameraFn.object(), "orthographicWidth", colladaCamera->GetMagY(), kSingle, new OrthoWidthConverter(1.0f));
		}
		if (colladaCamera->HasVerticalMag() ^ colladaCamera->HasHorizontalMag())
		{
			CHECK_MSTATUS(cameraFn.setAspectRatio(colladaCamera->GetAspectRatio()));
		}
	}

	// Set the near/far clipping planes
	CHECK_MSTATUS(cameraFn.setNearClippingPlane(colladaCamera->GetNearZ()));
	CHECK_MSTATUS(cameraFn.setFarClippingPlane(colladaCamera->GetFarZ()));
	ANIM->AddPlugAnimation(cameraFn.object(), "nearClipPlane", colladaCamera->GetNearZ(), kSingle);
	ANIM->AddPlugAnimation(cameraFn.object(), "farClipPlane", colladaCamera->GetFarZ(), kSingle);

	// Import the extra Maya-specific parameters
	if (colladaMayaTechnique != NULL)
	{
		FCDENode* filmFitNode = colladaMayaTechnique->FindParameter(DAEMAYA_CAMERA_FILMFIT);
		if (filmFitNode != NULL)
		{
			int filmFit = FUStringConversion::ToInt32(filmFitNode->GetContent());
			CHECK_MSTATUS(cameraFn.setFilmFit((MFnCamera::FilmFit) filmFit));
			SAFE_RELEASE(filmFitNode);
		}
		FCDENode* filmFitOffset = colladaMayaTechnique->FindParameter(DAEMAYA_CAMERA_FILMFITOFFSET);
		if (filmFitOffset != NULL)
		{
			float offset = FUStringConversion::ToFloat(filmFitOffset->GetContent());
			CHECK_MSTATUS(cameraFn.setFilmFitOffset(offset));
			ANIM->AddPlugAnimation(cameraFn.object(), "filmFitOffset", filmFitOffset->GetAnimated(), kSingle);
			SAFE_RELEASE(filmFitOffset);
		}
		FCDENode* filmOffsetX = colladaMayaTechnique->FindParameter(DAEMAYA_CAMERA_FILMOFFSETX);
		if (filmOffsetX != NULL)
		{
			float offset = FUStringConversion::ToFloat(filmOffsetX->GetContent());
			CHECK_MSTATUS(cameraFn.setHorizontalFilmOffset(offset));
			ANIM->AddPlugAnimation(cameraFn.object(), "horizontalFilmOffset", filmOffsetX->GetAnimated(), kSingle);
			SAFE_RELEASE(filmOffsetX);
		}
		FCDENode* filmOffsetY = colladaMayaTechnique->FindParameter(DAEMAYA_CAMERA_FILMOFFSETY);
		if (filmOffsetY != NULL)
		{
			float offset = FUStringConversion::ToFloat(filmOffsetY->GetContent());
			CHECK_MSTATUS(cameraFn.setVerticalFilmOffset(offset));
			ANIM->AddPlugAnimation(cameraFn.object(), "verticalFilmOffset", filmOffsetY->GetAnimated(), kSingle);
			SAFE_RELEASE(filmOffsetY);
		}
	}

	MObject cameraObject = cameraFn.object();
	doc->GetEntityManager()->ImportEntity(cameraObject, element);
	return element;
}

void DaeCameraLibrary::InstantiateCamera(DaeEntity* sceneNode, FCDCamera* colladaCamera)
{
	DaeEntity* camera = ImportCamera(colladaCamera);
	if (camera == NULL) return;

	// Parent the camera to this scene node
	camera->Instantiate(sceneNode);
}
