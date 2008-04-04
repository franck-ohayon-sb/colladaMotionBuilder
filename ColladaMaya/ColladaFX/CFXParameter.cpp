/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include <maya/MAnimControl.h>
#include <maya/MCommandResult.h>
#include <maya/MImage.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include "CFXParameter.h"
#include "CFXShaderNode.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "DDSFormat.h"
#include "FUtils/FUFileManager.h"
#include "FUtils/FUFile.h"

struct DDS_IMAGE_DATA
{
    GLsizei  width;
    GLsizei  height;
    GLenum   format;
    int      numMipMaps;
    GLubyte *pixels;
};

//
// Inline functions/macros
//
inline MString GetTextureNameFromPlug(const MPlug& plug)
{
	MString filename;
	if (plug.isConnected())
	{
		MObject fileTextureNode = CDagHelper::GetNodeConnectedTo(plug);
		CDagHelper::GetPlugValue(fileTextureNode, "fileTextureName", filename);
	}
	return filename;
}

//
// CFXParameter
//

CFXParameter::CFXParameter(CFXShaderNode* _parent)
:	parent(_parent), m_parameter(0)
,	textureOffset(0), textureid(0)
,	m_r(0.0f), m_g(0.0f), m_b(0.0f)
,	m_target(kNONE), m_source(FC("null")), uiType(FC("Color"))
,	attribute(MObject::kNullObj), dirty(true), name(FC(""))
,	UIName(FC(""))
{
}

bool CFXParameter::create(CGparameter parameter)
{
	m_parameter = parameter;

	// Generate the wanted name for this attribute
	name = cgGetParameterName(parameter);
	const char* dotToken = strrchr(name.asChar(), '.');
	attributeName = MString((dotToken == NULL) ? name.asChar() : (dotToken + 1));

	// Look for an attribute with this name.
	MFnDependencyNode shaderFn(parent->thisMObject());
	MObject oldAttribute = shaderFn.findPlug(attributeName).attribute();

	// default program entry, will be changed if it is an effect parameter fed to a program
	m_programEntry = attributeName;

	type = convertCGtype(cgGetParameterType(parameter));
	semantic = convertCGsemantic(cgGetParameterSemantic(parameter));

	// create and store the maya attribute
	MStatus status;

	MFnNumericAttribute nAttr;
	MFnTypedAttribute tAttr;
	MFnNumericAttribute nOldAttr(oldAttribute);
	MFnTypedAttribute tOldAttr(oldAttribute);

	// Create a new plug
	switch (type)
	{
	case kBool:
		if (!oldAttribute.isNull() && oldAttribute.hasFn(MFn::kNumericAttribute) && nOldAttr.unitType() == MFnNumericData::kBoolean)
		{
			attribute = oldAttribute;
		}
		else
		{
			attribute = nAttr.create(attributeName, attributeName, MFnNumericData::kBoolean);
		}
		break;

	case kFloat:
	case kHalf:
		if (!oldAttribute.isNull() && oldAttribute.hasFn(MFn::kNumericAttribute) && nOldAttr.unitType() == MFnNumericData::kFloat)
		{
			attribute = oldAttribute;
		}
		else
		{
			attribute = nAttr.create(attributeName, attributeName, MFnNumericData::kFloat);
			nAttr.setDefault(0.5f); nAttr.setMin(0.0f); nAttr.setMax(1.0f);
			nAttr.setKeyable(true);
		}
		break;

	case kFloat2:
	case kHalf2:
		if (!oldAttribute.isNull() && oldAttribute.hasFn(MFn::kNumericAttribute) && nOldAttr.unitType() == MFnNumericData::k2Float)
		{
			attribute = oldAttribute;
		}
		else
		{
			attribute = nAttr.create(attributeName, attributeName, MFnNumericData::k2Float);
			nAttr.setDefault(0.5f, 0.5f);
			nAttr.setKeyable(true);
		}
		break;

	case kFloat3:
	case kHalf3:
	case kFloat4:
	case kHalf4:
		if (!oldAttribute.isNull() && oldAttribute.hasFn(MFn::kNumericAttribute) && (nOldAttr.unitType() == MFnNumericData::k3Double || nOldAttr.unitType() == MFnNumericData::k3Float))
		{
			attribute = oldAttribute;
		}
		else
		{
			attribute = nAttr.createColor(attributeName, attributeName);
			nAttr.setDefault(0.5f, 0.5f, 0.5f);
			nAttr.setKeyable(true);
			readUIWidget();
		}
		break;

	case kFloat4x4:
	case kHalf4x4:
		if (!oldAttribute.isNull() && oldAttribute.hasFn(MFn::kTypedAttribute) && tOldAttr.attrType() == MFnData::kMatrix)
		{
			attribute = oldAttribute;
		}
		else
		{
			attribute = tAttr.create(attributeName, attributeName, MFnData::kMatrix);
			MFnMatrixData dataFn;
			MObject identityMx = dataFn.create(MMatrix::identity);
			tAttr.setDefault(identityMx);
		}
		break;

	case kSampler2D:
	case kSamplerCUBE:
		{
			if (!oldAttribute.isNull() && oldAttribute.hasFn(MFn::kNumericAttribute) && (nOldAttr.unitType() == MFnNumericData::k3Double || nOldAttr.unitType() == MFnNumericData::k3Float))
			{
				attribute = oldAttribute;
			}
			else
			{
				attribute = nAttr.createColor(attributeName, attributeName);
				nAttr.setDefault(0.5f, 0.5f, 0.5f);
			}
			break;
		}

	case kSurface:
	default:
		if (!oldAttribute.isNull() && oldAttribute.hasFn(MFn::kTypedAttribute) && tOldAttr.attrType() == MFnData::kString)
		{
			attribute = oldAttribute;
		}
		else
		{
			attribute = tAttr.create(attributeName, attributeName, MFnData::kString);
		}
		break;
	}

	if (attribute != oldAttribute && !oldAttribute.isNull())
	{
		shaderFn.removeAttribute(oldAttribute);
	}

	return attribute != oldAttribute; // needs to be added after parsing the CgFX informations.
}

CFXParameter::~CFXParameter()
{
	// Strangely: don't remove the attribute on all destructions.

	if (isTexture() && textureid > 0)
	{
		glActiveTexture(GL_TEXTURE0_ARB + textureOffset);
		glDeleteTextures(1, &textureid);
	}
}

void CFXParameter::addAttribute()
{
	if (parent != NULL)
	{
		MObject o = parent->thisMObject();
		if (!o.isNull())
		{
			MFnDependencyNode parentFn(o);
			parentFn.addAttribute(attribute);

			// For the time semantic, connect to the time node.
			if (semantic == kTIME)
			{
				MPlug timePlug = getPlug();
				CDagHelper::CreateExpression(timePlug, "currentTime -q");
			}
		}
	}
}

void CFXParameter::removeAttribute()
{
	if (parent != NULL)
	{
		MObject o = parent->thisMObject();
		if (!o.isNull())
		{
			MFnDependencyNode parentFn(o);
			attribute = parentFn.findPlug(attributeName).attribute();
			if (!attribute.isNull())
			{
				parentFn.removeAttribute(attribute);
				attribute = MObject::kNullObj;
			}
			attributeName = "";
		}
	}
}

void CFXParameter::updateMeshData(const float * vertexArray, const float ** colorArrays, int id[10], int colorCount, const float ** normalArrays, int normalId, int normalCount, const float ** uvArrays, int uvId, int uvCount, int isUV[10], MString setName[10])
{	
	switch (semantic) 
	{
		case kPOSITION :
		{
			cgGLEnableClientState(m_parameter);
			cgGLSetParameterPointer(m_parameter, 3, GL_FLOAT, 0, &vertexArray[0]);
			break;
		}
		case kNORMAL :
		{	
			int normalIsValid = 0;
			if (normalCount >= (normalId+1)*3) normalIsValid = 1;
			cgGLEnableClientState(m_parameter);
			cgGLSetParameterPointer(m_parameter, 3, GL_FLOAT, 0, &normalArrays[normalId*3*normalIsValid][0]);
			break;
		}
		case kCOLOR0 :
		{
			setColorOrUV(colorArrays, id[0], colorCount, uvArrays, uvCount, isUV[0], setName[0], normalArrays, normalId, normalCount);
			break;
		}
		case kCOLOR1 :
		{
			setColorOrUV(colorArrays, id[1], colorCount, uvArrays, uvCount, isUV[1], setName[1], normalArrays, normalId, normalCount);
			break;
		}
		case kTEXCOORD0 :
 		{
			setColorOrUV(colorArrays, id[2], colorCount, uvArrays, uvCount, isUV[2], setName[2], normalArrays, normalId, normalCount);
			break;
		}
		
		case kTEXCOORD1 :
		{	
			setColorOrUV(colorArrays, id[8], colorCount, uvArrays, uvCount, isUV[8], setName[8], normalArrays, normalId, normalCount);
			break;
		}
		case kTEXCOORD2 :
		{	
			setColorOrUV(colorArrays, id[9], colorCount, uvArrays, uvCount, isUV[9], setName[9], normalArrays, normalId, normalCount);
			break;
		}
		case kTEXCOORD3 :
		{
			setColorOrUV(colorArrays, id[3], colorCount, uvArrays, uvCount, isUV[3], setName[3], normalArrays, normalId, normalCount);
			break;
		}
		case kTEXCOORD4 :
		{
			setColorOrUV(colorArrays, id[4], colorCount, uvArrays, uvCount, isUV[4], setName[4], normalArrays, normalId, normalCount);
			break;
		}
		
		case kTEXCOORD5 :
		{
			setColorOrUV(colorArrays, id[5], colorCount, uvArrays, uvCount, isUV[5], setName[5], normalArrays, normalId, normalCount);
			break;
		}
		
		case kTEXCOORD6 :
		{
			setColorOrUV(colorArrays, id[6], colorCount, uvArrays, uvCount, isUV[6], setName[6], normalArrays, normalId, normalCount);
			break;
		}
		case kTEXCOORD7 :
		{
			setColorOrUV(colorArrays, id[7], colorCount, uvArrays, uvCount, isUV[7], setName[7], normalArrays, normalId, normalCount);
			break;
		}
	}
}

void CFXParameter::setColorOrUV(const float ** colorArrays, int colorId, int colorCount, const float ** uvArrays, int uvCount, int isUV, MString name, const float ** normalArrays, int normalId, int normalCount)
{
	if (name == "tangent" || name == "binormal") 
	{
		int normalIsValid = 0;
		if (normalCount >= (normalId+1)*3) normalIsValid = 1;
		if (name == "tangent") 
		{
			cgGLEnableClientState(m_parameter);
			cgGLSetParameterPointer(m_parameter, 3, GL_FLOAT, 0, &normalArrays[1+normalId*3*normalIsValid][0]);
		}
		else 
		{
			cgGLEnableClientState(m_parameter);
			cgGLSetParameterPointer(m_parameter, 3, GL_FLOAT, 0, &normalArrays[2+normalId*3*normalIsValid][0]);
		}
	}
	else if (isUV) 
	{
		if (uvCount > colorId && colorId > -1) 
		{		
			cgGLEnableClientState(m_parameter);
			cgGLSetParameterPointer(m_parameter, 2, GL_FLOAT, 0, uvArrays[colorId]);
		}

	}
	else 
	{
		if (colorCount > colorId && colorId > -1) 
		{	
			cgGLEnableClientState(m_parameter);
			cgGLSetParameterPointer(m_parameter, 4, GL_FLOAT, 0, colorArrays[colorId]);
		}
	}
}

void CFXParameter::updateAttrValue(const MMatrix& shaderSpace, const MMatrix& worldSpace)
{
	switch (type) 
	{
		case kBool:
		{
			cgGLSetParameter1f(m_parameter, m_r);
			break;
		}
		case kFloat:
		case kHalf: 
		{
			if (semantic == kTIME)
			{
				float time = (float) MAnimControl::currentTime().as(MTime::kSeconds);
				cgGLSetParameter1f(m_parameter, time);
			}
			else
			{
				cgGLSetParameter1f(m_parameter, m_r);
			}
			break;
		}
		case kFloat2:
		case kHalf2:
		{
			cgGLSetParameter2f(m_parameter, m_r, m_g);
			break;
		}
		case kFloat3:
		case kHalf3:
		{
			cgGLSetParameter3f(m_parameter, m_r, m_g, m_b);
			break;
		}
		case kFloat4:
		case kHalf4:
		{
			cgGLSetParameter4f(m_parameter, m_r, m_g, m_b, 1.0f);
			break;
		}
		case kFloat4x4:
		case kHalf4x4:
		{
			float tmp[4][4];
			switch (semantic) 
			{
				case kUNKNOWN:
				{	
					matrixValue.transpose().get(tmp);
					cgGLSetMatrixParameterfc(m_parameter, &tmp[0][0]);
					break;
				}
				case kVIEWPROJ:
				{
					// calculate the view matrix
					glGetFloatv(GL_MODELVIEW_MATRIX, &tmp[0][0]);
					MMatrix worldView = MMatrix(tmp);
					glGetFloatv(GL_PROJECTION_MATRIX, &tmp[0][0]);
					MMatrix projection = MMatrix(tmp);
					MMatrix view = worldSpace.inverse() * worldView;
					(view * projection).transpose().get(tmp);
					cgSetMatrixParameterfr(m_parameter, &tmp[0][0]);	
					break;
				}
				case kVIEWPROJI :
				{
					// calculate the view matrix
					glGetFloatv(GL_MODELVIEW_MATRIX, &tmp[0][0]);
					MMatrix worldView = MMatrix(tmp);
					glGetFloatv(GL_PROJECTION_MATRIX, &tmp[0][0]);
					MMatrix projection = MMatrix(tmp);
					MMatrix view = worldSpace.inverse() * worldView;
					(view * projection).inverse().transpose().get(tmp);
					cgSetMatrixParameterfr(m_parameter, &tmp[0][0]);	
					break;
				}
				case kVIEWPROJIT:
				{
					// calculate the view matrix
					glGetFloatv(GL_MODELVIEW_MATRIX, &tmp[0][0]);
					MMatrix worldView = MMatrix(tmp);
					glGetFloatv(GL_PROJECTION_MATRIX, &tmp[0][0]);
					MMatrix projection = MMatrix(tmp);
					MMatrix view = worldSpace.inverse() * worldView;
					(view * projection).inverse().get(tmp);
					cgSetMatrixParameterfr(m_parameter, &tmp[0][0]);	
					break;
				}
				case kWORLDVIEWPROJ:
				{
					cgGLSetStateMatrixParameter(m_parameter, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY);
					break;
				}
				
				case kWORLDVIEWPROJI :
				{
					cgGLSetStateMatrixParameter(m_parameter, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_INVERSE);
					break;
				}
				case kWORLDVIEWPROJIT:
				{
					cgGLSetStateMatrixParameter(m_parameter, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_INVERSE_TRANSPOSE);
					break;
				}
				case kWORLDVIEW:
				{
					cgGLSetStateMatrixParameter(m_parameter, CG_GL_MODELVIEW_MATRIX, CG_GL_MATRIX_IDENTITY);
					break;
				}
				case kWORLDVIEWI:
				{
					cgGLSetStateMatrixParameter(m_parameter, CG_GL_MODELVIEW_MATRIX, CG_GL_MATRIX_INVERSE);
					break;
				}
				case kWORLDVIEWIT :
				{
					cgGLSetStateMatrixParameter(m_parameter, CG_GL_MODELVIEW_MATRIX, CG_GL_MATRIX_INVERSE_TRANSPOSE);
					break;
				}
				case kOBJECT:
				{
					shaderSpace.transpose().get(tmp);
					cgSetMatrixParameterfr(m_parameter, &tmp[0][0]);
					break;
				}
				case kOBJECTI:
				{
					shaderSpace.inverse().transpose().get(tmp);
					cgSetMatrixParameterfr(m_parameter, &tmp[0][0]);
					break;
				}
				case kOBJECTIT :
				{
					shaderSpace.inverse().get(tmp);
					cgSetMatrixParameterfr(m_parameter, &tmp[0][0]);
					break;
				}
				case kWORLD :
				{	
					worldSpace.transpose().get(tmp);
					cgSetMatrixParameterfr(m_parameter, &tmp[0][0]);
					break;
				}
				case kWORLDI:
				{
					worldSpace.inverse().transpose().get(tmp);
					cgSetMatrixParameterfr(m_parameter, &tmp[0][0]);
					break;
				}
				case kWORLDIT :
				{
					worldSpace.inverse().get(tmp);
					cgSetMatrixParameterfr(m_parameter, &tmp[0][0]);
					break;
				}
				case kVIEW :
				{
					// calculate the view matrix
					glGetFloatv(GL_MODELVIEW_MATRIX, &tmp[0][0]);
					MMatrix wvMatrix = MMatrix(tmp);
					MMatrix vMatrix = worldSpace.inverse() * wvMatrix;
					vMatrix.transpose().get(tmp);
					cgSetMatrixParameterfr(m_parameter, &tmp[0][0]);	
					break;
				}
				case kVIEWI :
				{	
					// calculate the view matrix
					glGetFloatv(GL_MODELVIEW_MATRIX, &tmp[0][0]);
					MMatrix wvMatrix = MMatrix(tmp);
					MMatrix vMatrix = worldSpace.inverse() * wvMatrix;
					vMatrix.inverse().transpose().get(tmp);
					cgSetMatrixParameterfr(m_parameter, &tmp[0][0]);
					break;
				}
				case kVIEWIT :
				{	
					// calculate the view matrix
					glGetFloatv(GL_MODELVIEW_MATRIX, &tmp[0][0]);
					MMatrix wvMatrix = MMatrix(tmp);
					MMatrix vMatrix = worldSpace.inverse() * wvMatrix;
					vMatrix.inverse().get(tmp);
					cgSetMatrixParameterfr(m_parameter, &tmp[0][0]);
					break;
				}
				case kPROJ :
				{	
					cgGLSetStateMatrixParameter(m_parameter, CG_GL_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY);	
					break;
				}
				case kPROJI :
				{	
					cgGLSetStateMatrixParameter(m_parameter, CG_GL_PROJECTION_MATRIX, CG_GL_MATRIX_INVERSE);	
					break;
				}
				case kPROJIT :
				{	
					cgGLSetStateMatrixParameter(m_parameter, CG_GL_PROJECTION_MATRIX, CG_GL_MATRIX_INVERSE_TRANSPOSE);	
					break;
				}
			}
			break;
		}
		// for sampler2D 
		//
		case kSampler2D:
		case kSamplerCUBE:
		{
			if (textureid == 0)
			{
				loadDefaultTexture();
			}
			break;
		}
		case kSurface:
		{
			// nothing to do here
			break;
		}
	}
}

void CFXParameter::loadDefaultTexture()
{
    // get texture state assignment
    CGstateassignment textureAssign = cgGetNamedSamplerStateAssignment(m_parameter, "texture");
    if (textureAssign)
    {
        CGparameter textureParam = cgGetTextureStateAssignmentValue(textureAssign);

        // get annotation for default value
	    CGannotation ann = cgGetNamedParameterAnnotation(textureParam, "ResourceName");
        if (ann)
        {
            // get path to texture
            const char* annValue = cgGetStringAnnotationValue(ann);
			fstring rootFile = parent->getVertexProgramFilename();
			FUFileManager manager;
			manager.PushRootFile(rootFile);
			fstring relFile = manager.GetCurrentUri().MakeAbsolute(TO_FSTRING(annValue));

            if (type == kSampler2D)
                textureid = loadTexture(relFile.c_str(), textureOffset);
            else
                textureid = loadCompressedTextureCUBE(relFile.c_str(), textureOffset);

			if (textureid != 0)
			{
				// default texture loaded successfully from annotation, synchronize plug
				// algorithm:
				//	1. Create file texture shading node
				FUStringBuilder cmd;
				MString param = parent->name() + "." + name;
				cmd.append(FC("string $file = `shadingNode -asTexture file`;\n"));
				//	2. Connect the file's outColor attribute to the shader's sampler
				cmd.append(FC("connectAttr -force ($file + \".outColor\") "));
				cmd.append(MConvert::ToFChar(param)); cmd.append(FC(";\n"));
				//	3. Assign the image
				relFile.replace('\\', '/');
				cmd.append(FC("AEassignTextureCB ($file + \".fileTextureName\") \""));
				cmd.append(relFile.c_str()); cmd.append(FC("\" \"image\";\n"));
				//	4. Execute the MEL command
				MGlobal::executeCommand(MString(cmd.ToCharPtr()));
			}
        }
    }

    if (textureid == 0)
    {
        // Create and assign an empty texture
		textureid = type == kSampler2D ? CreateDefault2DTexture() : CreateDefaultCUBETexture();
    }

	if (textureid != 0)
	{
		cgGLSetupSampler(m_parameter, textureid);
	}
}

void CFXParameter::setDefaultValue(float v)
{
	MFnNumericAttribute attrFn(attribute);
	attrFn.setDefault(v);
}

void CFXParameter::setDefaultValue(float r, float g, float b)
{
	MFnNumericAttribute attrFn(attribute);
	attrFn.setDefault(r, g, b);
}

void CFXParameter::setBounds(float min, float max)
{
	MFnNumericAttribute attrFn(attribute);
	attrFn.setMin(min);
	attrFn.setSoftMin(min);
	attrFn.setMax(max);
	attrFn.setSoftMax(max);
}

const char* CFXParameter::getSemanticStringForFXC()
{ 
	switch (semantic)
	{
	case kVIEWPROJ: return "ViewProjection";
	case kVIEWPROJI: return "ViewProjectionInverse";
	case kVIEWPROJIT: return "ViewProjectionInverseTranspose";
	case kVIEW: return "View";
	case kVIEWI: return "ViewInverse";
	case kVIEWIT: return "ViewInverseTranspose";
	case kPROJ: return "Projection";
	case kPROJI: return "ProjectionInverse";
	case kPROJIT: return "ProjectionInverseTranspose";
	case kOBJECT: return "Object";
	case kOBJECTI: return "ObjectInverse";
	case kOBJECTIT: return "ObjectInverseTranspose";
	case kWORLD: return "World";
	case kWORLDI: return "WorldInverse";
	case kWORLDIT: return "WorldInverseTranspose";
	case kWORLDVIEW: return "WorldView";
	case kWORLDVIEWI: return "WorldViewInverse";
	case kWORLDVIEWIT: return "WorldViewInverseTranspose";
	case kWORLDVIEWPROJ: return "WorldViewProjection";
	case kWORLDVIEWPROJI: return "WorldViewProjectionInverse";
	case kWORLDVIEWPROJIT: return "WorldViewProjectionInverseTranspose";
	default: { 
		const char* s = cgGetParameterSemantic(m_parameter);
		return (s != NULL) ? s : ""; }
	}
}

MPlug CFXParameter::getPlug()
{
	return MFnDependencyNode(parent->thisMObject()).findPlug(attributeName);
}

void CFXParameter::readUIWidget()
{
	CGannotation ann = cgGetNamedParameterAnnotation(m_parameter, CFX_ANNOTATION_UITYPE);
	if (ann)
	{
		uiType = cgGetStringAnnotationValue(ann);
	}
}

CFXParameter::Type CFXParameter::convertCGtype(CGtype type)
{
	switch (type)
	{
	case CG_BOOL: return kBool;

	case CG_FLOAT: return kFloat;
	case CG_FLOAT2: return kFloat2;
	case CG_FLOAT3: return kFloat3;
	case CG_FLOAT4: return kFloat4;
	case CG_FLOAT4x4: return kFloat4x4;

	case CG_HALF: return kHalf;
	case CG_HALF2: return kHalf2;
	case CG_HALF3: return kHalf3;
	case CG_HALF4: return kHalf4;
	case CG_HALF4x4: return kHalf4x4;

	case CG_SAMPLER2D: return kSampler2D;
	case CG_SAMPLERCUBE: return kSamplerCUBE;
	case CG_TEXTURE: return kSurface;
	case CG_STRUCT: return kStruct;
	default: return kUnknown;
	}
}

CFXParameter::Semantic CFXParameter::convertCGsemantic(const char* name)
{
	if (name == NULL || *name == 0) return kUNKNOWN;
	else if (IsEquivalentI(name, "POSITION")) return kPOSITION;
	else if (IsEquivalentI(name, "NORMAL")) return kNORMAL;
	else if (IsEquivalentI(name, "COLOR0")) return kCOLOR0;
	else if (IsEquivalentI(name, "COLOR1")) return kCOLOR1;
	else if (IsEquivalentI(name, "TEXCOORD0")) return kTEXCOORD0;
	else if (IsEquivalentI(name, "TEXCOORD1")) return kTEXCOORD1;
	else if (IsEquivalentI(name, "TEXCOORD2")) return kTEXCOORD2;
	else if (IsEquivalentI(name, "TEXCOORD3")) return kTEXCOORD3;
	else if (IsEquivalentI(name, "TEXCOORD4")) return kTEXCOORD4;
	else if (IsEquivalentI(name, "TEXCOORD5")) return kTEXCOORD5;
	else if (IsEquivalentI(name, "TEXCOORD6")) return kTEXCOORD6;
	else if (IsEquivalentI(name, "TEXCOORD7")) return kTEXCOORD7;
	else if (IsEquivalentI(name, "WORLDVIEWPROJ")) return kWORLDVIEWPROJ;
	else if (IsEquivalentI(name, "WORLDVIEWPROJECTION")) return kWORLDVIEWPROJ;
	else if (IsEquivalentI(name, "WORLDVIEWPROJECTIONINVERSE")) return kWORLDVIEWPROJI;
	else if (IsEquivalentI(name, "WORLDVIEWPROJI")) return kWORLDVIEWPROJI;
	else if (IsEquivalentI(name, "WORLDVIEWPROJECTIONI")) return kWORLDVIEWPROJI;
	else if (IsEquivalentI(name, "WORLDVIEWPROJECTIONINVERSETRANSPOSE")) return kWORLDVIEWPROJIT;
	else if (IsEquivalentI(name, "WORLDVIEWPROJIT")) return kWORLDVIEWPROJIT;
	else if (IsEquivalentI(name, "WORLDVIEWPROJECTIONIT")) return kWORLDVIEWPROJIT;
	else if (IsEquivalentI(name, "VIEWPROJ")) return kVIEWPROJ;
	else if (IsEquivalentI(name, "VIEWPROJECTION")) return kVIEWPROJ;
	else if (IsEquivalentI(name, "VIEWPROJI")) return kVIEWPROJI;
	else if (IsEquivalentI(name, "VIEWPROJECTIONINVERSE")) return kVIEWPROJI;
	else if (IsEquivalentI(name, "VIEWPROJECTIONI")) return kVIEWPROJI;
	else if (IsEquivalentI(name, "VIEWPROJIT")) return kVIEWPROJIT;
	else if (IsEquivalentI(name, "VIEWPROJECTIONINVERSETRANSPOSE")) return kVIEWPROJIT;
	else if (IsEquivalentI(name, "VIEWPROJECTIONIT")) return kVIEWPROJIT;
	else if (IsEquivalentI(name, "VIEW")) return kVIEW;
	else if (IsEquivalentI(name, "VIEWI")) return kVIEWI;
	else if (IsEquivalentI(name, "VIEWINVERSE")) return kVIEWI;
	else if (IsEquivalentI(name, "VIEWIT")) return kVIEWIT;
	else if (IsEquivalentI(name, "VIEWINVERSETRANSPOSE")) return kVIEWIT;
	else if (IsEquivalentI(name, "PROJECTION")) return kPROJ;
	else if (IsEquivalentI(name, "PROJECTIONI")) return kPROJI;
	else if (IsEquivalentI(name, "PROJECTIONINVERSE")) return kPROJI;
	else if (IsEquivalentI(name, "PROJECTIONIT")) return kPROJIT;
	else if (IsEquivalentI(name, "PROJECTIONINVERSETRANSPOSE")) return kPROJIT;
	else if (IsEquivalentI(name, "OBJECT")) return kOBJECT;
	else if (IsEquivalentI(name, "OBJECTI")) return kOBJECTI;
	else if (IsEquivalentI(name, "OBJECTINVERSE")) return kOBJECTI;
	else if (IsEquivalentI(name, "OBJECTIT")) return kOBJECTIT;
	else if (IsEquivalentI(name, "OBJECTINVERSETRANSPOSE")) return kOBJECTIT;
	else if (IsEquivalentI(name, "WORLD")) return kWORLD;
	else if (IsEquivalentI(name, "WORLDI")) return kWORLDI;
	else if (IsEquivalentI(name, "WORLDINVERSE")) return kWORLDI;
	else if (IsEquivalentI(name, "WORLDIT")) return kWORLDIT;
	else if (IsEquivalentI(name, "WORLDINVERSETRANSPOSE")) return kWORLDIT;
	else if (IsEquivalentI(name, "WORLDVIEW")) return kWORLDVIEW;
	else if (IsEquivalentI(name, "WORLDVIEWI")) return kWORLDVIEWI;
	else if (IsEquivalentI(name, "WORLDVIEWINVERSE")) return kWORLDVIEWI;
	else if (IsEquivalentI(name, "WORLDVIEWIT")) return kWORLDVIEWIT;
	else if (IsEquivalentI(name, "WORLDVIEWINVERSETRANSPOSE")) return kWORLDVIEWIT;
	else if (IsEquivalentI(name, "TIME")) return kTIME;
	else return kUNKNOWN;
}

void CFXParameter::Synchronize()
{
	MPlug plug = getPlug();

	switch (type)
	{
	case CFXParameter::kBool:
	case CFXParameter::kFloat: 
	case CFXParameter::kHalf: plug.getValue(m_r); break;
	case CFXParameter::kFloat2:
	case CFXParameter::kHalf2: CDagHelper::GetPlugValue(plug, m_r, m_g); break;
	case CFXParameter::kFloat3:
	case CFXParameter::kHalf3:
	case CFXParameter::kFloat4:
	case CFXParameter::kHalf4: CDagHelper::GetPlugValue(plug, m_r, m_g, m_b); break;
	case CFXParameter::kFloat4x4: 
	case CFXParameter::kHalf4x4: CDagHelper::GetPlugValue(plug, matrixValue); break;	

	case CFXParameter::kSampler2D: {
		MString texFilename = GetTextureNameFromPlug(plug);

		if (textureid > 0)
		{
			// delete the old texture
			glDeleteTextures(1, &textureid);
			textureid = 0;
		}

		if (texFilename.length() > 0)
		{
			textureid = loadTexture(MConvert::ToFChar(texFilename),textureOffset);
			if (textureid != 0)
				cgGLSetupSampler(m_parameter, textureid);
		}
		break; }

	case CFXParameter::kSamplerCUBE: {
		MString texFilename = GetTextureNameFromPlug(plug);

		// Delete the texture first
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		if (textureid > 0)
		{
			glDeleteTextures(1, &textureid);
			textureid = 0;
		}

		// Load the cube texture
		if (texFilename.length() > 0)
		{
			textureid = loadCompressedTextureCUBE(MConvert::ToFChar(texFilename), textureOffset);
			if (textureid != 0)
				cgGLSetupSampler(m_parameter, textureid);
		}
		glPopAttrib();
		break; }
	}

	ResetDirty();
}

inline DDS_IMAGE_DATA* loadDDSTextureFile(const fchar* filename, int& isCUBE, int& isCompressed)
{
	DDS_IMAGE_DATA* pDDSImageData;
   	DDSURFACEDESC2 ddsd;
   	DDSCAPS2 ddsCaps;

	isCUBE = 0;
	isCompressed = 0;
    	
   	char filecode[4];
    int bytesPerBlock = 8;
    int bufferSize;

    // Open the file
	FUFile file(filename, FUFile::READ);
	if (!file.IsOpen())
    {
        char str[255];
        sprintf(str, "loadDDSTextureFile couldn't find, or failed to load \"%s\"", filename);
        MGlobal::displayWarning(str);
        return NULL;
    }

    // Verify the file is a true .dds file
    file.Read(filecode, 4);
    if (strncmp(filecode, "DDS ", 4) != 0)
    {
        char str[255];
        sprintf(str, "The file \"%s\" doesn't appear to be a valid .dds file!", filename);
        MGlobal::displayWarning(str);
        return NULL;
    }

    // Get the surface descriptor
    file.Read(&ddsd, sizeof(DDSURFACEDESC2));

    pDDSImageData = (DDS_IMAGE_DATA*) malloc(sizeof(DDS_IMAGE_DATA));
    memset(pDDSImageData, 0, sizeof(DDS_IMAGE_DATA));

	bool support = true;

	if ((ddsd.dwSize != sizeof(DDSURFACEDESC2)) ||
		(ddsd.dwFlags & DDSD_CAPS) == 0 ||
		(ddsd.dwFlags & DDSD_PIXELFORMAT) == 0 ||				
		(ddsd.dwFlags & DDSD_WIDTH) == 0 ||
		(ddsd.dwFlags & DDSD_HEIGHT) == 0 ||
		(ddsd.ddsCaps.dwCaps2 & DDSCAPS2_VOLUME) != 0)
	{
		support = false;
	}

	if ((ddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC) == 0)
	{
		if ((ddsd.ddpfPixelFormat.dwFlags & DDPF_RGB) != 0)
		{
			if (ddsd.ddpfPixelFormat.dwRGBBitCount == 32)
			{
				if (ddsd.ddpfPixelFormat.dwRBitMask == 0x00FF0000)
				{
					pDDSImageData->format = GL_RGBA;
				}
				else
				{
					support = false;
				}
			}
			else if (ddsd.ddpfPixelFormat.dwRGBBitCount == 24)
			{
				if (ddsd.ddpfPixelFormat.dwRBitMask == 0x00FF0000)
				{
					pDDSImageData->format = GL_RGB;
				}
				else
				{
					support = false;
				}
			}
			else
			{
				support = false;
			}
		}
		else
		{
			support = false;
		}
	}
	else
	{
		//
		// This .dds loader supports the loading of compressed formats DXT1, DXT3 
		// and DXT5.
		//
		isCompressed = 1;
		switch (ddsd.ddpfPixelFormat.dwFourCC)
		{
			case FOURCC_DXT1:
				// DXT1 is 64 bits per 4x4 texel block
				pDDSImageData->format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
				break;

			case FOURCC_DXT3:
				// DXT3 is 128 bits per 4x4 texel block
				pDDSImageData->format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
				bytesPerBlock = 16;
				break;

			case FOURCC_DXT5:
				// DXT5 is 128 bits per 4x4 texel block
				pDDSImageData->format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
				bytesPerBlock = 16;
				break;

			default:
				support = false;
				break;
		}
	}

	if (!support)
	{
		// NO SUPPORT
		char str[255];
        sprintf(str, "The file \"%s\" is not supported by this DDS file format "
            "implementation!", filename);
        MGlobal::displayWarning(str);
		free(pDDSImageData);
        return NULL;
	}

    
	// is it a cube map?
	//
	ddsCaps = ddsd.ddsCaps;
	    
	if (ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP)
	{
		isCUBE = 1;
	}

	//
    // How big will the buffer need to be to load all of the pixel data 
    // including mip-maps?
    //

	int numMipMaps = (ddsd.dwFlags & DDSD_MIPMAPCOUNT) ? ddsd.dwMipMapCount : 1;

	// See http://msdn2.microsoft.com/en-us/library/bb204843.aspx
	// for details on how to calculate the compressed image buffer size
    
	bufferSize = 0;
	uint32 w = ddsd.dwWidth, h = ddsd.dwHeight;

	if (isCompressed)
	{
		for (int m = 0; m < numMipMaps; ++m)
		{
			// calculate the number of 4x4 blocks, padding if necessary
			uint32 bw = w >> 2; if (bw == 0) bw = 1;
			uint32 bh = h >> 2; if (bh == 0) bh = 1;

			bufferSize += (bw * bh) * bytesPerBlock;

			w >>= 1;
			h >>= 1;
		}
	}
	else
	{
		for (int m = 0; m < numMipMaps; ++m)
		{
			if (w == 0) w = 1;
			if (h == 0) h = 1;

			bufferSize += (w * h) * (ddsd.ddpfPixelFormat.dwRGBBitCount >> 3);

			w >>= 1;
			h >>= 1;
		}
	}
	
	if (isCUBE)
		bufferSize *= 6;

    pDDSImageData->pixels = (unsigned char*)malloc(bufferSize * sizeof(unsigned char));

    file.Read(pDDSImageData->pixels, bufferSize);

    pDDSImageData->width      = ddsd.dwWidth;
    pDDSImageData->height     = ddsd.dwHeight;
    pDDSImageData->numMipMaps = numMipMaps;
   	
   	return pDDSImageData;
}

GLuint CFXParameter::loadTexture(const fchar* filename, int offset)
{
	GLuint tid = 0;
	// Load in the 2D texture.
	MImage imageFn;					
	if (imageFn.readFromFile(filename)) 
	{				
		glGenTextures(1, &tid);
		
		glActiveTexture(GL_TEXTURE0_ARB + offset);
		glBindTexture(GL_TEXTURE_2D, tid);

		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, true);

		uint mw, mh;
		imageFn.getSize (mw, mh);
		glTexImage2D(GL_TEXTURE_2D, 0, imageFn.depth(), mw, mh, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageFn.pixels());

		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	}
	imageFn.release();
	return tid;
}

GLuint CFXParameter::loadCompressedTextureCUBE(const fchar *filename, int offset)
{
	int stat = FALSE;
	int isCUBE = 0;
	int isCompressed = 0;
	
	GLuint texid;

	// NOTE: Unlike "lena.bmp", "lena.dds" actually contains its own mip-map 
	// levels, which are also compressed.
	DDS_IMAGE_DATA* pDDSImageData = loadDDSTextureFile(filename, isCUBE, isCompressed);
	if (pDDSImageData != NULL && isCUBE == 1) 
	{
		stat = TRUE;
		int nNumMipMaps = pDDSImageData->numMipMaps;

		int nBlockSize;

		switch (pDDSImageData->format)
		{
		// raw formats - a block is a pixel
		case GL_RGBA:
			nBlockSize = 4;
			break;
		case GL_RGB:
			nBlockSize = 3;
			break;

		// DXT formats - a block is a 4x4 pixel array
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			nBlockSize = 8;
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
		default:
			nBlockSize = 16;
			break;
		}	

		glGenTextures(1, &texid);
		glActiveTexture(GL_TEXTURE0_ARB + offset);

		glEnable(GL_TEXTURE_CUBE_MAP_ARB);
		glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, texid);

		if (nNumMipMaps > 1)
			glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		else
			glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		int nSize;
		int nOffset = 0;

		for (int n = 0; n < 6; n++) 
		{
			int nHeight = max((GLsizei) 1, pDDSImageData->height);
			int nWidth = max((GLsizei) 1, pDDSImageData->width);

			// Load the mip-map levels
			for (int i = 0; i < nNumMipMaps; ++i) 
			{
				if (nWidth == 0) nWidth = 1;
				if (nHeight == 0) nHeight = 1;

				if (isCompressed)
				{
					nSize = ((nWidth+3)/4) * ((nHeight+3)/4) * nBlockSize;

					glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + n,
						i,
						pDDSImageData->format,
						nWidth,
						nHeight,
						0,
						nSize,
						pDDSImageData->pixels + nOffset);

					nOffset += nSize;
				}
				else
				{
					glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
					glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + n,
						i,
						pDDSImageData->format,
						nWidth,
						nHeight,
						0,
						GL_RGBA,
						GL_UNSIGNED_BYTE,
						pDDSImageData->pixels + nOffset);

					nOffset += nWidth * nHeight * nBlockSize;
				}
				
				nWidth  = (nWidth  / 2);
				nHeight = (nHeight / 2);
			} 
		}  	
	}
	if (isCUBE == 0) MGlobal::displayWarning(MString("Failed to load CUBE map from: ") + filename);
	if (pDDSImageData != NULL)
	{
		if (pDDSImageData->pixels != NULL) free(pDDSImageData->pixels);
		free(pDDSImageData);
	}
	
	return texid;
}

GLuint CFXParameter::CreateDefault2DTexture()
{
	GLuint texid;
	glGenTextures(1, &texid);
	glActiveTexture(GL_TEXTURE0_ARB + textureOffset);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texid);

	const uint32 pixels[] = { 0xFFFFFFFF, 0x7F7F7F7F, 0x7F7F7F7F, 0xFFFFFFFF };
	glTexImage2D(GL_TEXTURE_2D, 0, sizeof(uint32), 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // Point-sampling: purposedly make this ugly.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	return texid;
}

GLuint CFXParameter::CreateDefaultCUBETexture()
{
	GLuint texid;
	glGenTextures(1, &texid);
	glActiveTexture(GL_TEXTURE0_ARB + textureOffset);
	glEnable(GL_TEXTURE_CUBE_MAP_ARB);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, texid);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // Point-sampling: purposedly make this ugly.
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	for (int n = 0; n < 6; ++n) 
	{
		const uint32 pixels[] = { 0xFFFFFFFF, 0x7F7F7F7F, 0x7F7F7F7F, 0xFFFFFFFF };
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + n, 0, sizeof(uint32), 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	}  	

	return texid;
}
