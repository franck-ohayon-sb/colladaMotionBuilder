/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"

// COLLADA Texture Library
//
#include <maya/MFnLambertShader.h>
#include <maya/MFnPhongShader.h>
#include <maya/MDGModifier.h>
#include "TranslatorHelpers/CDagHelper.h"
#include "TranslatorHelpers/CShaderHelper.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDEffectParameter.h"
#include "FCDocument/FCDEffectParameterFactory.h"
#include "FCDocument/FCDEffectParameterSampler.h"
#include "FCDocument/FCDEffectParameterSurface.h"
#include "FCDocument/FCDEffect.h"
#include "FCDocument/FCDExtra.h"
#include "FCDocument/FCDImage.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDTexture.h"
//#include "FUtils/FUDaeParser.h"
//using namespace FUDaeParser;

DaeTextureLibrary::DaeTextureLibrary(DaeDoc* _doc)
:	doc(_doc)
{
}

DaeTextureLibrary::~DaeTextureLibrary()
{
}

void DaeTextureLibrary::ExportTexture(FCDTexture* colladaTexture, const MObject& texture, int blendMode, int textureIndex)
{
	FCDImage* colladaImage = ExportImage(colladaTexture->GetDocument(), texture);
	colladaTexture->SetImage(colladaImage);
	FCDETechnique* colladaMayaTechnique = colladaTexture->GetExtra()->GetDefaultType()->AddTechnique(DAEMAYA_MAYA_PROFILE);

	// Add 2D placement parameters
	Add2DPlacement(colladaTexture, texture);

	// Check for 3D projection node
	MObject colorReceiver = CDagHelper::GetSourceNodeConnectedTo(texture, "outColor");
	if (colorReceiver != MObject::kNullObj && colorReceiver.apiType() == MFn::kProjection)
	{
		Add3DProjection(colladaTexture, colorReceiver);
	}
	
	// Add blend mode information
	const char* blendModeString = "";
	switch (blendMode)
	{
	case 0: blendModeString = FUDaeBlendMode::ToString(FUDaeBlendMode::NONE); break;
	case 1: blendModeString = FUDaeBlendMode::ToString(FUDaeBlendMode::OVER); break;
	case 2: blendModeString = FUDaeBlendMode::ToString(FUDaeBlendMode::IN); break;
	case 3: blendModeString = FUDaeBlendMode::ToString(FUDaeBlendMode::OUT); break;
	case 4: blendModeString = FUDaeBlendMode::ToString(FUDaeBlendMode::ADD); break;
	case 5: blendModeString = FUDaeBlendMode::ToString(FUDaeBlendMode::SUBTRACT); break;
	case 6: blendModeString = FUDaeBlendMode::ToString(FUDaeBlendMode::MULTIPLY); break;
	case 7: blendModeString = FUDaeBlendMode::ToString(FUDaeBlendMode::DIFFERENCE); break;
	case 8: blendModeString = FUDaeBlendMode::ToString(FUDaeBlendMode::LIGHTEN); break;
	case 9: blendModeString = FUDaeBlendMode::ToString(FUDaeBlendMode::DARKEN); break;
	case 10: blendModeString = FUDaeBlendMode::ToString(FUDaeBlendMode::SATURATE); break;
	case 11: blendModeString = FUDaeBlendMode::ToString(FUDaeBlendMode::DESATURATE); break;
	case 12: blendModeString = FUDaeBlendMode::ToString(FUDaeBlendMode::ILLUMINATE); break;
	default: break;
	}
	colladaMayaTechnique->AddParameter(DAEMAYA_TEXTURE_BLENDMODE_PARAMETER, blendModeString);

	// Figure out the UV set index used by this texture.
	FUSStringBuilder builder("TEX"); builder.append(textureIndex);
	colladaTexture->GetSet()->SetSemantic(builder.ToCharPtr());
}


// Export an image
FCDImage* DaeTextureLibrary::ExportImage(FCDocument* colladaDocument, const MObject& texture)
{
	// Retrieve the texture filename
    MFnDependencyNode dgFn(texture);
	MString mayaName = dgFn.name(), filename;
	MPlug filenamePlug = dgFn.findPlug("fileTextureName");
	filenamePlug.getValue(filename);
	if (filename.length() == 0) return NULL;

	// Have we seen this texture node before?
	NameImageMap::iterator it = exportedImages.find(filename);
	if (it != exportedImages.end()) return (*it).second;

	// Create a new image structure
	FCDImage* colladaImage = CDOC->GetImageLibrary()->AddEntity();
	fstring colladaImageName = MConvert::ToFChar(doc->MayaNameToColladaName(dgFn.name()));
	colladaImage->SetName(colladaImageName);
	colladaImage->SetDaeId(TO_STRING(colladaImageName));
	colladaImage->SetFilename(MConvert::ToFChar(filename));
	
	// Export the node type, because PSD textures don't behave the same as File textures.
	const char* nodeType = texture.hasFn(MFn::kPsdFileTexture) ? DAEMAYA_TEXTURE_PSDTEXTURE : DAEMAYA_TEXTURE_FILETEXTURE;
	FCDETechnique* colladaMayaTechnique = colladaImage->GetExtra()->GetDefaultType()->AddTechnique(DAEMAYA_MAYA_PROFILE);
	colladaMayaTechnique->AddParameter(DAEMAYA_TEXTURE_NODETYPE, nodeType);

	// Export whether this image is in fact an image sequence
	MPlug imgSeqPlug = dgFn.findPlug("useFrameExtension");
	bool isImgSeq = false;
	imgSeqPlug.getValue(isImgSeq);
	colladaMayaTechnique->AddParameter(DAEMAYA_TEXTURE_IMAGE_SEQUENCE, FUStringConversion::ToString(isImgSeq));

	// Add this texture to our list of exported images
	exportedImages[filename] = colladaImage;
	return colladaImage;
}

#define ADDBOOLPARAM(plugName, colladaName) \
		plug = placement2d.findPlug(plugName, &status); \
		if (status == MStatus::kSuccess) { \
			bool value = false; \
			plug.getValue(value); \
			FCDENode* colladaParameter = colladaMayaTechnique->AddParameter(colladaName, value); \
			ANIM->AddPlugAnimation(placementNode, plugName, colladaParameter->GetAnimated(), kBoolean); }

#define ADDFLOATPARAM(plugName, colladaName) \
		plug = placement2d.findPlug(plugName, &status); \
		if (status == MStatus::kSuccess) { \
			float value; \
			plug.getValue(value); \
			FCDENode* colladaParameter = colladaMayaTechnique->AddParameter(colladaName, value); \
			ANIM->AddPlugAnimation(placementNode, plugName, colladaParameter->GetAnimated(), kSingle); }

#define ADDANGLEPARAM(plugName, colladaName) \
		plug = placement2d.findPlug(plugName, &status); \
		if (status == MStatus::kSuccess) { \
			float value; \
			plug.getValue(value); \
			FCDENode* colladaParameter = colladaMayaTechnique->AddParameter(colladaName, FMath::RadToDeg(value)); \
			ANIM->AddPlugAnimation(placementNode, plugName, colladaParameter->GetAnimated(), kSingle | kAngle); }

// Helper to dump a place2dTexture node
void DaeTextureLibrary::Add2DPlacement(FCDTexture* colladaTexture, MObject texture)
{
	// Is there a texture placement applied to this texture?
	MObject placementNode = CDagHelper::GetSourceNodeConnectedTo(texture, "uvCoord");
	if (placementNode.hasFn(MFn::kPlace2dTexture))
	{
		MStatus status;
		MFnDependencyNode placement2d(placementNode);		
		MPlug plug;
		FCDETechnique* colladaMayaTechnique = colladaTexture->GetExtra()->GetDefaultType()->AddTechnique(DAEMAYA_MAYA_PROFILE);

		ADDBOOLPARAM("wrapU", DAEMAYA_TEXTURE_WRAPU_PARAMETER);
		ADDBOOLPARAM("wrapV", DAEMAYA_TEXTURE_WRAPV_PARAMETER);
		ADDBOOLPARAM("mirrorU", DAEMAYA_TEXTURE_MIRRORU_PARAMETER);
		ADDBOOLPARAM("mirrorV", DAEMAYA_TEXTURE_MIRRORV_PARAMETER);

		ADDFLOATPARAM("coverageU", DAEMAYA_TEXTURE_COVERAGEU_PARAMETER);
		ADDFLOATPARAM("coverageV", DAEMAYA_TEXTURE_COVERAGEV_PARAMETER);
		ADDFLOATPARAM("translateFrameU", DAEMAYA_TEXTURE_TRANSFRAMEU_PARAMETER);
		ADDFLOATPARAM("translateFrameV", DAEMAYA_TEXTURE_TRANSFRAMEV_PARAMETER);
		ADDANGLEPARAM("rotateFrame", DAEMAYA_TEXTURE_ROTFRAME_PARAMETER);

		ADDBOOLPARAM("stagger", DAEMAYA_TEXTURE_STAGGER_PARAMETER);
		ADDBOOLPARAM("fast", DAEMAYA_TEXTURE_FAST_PARAMETER);
		ADDFLOATPARAM("repeatU", DAEMAYA_TEXTURE_REPEATU_PARAMETER);
		ADDFLOATPARAM("repeatV", DAEMAYA_TEXTURE_REPEATV_PARAMETER);
		ADDFLOATPARAM("offsetU", DAEMAYA_TEXTURE_OFFSETU_PARAMETER);
		ADDFLOATPARAM("offsetV", DAEMAYA_TEXTURE_OFFSETV_PARAMETER);
		ADDANGLEPARAM("rotateUV", DAEMAYA_TEXTURE_ROTATEUV_PARAMETER);
		ADDFLOATPARAM("noiseU", DAEMAYA_TEXTURE_NOISEU_PARAMETER);
		ADDFLOATPARAM("noiseV", DAEMAYA_TEXTURE_NOISEV_PARAMETER);
	}
}

#undef ADDANGLEPARAM
#undef ADDFLOATPARAM
#undef ADDBOOLPARAM

void DaeTextureLibrary::Import2DPlacement(const FCDTexture* sampler, MObject& placementNode, fm::vector<MObject>& existingFileNodes)
{
	if (placementNode == MObject::kNullObj) return;

	// Retrieve the MAYA technique, if there is a sampler.
	// Otherwise, we will assign default values.
	FCDETechnique* colladaMayaTechnique = NULL;
	if (sampler != NULL && sampler->GetExtra() != NULL)
	{
		colladaMayaTechnique = const_cast<FCDETechnique*>(sampler->GetExtra()->GetDefaultType()->FindTechnique(DAEMAYA_MAYA_PROFILE));
	}

	// Make up a list of the existing placement nodes: these are more interesting than the file nodes.
	size_t nodeCount = existingFileNodes.size();
	fm::vector<MObject> placementNodes(nodeCount, MObject::kNullObj);
	BooleanList validity(nodeCount, true);
	for (uint i = 0; i < nodeCount; ++i)
	{
		placementNodes[i] = CDagHelper::GetNodeConnectedTo(existingFileNodes[i], "wrapU");
	}

#define COMPARE_VALUE(type, plugName) { \
		for (uint i = 0; i < nodeCount; ++i) {\
			type oldValue = value, newValue; \
			CDagHelper::GetPlugValue(placementNodes[i], plugName, newValue); \
			validity[i] |= IsEquivalent(oldValue, newValue); } }

#define BOOLPARAM(plugName, paramName, defaultV) { \
		bool value = defaultV; \
		FCDENode* node = colladaMayaTechnique != NULL ? colladaMayaTechnique->FindParameter(paramName) : NULL; \
		if (node != NULL) { \
			MPlug plug = CShaderHelper::FindPlug(placementNode, plugName); \
			value = FUStringConversion::ToBoolean(node->GetContent()); \
            ANIM->AddPlugAnimation(placementNode, plugName, node->GetAnimated(), kBoolean); \
			plug.setValue(value); } \
		COMPARE_VALUE(bool, plugName); }

#define FLOATPARAM(plugName, paramName, defaultV) { \
		float value = defaultV; \
		FCDENode* node = colladaMayaTechnique != NULL ? colladaMayaTechnique->FindParameter(paramName) : NULL; \
		if (node != NULL) { \
			MPlug plug = CShaderHelper::FindPlug(placementNode, plugName); \
			value = FUStringConversion::ToFloat(node->GetContent()); \
			ANIM->AddPlugAnimation(placementNode, plugName, node->GetAnimated(), kSingle); \
			plug.setValue(value); } \
		COMPARE_VALUE(float, plugName); }
	
#define ANGLEPARAM(plugName, paramName, defaultV) { \
		float value = defaultV; \
		FCDENode* node = colladaMayaTechnique != NULL ? colladaMayaTechnique->FindParameter(paramName) : NULL; \
		if (node != NULL) { \
			MPlug plug = CShaderHelper::FindPlug(placementNode, plugName); \
			value = FUStringConversion::ToFloat(node->GetContent()); \
			ANIM->AddPlugAnimation(placementNode, plugName, node->GetAnimated(), kSingle | kAngle); \
			plug.setValue(FMath::DegToRad(value)); } \
		COMPARE_VALUE(float, plugName); }


		BOOLPARAM("wrapU", DAEMAYA_TEXTURE_WRAPU_PARAMETER, true);
		BOOLPARAM("wrapV", DAEMAYA_TEXTURE_WRAPV_PARAMETER, true);
		BOOLPARAM("mirrorU", DAEMAYA_TEXTURE_MIRRORU_PARAMETER, false);
		BOOLPARAM("mirrorV", DAEMAYA_TEXTURE_MIRRORV_PARAMETER, false);

		FLOATPARAM("coverageU", DAEMAYA_TEXTURE_COVERAGEU_PARAMETER, 1.0f);
		FLOATPARAM("coverageV", DAEMAYA_TEXTURE_COVERAGEV_PARAMETER, 1.0f);
		FLOATPARAM("translateFrameU", DAEMAYA_TEXTURE_TRANSFRAMEU_PARAMETER, 0.0f);
		FLOATPARAM("translateFrameV", DAEMAYA_TEXTURE_TRANSFRAMEV_PARAMETER, 0.0f);
		ANGLEPARAM("rotateFrame", DAEMAYA_TEXTURE_ROTFRAME_PARAMETER, 0.0f);

		BOOLPARAM("stagger", DAEMAYA_TEXTURE_STAGGER_PARAMETER, false);
		BOOLPARAM("fast", DAEMAYA_TEXTURE_FAST_PARAMETER, false);
		FLOATPARAM("repeatU", DAEMAYA_TEXTURE_REPEATU_PARAMETER, 1.0f);
		FLOATPARAM("repeatV", DAEMAYA_TEXTURE_REPEATV_PARAMETER, 1.0f);
		FLOATPARAM("offsetU", DAEMAYA_TEXTURE_OFFSETU_PARAMETER, 0.0f);
		FLOATPARAM("offsetV", DAEMAYA_TEXTURE_OFFSETV_PARAMETER, 0.0f);
		ANGLEPARAM("rotateUV", DAEMAYA_TEXTURE_ROTATEUV_PARAMETER, 0.0f);
		FLOATPARAM("noiseU", DAEMAYA_TEXTURE_NOISEU_PARAMETER, 0.0f);
		FLOATPARAM("noiseV", DAEMAYA_TEXTURE_NOISEV_PARAMETER, 0.0f);

#undef ANGLEPARAM
#undef BOOLPARAM
#undef FLOATPARAM

	// Remove the existing file nodes that didn't survive the comparison.
	for (intptr_t i = nodeCount - 1; i >= 0; --i)
	{
		if (!validity[i]) existingFileNodes.erase((uint) i);
	}
}

void DaeTextureLibrary::Add3DProjection(FCDTexture* colladaTexture, MObject projection)
{
	FCDETechnique* colladaMayaTechnique = colladaTexture->GetExtra()->GetDefaultType()->AddTechnique(DAEMAYA_MAYA_PROFILE);
	FCDENode* projectionExtra = colladaMayaTechnique->AddChildNode(DAEMAYA_PROJECTION_ELEMENT);

	int projectionType;
	CDagHelper::GetPlugValue(projection, "projType", projectionType);
	projectionExtra->AddParameter(DAEMAYA_PROJECTION_TYPE_PARAMETER, CShaderHelper::ProjectionTypeToString(projectionType));

	MMatrix projectionMx;
	CDagHelper::GetPlugValue(projection, "placementMatrix", projectionMx);
	projectionExtra->AddParameter(DAEMAYA_PROJECTION_MATRIX_PARAMETER, MConvert::ToFMatrix(projectionMx));
}

MObject DaeTextureLibrary::Import3DProjection(const FCDTexture* sampler, MObject texture)
{
	// Retrieve the extra projection information element
	const FCDETechnique* colladaMayaTechnique = sampler->GetExtra()->GetDefaultType()->FindTechnique(DAEMAYA_MAYA_PROFILE);
	if (colladaMayaTechnique == NULL) return MObject::kNullObj;
	const FCDENode* projectionExtra = colladaMayaTechnique->FindChildNode(DAEMAYA_PROJECTION_ELEMENT);
	if (projectionExtra == NULL) return MObject::kNullObj;

	// First, gather the parameters
	int projectionType = 0;
	FMMatrix44 projectionMatrix;
	const FCDENode* projectionTypeNode = projectionExtra->FindParameter(DAEMAYA_PROJECTION_TYPE_PARAMETER);
	if (projectionTypeNode != NULL)
	{
		projectionType = CShaderHelper::ToProjectionType(projectionTypeNode->GetContent());
	}
	const FCDENode* projectionMatrixNode = projectionExtra->FindParameter(DAEMAYA_PROJECTION_MATRIX_PARAMETER);
	if (projectionMatrixNode != NULL)
	{
		FUStringConversion::ToMatrix(projectionMatrixNode->GetContent(), projectionMatrix);
	}

	// Create the nodes and connect them
	MFnDependencyNode projectionFn;
	MObject projectionNode = projectionFn.create("projection");
	CDagHelper::SetPlugValue(projectionNode, "projType", projectionType);

	MFnDagNode placement3dFn;
	MObject placement3dNode = placement3dFn.create("place3dTexture");
	MFnTransform t(placement3dNode);
	MMatrix _projMx = MConvert::ToMMatrix(projectionMatrix);
	t.set(MTransformationMatrix(_projMx.inverse()));
	MPlug worldMxArray = CShaderHelper::FindPlug(placement3dNode, "worldInverseMatrix");
	MPlug worldMx = worldMxArray.elementByLogicalIndex(0);
	CDagHelper::Connect(worldMx, projectionNode, "placementMatrix");
	CDagHelper::Connect(texture, "outColor", projectionNode, "image");

	return projectionNode;
}

// Creates a new file texture node whether one exists already or not.
// This is needed for multiple UV sets support
//
bool DaeTextureLibrary::ImportTexture(DaeTexture& texture, const FCDTexture* sampler)
{
	if (sampler == NULL || sampler->GetImage() == NULL) return false;

	texture.frontObject = texture.textureObject = ImportImage(sampler->GetImage(), sampler);
	if (texture.textureObject == MObject::kNullObj) return false;

	// Import the blend mode
	texture.blendMode = (int) FUDaeBlendMode::DEFAULT;
	const FCDETechnique* colladaMayaTechnique = sampler->GetExtra()->GetDefaultType()->FindTechnique(DAEMAYA_MAYA_PROFILE);
	if (colladaMayaTechnique != NULL)
	{
		const FCDENode* blendModeExtra = colladaMayaTechnique->FindParameter(DAEMAYA_TEXTURE_BLENDMODE_PARAMETER);
		if (blendModeExtra != NULL)
		{
			texture.blendMode = (int) FUDaeBlendMode::FromString(TO_STRING(blendModeExtra->GetContent()));
		}
	}

	// Import the texture placement nodes
	MObject projectionObject = Import3DProjection(sampler, texture.frontObject);
	if (!projectionObject.isNull()) texture.frontObject = projectionObject;

	return true;
}

//
// Create/Re-create an image node
//
MObject DaeTextureLibrary::ImportImage(const FCDImage* image, const FCDTexture* sampler)
{
	MStatus rc;

	if (image->GetName().empty() || image->GetFilename().empty()) return MObject::kNullObj;
	MString filename = image->GetFilename().c_str();

	// Look for the list of file texture nodes already imported with this filename.
	fm::vector<MObject> existingFileNodes;
	ImportedFileMap::iterator itImage = importedImages.find(filename);
	if (itImage != importedImages.end()) existingFileNodes = (*itImage).second;

	const char* nodeType = "file";
	bool isImgSequence = false;

	// Check for a ColladaMaya node type.
	const FCDETechnique* colladaMayaTechnique = image->GetExtra()->GetDefaultType()->FindTechnique(DAEMAYA_MAYA_PROFILE);
	if (colladaMayaTechnique != NULL)
	{
		const FCDENode* parameterNode = colladaMayaTechnique->FindParameter(DAEMAYA_TEXTURE_NODETYPE);
		fm::string nodeTypeStr = TO_STRING(parameterNode->GetContent());
		if (IsEquivalent(nodeTypeStr, DAEMAYA_TEXTURE_PSDTEXTURE)) nodeType = "psdFileTex";
		else if (IsEquivalent(nodeTypeStr, DAEMAYA_TEXTURE_FILETEXTURE)) nodeType = "file"; // redundant..

		// check for image sequence
		parameterNode = colladaMayaTechnique->FindParameter(DAEMAYA_TEXTURE_IMAGE_SEQUENCE);
		if (parameterNode != NULL) // backward compatibility: makes it pass CTF
		{
			isImgSequence = FUStringConversion::ToBoolean(parameterNode->GetContent());
		}
	}

	// Now create and configure the Maya file texture node ...
	// Create it using MEL command because this will setup node so that it will display in Hypershade.
	//
	MString cmd = MString("shadingNode -asTexture -name \"") + image->GetName().c_str() + "\"" + nodeType;
	MString result;
	MGlobal::executeCommand(cmd, result);

	MObject fileTexture = CDagHelper::GetNode(result);
	MFnDependencyNode dgFn(fileTexture, &rc);
	if (rc == MStatus::kSuccess)
	{
		MString path = image->GetFilename().c_str();
		MPlug plug = dgFn.findPlug("fileTextureName");
		plug.setValue(path);

		plug = dgFn.findPlug("useFrameExtension");
		plug.setValue(isImgSequence);
	}
	else
	{
		MGlobal::displayError(MString("Error creating texture file node"));
	}

	// Import the file texture sampling parameters.
	MObject placementNode = CShaderHelper::Create2DPlacementNode(fileTexture);
	Import2DPlacement(sampler, placementNode, existingFileNodes);
	if (!existingFileNodes.empty())
	{
		// Delete the newly created placement and file nodes.
		// Re-use this existing file node.
		MGlobal::executeCommand(MString("delete ") + MFnDependencyNode(placementNode).name());
		MGlobal::executeCommand(MString("delete ") + MFnDependencyNode(fileTexture).name());
		return existingFileNodes[0];
	}

	// Add this file texture node to the list.
	if (itImage != importedImages.end())
	{
		(*itImage).second.push_back(fileTexture);
	}
	else
	{
		importedImages.insert(filename, fm::vector<MObject>(&fileTexture, 1));
	}
	return fileTexture;
}
