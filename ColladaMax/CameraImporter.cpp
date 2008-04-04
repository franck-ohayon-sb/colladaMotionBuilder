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

// Camera Importer Class

#include "StdAfx.h"
#include "AnimationImporter.h"
#include "DocumentImporter.h"
#include "CameraImporter.h"
#include "NodeImporter.h"
#include "TargetImporter.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDCamera.h"
#include "FCDocument/FCDExtra.h"

#include "Common/CameraMultiPassEffects.h"
#include "Common/ConversionFunctions.h"

// Import Functor
class FCDConversionOrthoFOVFunctor : public FCDConversionFunctor
{
private:
	float distance;
public:
	FCDConversionOrthoFOVFunctor(float _distance) : distance(_distance) {}
	virtual ~FCDConversionOrthoFOVFunctor() {}
	virtual float operator() (float v)  { return 2.0f * atanf(v / distance); }
};


CameraImporter::CameraImporter(DocumentImporter* document, NodeImporter* parent)
:	EntityImporter(document, parent)
{
	colladaCamera = NULL;
	cameraObject = NULL;
}

CameraImporter::~CameraImporter()
{
	colladaCamera = NULL;
	cameraObject = NULL;
}

void CameraImporter::ImportExtraParameter(FCDENode* parentNode, Animatable* animObj, char* colladaID, int blockIndex, int paramID)
{
	if (parentNode == NULL || animObj == NULL || colladaID == NULL) return;
	FCDENode* child = parentNode->FindChildNode(colladaID);
	if (child == NULL) return;

	// sanity check
	if (animObj->SubAnim(blockIndex) == NULL) return;

	IParamBlock2* params = animObj->GetParamBlock(blockIndex);
	if (params == NULL) return;

	// convert the parameter ID to an animation ID
	int animIdx = params->GetAnimNum(paramID);

	// Get the first level sub-animation at the block index
	Animatable* sub = animObj->SubAnim(blockIndex);
	if (sub == NULL) return;

	// Get the second level sub-animation from the animation ID
	// This is the name used by the FindAnimatable method in ImportAnimatedFloat
	// this is not the alias internal name.
	TSTR animName = sub->SubAnimName(animIdx);
	char* alias = (char*)animName;
	if (alias == NULL) return;

	// do we have an animation?
	bool useAnimated = (child->GetAnimated() != NULL);
	float content = FUStringConversion::ToFloat(child->GetContent());

	int type = (int)params->GetParameterType(paramID);
	switch (type)
	{
	case TYPE_BOOL:
		params->SetValue(paramID, 0, (content > 0.5f) ? TRUE : FALSE);
		// NO ANIMATION ON BOOLEANS YET
		break;
	case TYPE_INT:
		params->SetValue(paramID, 0, (int) content);
		if (useAnimated) ANIM->ImportAnimatedFloat(animObj, alias, child->GetAnimated(), NULL);
		break;
	case TYPE_FLOAT:
		params->SetValue(paramID, 0, content);
		if (useAnimated) ANIM->ImportAnimatedFloat(animObj, alias, child->GetAnimated(), NULL);
		break;
	default:
		// not supported yet
		break;
	}
}

IMultiPassCameraEffect* CameraImporter::CreateMultiPassEffect(Class_ID id)
{
	// this is how they create the Multi Pass effect in the samples.
	// Another way could be to call the import interface's Create method directly
	// with both the super class id and the sub class id.
	SubClassList* subList = GetCOREInterface()->GetDllDir().ClassDir().GetClassList(MPASS_CAM_EFFECT_CLASS_ID);
	if (!subList) return NULL;
	
	int i = subList->GetFirst(ACC_PUBLIC);
	while (i >= 0)
	{
		ClassEntry* c = &(*subList)[i];
		if (c->ClassID() == id)
		{
			IMultiPassCameraEffect* retval = reinterpret_cast<IMultiPassCameraEffect* >(c->CD()->Create(0));
			return retval;
		}
		i = subList->GetNext(ACC_PUBLIC);
	}
	return NULL;
}

CameraObject* CameraImporter::ImportCamera(FCDCamera* _colladaCamera)
{
	colladaCamera = _colladaCamera;

	// Create a camera of the correct type
	bool isTargeted = colladaCamera->HasTarget();
	GenCamera* camera = GetDocument()->GetImportInterface()->CreateCameraObject(isTargeted ? TARGETED_CAMERA : FREE_CAMERA);

	// Create the camera's target
	TargetImporter targetImporter(GetDocument());
	float targetDistance = 100.0f; // default value
	if (isTargeted) 
	{
		targetImporter.ImportTarget(GetParent(), colladaCamera->GetTargetNode());
		targetDistance = targetImporter.GetTargetDistance();
	}
	else
	{
		// do we have a <target_distance> node?
		FCDETechnique* maxTech = colladaCamera->GetExtra()->GetDefaultType()->FindTechnique(DAEMAX_MAX_PROFILE);
		if (maxTech != NULL)
		{
			FCDENode* tdNode = maxTech->FindChildNode(DAEMAX_CAMERA_TARGETDISTANCE_PARAMETER);
			if (tdNode != NULL)
			{
				targetDistance = FUStringConversion::ToFloat(tdNode->GetContent());
			}
		}
	}
	camera->SetTDist(0, targetDistance);

	if (colladaCamera->GetProjectionType() == FCDCamera::PERSPECTIVE)
	{
		// Perspective camera
		camera->SetOrtho(FALSE);
		if (colladaCamera->HasHorizontalFov())
		{
			camera->SetFOV(0, FMath::DegToRad(colladaCamera->GetFovX()));
			ANIM->ImportAnimatedFloat(camera, "FOV", colladaCamera->GetFovX(), &FSConvert::DegToRad);
		}
		else
		{
			FCDConversionScaleFunctor scaleFunctor(FMath::DegToRad(1.0f) * colladaCamera->GetAspectRatio());
			camera->SetFOV(0, scaleFunctor(colladaCamera->GetFovY()));
			ANIM->ImportAnimatedFloat(camera, "FOV", colladaCamera->GetFovY(), &scaleFunctor);
		}
	}
	else
	{
		// Orthographic camera
		camera->SetOrtho(TRUE);

		// Consider only the correct magnification and the target distance,
		// which is kept constant, to calculate the FOV.
		FCDConversionOrthoFOVFunctor orthoConversion(targetDistance);
		const FCDParameterAnimatableFloat& magnification = colladaCamera->HasHorizontalMag() ? colladaCamera->GetMagX() : colladaCamera->GetMagY();
		camera->SetFOV(0, orthoConversion(magnification));
		ANIM->ImportAnimatedFloat(camera, "FOV", magnification, &orthoConversion);
	}

	// Common camera parameters: nearZ, farZ
	// The "Far Clip " animatable parameter intentionally has an extra space in the definition.
	camera->SetClipDist(0, CAM_HITHER_CLIP, colladaCamera->GetNearZ());
	ANIM->ImportAnimatedFloat(camera, "Near Clip", colladaCamera->GetNearZ());
	camera->SetClipDist(0, CAM_YON_CLIP, colladaCamera->GetFarZ());
	ANIM->ImportAnimatedFloat(camera, "Far Clip ", colladaCamera->GetFarZ());
	camera->SetManualClip(TRUE);
	camera->Enable(TRUE);

	// import Multi-Pass effects
	bool hasMotionBlur = false;
	FCDETechnique* extraTech = colladaCamera->GetExtra()->GetDefaultType()->FindTechnique(DAEMAX_MAX_PROFILE);
	if (extraTech != NULL)
	{
		// check for Motion Blur node
		FCDENode* mbNode = extraTech->FindChildNode(DAEMAX_CAMERA_MOTIONBLUR_ELEMENT);
		if (mbNode != NULL)
		{
			// create the Motion Blur effect
			IMultiPassCameraEffect* effect = CreateMultiPassEffect(FMULTI_PASS_MOTION_BLUR_CLASS_ID);
			if (effect != NULL)
			{
				camera->SetMultiPassEffectEnabled(0,TRUE);

				// bias
				ImportExtraParameter(mbNode, effect, DAEMAX_CAMERA_MB_BIAS_PARAMETER,
					MultiPassEffectMB::multiPassMotionBlur_params, MultiPassEffectMB::prm_bias);

				// disable antialiasing
				ImportExtraParameter(mbNode, effect, DAEMAX_CAMERA_MB_DISABLEANTIALIAS_PARAMETER,
					MultiPassEffectMB::multiPassMotionBlur_params, MultiPassEffectMB::prm_disableAntialiasing);

				// disable filtering
				ImportExtraParameter(mbNode, effect, DAEMAX_CAMERA_MB_DISABLEFILTER_PARAMETER,
					MultiPassEffectMB::multiPassMotionBlur_params, MultiPassEffectMB::prm_disableFiltering);

				// display passes
				ImportExtraParameter(mbNode, effect, DAEMAX_CAMERA_MB_DISPLAYPASSES_PARAMETER,
					MultiPassEffectMB::multiPassMotionBlur_params, MultiPassEffectMB::prm_displayPasses);

				// dither strength
				ImportExtraParameter(mbNode, effect, DAEMAX_CAMERA_MB_DITHERSTRENGTH_PARAMETER,
					MultiPassEffectMB::multiPassMotionBlur_params, MultiPassEffectMB::prm_ditherStrength);

				// duration
				ImportExtraParameter(mbNode, effect, DAEMAX_CAMERA_MB_DURATION_PARAMETER,
					MultiPassEffectMB::multiPassMotionBlur_params, MultiPassEffectMB::prm_duration);

				// normalize weights
				ImportExtraParameter(mbNode, effect, DAEMAX_CAMERA_MB_NORMWEIGHTS_PARAMETER,
					MultiPassEffectMB::multiPassMotionBlur_params, MultiPassEffectMB::prm_normalizeWeights);

				// tile size
				ImportExtraParameter(mbNode, effect, DAEMAX_CAMERA_MB_TILESIZE_PARAMETER,
					MultiPassEffectMB::multiPassMotionBlur_params, MultiPassEffectMB::prm_tileSize);

				// total passes
				ImportExtraParameter(mbNode, effect, DAEMAX_CAMERA_MB_TOTALPASSES_PARAMETER,
					MultiPassEffectMB::multiPassMotionBlur_params, MultiPassEffectMB::prm_totalPasses);

				camera->SetIMultiPassCameraEffect(effect);
				hasMotionBlur = true;
			}
		}
	}

	if (!hasMotionBlur && (extraTech = colladaCamera->GetExtra()->GetDefaultType()->FindTechnique(DAE_FCOLLADA_PROFILE)) != NULL)
	{
		// check for Depth of Field
		FCDENode* dofNode = extraTech->FindChildNode(DAEFC_CAMERA_DEPTH_OF_FIELD_ELEMENT);
		if (dofNode != NULL)
		{
			IMultiPassCameraEffect* effect = CreateMultiPassEffect(FMULTI_PASS_DOF_CLASS_ID);
			if (effect != NULL)
			{
				camera->SetMultiPassEffectEnabled(0,TRUE);
				camera->SetIMultiPassCameraEffect(effect);
				
				// disable antialiasing
				ImportExtraParameter(dofNode, effect, DAEFC_CAMERA_DOF_DISANTIALIAS_PARAMETER,
					MultiPassEffectDOF::multiPassDOF_params, MultiPassEffectDOF::prm_disableAntialiasing);

				// disable filtering
				ImportExtraParameter(dofNode, effect, DAEFC_CAMERA_DOF_DISFILTERING_PARAMETER,
					MultiPassEffectDOF::multiPassDOF_params, MultiPassEffectDOF::prm_disableFiltering);

				// display passes
				ImportExtraParameter(dofNode, effect, DAEFC_CAMERA_DOF_DISPLAYPASSES_PARAMETER,
					MultiPassEffectDOF::multiPassDOF_params, MultiPassEffectDOF::prm_displayPasses);

				// dither strength
				ImportExtraParameter(dofNode, effect, DAEFC_CAMERA_DOF_DITHERSTR_PARAMETER,
					MultiPassEffectDOF::multiPassDOF_params, MultiPassEffectDOF::prm_ditherStrength);

				// focal depth
				ImportExtraParameter(dofNode, effect, DAEFC_CAMERA_DOF_FOCALDEPTH_PARAMETER,
					MultiPassEffectDOF::multiPassDOF_params, MultiPassEffectDOF::prm_focalDepth);

				// normalize weights
				ImportExtraParameter(dofNode, effect, DAEFC_CAMERA_DOF_NORMWEIGHTS_PARAMETER,
					MultiPassEffectDOF::multiPassDOF_params, MultiPassEffectDOF::prm_normalizeWeights);

				// sample bias
				ImportExtraParameter(dofNode, effect, DAEFC_CAMERA_DOF_SAMPLEBIAS_PARAMETER,
					MultiPassEffectDOF::multiPassDOF_params, MultiPassEffectDOF::prm_sampleBias);

				// sample radius
				ImportExtraParameter(dofNode, effect, DAEFC_CAMERA_DOF_SAMPLERADIUS_PARAMETER,
					MultiPassEffectDOF::multiPassDOF_params, MultiPassEffectDOF::prm_sampleRadius);

				// tile size
				ImportExtraParameter(dofNode, effect, DAEFC_CAMERA_DOF_TILESIZE_PARAMETER,
					MultiPassEffectDOF::multiPassDOF_params, MultiPassEffectDOF::prm_tileSize);

				// total passes
				ImportExtraParameter(dofNode, effect, DAEFC_CAMERA_DOF_TOTALPASSES_PARAMETER,
					MultiPassEffectDOF::multiPassDOF_params, MultiPassEffectDOF::prm_totalPasses);

				// use original location
				ImportExtraParameter(dofNode, effect, DAEFC_CAMERA_DOF_USEORIGLOC_PARAMETER,
					MultiPassEffectDOF::multiPassDOF_params, MultiPassEffectDOF::prm_useOriginalLocation);

				// use target distance
				ImportExtraParameter(dofNode, effect, DAEFC_CAMERA_DOF_USETARGETDIST_PARAMETER,
					MultiPassEffectDOF::multiPassDOF_params, MultiPassEffectDOF::prm_useTargetDistance);
			}
		}
	}

	cameraObject = camera;
	return camera;
}
