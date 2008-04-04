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
#include "AnimationExporter.h"
#include "DocumentExporter.h"
#include "CameraExporter.h"
#include "EntityExporter.h"
#include "NodeExporter.h"
#include "ColladaOptions.h"
#include "Common/ConversionFunctions.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDCamera.h"
#include "FCDocument/FCDExtra.h"
#include "FCDocument/FCDLibrary.h"

// for the FindProperty static method
// TODO. Move these utility methods in separate files.
#include "AnimationImporter.h"
#include "Common/Camera.h"
#include "Common/CameraMultiPassEffects.h"

// Export Functor - Returns the orthographic left for a camera's FOV
class FCDConversionInverseOrthoFOVFunctor : public FCDConversionFunctor
{
private:
	float targetDistance;
public:
	FCDConversionInverseOrthoFOVFunctor(float _targetDistance) { targetDistance = _targetDistance; }
	virtual ~FCDConversionInverseOrthoFOVFunctor() {}
	virtual float operator() (float fov) { return targetDistance * tanf(fov / 2.0f); }
};


CameraExporter::CameraExporter(DocumentExporter* _document)
:	BaseExporter(_document)
{
}

CameraExporter::~CameraExporter()
{
}

void CameraExporter::ExportExtraParameter(FCDENode *parentNode, const char *colladaID, IParamBlock2 *params, int pid, int maxType, const fm::string &animID)
{
	FCDENode *eNode = parentNode->AddChildNode(colladaID);

	// use the animation number with the GetController method
	// since the parameters enumeration doesn't reflect the param
	// block order.
	int aid = params->GetAnimNum(pid);
	Control *ctrl = params->GetController(aid);

	switch (maxType)
	{
	case TYPE_BOOL:
		eNode->SetContent(TO_STRING((params->GetInt(pid) == TRUE)));
		// NO ANIMATION ON BOOLEANS
		break;
	case TYPE_INT:
		eNode->SetContent(TO_STRING((params->GetInt(pid))));
		ANIM->ExportAnimatedFloat(animID, ctrl, eNode->GetAnimated());
		break;
	case TYPE_FLOAT:
		eNode->SetContent(TO_STRING((params->GetFloat(pid))));
		ANIM->ExportAnimatedFloat(animID, ctrl, eNode->GetAnimated());
		break;
	default:
		break;
	}
}

FCDEntity* CameraExporter::ExportCamera(INode* node, CameraObject* camera)
{
	if (node == NULL || camera == NULL) return NULL;
	if (camera->SuperClassID() != CAMERA_CLASS_ID) return NULL;

	// Retrieve the camera target
	bool isTargeted = camera->ClassID().PartA() == LOOKAT_CAM_CLASS_ID;
	INode* targetNode = !isTargeted ? NULL : node->GetTarget();

	// Create the COLLADA camera
	FCDCamera* colladaCamera = CDOC->GetCameraLibrary()->AddEntity();
	colladaCamera->SetUserHandle(targetNode);
	ENTE->ExportEntity(camera, colladaCamera, (fm::string(node->GetName()) + "-camera").c_str());

	// Retrieve the camera parameters block
	IParamBlock* parameters = (IParamBlock*) camera->GetReference(MaxCamera::PBLOCK_REF);

	// Determine the camera's type
	if (camera->IsOrtho())
	{
		// Calculate the target distance for FOV calculations
		float targetDistance = (targetNode == NULL) ? camera->GetTDist(TIME_EXPORT_START)
			: (NDEX->GetWorldTransform(targetNode).GetTrans() - NDEX->GetWorldTransform(node).GetTrans()).Length();

		colladaCamera->SetProjectionType(FCDCamera::ORTHOGRAPHIC);
		colladaCamera->SetMagX(1.0f); // this value will be replaced, but it sets the 'horizontal FOV' flag.
		FCDConversionInverseOrthoFOVFunctor orthoConvert(targetDistance);
		ANIM->ExportProperty(colladaCamera->GetDaeId() + "-xmag", parameters, MaxCamera::PB_FOV, colladaCamera->GetMagX(), &orthoConvert);
		colladaCamera->SetAspectRatio(1.0f);
	}
	else
	{
		colladaCamera->SetProjectionType(FCDCamera::PERSPECTIVE);
		colladaCamera->SetFovX(1.0f); // this value will be replaced, but it sets the 'horizontal FOV' flag.
		ANIM->ExportProperty(colladaCamera->GetDaeId() + "-xfov", parameters, MaxCamera::PB_FOV, colladaCamera->GetFovX(), &FSConvert::RadToDeg);
		colladaCamera->SetAspectRatio(1.0f);
	}

	// Near/Far clip planes
	ANIM->ExportProperty(colladaCamera->GetDaeId() + "-znear", parameters, MaxCamera::PB_HITHER, colladaCamera->GetNearZ());
	ANIM->ExportProperty(colladaCamera->GetDaeId() + "-zfar", parameters, MaxCamera::PB_YON, colladaCamera->GetFarZ());

	// Multi-Pass effects export
	bool isMultiPass = (camera->GetMultiPassEffectEnabled(0, FOREVER) == TRUE);
	if (isMultiPass)
	{
		IMultiPassCameraEffect *mpEffect = camera->GetIMultiPassCameraEffect();
		if (mpEffect != NULL)
		{
			Class_ID id = mpEffect->ClassID();
			const fm::string& strId = colladaCamera->GetDaeId();

			// the camera could have both effects, but not in Max
			if (id == FMULTI_PASS_MOTION_BLUR_CLASS_ID)
			{
				FCDETechnique *maxETech = colladaCamera->GetExtra()->GetDefaultType()->AddTechnique(DAEMAX_MAX_PROFILE);
				FCDENode * mbNode = maxETech->AddChildNode(DAEMAX_CAMERA_MOTIONBLUR_ELEMENT);

				IParamBlock2 *params = mpEffect->GetParamBlock(0);
				if (params != NULL)
				{
					// display passes
					ExportExtraParameter(
						mbNode, DAEMAX_CAMERA_MB_DISPLAYPASSES_PARAMETER, params, 
						MultiPassEffectMB::prm_displayPasses, TYPE_BOOL, strId + "-displayPasses");

					// total passes
					ExportExtraParameter(
						mbNode, DAEMAX_CAMERA_MB_TOTALPASSES_PARAMETER, params, 
						MultiPassEffectMB::prm_totalPasses, TYPE_INT, strId + "-totalPasses");

					// duration
					ExportExtraParameter(
						mbNode, DAEMAX_CAMERA_MB_DURATION_PARAMETER, params, 
						MultiPassEffectMB::prm_duration, TYPE_FLOAT, strId + "-duration");
	
					// bias
					ExportExtraParameter(
						mbNode, DAEMAX_CAMERA_MB_BIAS_PARAMETER, params, 
						MultiPassEffectMB::prm_bias, TYPE_FLOAT, strId + "-bias");

					// normalized weights
					ExportExtraParameter(
						mbNode, DAEMAX_CAMERA_MB_NORMWEIGHTS_PARAMETER, params, 
						MultiPassEffectMB::prm_normalizeWeights, TYPE_BOOL, strId + "-normWeights");

					// dither strength
					ExportExtraParameter(
						mbNode, DAEMAX_CAMERA_MB_DITHERSTRENGTH_PARAMETER, params, 
						MultiPassEffectMB::prm_ditherStrength, TYPE_FLOAT, strId + "-ditherStr");

					// tile size
					ExportExtraParameter(
						mbNode, DAEMAX_CAMERA_MB_TILESIZE_PARAMETER, params, 
						MultiPassEffectMB::prm_tileSize, TYPE_INT, strId + "-tileSize");

					// disable filtering
					ExportExtraParameter(
						mbNode, DAEMAX_CAMERA_MB_DISABLEFILTER_PARAMETER, params, 
						MultiPassEffectMB::prm_disableFiltering, TYPE_BOOL, strId + "-disableFiltering");

					// disable antialiasing
					ExportExtraParameter(
						mbNode, DAEMAX_CAMERA_MB_DISABLEANTIALIAS_PARAMETER, params, 
						MultiPassEffectMB::prm_disableAntialiasing, TYPE_BOOL, strId + "-disableAntialiasing");
				}
			}
			else if (id == FMULTI_PASS_DOF_CLASS_ID)
			{
				FCDETechnique *fcETech = colladaCamera->GetExtra()->GetDefaultType()->AddTechnique(DAE_FCOLLADA_PROFILE);
				FCDENode * dofNode = fcETech->AddChildNode(DAEFC_CAMERA_DEPTH_OF_FIELD_ELEMENT);

				IParamBlock2 *params = mpEffect->GetParamBlock(0);
				if (params != NULL)
				{
					// use target distance
					ExportExtraParameter(
						dofNode, DAEFC_CAMERA_DOF_USETARGETDIST_PARAMETER, params, 
						MultiPassEffectDOF::prm_useTargetDistance, TYPE_BOOL, strId + "-useTargetDist");

					// focal depth
					ExportExtraParameter(
						dofNode, DAEFC_CAMERA_DOF_FOCALDEPTH_PARAMETER, params, 
						MultiPassEffectDOF::prm_focalDepth, TYPE_FLOAT, strId + "-focalDepth");

					// display passes
					ExportExtraParameter(
						dofNode, DAEFC_CAMERA_DOF_DISPLAYPASSES_PARAMETER, params, 
						MultiPassEffectDOF::prm_displayPasses, TYPE_BOOL, strId + "-displayPasses");

					// use original location
					ExportExtraParameter(
						dofNode, DAEFC_CAMERA_DOF_USEORIGLOC_PARAMETER, params, 
						MultiPassEffectDOF::prm_useOriginalLocation, TYPE_BOOL, strId + "-useOrigLoc");
					
					// total passes
					ExportExtraParameter(
						dofNode, DAEFC_CAMERA_DOF_TOTALPASSES_PARAMETER, params, 
						MultiPassEffectDOF::prm_totalPasses, TYPE_INT, strId + "-totalPasses");

					// sample radius
					ExportExtraParameter(
						dofNode, DAEFC_CAMERA_DOF_SAMPLERADIUS_PARAMETER, params, 
						MultiPassEffectDOF::prm_sampleRadius, TYPE_FLOAT, strId + "-sampleRadius");

					// sample bias
					ExportExtraParameter(
						dofNode, DAEFC_CAMERA_DOF_SAMPLEBIAS_PARAMETER, params, 
						MultiPassEffectDOF::prm_sampleBias, TYPE_FLOAT, strId + "-sampleBias");

					// normalize weights
					ExportExtraParameter(
						dofNode, DAEFC_CAMERA_DOF_NORMWEIGHTS_PARAMETER, params, 
						MultiPassEffectDOF::prm_normalizeWeights, TYPE_BOOL, strId + "-normWeights");

					// dither strength
					ExportExtraParameter(
						dofNode, DAEFC_CAMERA_DOF_DITHERSTR_PARAMETER, params, 
						MultiPassEffectDOF::prm_ditherStrength, TYPE_FLOAT, strId + "-ditherStr");

					// tile size
					ExportExtraParameter(
						dofNode, DAEFC_CAMERA_DOF_TILESIZE_PARAMETER, params, 
						MultiPassEffectDOF::prm_tileSize, TYPE_INT, strId + "-tileSize");

					// disable filtering
					ExportExtraParameter(
						dofNode, DAEFC_CAMERA_DOF_DISFILTERING_PARAMETER, params, 
						MultiPassEffectDOF::prm_disableFiltering, TYPE_BOOL, strId + "-disableFiltering");

					// disable antialiasing
					ExportExtraParameter(
						dofNode, DAEFC_CAMERA_DOF_DISANTIALIAS_PARAMETER, params, 
						MultiPassEffectDOF::prm_disableAntialiasing, TYPE_BOOL, strId + "-disableAntialiasing");

					// finally export the Target Distance parameter in a Max extra node
					// we do not support the animation of this parameter.
					FCDETechnique *maxETech = colladaCamera->GetExtra()->GetDefaultType()->AddTechnique(DAEMAX_MAX_PROFILE);
					maxETech->AddParameter(DAEMAX_CAMERA_TARGETDISTANCE_PARAMETER, camera->GetTDist(0));
				}
			}
		}
	}

	return colladaCamera;
}

void CameraExporter::CalculateForcedNodes(INode* node, CameraObject* camera, NodeList& forcedNodes)
{
	if (node == NULL || camera == NULL || camera->SuperClassID() != CAMERA_CLASS_ID) return;

	// Retrieve the camera target
	bool isTargeted = camera->ClassID().PartA() == LOOKAT_CAM_CLASS_ID;
	if (isTargeted)
	{
		INode* targetNode = node->GetTarget();

		// Add to the list of nodes that must be exported (even if hidden), the cameras targets.
		if (targetNode != NULL) forcedNodes.push_back(targetNode);
	}
}

void CameraExporter::LinkCameras()
{
	// Once the scene graph is fully exported: connect the targeted cameras with their targets
	size_t cameraCount = CDOC->GetCameraLibrary()->GetEntityCount();
	for (size_t index = 0; index < cameraCount; ++index)
	{
		FCDCamera* colladaCamera = CDOC->GetCameraLibrary()->GetEntity(index);
		INode* targetNode = (INode*) colladaCamera->GetUserHandle();
		if (targetNode == NULL) continue;
		FCDSceneNode* colladaTarget = document->GetNodeExporter()->FindExportedNode(targetNode);
		FUAssert(colladaTarget != NULL, continue);
		colladaCamera->SetTargetNode(colladaTarget);
	}
}
