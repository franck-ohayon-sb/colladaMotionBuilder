/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "FCDocument/FCDEffectPassState.h"
#include "FUtils/FUDaeEnum.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "ColladaFX/CFXShaderNode.h"
#include "ColladaFX/CFXRenderState.h"

#include <maya/MFnCompoundAttribute.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnNumericAttribute.h>

// initialize CG_RENDER_STATE_NAMES
#include "ColladaFX/CFXRenderStateStaticInit.hpp"

//
// CFXRenderState
//

CFXRenderState::CFXRenderState(CFXShaderNode* _parent)
:	parent(_parent)
,	colladaState(NULL)
,	attribute(MObject::kNullObj)
,	isDirty(true)
,	isDead(false)
,	index(-1)
{
}

CFXRenderState::~CFXRenderState()
{
	if (parent != NULL && !attribute.isNull())
	{
		MFnDependencyNode parentFn(parent->thisMObject());
		parentFn.removeAttribute(attribute);
	}

	parent = NULL;
	SAFE_DELETE(colladaState);
}

void CFXRenderState::Initialize(FCDEffectPassState* renderState)
{
	SAFE_DELETE(colladaState);

	colladaState = new FCDEffectPassState(NULL, renderState->GetType());
	renderState->Clone(colladaState);
	CreateAttribute();
}

bool CFXRenderState::Initialize(FUDaePassState::State stateType)
{
	// Default initialization
	SAFE_DELETE(colladaState);

	if (stateType == FUDaePassState::INVALID) return false;
	colladaState = new FCDEffectPassState(NULL, stateType);

	CreateAttribute();

	return true;
}

bool CFXRenderState::Initialize(_CGstateassignment* effectState)
{
	SAFE_DELETE(colladaState);

	// Figure out the state type and create a structure for it.
	FUDaePassState::State stateType = TranslateState(cgGetStateAssignmentState(effectState));
	if (stateType == FUDaePassState::INVALID) return false;
	colladaState = new FCDEffectPassState(NULL, stateType);

#define SET_VALUE(offset, valueType, actualValue) *((valueType*)(data + offset)) = (valueType) (actualValue);

	// Parse and set the render state value(s).
	int valueCount = 0;
	uint8* data = colladaState->GetData();
	size_t dataSize = colladaState->GetDataSize();
	switch (stateType)
	{
	case FUDaePassState::ALPHA_FUNC: {
		const float* f = cgGetFloatStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, f[0]);
		if (valueCount > 1) SET_VALUE(4, float, f[1]);
		break; }

	case FUDaePassState::BLEND_FUNC: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		if (valueCount > 1) SET_VALUE(4, uint32, i[1]);
		break; }

	case FUDaePassState::BLEND_FUNC_SEPARATE: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		if (valueCount > 1) SET_VALUE(4, uint32, i[1]);
		if (valueCount > 2) SET_VALUE(8, uint32, i[2]);
		if (valueCount > 3) SET_VALUE(12, uint32, i[3]);
		break; }

	case FUDaePassState::BLEND_EQUATION: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		break; }

	case FUDaePassState::BLEND_EQUATION_SEPARATE: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		if (valueCount > 1) SET_VALUE(4, uint32, i[1]);
		break; }

	case FUDaePassState::COLOR_MATERIAL: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		if (valueCount > 1) SET_VALUE(4, uint32, i[1]);
		break; }

	case FUDaePassState::CULL_FACE: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		break; }

	case FUDaePassState::DEPTH_FUNC: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		break; }

	case FUDaePassState::FOG_MODE: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		break; }

	case FUDaePassState::FOG_COORD_SRC: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		break; }

	case FUDaePassState::FRONT_FACE: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		break; }

	case FUDaePassState::LIGHT_MODEL_COLOR_CONTROL: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		break; }

	case FUDaePassState::LOGIC_OP: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		break; }

	case FUDaePassState::POLYGON_MODE: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		if (valueCount > 1) SET_VALUE(4, uint32, i[1]);
		break; }

	case FUDaePassState::SHADE_MODEL: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		break; }

	case FUDaePassState::STENCIL_FUNC: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		if (valueCount > 1) SET_VALUE(4, uint8, i[1]);
		if (valueCount > 2) SET_VALUE(5, uint8, i[2]);
		break; }

	case FUDaePassState::STENCIL_OP: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		if (valueCount > 1) SET_VALUE(4, uint32, i[1]);
		if (valueCount > 2) SET_VALUE(8, uint32, i[2]);
		break; }

	case FUDaePassState::STENCIL_FUNC_SEPARATE: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		if (valueCount > 1) SET_VALUE(4, uint32, i[1]);
		if (valueCount > 2) SET_VALUE(8, uint8, i[2]);
		if (valueCount > 3) SET_VALUE(9, uint8, i[3]);
		break; }

	case FUDaePassState::STENCIL_OP_SEPARATE: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		if (valueCount > 1) SET_VALUE(4, uint32, i[1]);
		if (valueCount > 2) SET_VALUE(8, uint32, i[2]);
		if (valueCount > 3) SET_VALUE(12, uint32, i[3]);
		break; }

	case FUDaePassState::STENCIL_MASK_SEPARATE: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		if (valueCount > 1) SET_VALUE(4, uint8, i[1]);
		break; }

	case FUDaePassState::LIGHT_ENABLE:
	case FUDaePassState::TEXTURE1D_ENABLE:
	case FUDaePassState::TEXTURE2D_ENABLE:
	case FUDaePassState::TEXTURE3D_ENABLE:
	case FUDaePassState::TEXTURECUBE_ENABLE:
	case FUDaePassState::TEXTURERECT_ENABLE:
	case FUDaePassState::TEXTUREDEPTH_ENABLE:
	case FUDaePassState::CLIP_PLANE_ENABLE: {
		SET_VALUE(0, uint8, cgGetStateAssignmentIndex(effectState));
		const CGbool* b = cgGetBoolStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(1, bool, b[0] != FALSE);
		break; }

	case FUDaePassState::LIGHT_AMBIENT:
	case FUDaePassState::LIGHT_DIFFUSE:
	case FUDaePassState::LIGHT_POSITION:
	case FUDaePassState::LIGHT_SPECULAR:
	case FUDaePassState::TEXTURE_ENV_COLOR:
	case FUDaePassState::CLIP_PLANE: {
		SET_VALUE(0, uint8, cgGetStateAssignmentIndex(effectState));
		const float* f = cgGetFloatStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(1, float, f[0]);
		if (valueCount > 1) SET_VALUE(5, float, f[1]);
		if (valueCount > 2) SET_VALUE(9, float, f[2]);
		if (valueCount > 3) SET_VALUE(13, float, f[3]);
		break; }

	case FUDaePassState::LIGHT_CONSTANT_ATTENUATION:
	case FUDaePassState::LIGHT_LINEAR_ATTENUATION:
	case FUDaePassState::LIGHT_QUADRATIC_ATTENUATION:
	case FUDaePassState::LIGHT_SPOT_CUTOFF:
	case FUDaePassState::LIGHT_SPOT_EXPONENT: {
		SET_VALUE(0, uint8, cgGetStateAssignmentIndex(effectState));
		const float* f = cgGetFloatStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(1, float, f[0]);
		break; }

	case FUDaePassState::LIGHT_SPOT_DIRECTION: {
		SET_VALUE(0, uint8, cgGetStateAssignmentIndex(effectState));
		const float* f = cgGetFloatStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(1, float, f[0]);
		if (valueCount > 1) SET_VALUE(5, float, f[1]);
		if (valueCount > 2) SET_VALUE(9, float, f[2]);
		break; }

	case FUDaePassState::TEXTURE1D:
	case FUDaePassState::TEXTURE2D:
	case FUDaePassState::TEXTURE3D:
	case FUDaePassState::TEXTURECUBE:
	case FUDaePassState::TEXTURERECT:
	case FUDaePassState::TEXTUREDEPTH: {
		SET_VALUE(0, uint8, cgGetStateAssignmentIndex(effectState));
		// Don't touch samplers yet.
		break; }

	case FUDaePassState::TEXTURE_ENV_MODE: {
		SET_VALUE(0, uint8, cgGetStateAssignmentIndex(effectState));
		break; }

	case FUDaePassState::BLEND_COLOR:
	case FUDaePassState::CLEAR_COLOR:
	case FUDaePassState::FOG_COLOR:
	case FUDaePassState::SCISSOR:
	case FUDaePassState::LIGHT_MODEL_AMBIENT:
	case FUDaePassState::MATERIAL_AMBIENT:
	case FUDaePassState::MATERIAL_DIFFUSE:
	case FUDaePassState::MATERIAL_EMISSION:
	case FUDaePassState::MATERIAL_SPECULAR: {
		const float* f = cgGetFloatStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, float, f[0]);
		if (valueCount > 1) SET_VALUE(4, float, f[1]);
		if (valueCount > 2) SET_VALUE(8, float, f[2]);
		if (valueCount > 3) SET_VALUE(12, float, f[3]);
		break; }

	case FUDaePassState::POINT_DISTANCE_ATTENUATION: {
		const float* f = cgGetFloatStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, float, f[0]);
		if (valueCount > 1) SET_VALUE(4, float, f[1]);
		if (valueCount > 2) SET_VALUE(8, float, f[2]);
		break; }

	case FUDaePassState::DEPTH_BOUNDS:
	case FUDaePassState::DEPTH_RANGE:
	case FUDaePassState::POLYGON_OFFSET: {
		const float* f = cgGetFloatStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, float, f[0]);
		if (valueCount > 1) SET_VALUE(4, float, f[1]);
		break; }

	case FUDaePassState::CLEAR_STENCIL:
	case FUDaePassState::STENCIL_MASK: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint32, i[0]);
		break; }

	case FUDaePassState::CLEAR_DEPTH:
	case FUDaePassState::FOG_DENSITY:
	case FUDaePassState::FOG_END:
	case FUDaePassState::LINE_WIDTH:
	case FUDaePassState::POINT_FADE_THRESHOLD_SIZE:
	case FUDaePassState::POINT_SIZE:
	case FUDaePassState::POINT_SIZE_MAX:
	case FUDaePassState::FOG_START:
	case FUDaePassState::MATERIAL_SHININESS:
	case FUDaePassState::POINT_SIZE_MIN: {
		const float* f = cgGetFloatStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, float, f[0]);
		break; }

	case FUDaePassState::COLOR_MASK: {
		const CGbool* b = cgGetBoolStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, bool, b[0] != FALSE);
		if (valueCount > 1) SET_VALUE(1, bool, b[1] != FALSE);
		if (valueCount > 2) SET_VALUE(2, bool, b[2] != FALSE);
		if (valueCount > 3) SET_VALUE(3, bool, b[3] != FALSE);
		break; }

	case FUDaePassState::LINE_STIPPLE: {
		const int* i = cgGetIntStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, uint16, i[0]);
		if (valueCount > 1) SET_VALUE(2, uint16, i[1]);
		break; }

	case FUDaePassState::MODEL_VIEW_MATRIX: {
	case FUDaePassState::PROJECTION_MATRIX:
		const float* f = cgGetFloatStateAssignmentValues(effectState, &valueCount);
		if (valueCount >= 16) SET_VALUE(0, FMMatrix44, FMMatrix44(f));
		break; }

	case FUDaePassState::DEPTH_MASK:
	case FUDaePassState::LIGHTING_ENABLE:
	case FUDaePassState::ALPHA_TEST_ENABLE:
	case FUDaePassState::AUTO_NORMAL_ENABLE:
	case FUDaePassState::BLEND_ENABLE:
	case FUDaePassState::COLOR_LOGIC_OP_ENABLE:
	case FUDaePassState::CULL_FACE_ENABLE:
	case FUDaePassState::DEPTH_BOUNDS_ENABLE:
	case FUDaePassState::DEPTH_CLAMP_ENABLE:
	case FUDaePassState::DEPTH_TEST_ENABLE:
	case FUDaePassState::DITHER_ENABLE:
	case FUDaePassState::FOG_ENABLE:
	case FUDaePassState::LIGHT_MODEL_LOCAL_VIEWER_ENABLE:
	case FUDaePassState::LIGHT_MODEL_TWO_SIDE_ENABLE:
	case FUDaePassState::LINE_SMOOTH_ENABLE:
	case FUDaePassState::LINE_STIPPLE_ENABLE:
	case FUDaePassState::LOGIC_OP_ENABLE:
	case FUDaePassState::MULTISAMPLE_ENABLE:
	case FUDaePassState::NORMALIZE_ENABLE:
	case FUDaePassState::POINT_SMOOTH_ENABLE:
	case FUDaePassState::POLYGON_OFFSET_FILL_ENABLE:
	case FUDaePassState::POLYGON_OFFSET_LINE_ENABLE:
	case FUDaePassState::POLYGON_OFFSET_POINT_ENABLE:
	case FUDaePassState::POLYGON_SMOOTH_ENABLE:
	case FUDaePassState::POLYGON_STIPPLE_ENABLE:
	case FUDaePassState::RESCALE_NORMAL_ENABLE:
	case FUDaePassState::SAMPLE_ALPHA_TO_COVERAGE_ENABLE:
	case FUDaePassState::SAMPLE_ALPHA_TO_ONE_ENABLE:
	case FUDaePassState::SAMPLE_COVERAGE_ENABLE:
	case FUDaePassState::SCISSOR_TEST_ENABLE:
	case FUDaePassState::STENCIL_TEST_ENABLE:
	case FUDaePassState::COLOR_MATERIAL_ENABLE: {
		const CGbool* b = cgGetBoolStateAssignmentValues(effectState, &valueCount);
		if (valueCount > 0) SET_VALUE(0, bool, b[0] != FALSE);
		break; }
	}

#undef SET_VALUE

	// create the persistent attribute
	CreateAttribute();

	return true;
}

MPlug CFXRenderState::GetPlug()
{
	// Open the dependency node and retrieve the attribute's plug.
	MFnDependencyNode shaderFn(parent->thisMObject());
	return shaderFn.findPlug(attribute);
}

FUDaePassState::State CFXRenderState::GetType() const
{
	return colladaState->GetType();
}

void CFXRenderState::CreateAttribute()
{
	// Look for an attribute with this name.
	attributeName = FUDaePassState::ToString(colladaState->GetType());
	if (IsIndexed())
	{
		attributeName += "-";
		attributeName += FUStringConversion::ToString(index);
	}

	MFnDependencyNode shaderFn(parent->thisMObject());
	MObject oldAttribute = shaderFn.findPlug(attributeName.c_str()).attribute();

	// we always create the attribute. If there was an old attribute with the same name,
	// compare its structure with the newly created attribute to decide if it should replace
	// it or not.

	MFnCompoundAttribute cAttr;
	// create the compound attribute
	MStatus status;
	attribute = cAttr.create(attributeName.c_str(),attributeName.c_str(),&status);

	uint8* data = colladaState->GetData();

#define GET_VALUE(offset, valueType) *((valueType*)(data + offset))

#define ADD_NUMERIC(name,type,offset,valueType) \
	{ \
		MFnNumericAttribute nAttr; \
		MString n(attributeName.c_str()); n += "_"; n += name; \
		nAttr.create(n,n,type,GET_VALUE(offset,valueType)); \
		cAttr.addChild(nAttr.object()); \
	}

#define ADD_COLOR(name,offset) \
	{ \
		MFnNumericAttribute nAttr; \
		MString n(attributeName.c_str()); n += "_"; n += name; \
		nAttr.createColor(n,n); \
		nAttr.setDefault(GET_VALUE(offset,float),GET_VALUE(offset+4,float),GET_VALUE(offset+8,float)); \
		cAttr.addChild(nAttr.object()); \
	}

#define ADD_POINT(name,offset) \
	{ \
		MFnNumericAttribute nAttr; \
		MString n(attributeName.c_str()); n += "_"; n += name; \
		nAttr.createPoint(n,n); \
		nAttr.setDefault(GET_VALUE(offset,float),GET_VALUE(offset+4,float),GET_VALUE(offset+8,float)); \
		cAttr.addChild(nAttr.object()); \
	}

#define ADD_MATRIX(name,offset) \
	{ \
		MFnMatrixAttribute matrixAttr; \
		MString n(attributeName.c_str()); n += "_"; n += name; \
		matrixAttr.create(n,n,MFnMatrixAttribute::kFloat); \
		MMatrix matrix(MConvert::ToMMatrix(GET_VALUE(offset,FMMatrix44))); \
		matrixAttr.setDefault(matrix); \
		cAttr.addChild(matrixAttr.object()); \
	}

	// Create an attribute for this state
	switch (colladaState->GetType())
	{
	case FUDaePassState::ALPHA_FUNC:
		{
			ADD_NUMERIC("func",MFnNumericData::kLong,0,uint32);
			ADD_NUMERIC("ref",MFnNumericData::kFloat,4,float);
		}break;
	case FUDaePassState::CULL_FACE:
		{
			ADD_NUMERIC("mode",MFnNumericData::kLong,0,uint32);
		}break;
	case FUDaePassState::FRONT_FACE:
		{
			ADD_NUMERIC("mode",MFnNumericData::kLong,0,uint32);
		}break;
	case FUDaePassState::LOGIC_OP:
		{
			ADD_NUMERIC("opcode",MFnNumericData::kLong,0,uint32);
		}break;
	case FUDaePassState::POLYGON_MODE:
		{
			ADD_NUMERIC("face",MFnNumericData::kLong,0,uint32);
			ADD_NUMERIC("mode",MFnNumericData::kLong,4,uint32);
		}break;
	case FUDaePassState::SHADE_MODEL:
		{
			ADD_NUMERIC("face",MFnNumericData::kLong,0,uint32);	
		}break;
	case FUDaePassState::CLEAR_COLOR:
		{
			ADD_COLOR("color",0);
			ADD_NUMERIC("alpha",MFnNumericData::kFloat,12,float);
		}break;
	case FUDaePassState::SCISSOR:
		{
			ADD_NUMERIC("x",MFnNumericData::kInt,0,int);
			ADD_NUMERIC("y",MFnNumericData::kInt,4,int);
			ADD_NUMERIC("width",MFnNumericData::kInt,8,int);
			ADD_NUMERIC("height",MFnNumericData::kInt,12,int);
		}break;
	case FUDaePassState::COLOR_MASK:
		{
			ADD_NUMERIC("red",MFnNumericData::kBoolean,0,bool);
			ADD_NUMERIC("green",MFnNumericData::kBoolean,1,bool);
			ADD_NUMERIC("blue",MFnNumericData::kBoolean,2,bool);
			ADD_NUMERIC("alpha",MFnNumericData::kBoolean,3,bool);
		}break;

	case FUDaePassState::BLEND_FUNC:
		{
			ADD_NUMERIC("sRGBA",MFnNumericData::kLong,0,uint32);
			ADD_NUMERIC("dRGBA",MFnNumericData::kLong,4,uint32);
		}break;
	case FUDaePassState::BLEND_FUNC_SEPARATE:
		{
			ADD_NUMERIC("sRGB",MFnNumericData::kLong,0,uint32);
			ADD_NUMERIC("dRGB",MFnNumericData::kLong,4,uint32);	
			ADD_NUMERIC("sALPHA",MFnNumericData::kLong,8,uint32);
			ADD_NUMERIC("dALPHA",MFnNumericData::kLong,12,uint32);	
		}break;
	case FUDaePassState::BLEND_EQUATION:
		{
			ADD_NUMERIC("mode",MFnNumericData::kLong,0,uint32);	
		}break;
	case FUDaePassState::BLEND_EQUATION_SEPARATE:
		{
			ADD_NUMERIC("modeRGB",MFnNumericData::kLong,0,uint32);
			ADD_NUMERIC("modeALPHA",MFnNumericData::kLong,4,uint32);
		} break;
	case FUDaePassState::BLEND_COLOR:
		{
			ADD_COLOR("color",0);
			ADD_NUMERIC("alpha",MFnNumericData::kFloat,12,float);
		} break;

	case FUDaePassState::DEPTH_FUNC:
		{
			ADD_NUMERIC("mode",MFnNumericData::kLong,0,uint32);
		} break;
	case FUDaePassState::DEPTH_BOUNDS:
		{
			ADD_NUMERIC("zmin",MFnNumericData::kFloat,0,float);
			ADD_NUMERIC("zmax",MFnNumericData::kFloat,4,float);
		} break;
	case FUDaePassState::DEPTH_RANGE:
		{
			ADD_NUMERIC("near",MFnNumericData::kFloat,0,float);
			ADD_NUMERIC("far",MFnNumericData::kFloat,4,float);
		} break;
	case FUDaePassState::DEPTH_MASK:
		{
			ADD_NUMERIC("enabled",MFnNumericData::kBoolean,0,bool);
		} break;
	case FUDaePassState::CLEAR_DEPTH:
		{
			ADD_NUMERIC("clear",MFnNumericData::kFloat,0,float);
		} break;
	case FUDaePassState::FOG_MODE:
		{
			ADD_NUMERIC("mode",MFnNumericData::kLong,0,uint32);
		} break;
	case FUDaePassState::FOG_COORD_SRC:
		{
			ADD_NUMERIC("mode",MFnNumericData::kLong,0,uint32);
		} break;break;
	case FUDaePassState::FOG_COLOR:
		{
			ADD_COLOR("color",0);
			ADD_NUMERIC("alpha",MFnNumericData::kFloat,12,float);
		}
		break;
	case FUDaePassState::FOG_DENSITY:
	case FUDaePassState::FOG_END:
	case FUDaePassState::FOG_START:
		{
			ADD_NUMERIC("value",MFnNumericData::kLong,0,uint32);
		} break;
	case FUDaePassState::STENCIL_FUNC:
		{
			ADD_NUMERIC("func",MFnNumericData::kLong,0,uint32);
			ADD_NUMERIC("ref",MFnNumericData::kByte,4,uint8);
			ADD_NUMERIC("mask",MFnNumericData::kByte,5,uint8);
		} break;
	case FUDaePassState::STENCIL_OP:
		{
			ADD_NUMERIC("fail",MFnNumericData::kLong,0,uint32);
			ADD_NUMERIC("zfail",MFnNumericData::kLong,4,uint32);
			ADD_NUMERIC("zpass",MFnNumericData::kLong,8,uint32);
		} break;
	case FUDaePassState::STENCIL_FUNC_SEPARATE:
		{
			ADD_NUMERIC("face",MFnNumericData::kLong,0,uint32);
			ADD_NUMERIC("func",MFnNumericData::kLong,4,uint32);
			ADD_NUMERIC("ref",MFnNumericData::kByte,8,uint8);
			ADD_NUMERIC("mask",MFnNumericData::kByte,9,uint8);
		}
		break;
	case FUDaePassState::STENCIL_OP_SEPARATE:
		{
			ADD_NUMERIC("face",MFnNumericData::kLong,0,uint32);
			ADD_NUMERIC("fail",MFnNumericData::kLong,4,uint32);
			ADD_NUMERIC("zfail",MFnNumericData::kLong,8,uint32);
			ADD_NUMERIC("zpass",MFnNumericData::kLong,12,uint32);
		}
		break;
	case FUDaePassState::STENCIL_MASK_SEPARATE:
		{
			ADD_NUMERIC("face",MFnNumericData::kLong,0,uint32);
			ADD_NUMERIC("mask",MFnNumericData::kLong,4,uint32);
		} break;
	case FUDaePassState::STENCIL_MASK:
		{
			ADD_NUMERIC("mask",MFnNumericData::kLong,0,uint32);
		} break;
	case FUDaePassState::CLEAR_STENCIL:
		{
			ADD_NUMERIC("s",MFnNumericData::kLong,0,uint32);
		}
		break;
	case FUDaePassState::LIGHT_ENABLE:
		{
			ADD_NUMERIC("index",MFnNumericData::kByte,0,uint8);
			ADD_NUMERIC("enabled",MFnNumericData::kBoolean,1,bool);
		} break;
	case FUDaePassState::LIGHT_AMBIENT:
	case FUDaePassState::LIGHT_DIFFUSE:
	case FUDaePassState::LIGHT_SPECULAR:
		{
			ADD_NUMERIC("index",MFnNumericData::kByte,0,uint8);
			ADD_COLOR("color",1);
			ADD_NUMERIC("alpha",MFnNumericData::kFloat,13,float);
		} break;
	case FUDaePassState::LIGHT_POSITION:
		{
			ADD_NUMERIC("index",MFnNumericData::kByte,0,uint8);
			ADD_POINT("point",1);
			ADD_NUMERIC("w",MFnNumericData::kFloat,13,float);
		} break;
	case FUDaePassState::LIGHT_CONSTANT_ATTENUATION:
	case FUDaePassState::LIGHT_LINEAR_ATTENUATION:
	case FUDaePassState::LIGHT_QUADRATIC_ATTENUATION:
		{
			ADD_NUMERIC("index",MFnNumericData::kByte,0,uint8);
			ADD_NUMERIC("factor",MFnNumericData::kFloat,1,float);
		} break;
	case FUDaePassState::LIGHT_SPOT_CUTOFF:
		{
			ADD_NUMERIC("index",MFnNumericData::kByte,0,uint8);
			ADD_NUMERIC("cutoff",MFnNumericData::kFloat,1,float);
		} break;
	case FUDaePassState::LIGHT_SPOT_EXPONENT:
		{
			ADD_NUMERIC("index",MFnNumericData::kByte,0,uint8);
			ADD_NUMERIC("exp",MFnNumericData::kFloat,1,float);
		} break;
	case FUDaePassState::LIGHT_SPOT_DIRECTION:
		{
			ADD_NUMERIC("index",MFnNumericData::kByte,0,uint8);
			ADD_POINT("dir",1);
		} break;
	case FUDaePassState::LIGHT_MODEL_COLOR_CONTROL:
		{
			ADD_NUMERIC("control",MFnNumericData::kLong,0,uint32);
		} break;
	case FUDaePassState::LIGHT_MODEL_AMBIENT:
		{
			ADD_COLOR("color",0);
			ADD_NUMERIC("alpha",MFnNumericData::kFloat,12,float);
		} break;
	case FUDaePassState::TEXTURE1D_ENABLE:
	case FUDaePassState::TEXTURE2D_ENABLE:
	case FUDaePassState::TEXTURE3D_ENABLE:
	case FUDaePassState::TEXTURECUBE_ENABLE:
	case FUDaePassState::TEXTURERECT_ENABLE:
	case FUDaePassState::TEXTUREDEPTH_ENABLE:
		{
			ADD_NUMERIC("index",MFnNumericData::kByte,0,uint8);
			ADD_NUMERIC("enabled",MFnNumericData::kBoolean,1,bool);
		} break;
	case FUDaePassState::TEXTURE_ENV_COLOR:
		{
			ADD_NUMERIC("index",MFnNumericData::kByte,0,uint8);
			ADD_COLOR("color",1);
			ADD_NUMERIC("alpha",MFnNumericData::kFloat,13,float);
		} break;
	case FUDaePassState::CLIP_PLANE_ENABLE:
		{
			ADD_NUMERIC("index",MFnNumericData::kByte,0,uint8);
			ADD_NUMERIC("enabled",MFnNumericData::kBoolean,1,bool);
		} break;
	case FUDaePassState::CLIP_PLANE:
		{
			ADD_NUMERIC("index",MFnNumericData::kByte,0,uint8);
			ADD_NUMERIC("a",MFnNumericData::kFloat,1,float);
			ADD_NUMERIC("b",MFnNumericData::kFloat,5,float);
			ADD_NUMERIC("c",MFnNumericData::kFloat,9,float);
			ADD_NUMERIC("d",MFnNumericData::kFloat,13,float);
		} break;
	case FUDaePassState::COLOR_MATERIAL:
		{
			ADD_NUMERIC("face",MFnNumericData::kLong,0,uint32);
			ADD_NUMERIC("type",MFnNumericData::kLong,4,uint32);
		} break;
	case FUDaePassState::MATERIAL_AMBIENT:
	case FUDaePassState::MATERIAL_DIFFUSE:
	case FUDaePassState::MATERIAL_EMISSION:
	case FUDaePassState::MATERIAL_SPECULAR:
		{
			ADD_COLOR("color",0);
			ADD_NUMERIC("alpha",MFnNumericData::kFloat,12,float);
		} break;
	case FUDaePassState::MATERIAL_SHININESS:
		{
			ADD_NUMERIC("shininess",MFnNumericData::kFloat,0,float);
		} break;

	case FUDaePassState::LINE_WIDTH:
		{
			ADD_NUMERIC("width",MFnNumericData::kFloat,0,float);	
		} break;
	case FUDaePassState::LINE_STIPPLE:
		{
			ADD_NUMERIC("factor",MFnNumericData::kShort,0,uint16);
			ADD_NUMERIC("pattern",MFnNumericData::kShort,2,uint16);
		} break;
	case FUDaePassState::POINT_SIZE:
	case FUDaePassState::POINT_SIZE_MIN:
	case FUDaePassState::POINT_SIZE_MAX:
		{
			ADD_NUMERIC("size",MFnNumericData::kFloat,0,float);	
		} break;
	case FUDaePassState::POINT_FADE_THRESHOLD_SIZE:
		{
			ADD_NUMERIC("size",MFnNumericData::kFloat,0,float);	
		} break;
	case FUDaePassState::POINT_DISTANCE_ATTENUATION:
		{
			ADD_NUMERIC("a",MFnNumericData::kFloat,0,float);
			ADD_NUMERIC("b",MFnNumericData::kFloat,4,float);
			ADD_NUMERIC("c",MFnNumericData::kFloat,8,float);
		} break;
	case FUDaePassState::POLYGON_OFFSET:
		{
			ADD_NUMERIC("factor",MFnNumericData::kFloat,0,float);
			ADD_NUMERIC("units",MFnNumericData::kFloat,4,float);
		} break;
	case FUDaePassState::MODEL_VIEW_MATRIX:
		{
			ADD_MATRIX("matrix",0);
		} break;
	case FUDaePassState::PROJECTION_MATRIX:
		{
			ADD_MATRIX("matrix",0);
		} break;

	case FUDaePassState::LIGHTING_ENABLE:
	case FUDaePassState::ALPHA_TEST_ENABLE:
	case FUDaePassState::AUTO_NORMAL_ENABLE:
	case FUDaePassState::BLEND_ENABLE:
	case FUDaePassState::COLOR_LOGIC_OP_ENABLE:
	case FUDaePassState::CULL_FACE_ENABLE:
	case FUDaePassState::DEPTH_BOUNDS_ENABLE:
	case FUDaePassState::DEPTH_CLAMP_ENABLE:
	case FUDaePassState::DEPTH_TEST_ENABLE:
	case FUDaePassState::DITHER_ENABLE:
	case FUDaePassState::FOG_ENABLE:
	case FUDaePassState::LIGHT_MODEL_LOCAL_VIEWER_ENABLE:
	case FUDaePassState::LIGHT_MODEL_TWO_SIDE_ENABLE:
	case FUDaePassState::LINE_SMOOTH_ENABLE:
	case FUDaePassState::LINE_STIPPLE_ENABLE:
	case FUDaePassState::LOGIC_OP_ENABLE:
	case FUDaePassState::MULTISAMPLE_ENABLE:
	case FUDaePassState::NORMALIZE_ENABLE:
	case FUDaePassState::POINT_SMOOTH_ENABLE:
	case FUDaePassState::POLYGON_OFFSET_FILL_ENABLE:
	case FUDaePassState::POLYGON_OFFSET_LINE_ENABLE:
	case FUDaePassState::POLYGON_OFFSET_POINT_ENABLE:
	case FUDaePassState::POLYGON_SMOOTH_ENABLE:
	case FUDaePassState::POLYGON_STIPPLE_ENABLE:
	case FUDaePassState::RESCALE_NORMAL_ENABLE:
	case FUDaePassState::SAMPLE_ALPHA_TO_COVERAGE_ENABLE:
	case FUDaePassState::SAMPLE_ALPHA_TO_ONE_ENABLE:
	case FUDaePassState::SAMPLE_COVERAGE_ENABLE:
	case FUDaePassState::SCISSOR_TEST_ENABLE:
	case FUDaePassState::STENCIL_TEST_ENABLE:
	case FUDaePassState::COLOR_MATERIAL_ENABLE:
		{
			ADD_NUMERIC("enabled",MFnNumericData::kBoolean,0,bool);
		} break;

	case FUDaePassState::INVALID:
	default: break;
	}

	if (!attribute.isNull() && !oldAttribute.isNull() && 
		oldAttribute.hasFn(MFn::kCompoundAttribute))
	{
		MFnCompoundAttribute original(attribute), previous(oldAttribute);
		if (original.numChildren() == previous.numChildren())
		{
			// this heuristic should be enough to determine that the two attributes are equal
			attribute = oldAttribute;
		}
	}

	// finally add the attribute
	AddAttribute();

#undef ADD_MATRIX
#undef ADD_POINT
#undef ADD_COLOR
#undef ADD_NUMERIC
#undef GET_VALUE
}

void CFXRenderState::AddAttribute()
{
	if (attribute.isNull())
		return;

	// add the *compound* attribute to the parent node
	MFnDependencyNode shaderFn(parent->thisMObject());
	shaderFn.addAttribute(attribute);
}

void CFXRenderState::Synchronize()
{
	MPlug plug = GetPlug();
	uint8* data = colladaState->GetData();
	uint32 currentAttrib = 0;

#define SET_VALUE(offset, valueType, actualValue) *((valueType*)(data + offset)) = (valueType) (actualValue);

#define SYNC_NUMERIC(offset,valueType){ \
	MPlug child = plug.child(currentAttrib); \
	valueType value; \
	CDagHelper::GetPlugValue(child,value); \
	DEBUG_OUT("value = %s\n",FUStringConversion::ToString(value).c_str()); \
	SET_VALUE(offset,valueType,value); \
	currentAttrib++; }

#define SYNC_COLOR(offset){ \
	MPlug child = plug.child(currentAttrib); \
	float r,g,b; \
	CDagHelper::GetPlugValue(child,r,g,b); \
	DEBUG_OUT("value = %.4f\n",r); \
	DEBUG_OUT("value = %.4f\n",g); \
	DEBUG_OUT("value = %.4f\n",b); \
	SET_VALUE(offset,float,r); \
	SET_VALUE(offset+4,float,g); \
	SET_VALUE(offset+8,float,b); \
	currentAttrib++; }

#define SYNC_POINT(offset) SYNC_COLOR(offset)

#define SYNC_MATRIX(offset){ \
	MPlug child = plug.child(currentAttrib); \
	MMatrix value; \
	CDagHelper::GetPlugValue(child,value); \
	FMMatrix44 fmValue = MConvert::ToFMatrix(value); \
	SET_VALUE(offset,FMMatrix44,fmValue); \
	currentAttrib++; }

	/** Fast render states switch/case generation using sed on Cygwin:
		$ sed 's/ADD_NUMERIC(\(.*\)\,\(.*\)\,\(.*\)$/SYNC_NUMERIC(\2,\3/' < file1 > file2
	*/

	switch (colladaState->GetType())
	{
	case FUDaePassState::ALPHA_FUNC:
		{
			SYNC_NUMERIC(0,uint32);
			SYNC_NUMERIC(4,float);
		}break;
	case FUDaePassState::CULL_FACE:
		{
			SYNC_NUMERIC(0,uint32);
		}break;
	case FUDaePassState::FRONT_FACE:
		{
			SYNC_NUMERIC(0,uint32);
		}break;
	case FUDaePassState::LOGIC_OP:
		{
			SYNC_NUMERIC(0,uint32);
		}break;
	case FUDaePassState::POLYGON_MODE:
		{
			SYNC_NUMERIC(0,uint32);
			SYNC_NUMERIC(4,uint32);
		}break;
	case FUDaePassState::SHADE_MODEL:
		{
			SYNC_NUMERIC(0,uint32);	
		}break;
	case FUDaePassState::CLEAR_COLOR:
		{
			SYNC_COLOR(0);
			SYNC_NUMERIC(12,float);
		}break;
	case FUDaePassState::SCISSOR:
		{
			SYNC_NUMERIC(0,int);
			SYNC_NUMERIC(4,int);
			SYNC_NUMERIC(8,int);
			SYNC_NUMERIC(12,int);
		}break;
	case FUDaePassState::COLOR_MASK:
		{
			SYNC_NUMERIC(0,bool);
			SYNC_NUMERIC(1,bool);
			SYNC_NUMERIC(2,bool);
			SYNC_NUMERIC(3,bool);
		}break;

	case FUDaePassState::BLEND_FUNC:
		{
			SYNC_NUMERIC(0,uint32);
			SYNC_NUMERIC(4,uint32);
		}break;
	case FUDaePassState::BLEND_FUNC_SEPARATE:
		{
			SYNC_NUMERIC(0,uint32);
			SYNC_NUMERIC(4,uint32);	
			SYNC_NUMERIC(8,uint32);
			SYNC_NUMERIC(12,uint32);	
		}break;
	case FUDaePassState::BLEND_EQUATION:
		{
			SYNC_NUMERIC(0,uint32);	
		}break;
	case FUDaePassState::BLEND_EQUATION_SEPARATE:
		{
			SYNC_NUMERIC(0,uint32);
			SYNC_NUMERIC(4,uint32);
		} break;
	case FUDaePassState::BLEND_COLOR:
		{
			SYNC_COLOR(0);
			SYNC_NUMERIC(12,float);
		} break;

	case FUDaePassState::DEPTH_FUNC:
		{
			SYNC_NUMERIC(0,uint32);
		} break;
	case FUDaePassState::DEPTH_BOUNDS:
		{
			SYNC_NUMERIC(0,float);
			SYNC_NUMERIC(4,float);
		} break;
	case FUDaePassState::DEPTH_RANGE:
		{
			SYNC_NUMERIC(0,float);
			SYNC_NUMERIC(4,float);
		} break;
	case FUDaePassState::DEPTH_MASK:
		{
			SYNC_NUMERIC(0,bool);
		} break;
	case FUDaePassState::CLEAR_DEPTH:
		{
			SYNC_NUMERIC(0,float);
		} break;
	case FUDaePassState::FOG_MODE:
		{
			SYNC_NUMERIC(0,uint32);
		} break;
	case FUDaePassState::FOG_COORD_SRC:
		{
			SYNC_NUMERIC(0,uint32);
		} break;break;
	case FUDaePassState::FOG_COLOR:
		{
			SYNC_COLOR(0);
			SYNC_NUMERIC(12,float);
		}
		break;
	case FUDaePassState::FOG_DENSITY:
	case FUDaePassState::FOG_END:
	case FUDaePassState::FOG_START:
		{
			SYNC_NUMERIC(0,uint32);
		} break;
	case FUDaePassState::STENCIL_FUNC:
		{
			SYNC_NUMERIC(0,uint32);
			SYNC_NUMERIC(4,uint8);
			SYNC_NUMERIC(5,uint8);
		} break;
	case FUDaePassState::STENCIL_OP:
		{
			SYNC_NUMERIC(0,uint32);
			SYNC_NUMERIC(4,uint32);
			SYNC_NUMERIC(8,uint32);
		} break;
	case FUDaePassState::STENCIL_FUNC_SEPARATE:
		{
			SYNC_NUMERIC(0,uint32);
			SYNC_NUMERIC(4,uint32);
			SYNC_NUMERIC(8,uint8);
			SYNC_NUMERIC(9,uint8);
		}
		break;
	case FUDaePassState::STENCIL_OP_SEPARATE:
		{
			SYNC_NUMERIC(0,uint32);
			SYNC_NUMERIC(4,uint32);
			SYNC_NUMERIC(8,uint32);
			SYNC_NUMERIC(12,uint32);
		}
		break;
	case FUDaePassState::STENCIL_MASK_SEPARATE:
		{
			SYNC_NUMERIC(0,uint32);
			SYNC_NUMERIC(4,uint32);
		} break;
	case FUDaePassState::STENCIL_MASK:
		{
			SYNC_NUMERIC(0,uint32);
		} break;
	case FUDaePassState::CLEAR_STENCIL:
		{
			SYNC_NUMERIC(0,uint32);
		}
		break;
	case FUDaePassState::LIGHT_ENABLE:
		{
			SYNC_NUMERIC(0,uint8);
			SYNC_NUMERIC(1,bool);
		} break;
	case FUDaePassState::LIGHT_AMBIENT:
	case FUDaePassState::LIGHT_DIFFUSE:
	case FUDaePassState::LIGHT_SPECULAR:
		{
			SYNC_NUMERIC(0,uint8);
			SYNC_COLOR(1);
			SYNC_NUMERIC(13,float);
		} break;
	case FUDaePassState::LIGHT_POSITION:
		{
			SYNC_NUMERIC(0,uint8);
			SYNC_POINT(1);
			SYNC_NUMERIC(13,float);
		} break;
	case FUDaePassState::LIGHT_CONSTANT_ATTENUATION:
	case FUDaePassState::LIGHT_LINEAR_ATTENUATION:
	case FUDaePassState::LIGHT_QUADRATIC_ATTENUATION:
		{
			SYNC_NUMERIC(0,uint8);
			SYNC_NUMERIC(1,float);
		} break;
	case FUDaePassState::LIGHT_SPOT_CUTOFF:
		{
			SYNC_NUMERIC(0,uint8);
			SYNC_NUMERIC(1,float);
		} break;
	case FUDaePassState::LIGHT_SPOT_EXPONENT:
		{
			SYNC_NUMERIC(0,uint8);
			SYNC_NUMERIC(1,float);
		} break;
	case FUDaePassState::LIGHT_SPOT_DIRECTION:
		{
			SYNC_NUMERIC(0,uint8);
			SYNC_POINT(1);
		} break;
	case FUDaePassState::LIGHT_MODEL_COLOR_CONTROL:
		{
			SYNC_NUMERIC(0,uint32);
		} break;
	case FUDaePassState::LIGHT_MODEL_AMBIENT:
		{
			SYNC_COLOR(0);
			SYNC_NUMERIC(12,float);
		} break;
	case FUDaePassState::TEXTURE1D_ENABLE:
	case FUDaePassState::TEXTURE2D_ENABLE:
	case FUDaePassState::TEXTURE3D_ENABLE:
	case FUDaePassState::TEXTURECUBE_ENABLE:
	case FUDaePassState::TEXTURERECT_ENABLE:
	case FUDaePassState::TEXTUREDEPTH_ENABLE:
		{
			SYNC_NUMERIC(0,uint8);
			SYNC_NUMERIC(1,bool);
		} break;
	case FUDaePassState::TEXTURE_ENV_COLOR:
		{
			SYNC_NUMERIC(0,uint8);
			SYNC_COLOR(1);
			SYNC_NUMERIC(13,float);
		} break;
	case FUDaePassState::CLIP_PLANE_ENABLE:
		{
			SYNC_NUMERIC(0,uint8);
			SYNC_NUMERIC(1,bool);
		} break;
	case FUDaePassState::CLIP_PLANE:
		{
			SYNC_NUMERIC(0,uint8);
			SYNC_NUMERIC(1,float);
			SYNC_NUMERIC(5,float);
			SYNC_NUMERIC(9,float);
			SYNC_NUMERIC(13,float);
		} break;
	case FUDaePassState::COLOR_MATERIAL:
		{
			SYNC_NUMERIC(0,uint32);
			SYNC_NUMERIC(4,uint32);
		} break;
	case FUDaePassState::MATERIAL_AMBIENT:
	case FUDaePassState::MATERIAL_DIFFUSE:
	case FUDaePassState::MATERIAL_EMISSION:
	case FUDaePassState::MATERIAL_SPECULAR:
		{
			SYNC_COLOR(0);
			SYNC_NUMERIC(12,float);
		} break;
	case FUDaePassState::MATERIAL_SHININESS:
		{
			SYNC_NUMERIC(0,float);
		} break;

	case FUDaePassState::LINE_WIDTH:
		{
			SYNC_NUMERIC(0,float);	
		} break;
	case FUDaePassState::LINE_STIPPLE:
		{
			SYNC_NUMERIC(0,uint16);
			SYNC_NUMERIC(2,uint16);
		} break;
	case FUDaePassState::POINT_SIZE:
	case FUDaePassState::POINT_SIZE_MIN:
	case FUDaePassState::POINT_SIZE_MAX:
		{
			SYNC_NUMERIC(0,float);	
		} break;
	case FUDaePassState::POINT_FADE_THRESHOLD_SIZE:
		{
			SYNC_NUMERIC(0,float);	
		} break;
	case FUDaePassState::POINT_DISTANCE_ATTENUATION:
		{
			SYNC_NUMERIC(0,float);
			SYNC_NUMERIC(4,float);
			SYNC_NUMERIC(8,float);
		} break;
	case FUDaePassState::POLYGON_OFFSET:
		{
			SYNC_NUMERIC(0,float);
			SYNC_NUMERIC(4,float);
		} break;
	case FUDaePassState::MODEL_VIEW_MATRIX:
		{
			SYNC_MATRIX(0);
		} break;
	case FUDaePassState::PROJECTION_MATRIX:
		{
			SYNC_MATRIX(0);
		} break;

	case FUDaePassState::LIGHTING_ENABLE:
	case FUDaePassState::ALPHA_TEST_ENABLE:
	case FUDaePassState::AUTO_NORMAL_ENABLE:
	case FUDaePassState::BLEND_ENABLE:
	case FUDaePassState::COLOR_LOGIC_OP_ENABLE:
	case FUDaePassState::CULL_FACE_ENABLE:
	case FUDaePassState::DEPTH_BOUNDS_ENABLE:
	case FUDaePassState::DEPTH_CLAMP_ENABLE:
	case FUDaePassState::DEPTH_TEST_ENABLE:
	case FUDaePassState::DITHER_ENABLE:
	case FUDaePassState::FOG_ENABLE:
	case FUDaePassState::LIGHT_MODEL_LOCAL_VIEWER_ENABLE:
	case FUDaePassState::LIGHT_MODEL_TWO_SIDE_ENABLE:
	case FUDaePassState::LINE_SMOOTH_ENABLE:
	case FUDaePassState::LINE_STIPPLE_ENABLE:
	case FUDaePassState::LOGIC_OP_ENABLE:
	case FUDaePassState::MULTISAMPLE_ENABLE:
	case FUDaePassState::NORMALIZE_ENABLE:
	case FUDaePassState::POINT_SMOOTH_ENABLE:
	case FUDaePassState::POLYGON_OFFSET_FILL_ENABLE:
	case FUDaePassState::POLYGON_OFFSET_LINE_ENABLE:
	case FUDaePassState::POLYGON_OFFSET_POINT_ENABLE:
	case FUDaePassState::POLYGON_SMOOTH_ENABLE:
	case FUDaePassState::POLYGON_STIPPLE_ENABLE:
	case FUDaePassState::RESCALE_NORMAL_ENABLE:
	case FUDaePassState::SAMPLE_ALPHA_TO_COVERAGE_ENABLE:
	case FUDaePassState::SAMPLE_ALPHA_TO_ONE_ENABLE:
	case FUDaePassState::SAMPLE_COVERAGE_ENABLE:
	case FUDaePassState::SCISSOR_TEST_ENABLE:
	case FUDaePassState::STENCIL_TEST_ENABLE:
	case FUDaePassState::COLOR_MATERIAL_ENABLE:
		{
			SYNC_NUMERIC(0,bool);
		} break;

	case FUDaePassState::INVALID:
	default: break;
	}

#undef SYNC_MATRIX
#undef SYNC_POINT
#undef SYNC_COLOR
#undef SYNC_NUMERIC
#undef SET_VALUE

	ResetDirty();
}

void CFXRenderState::Use()
{
	if (colladaState == NULL) return;

	uint8* data = colladaState->GetData();
#define GET_VALUE(offset, valueType) *((valueType*)(data + offset))

	switch (colladaState->GetType())
	{
	case FUDaePassState::ALPHA_FUNC: glAlphaFunc(GET_VALUE(0, uint32), GET_VALUE(4, float)); break;
	case FUDaePassState::CULL_FACE: glCullFace(GET_VALUE(0, uint32)); break;
	case FUDaePassState::FRONT_FACE: glFrontFace(GET_VALUE(0, uint32)); break;
	case FUDaePassState::LOGIC_OP: glLogicOp(GET_VALUE(0, uint32)); break;
	case FUDaePassState::POLYGON_MODE: glPolygonMode(GET_VALUE(0, uint32), GET_VALUE(4, uint32)); break;
	case FUDaePassState::SHADE_MODEL: glShadeModel(GET_VALUE(0, uint32)); break;
	case FUDaePassState::CLEAR_COLOR: glClearColor(GET_VALUE(0, float), GET_VALUE(4, float), GET_VALUE(8, float), GET_VALUE(12, float)); break;
	case FUDaePassState::SCISSOR: glScissor((GLint) GET_VALUE(0, float), (GLint) GET_VALUE(4, float), (GLsizei) GET_VALUE(8, float), (GLsizei) GET_VALUE(12, float)); break;
	case FUDaePassState::COLOR_MASK: glColorMask(GET_VALUE(0, bool), GET_VALUE(1, bool), GET_VALUE(2, bool), GET_VALUE(3, bool)); break;

	case FUDaePassState::BLEND_FUNC: glBlendFunc(GET_VALUE(0, uint32), GET_VALUE(4, uint32)); break;
	case FUDaePassState::BLEND_FUNC_SEPARATE: glBlendFuncSeparate(GET_VALUE(0, uint32), GET_VALUE(4, uint32), GET_VALUE(8, uint32), GET_VALUE(12, uint32)); break;
	case FUDaePassState::BLEND_EQUATION: glBlendEquation(GET_VALUE(0, uint32)); break;
	case FUDaePassState::BLEND_EQUATION_SEPARATE: glBlendEquationSeparate(GET_VALUE(0, uint32), GET_VALUE(4, uint32)); break;
	case FUDaePassState::BLEND_COLOR: glBlendColor(GET_VALUE(0, float), GET_VALUE(4, float), GET_VALUE(8, float), GET_VALUE(12, float)); break;

	case FUDaePassState::DEPTH_FUNC: glDepthFunc(GET_VALUE(0, uint32)); break;
	case FUDaePassState::DEPTH_BOUNDS: glDepthBoundsEXT(GET_VALUE(0, float), GET_VALUE(4, float)); break;
	case FUDaePassState::DEPTH_RANGE: glDepthRange(GET_VALUE(0, float), GET_VALUE(4, float)); break;
	case FUDaePassState::DEPTH_MASK: glDepthMask(GET_VALUE(0, bool)); break;
	case FUDaePassState::CLEAR_DEPTH: glClearDepth(GET_VALUE(0, float)); break;

	case FUDaePassState::FOG_MODE: glFogi(GL_FOG_MODE, GET_VALUE(0, uint32)); break;
	case FUDaePassState::FOG_COORD_SRC: glFogi(GL_FOG_COORD_SRC, GET_VALUE(0, uint32)); break;
	case FUDaePassState::FOG_COLOR: glFogfv(GL_FOG_COLOR, GET_VALUE(0, FMVector4)); break;
	case FUDaePassState::FOG_DENSITY: glFogf(GL_FOG_DENSITY, GET_VALUE(0, float)); break;
	case FUDaePassState::FOG_END: glFogf(GL_FOG_END, GET_VALUE(0, float)); break;
	case FUDaePassState::FOG_START: glFogf(GL_FOG_START, GET_VALUE(0, float)); break;

	case FUDaePassState::STENCIL_FUNC: glStencilFunc(GET_VALUE(0, uint32), GET_VALUE(4, uint8), GET_VALUE(5, uint8)); break;
	case FUDaePassState::STENCIL_OP: glStencilOp(GET_VALUE(0, uint32), GET_VALUE(4, uint32), GET_VALUE(8, uint32)); break;
	case FUDaePassState::STENCIL_FUNC_SEPARATE: glStencilFuncSeparate(GET_VALUE(0, uint32), GET_VALUE(4, uint32), GET_VALUE(8, uint8), GET_VALUE(9, uint8)); break;
	case FUDaePassState::STENCIL_OP_SEPARATE: glStencilOpSeparate(GET_VALUE(0, uint32), GET_VALUE(4, uint32), GET_VALUE(8, uint32), GET_VALUE(12, uint32)); break;
	case FUDaePassState::STENCIL_MASK_SEPARATE: glStencilMaskSeparate(GET_VALUE(0, uint32), GET_VALUE(4, uint8)); break;
	case FUDaePassState::STENCIL_MASK: glStencilMask(GET_VALUE(0, uint32)); break;
	case FUDaePassState::CLEAR_STENCIL: glClearStencil(GET_VALUE(0, uint32)); break;

	case FUDaePassState::LIGHT_ENABLE: if (GET_VALUE(1, bool)) glEnable(GL_LIGHT0 + GET_VALUE(0, uint8)); else glDisable(GL_LIGHT0 + GET_VALUE(0, uint8)); break;
	case FUDaePassState::LIGHT_AMBIENT: glLightfv(GL_LIGHT0 + GET_VALUE(0, uint8), GL_AMBIENT, GET_VALUE(1, FMVector4)); break;
	case FUDaePassState::LIGHT_DIFFUSE: glLightfv(GL_LIGHT0 + GET_VALUE(0, uint8), GL_DIFFUSE, GET_VALUE(1, FMVector4)); break;
	case FUDaePassState::LIGHT_SPECULAR: glLightfv(GL_LIGHT0 + GET_VALUE(0, uint8), GL_SPECULAR, GET_VALUE(1, FMVector4)); break;
	case FUDaePassState::LIGHT_POSITION: glLightfv(GL_LIGHT0 + GET_VALUE(0, uint8), GL_POSITION, GET_VALUE(1, FMVector4)); break;
	case FUDaePassState::LIGHT_CONSTANT_ATTENUATION: glLightf(GL_LIGHT0 + GET_VALUE(0, uint8), GL_CONSTANT_ATTENUATION, GET_VALUE(1, float)); break;
	case FUDaePassState::LIGHT_LINEAR_ATTENUATION: glLightf(GL_LIGHT0 + GET_VALUE(0, uint8), GL_LINEAR_ATTENUATION, GET_VALUE(1, float)); break;
	case FUDaePassState::LIGHT_QUADRATIC_ATTENUATION: glLightf(GL_LIGHT0 + GET_VALUE(0, uint8), GL_QUADRATIC_ATTENUATION, GET_VALUE(1, float)); break;
	case FUDaePassState::LIGHT_SPOT_CUTOFF: glLightf(GL_LIGHT0 + GET_VALUE(0, uint8), GL_SPOT_CUTOFF, GET_VALUE(1, float)); break;
	case FUDaePassState::LIGHT_SPOT_EXPONENT: glLightf(GL_LIGHT0 + GET_VALUE(0, uint8), GL_SPOT_EXPONENT, GET_VALUE(1, float)); break;
	case FUDaePassState::LIGHT_SPOT_DIRECTION: glLightfv(GL_LIGHT0 + GET_VALUE(0, uint8), GL_SPOT_DIRECTION, GET_VALUE(1, FMVector3)); break;
	case FUDaePassState::LIGHT_MODEL_COLOR_CONTROL: glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GET_VALUE(0, uint32)); break;
	case FUDaePassState::LIGHT_MODEL_AMBIENT: glLightModelfv(GL_LIGHT_MODEL_AMBIENT, GET_VALUE(0, FMVector4)); break;

	case FUDaePassState::TEXTURE1D: /* No support -- GET_VALUE(0, uint8); GET_VALUE(1, uint32); */ break;
	case FUDaePassState::TEXTURE2D: /* No support -- GET_VALUE(0, uint8); GET_VALUE(1, uint32); */ break;
	case FUDaePassState::TEXTURE3D: /* No support -- GET_VALUE(0, uint8); GET_VALUE(1, uint32); */ break;
	case FUDaePassState::TEXTURECUBE: /* No support -- GET_VALUE(0, uint8); GET_VALUE(1, uint32); */ break;
	case FUDaePassState::TEXTURERECT: /* No support -- GET_VALUE(0, uint8); GET_VALUE(1, uint32); */ break;
	case FUDaePassState::TEXTUREDEPTH: /* No support -- GET_VALUE(0, uint8); GET_VALUE(1, uint32); */ break;
	case FUDaePassState::TEXTURE1D_ENABLE: glActiveTexture(GL_TEXTURE0 + GET_VALUE(0, uint8)); if (GET_VALUE(1, bool)) glEnable(GL_TEXTURE_1D); else glDisable(GL_TEXTURE_1D); break;
	case FUDaePassState::TEXTURE2D_ENABLE: glActiveTexture(GL_TEXTURE0 + GET_VALUE(0, uint8)); if (GET_VALUE(1, bool)) glEnable(GL_TEXTURE_2D); else glDisable(GL_TEXTURE_2D); break;
	case FUDaePassState::TEXTURE3D_ENABLE: glActiveTexture(GL_TEXTURE0 + GET_VALUE(0, uint8)); if (GET_VALUE(1, bool)) glEnable(GL_TEXTURE_3D); else glDisable(GL_TEXTURE_3D); break;
	case FUDaePassState::TEXTURECUBE_ENABLE: glActiveTexture(GL_TEXTURE0 + GET_VALUE(0, uint8)); if (GET_VALUE(1, bool)) glEnable(GL_TEXTURE_CUBE_MAP_ARB); else glDisable(GL_TEXTURE_CUBE_MAP_ARB); break;
	case FUDaePassState::TEXTURERECT_ENABLE: glActiveTexture(GL_TEXTURE0 + GET_VALUE(0, uint8)); if (GET_VALUE(1, bool)) glEnable(GL_TEXTURE_RECTANGLE_ARB); else glDisable(GL_TEXTURE_RECTANGLE_ARB); break;
	case FUDaePassState::TEXTUREDEPTH_ENABLE: glActiveTexture(GL_TEXTURE0 + GET_VALUE(0, uint8)); if (GET_VALUE(1, bool)) glEnable(GL_TEXTURE_DEPTH); else glDisable(GL_TEXTURE_DEPTH); break;
	case FUDaePassState::TEXTURE_ENV_COLOR: glActiveTexture(GL_TEXTURE0 + GET_VALUE(0, uint8)); glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, GET_VALUE(1, FMVector4)); break;
	case FUDaePassState::TEXTURE_ENV_MODE: /* Not handled... */ break;

	case FUDaePassState::CLIP_PLANE_ENABLE: if (GET_VALUE(1, bool)) glEnable(GL_CLIP_PLANE0 + GET_VALUE(0, uint8)); else glDisable(GL_CLIP_PLANE0 + GET_VALUE(0, uint8)); break;
	case FUDaePassState::CLIP_PLANE: {
		double f[4] = { GET_VALUE(1, float), GET_VALUE(5, float), GET_VALUE(9, float), GET_VALUE(13, float) };
		glClipPlane(GL_CLIP_PLANE0 + GET_VALUE(0, uint8), f);
		break; }

	case FUDaePassState::COLOR_MATERIAL: glColorMaterial(GET_VALUE(0, uint32), GET_VALUE(4, uint32)); break;
	case FUDaePassState::MATERIAL_AMBIENT: glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, GET_VALUE(0, FMVector4)); break;
	case FUDaePassState::MATERIAL_DIFFUSE: glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, GET_VALUE(0, FMVector4)); break;
	case FUDaePassState::MATERIAL_EMISSION: glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, GET_VALUE(0, FMVector4)); break;
	case FUDaePassState::MATERIAL_SPECULAR: glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, GET_VALUE(0, FMVector4)); break;
	case FUDaePassState::MATERIAL_SHININESS: glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, GET_VALUE(0, float)); break;

	case FUDaePassState::LINE_WIDTH: glLineWidth(GET_VALUE(0, float)); break;
	case FUDaePassState::LINE_STIPPLE: glLineStipple((GLint) GET_VALUE(0, uint16), GET_VALUE(2, uint16)); break;

	case FUDaePassState::POINT_SIZE: glPointSize(GET_VALUE(0, float)); break;
	case FUDaePassState::POINT_SIZE_MIN: glPointParameterf(GL_POINT_SIZE_MIN, GET_VALUE(0, float)); break;
	case FUDaePassState::POINT_SIZE_MAX: glPointParameterf(GL_POINT_SIZE_MAX, GET_VALUE(0, float)); break;
	case FUDaePassState::POINT_FADE_THRESHOLD_SIZE: glPointParameterf(GL_POINT_FADE_THRESHOLD_SIZE, GET_VALUE(0, float)); break;
	case FUDaePassState::POINT_DISTANCE_ATTENUATION: glPointParameterfv(GL_POINT_DISTANCE_ATTENUATION, GET_VALUE(0, FMVector3)); break;

	case FUDaePassState::POLYGON_OFFSET: glPolygonOffset(GET_VALUE(0, float), GET_VALUE(4, float)); break;

	case FUDaePassState::MODEL_VIEW_MATRIX: glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glMultMatrixf(GET_VALUE(0, FMMatrix44)); break;
	case FUDaePassState::PROJECTION_MATRIX: glMatrixMode(GL_PROJECTION); glLoadIdentity(); glMultMatrixf(GET_VALUE(0, FMMatrix44)); break;

	case FUDaePassState::LIGHTING_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_LIGHTING); else glDisable(GL_LIGHTING); break;
	case FUDaePassState::ALPHA_TEST_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_ALPHA_TEST); else glDisable(GL_ALPHA_TEST); break;
	case FUDaePassState::AUTO_NORMAL_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_AUTO_NORMAL); else glDisable(GL_AUTO_NORMAL); break;
	case FUDaePassState::BLEND_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_BLEND); else glDisable(GL_BLEND); break;
	case FUDaePassState::COLOR_LOGIC_OP_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_COLOR_LOGIC_OP); else glDisable(GL_COLOR_LOGIC_OP); break;
	case FUDaePassState::CULL_FACE_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE); break;
	case FUDaePassState::DEPTH_BOUNDS_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_DEPTH_BOUNDS_EXT); else glDisable(GL_DEPTH_BOUNDS_EXT); break;
	case FUDaePassState::DEPTH_CLAMP_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_DEPTH_CLAMP_NV); else glDisable(GL_DEPTH_CLAMP_NV); break;
	case FUDaePassState::DEPTH_TEST_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST); break;
	case FUDaePassState::DITHER_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_DITHER); else glDisable(GL_DITHER); break;
	case FUDaePassState::FOG_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_FOG); else glDisable(GL_FOG); break;
	case FUDaePassState::LIGHT_MODEL_LOCAL_VIEWER_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_LIGHT_MODEL_LOCAL_VIEWER); else glDisable(GL_LIGHT_MODEL_LOCAL_VIEWER); break;
	case FUDaePassState::LIGHT_MODEL_TWO_SIDE_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_LIGHT_MODEL_TWO_SIDE); else glDisable(GL_LIGHT_MODEL_TWO_SIDE); break;
	case FUDaePassState::LINE_SMOOTH_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_LINE_SMOOTH); else glDisable(GL_LINE_SMOOTH); break;
	case FUDaePassState::LINE_STIPPLE_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_LINE_STIPPLE); else glDisable(GL_LINE_STIPPLE); break;
	case FUDaePassState::LOGIC_OP_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_LOGIC_OP); else glDisable(GL_LOGIC_OP); break;
	case FUDaePassState::MULTISAMPLE_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_MULTISAMPLE); else glDisable(GL_MULTISAMPLE); break;
	case FUDaePassState::NORMALIZE_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_NORMALIZE); else glDisable(GL_NORMALIZE); break;
	case FUDaePassState::POINT_SMOOTH_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_POINT_SMOOTH); else glDisable(GL_POINT_SMOOTH); break;
	case FUDaePassState::POLYGON_OFFSET_FILL_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_POLYGON_OFFSET_FILL); else glDisable(GL_POLYGON_OFFSET_FILL); break;
	case FUDaePassState::POLYGON_OFFSET_LINE_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_POLYGON_OFFSET_LINE); else glDisable(GL_POLYGON_OFFSET_LINE); break;
	case FUDaePassState::POLYGON_OFFSET_POINT_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_POLYGON_OFFSET_POINT); else glDisable(GL_POLYGON_OFFSET_POINT); break;
	case FUDaePassState::POLYGON_SMOOTH_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_POLYGON_SMOOTH); else glDisable(GL_POLYGON_SMOOTH); break;
	case FUDaePassState::POLYGON_STIPPLE_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_POLYGON_STIPPLE); else glDisable(GL_POLYGON_STIPPLE); break;
	case FUDaePassState::RESCALE_NORMAL_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_RESCALE_NORMAL); else glDisable(GL_RESCALE_NORMAL); break;
	case FUDaePassState::SAMPLE_ALPHA_TO_COVERAGE_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_COLOR_MATERIAL); else glDisable(GL_COLOR_MATERIAL); break;
	case FUDaePassState::SAMPLE_ALPHA_TO_ONE_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE); else glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE); break;
	case FUDaePassState::SAMPLE_COVERAGE_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_SAMPLE_COVERAGE); else glDisable(GL_SAMPLE_COVERAGE); break;
	case FUDaePassState::SCISSOR_TEST_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST); break;
	case FUDaePassState::STENCIL_TEST_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST); break;
	case FUDaePassState::COLOR_MATERIAL_ENABLE: if (GET_VALUE(0, bool)) glEnable(GL_COLOR_MATERIAL); else glDisable(GL_COLOR_MATERIAL); break;

	case FUDaePassState::INVALID:
	default: break;
	}

#undef GET_VALUE
}

void CFXRenderState::Reset()
{
	if (colladaState == NULL) return;
	FUDaePassState::State stateType = colladaState->GetType();
	if (stateType == FUDaePassState::INVALID) return;
	
	// create a copy of the state with the same type, but with a default value
	CFXRenderState state(NULL);
	state.colladaState = new FCDEffectPassState(NULL, stateType);

	// use it and reset the OpenGL state
	state.Use();
}

void CFXRenderState::Restore()
{
	if (colladaState == NULL) return;

	uint8* data = colladaState->GetData();
#define SET_VALUE(offset, valueType, actualValue) *((valueType*)(data + offset)) = (valueType) (actualValue);

	switch (colladaState->GetType())
	{
	case FUDaePassState::ALPHA_FUNC:
	case FUDaePassState::CULL_FACE:
	case FUDaePassState::FRONT_FACE:
	case FUDaePassState::LOGIC_OP:
	case FUDaePassState::POLYGON_MODE:
	case FUDaePassState::SHADE_MODEL:
	case FUDaePassState::CLEAR_COLOR:
	case FUDaePassState::SCISSOR:
	case FUDaePassState::COLOR_MASK: 
	case FUDaePassState::BLEND_FUNC:
	case FUDaePassState::BLEND_FUNC_SEPARATE:
	case FUDaePassState::BLEND_EQUATION: 
	case FUDaePassState::BLEND_EQUATION_SEPARATE:
	case FUDaePassState::BLEND_COLOR:
	case FUDaePassState::DEPTH_FUNC:
	case FUDaePassState::DEPTH_BOUNDS:
	case FUDaePassState::DEPTH_RANGE:
	case FUDaePassState::DEPTH_MASK:
	case FUDaePassState::CLEAR_DEPTH:
	case FUDaePassState::FOG_MODE:
	case FUDaePassState::FOG_COORD_SRC:
	case FUDaePassState::FOG_COLOR:
	case FUDaePassState::FOG_DENSITY:
	case FUDaePassState::FOG_END:
	case FUDaePassState::FOG_START:

	case FUDaePassState::STENCIL_FUNC:
	case FUDaePassState::STENCIL_OP:
	case FUDaePassState::STENCIL_FUNC_SEPARATE:
	case FUDaePassState::STENCIL_OP_SEPARATE:
	case FUDaePassState::STENCIL_MASK_SEPARATE:
	case FUDaePassState::STENCIL_MASK:
	case FUDaePassState::CLEAR_STENCIL:

	case FUDaePassState::LIGHT_MODEL_COLOR_CONTROL:
	case FUDaePassState::LIGHT_MODEL_AMBIENT:

	case FUDaePassState::COLOR_MATERIAL:
	case FUDaePassState::MATERIAL_AMBIENT:
	case FUDaePassState::MATERIAL_DIFFUSE:
	case FUDaePassState::MATERIAL_EMISSION:
	case FUDaePassState::MATERIAL_SPECULAR:
	case FUDaePassState::MATERIAL_SHININESS:

	case FUDaePassState::LINE_WIDTH:
	case FUDaePassState::LINE_STIPPLE:

	case FUDaePassState::POINT_SIZE:
	case FUDaePassState::POINT_SIZE_MIN:
	case FUDaePassState::POINT_SIZE_MAX:
	case FUDaePassState::POINT_FADE_THRESHOLD_SIZE:
	case FUDaePassState::POINT_DISTANCE_ATTENUATION:

	case FUDaePassState::POLYGON_OFFSET:
		colladaState->SetDefaultValue();
		Use();
		break;

	// indexed lights -> 1. set to default, 2. Re-write index, 3. Synchronize
	case FUDaePassState::LIGHT_ENABLE:
	case FUDaePassState::LIGHT_AMBIENT:
	case FUDaePassState::LIGHT_DIFFUSE:
	case FUDaePassState::LIGHT_SPECULAR:
	case FUDaePassState::LIGHT_POSITION:
	case FUDaePassState::LIGHT_CONSTANT_ATTENUATION:
	case FUDaePassState::LIGHT_LINEAR_ATTENUATION:
	case FUDaePassState::LIGHT_QUADRATIC_ATTENUATION:
	case FUDaePassState::LIGHT_SPOT_CUTOFF:
	case FUDaePassState::LIGHT_SPOT_EXPONENT:
	case FUDaePassState::LIGHT_SPOT_DIRECTION:
		colladaState->SetDefaultValue();
		SET_VALUE(0,uint8,static_cast<uint8>(GetIndex()));
		Use();
		break;

	// indexed texture units -> 1. set to default, 2. Re-write index, 3. Synchronize
	case FUDaePassState::TEXTURE1D: /* No support -- GET_VALUE(0, uint8); GET_VALUE(1, uint32); */ break;
	case FUDaePassState::TEXTURE2D: /* No support -- GET_VALUE(0, uint8); GET_VALUE(1, uint32); */ break;
	case FUDaePassState::TEXTURE3D: /* No support -- GET_VALUE(0, uint8); GET_VALUE(1, uint32); */ break;
	case FUDaePassState::TEXTURECUBE: /* No support -- GET_VALUE(0, uint8); GET_VALUE(1, uint32); */ break;
	case FUDaePassState::TEXTURERECT: /* No support -- GET_VALUE(0, uint8); GET_VALUE(1, uint32); */ break;
	case FUDaePassState::TEXTUREDEPTH: /* No support -- GET_VALUE(0, uint8); GET_VALUE(1, uint32); */ break;

	case FUDaePassState::TEXTURE1D_ENABLE:
	case FUDaePassState::TEXTURE2D_ENABLE:
	case FUDaePassState::TEXTURE3D_ENABLE:
	case FUDaePassState::TEXTURECUBE_ENABLE:
	case FUDaePassState::TEXTURERECT_ENABLE:
	case FUDaePassState::TEXTUREDEPTH_ENABLE:
	case FUDaePassState::TEXTURE_ENV_COLOR:
		colladaState->SetDefaultValue();
		SET_VALUE(0,uint8,static_cast<uint8>(GetIndex()));
		Use();
		break;

	case FUDaePassState::TEXTURE_ENV_MODE: /* Not handled... */ break;

	// indexed clip planes -> 1. set to default, 2. Re-write index, 3. Synchronize
	case FUDaePassState::CLIP_PLANE_ENABLE:
	case FUDaePassState::CLIP_PLANE:
		colladaState->SetDefaultValue();
		SET_VALUE(0,uint8,static_cast<uint8>(GetIndex()));
		Use();
		break;


	case FUDaePassState::MODEL_VIEW_MATRIX:
	case FUDaePassState::PROJECTION_MATRIX:
		colladaState->SetDefaultValue();
		Use();
		break;

	// OpenGL boolean states -> 1. Reset to default, 2. Use to call glEnable or glDisable appropriately
	case FUDaePassState::LIGHTING_ENABLE:
	case FUDaePassState::ALPHA_TEST_ENABLE:
	case FUDaePassState::AUTO_NORMAL_ENABLE:
	case FUDaePassState::BLEND_ENABLE:
	case FUDaePassState::COLOR_LOGIC_OP_ENABLE:
	case FUDaePassState::CULL_FACE_ENABLE:
	case FUDaePassState::DEPTH_BOUNDS_ENABLE:
	case FUDaePassState::DEPTH_CLAMP_ENABLE:
	case FUDaePassState::DEPTH_TEST_ENABLE:
	case FUDaePassState::DITHER_ENABLE:
	case FUDaePassState::FOG_ENABLE:
	case FUDaePassState::LIGHT_MODEL_LOCAL_VIEWER_ENABLE:
	case FUDaePassState::LIGHT_MODEL_TWO_SIDE_ENABLE:
	case FUDaePassState::LINE_SMOOTH_ENABLE:
	case FUDaePassState::LINE_STIPPLE_ENABLE:
	case FUDaePassState::LOGIC_OP_ENABLE:
	case FUDaePassState::MULTISAMPLE_ENABLE:
	case FUDaePassState::NORMALIZE_ENABLE:
	case FUDaePassState::POINT_SMOOTH_ENABLE:
	case FUDaePassState::POLYGON_OFFSET_FILL_ENABLE:
	case FUDaePassState::POLYGON_OFFSET_LINE_ENABLE:
	case FUDaePassState::POLYGON_OFFSET_POINT_ENABLE:
	case FUDaePassState::POLYGON_SMOOTH_ENABLE:
	case FUDaePassState::POLYGON_STIPPLE_ENABLE:
	case FUDaePassState::RESCALE_NORMAL_ENABLE:
	case FUDaePassState::SAMPLE_ALPHA_TO_COVERAGE_ENABLE:
	case FUDaePassState::SAMPLE_ALPHA_TO_ONE_ENABLE:
	case FUDaePassState::SAMPLE_COVERAGE_ENABLE:
	case FUDaePassState::SCISSOR_TEST_ENABLE:
	case FUDaePassState::STENCIL_TEST_ENABLE:
	case FUDaePassState::COLOR_MATERIAL_ENABLE:
		colladaState->SetDefaultValue();
		Use();
		break;

	case FUDaePassState::INVALID:
	default: break;
	}

	SetDirtyFlag();
	SetDeadFlag();

#undef SET_VALUE
}

void CFXRenderState::GenerateUICommand(const CFXShaderNode* shader, FUStringBuilder& builder) const
{
	// we need the attribute created at this point
	if (attribute.isNull() || !attribute.hasFn(MFn::kCompoundAttribute))
	{
		return;
	}

	const char* cgName = CFXRenderState::ColladaToCgTranslation(GetType());

	// the $attribRoot string variable. Append specific attributes names to connect controls.
	builder.append(FC("string $attribRoot = \""));				// declaration
	builder.append(MConvert::ToFChar(shader->name()));				// shader node name
	builder.append(FC(".\""));									// . attribute scope
	builder.append(FC("; "));

	// the dynamic $attrib string variable. Used by the BUILD_ATTRIB macro defined below.
	builder.append(FC("string $attrib = \"\"; "));

	// the dynamic $control string variable. Used to remember the last control created.
	builder.append(FC("string $control = \"\"; "));

	// increment this variable on each attribute creation.
	uint32 currentAttrib = 0;
	MFnCompoundAttribute cAttrib(attribute);

	/*	The following macros will be used to add some more functionnalities 
		later (eg. create plugs, make a reactive UI, etc.
	*/

#define BUILD_ATTRIB {\
	MFnAttribute attr(cAttrib.child(currentAttrib)); \
	MString aName = attr.name(); \
	builder.append(FC("$attrib = $attribRoot + \"")); \
	builder.append(MConvert::ToFChar(aName.asChar())); builder.append(FC("\"; ")); }

#define BEGIN_ROW(Params) \
	builder.append(FC("rowLayout ")); \
	builder.append(Params); \
	builder.append((fchar) ';')

#define LABEL(expr) \
	builder.append(FC("text -l \"")); \
	builder.append(expr); \
	builder.append(FC("\"; "));

#define OPTION_BEGIN(Label) \
	BUILD_ATTRIB; \
	builder.append(FC("attrEnumOptionMenuGrp -l ") FC(#Label) FC(" -at $attrib ")); \
	++currentAttrib;

#define OPTION_ITEM(Namespace,Enum) \
	builder.append(FC("-ei ")); \
	builder.append(FUStringConversion::ToFString(Namespace::Enum)); builder.append(FC(" \"")); \
	builder.append(FUStringConversion::ToFString(Namespace::ToString(Namespace::Enum))); \
	builder.append(FC("\" "));

#define OPTION_ITEM_LOOP(Namespace,Type,Begin,End) \
	for (uint32 i = Namespace::Begin; i <= Namespace::End; i++) { \
		builder.append(FC("-ei ")); \
		builder.append(FUStringConversion::ToFString((Namespace::Type)i)); builder.append(FC(" \"")); \
		builder.append(FUStringConversion::ToFString(Namespace::ToString((Namespace::Type)i))); \
		builder.append(FC("\" ")); };

#define OPTION_END \
	builder.append((fchar) ';');

#define TEXTINPUT(Label) \
	BUILD_ATTRIB; \
	builder.append(FC("$control = `textFieldGrp -l \"") FC(#Label) FC("\"`;")); \
	builder.append(FC("connectControl $control $attrib;")); \
	currentAttrib++;

#define FLOATSLIDER(Label,Min,Max) \
	BUILD_ATTRIB; \
	builder.append(FC("$control = `floatSliderGrp -l \"") FC(#Label) FC("\" -min ") FC(#Min) FC(" -max ") FC(#Max) FC(" -pre 3 -f true`;")); \
	builder.append(FC("connectControl $control $attrib;")); \
	currentAttrib++;

#define INTSLIDER(Label) \
	BUILD_ATTRIB; \
	builder.append(FC("$control = `intSliderGrp -l \"") FC(#Label) FC("\" -min -9999 -max 9999 -fmn -9999999 -fmx 9999999 -f true`;")); \
	builder.append(FC("connectControl $control $attrib;")); \
	currentAttrib++;

#define INTSLIDER2(Label,Min,Max) \
	BUILD_ATTRIB; \
	builder.append(FC("$control = `intSliderGrp -l \"") FC(#Label) FC("\" -min ") FC(#Min) FC(" -max ") FC(#Max) FC(" -f true`;")); \
	builder.append(FC("connectControl $control $attrib;")); \
	currentAttrib++;

#define SIZESLIDER(Label) \
	BUILD_ATTRIB; \
	builder.append(FC("$control = `intSliderGrp -l \"") FC(#Label) FC("\" -min 0 -max 9999 -fmx 99999999 -f true`;")); \
	builder.append(FC("connectControl $control $attrib;")); \
	currentAttrib++;

#define CHECKBOX(Label) \
	BUILD_ATTRIB; \
	builder.append(FC("$control = `checkBoxGrp -l \"") FC(#Label) FC("\"`; ")); \
	builder.append(FC("connectControl -index 2 $control $attrib;")); \
	currentAttrib++;

#define COLOR_RGBA(Label) \
	BUILD_ATTRIB; \
	builder.append(FC("attrColorSliderGrp -l \"") FC(#Label) FC("\" -at $attrib;"));  \
	currentAttrib++; \
	FLOATSLIDER(alpha,0,1); \

#define END_ROW \
	builder.append(FC("setParent ..;"));

	fstring labelName(TO_FSTRING(cgName));
	if (index >= 0) labelName += FS("-") + TO_FSTRING(index);

	// name the state
	BEGIN_ROW(FC("-nc 1 -ct1 \"left\" -adj 1 -co1 10"));
		LABEL(labelName);
	END_ROW;

	// given the ColladaFX state type, add some more UI.
	switch (GetType())
	{
	case FUDaePassState::ALPHA_FUNC:
		OPTION_BEGIN(func);
			OPTION_ITEM_LOOP(FUDaePassStateFunction,Function,NEVER,ALWAYS);
		OPTION_END;
		FLOATSLIDER(ref,0,1);
		break;
	case FUDaePassState::CULL_FACE:
		OPTION_BEGIN(mode);
			OPTION_ITEM_LOOP(FUDaePassStateFaceType,Type,FRONT,FRONT_AND_BACK);
		OPTION_END;
		break;
	case FUDaePassState::FRONT_FACE: 
		OPTION_BEGIN(mode);
			OPTION_ITEM_LOOP(FUDaePassStateFrontFaceType,Type,CLOCKWISE,COUNTER_CLOCKWISE);
		OPTION_END;
		break;
	case FUDaePassState::LOGIC_OP:
		OPTION_BEGIN(opcode);
			OPTION_ITEM_LOOP(FUDaePassStateLogicOperation,Operation,CLEAR,SET);
		OPTION_END;
		break;
	case FUDaePassState::POLYGON_MODE: 
		OPTION_BEGIN(face);
			OPTION_ITEM(FUDaePassStateFaceType,FRONT);
			OPTION_ITEM(FUDaePassStateFaceType,BACK);
			OPTION_ITEM(FUDaePassStateFaceType,FRONT_AND_BACK);
		OPTION_END;
		OPTION_BEGIN(mode);
			OPTION_ITEM(FUDaePassStatePolygonMode,POINT);
			OPTION_ITEM(FUDaePassStatePolygonMode,LINE);
			OPTION_ITEM(FUDaePassStatePolygonMode,FILL);
		OPTION_END;
		break;
	case FUDaePassState::SHADE_MODEL:
		OPTION_BEGIN(face);
			OPTION_ITEM(FUDaePassStateShadeModel,FLAT);
			OPTION_ITEM(FUDaePassStateShadeModel,SMOOTH);
		OPTION_END;
		break;
	case FUDaePassState::CLEAR_COLOR:
	case FUDaePassState::BLEND_COLOR:
		COLOR_RGBA(color);
		break;
	case FUDaePassState::SCISSOR:
		INTSLIDER(x);
		INTSLIDER(y);
		SIZESLIDER(width);
		SIZESLIDER(height);
		break;
	case FUDaePassState::COLOR_MASK:
		CHECKBOX(red);
		CHECKBOX(green);
		CHECKBOX(blue);
		CHECKBOX(alpha);
		break;
	case FUDaePassState::BLEND_FUNC:
		OPTION_BEGIN(sRGBA);
			OPTION_ITEM(FUDaePassStateBlendType,ZERO);
			OPTION_ITEM(FUDaePassStateBlendType,ONE);
			OPTION_ITEM(FUDaePassStateBlendType,DESTINATION_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_DESTINATION_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,SOURCE_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_SOURCE_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,DESTINATION_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_DESTINATION_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,SOURCE_ALPHA_SATURATE);
			// GL_ARB_imaging
			OPTION_ITEM(FUDaePassStateBlendType,CONSTANT_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_CONSTANT_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,CONSTANT_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_CONSTANT_ALPHA);
		OPTION_END;
		OPTION_BEGIN(dRGBA);
			OPTION_ITEM(FUDaePassStateBlendType,ZERO);
			OPTION_ITEM(FUDaePassStateBlendType,ONE);
			OPTION_ITEM(FUDaePassStateBlendType,SOURCE_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_SOURCE_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,SOURCE_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_SOURCE_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,DESTINATION_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_DESTINATION_ALPHA);
			// GL_ARB_imaging
			OPTION_ITEM(FUDaePassStateBlendType,CONSTANT_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_CONSTANT_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,CONSTANT_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_CONSTANT_ALPHA);
		OPTION_END;
		break;
	case FUDaePassState::BLEND_FUNC_SEPARATE:
		OPTION_BEGIN(sRGB);
			OPTION_ITEM(FUDaePassStateBlendType,ZERO);
			OPTION_ITEM(FUDaePassStateBlendType,ONE);
			OPTION_ITEM(FUDaePassStateBlendType,DESTINATION_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_DESTINATION_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,SOURCE_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_SOURCE_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,DESTINATION_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_DESTINATION_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,SOURCE_ALPHA_SATURATE);
			// GL_ARB_imaging
			OPTION_ITEM(FUDaePassStateBlendType,CONSTANT_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_CONSTANT_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,CONSTANT_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_CONSTANT_ALPHA);
		OPTION_END;
		OPTION_BEGIN(dRGB);
			OPTION_ITEM(FUDaePassStateBlendType,ZERO);
			OPTION_ITEM(FUDaePassStateBlendType,ONE);
			OPTION_ITEM(FUDaePassStateBlendType,SOURCE_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_SOURCE_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,SOURCE_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_SOURCE_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,DESTINATION_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_DESTINATION_ALPHA);
			// GL_ARB_imaging
			OPTION_ITEM(FUDaePassStateBlendType,CONSTANT_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_CONSTANT_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,CONSTANT_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_CONSTANT_ALPHA);
		OPTION_END;
		OPTION_BEGIN(sALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ZERO);
			OPTION_ITEM(FUDaePassStateBlendType,ONE);
			OPTION_ITEM(FUDaePassStateBlendType,DESTINATION_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_DESTINATION_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,SOURCE_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_SOURCE_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,DESTINATION_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_DESTINATION_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,SOURCE_ALPHA_SATURATE);
			// GL_ARB_imaging
			OPTION_ITEM(FUDaePassStateBlendType,CONSTANT_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_CONSTANT_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,CONSTANT_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_CONSTANT_ALPHA);
		OPTION_END;
		OPTION_BEGIN(dALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ZERO);
			OPTION_ITEM(FUDaePassStateBlendType,ONE);
			OPTION_ITEM(FUDaePassStateBlendType,SOURCE_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_SOURCE_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,SOURCE_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_SOURCE_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,DESTINATION_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_DESTINATION_ALPHA);
			// GL_ARB_imaging
			OPTION_ITEM(FUDaePassStateBlendType,CONSTANT_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_CONSTANT_COLOR);
			OPTION_ITEM(FUDaePassStateBlendType,CONSTANT_ALPHA);
			OPTION_ITEM(FUDaePassStateBlendType,ONE_MINUS_CONSTANT_ALPHA);
		OPTION_END;
		break;
	case FUDaePassState::BLEND_EQUATION:
		OPTION_BEGIN(mode);
			OPTION_ITEM(FUDaePassStateBlendEquation,ADD);
			OPTION_ITEM(FUDaePassStateBlendEquation,SUBTRACT);
			OPTION_ITEM(FUDaePassStateBlendEquation,REVERSE_SUBTRACT);
			OPTION_ITEM(FUDaePassStateBlendEquation,MIN);
			OPTION_ITEM(FUDaePassStateBlendEquation,MAX);
		OPTION_END;
		break;
	case FUDaePassState::BLEND_EQUATION_SEPARATE:
		OPTION_BEGIN(modeRGB);
			OPTION_ITEM(FUDaePassStateBlendEquation,ADD);
			OPTION_ITEM(FUDaePassStateBlendEquation,SUBTRACT);
			OPTION_ITEM(FUDaePassStateBlendEquation,REVERSE_SUBTRACT);
			OPTION_ITEM(FUDaePassStateBlendEquation,MIN);
			OPTION_ITEM(FUDaePassStateBlendEquation,MAX);
		OPTION_END;
		OPTION_BEGIN(modeALPHA);
			OPTION_ITEM(FUDaePassStateBlendEquation,ADD);
			OPTION_ITEM(FUDaePassStateBlendEquation,SUBTRACT);
			OPTION_ITEM(FUDaePassStateBlendEquation,REVERSE_SUBTRACT);
			OPTION_ITEM(FUDaePassStateBlendEquation,MIN);
			OPTION_ITEM(FUDaePassStateBlendEquation,MAX);
		OPTION_END;
		break;
	case FUDaePassState::DEPTH_FUNC:
		OPTION_BEGIN(func);
			OPTION_ITEM_LOOP(FUDaePassStateFunction,Function,NEVER,ALWAYS);
		OPTION_END;
		break;
	case FUDaePassState::DEPTH_BOUNDS:
		FLOATSLIDER(zmin,0,1);
		FLOATSLIDER(zmax,0,1);
		break;
	case FUDaePassState::DEPTH_RANGE:
		FLOATSLIDER(near,0,1);
		FLOATSLIDER(far,0,1);
		break;
	case FUDaePassState::DEPTH_MASK:
		CHECKBOX(enabled);
		break;
	case FUDaePassState::CLEAR_DEPTH:
		FLOATSLIDER(clear,0,1);
		break;

	case FUDaePassState::FOG_MODE:
		OPTION_BEGIN(mode);
			OPTION_ITEM(FUDaePassStateFogType,LINEAR);
			OPTION_ITEM(FUDaePassStateFogType,EXP);
			OPTION_ITEM(FUDaePassStateFogType,EXP2);
		OPTION_END;
		break;
	case FUDaePassState::FOG_COORD_SRC:
		OPTION_BEGIN(mode);
			OPTION_ITEM(FUDaePassStateFogCoordinateType,FOG_COORDINATE);
			OPTION_ITEM(FUDaePassStateFogCoordinateType,FRAGMENT_DEPTH);
		OPTION_END;
		break;
	case FUDaePassState::FOG_COLOR:
		COLOR_RGBA(color);
		break;
	case FUDaePassState::FOG_DENSITY:
	case FUDaePassState::FOG_END:
	case FUDaePassState::FOG_START:
		FLOATSLIDER(value,0,1);
		break;

	case FUDaePassState::STENCIL_FUNC: 
		OPTION_BEGIN(func);
			OPTION_ITEM_LOOP(FUDaePassStateFunction,Function,NEVER,ALWAYS);
		OPTION_END;
		INTSLIDER2(ref,0,255);
		INTSLIDER2(mask,0,0xFF);
		break;
	case FUDaePassState::STENCIL_OP:
		for (int i = 0; i < 3; i++)
		{
			switch (i){
			case 0: OPTION_BEGIN(fail); break;
			case 1: OPTION_BEGIN(zfail); break;
			case 2: OPTION_BEGIN(zpass); break;
			}
				OPTION_ITEM(FUDaePassStateStencilOperation,KEEP);
				OPTION_ITEM(FUDaePassStateStencilOperation,ZERO);
				OPTION_ITEM(FUDaePassStateStencilOperation,REPLACE);
				OPTION_ITEM(FUDaePassStateStencilOperation,INCREMENT);
				OPTION_ITEM(FUDaePassStateStencilOperation,DECREMENT);
				OPTION_ITEM(FUDaePassStateStencilOperation,INVERT);
				OPTION_ITEM(FUDaePassStateStencilOperation,INCREMENT_WRAP);
				OPTION_ITEM(FUDaePassStateStencilOperation,DECREMENT_WRAP);
			OPTION_END;
		}
		break;
	case FUDaePassState::STENCIL_FUNC_SEPARATE:
        OPTION_BEGIN(face);
			OPTION_ITEM_LOOP(FUDaePassStateFaceType,Type,FRONT,FRONT_AND_BACK);
		OPTION_END;
        OPTION_BEGIN(func);
			OPTION_ITEM_LOOP(FUDaePassStateFunction,Function,NEVER,ALWAYS);
		OPTION_END;
		INTSLIDER2(ref,0,255);
		INTSLIDER2(mask,0,0xFF);
        break;
	case FUDaePassState::STENCIL_OP_SEPARATE:
        OPTION_BEGIN(face);
			OPTION_ITEM_LOOP(FUDaePassStateFaceType,Type,FRONT,FRONT_AND_BACK);
		OPTION_END;
        for (int i = 0; i < 3; i++)
		{
			switch (i){
			case 0: OPTION_BEGIN(fail); break;
			case 1: OPTION_BEGIN(zfail); break;
			case 2: OPTION_BEGIN(zpass); break;
			}
				OPTION_ITEM(FUDaePassStateStencilOperation,KEEP);
				OPTION_ITEM(FUDaePassStateStencilOperation,ZERO);
				OPTION_ITEM(FUDaePassStateStencilOperation,REPLACE);
				OPTION_ITEM(FUDaePassStateStencilOperation,INCREMENT);
				OPTION_ITEM(FUDaePassStateStencilOperation,DECREMENT);
				OPTION_ITEM(FUDaePassStateStencilOperation,INVERT);
				OPTION_ITEM(FUDaePassStateStencilOperation,INCREMENT_WRAP);
				OPTION_ITEM(FUDaePassStateStencilOperation,DECREMENT_WRAP);
			OPTION_END;
		}
        break;
	case FUDaePassState::STENCIL_MASK_SEPARATE:
		OPTION_BEGIN(face);
			OPTION_ITEM_LOOP(FUDaePassStateFaceType,Type,FRONT,FRONT_AND_BACK);
		OPTION_END;
		INTSLIDER2(mask,0,0xFFFFFFFF);
		break;
	case FUDaePassState::STENCIL_MASK:
		INTSLIDER2(mask,0,0xFFFFFFFF);
		break;
	case FUDaePassState::CLEAR_STENCIL:
		INTSLIDER(s);
		break;

	case FUDaePassState::LIGHT_ENABLE:
		CHECKBOX(enabled);
		break;
	case FUDaePassState::LIGHT_AMBIENT:
	case FUDaePassState::LIGHT_DIFFUSE:
	case FUDaePassState::LIGHT_SPECULAR:
		COLOR_RGBA(color);
		break;
	case FUDaePassState::LIGHT_POSITION:
		FLOATSLIDER(x,-99999999,99999999);
		FLOATSLIDER(y,-99999999,99999999);
		FLOATSLIDER(z,-99999999,99999999);
		FLOATSLIDER(w,-99999999,99999999);
		break;
	case FUDaePassState::LIGHT_CONSTANT_ATTENUATION:
	case FUDaePassState::LIGHT_LINEAR_ATTENUATION:
	case FUDaePassState::LIGHT_QUADRATIC_ATTENUATION:
		FLOATSLIDER(factor,0,9999);
		break;
	case FUDaePassState::LIGHT_SPOT_CUTOFF:
		FLOATSLIDER(cutoff,0,180);
		break;
	case FUDaePassState::LIGHT_SPOT_EXPONENT:
		FLOATSLIDER(exp,0,128);
		break;
	case FUDaePassState::LIGHT_SPOT_DIRECTION:
		FLOATSLIDER(x,-99999999,99999999);
		FLOATSLIDER(y,-99999999,99999999);
		FLOATSLIDER(z,-99999999,99999999);
		break;
	case FUDaePassState::LIGHT_MODEL_COLOR_CONTROL:
		OPTION_BEGIN(control);
			OPTION_ITEM(FUDaePassStateLightModelColorControlType,SINGLE_COLOR);
			OPTION_ITEM(FUDaePassStateLightModelColorControlType,SEPARATE_SPECULAR_COLOR);
		OPTION_END;
		break;
	case FUDaePassState::LIGHT_MODEL_AMBIENT:
		COLOR_RGBA(color);
		break;

	case FUDaePassState::TEXTURE1D: /* No support -- GET_VALUE(0, uint8); GET_VALUE(1, uint32); */ break;
	case FUDaePassState::TEXTURE2D: /* No support -- GET_VALUE(0, uint8); GET_VALUE(1, uint32); */ break;
	case FUDaePassState::TEXTURE3D: /* No support -- GET_VALUE(0, uint8); GET_VALUE(1, uint32); */ break;
	case FUDaePassState::TEXTURECUBE: /* No support -- GET_VALUE(0, uint8); GET_VALUE(1, uint32); */ break;
	case FUDaePassState::TEXTURERECT: /* No support -- GET_VALUE(0, uint8); GET_VALUE(1, uint32); */ break;
	case FUDaePassState::TEXTUREDEPTH: /* No support -- GET_VALUE(0, uint8); GET_VALUE(1, uint32); */ break;

	case FUDaePassState::TEXTURE1D_ENABLE:
	case FUDaePassState::TEXTURE2D_ENABLE:
	case FUDaePassState::TEXTURE3D_ENABLE:
	case FUDaePassState::TEXTURECUBE_ENABLE:
	case FUDaePassState::TEXTURERECT_ENABLE:
	case FUDaePassState::TEXTUREDEPTH_ENABLE:
		CHECKBOX(enabled);
		break;

	case FUDaePassState::TEXTURE_ENV_COLOR:
		COLOR_RGBA(color);
		break;
		
	case FUDaePassState::TEXTURE_ENV_MODE: /* Not handled... */ break;
		
	case FUDaePassState::CLIP_PLANE_ENABLE:
		CHECKBOX(enabled);
		break;
	case FUDaePassState::CLIP_PLANE:
		FLOATSLIDER(a,-99999999,99999999);
		FLOATSLIDER(b,-99999999,99999999);
		FLOATSLIDER(c,-99999999,99999999);
		FLOATSLIDER(d,-99999999,99999999);
		break;

	case FUDaePassState::COLOR_MATERIAL:
		OPTION_BEGIN(face);
			OPTION_ITEM_LOOP(FUDaePassStateFaceType,Type,FRONT,FRONT_AND_BACK);
		OPTION_END;
		OPTION_BEGIN(type);
			OPTION_ITEM(FUDaePassStateMaterialType,EMISSION);
			OPTION_ITEM(FUDaePassStateMaterialType,AMBIENT);
			OPTION_ITEM(FUDaePassStateMaterialType,DIFFUSE);
			OPTION_ITEM(FUDaePassStateMaterialType,SPECULAR);
			OPTION_ITEM(FUDaePassStateMaterialType,AMBIENT_AND_DIFFUSE);
		OPTION_END;
		break;
	case FUDaePassState::MATERIAL_AMBIENT:
	case FUDaePassState::MATERIAL_DIFFUSE:
	case FUDaePassState::MATERIAL_EMISSION:
	case FUDaePassState::MATERIAL_SPECULAR:
		COLOR_RGBA(color);
		break;
	case FUDaePassState::MATERIAL_SHININESS:
		FLOATSLIDER(shininess,0,128);
		break;

	case FUDaePassState::LINE_WIDTH:
		FLOATSLIDER(width,0,999);
		break;
	case FUDaePassState::LINE_STIPPLE:
		INTSLIDER2(factor,1,256);
		INTSLIDER2(pattern,0,0xFFFFFFFF);
		break;

	case FUDaePassState::POINT_SIZE:
		FLOATSLIDER(size,0,999);
		break;
	case FUDaePassState::POINT_SIZE_MIN:
	case FUDaePassState::POINT_SIZE_MAX:
		FLOATSLIDER(size,0,999);
		break;
	case FUDaePassState::POINT_FADE_THRESHOLD_SIZE:
        FLOATSLIDER(size,0,999);
        break;
	case FUDaePassState::POINT_DISTANCE_ATTENUATION:
        FLOATSLIDER(a,0,999);
        FLOATSLIDER(b,0,999);
        FLOATSLIDER(c,0,999);
        break;

	case FUDaePassState::POLYGON_OFFSET:
		FLOATSLIDER(factor,-999,999);
		FLOATSLIDER(units,-999,999);
		break;

		/* Todo. - Figure out how to handle matrices with UI
	case FUDaePassState::MODEL_VIEW_MATRIX: glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glMultMatrixf(GET_VALUE(0, FMMatrix44)); break;
	case FUDaePassState::PROJECTION_MATRIX: glMatrixMode(GL_PROJECTION); glLoadIdentity(); glMultMatrixf(GET_VALUE(0, FMMatrix44)); break;
		*/
	case FUDaePassState::LIGHTING_ENABLE:
	case FUDaePassState::ALPHA_TEST_ENABLE:
	case FUDaePassState::AUTO_NORMAL_ENABLE:
	case FUDaePassState::BLEND_ENABLE:
	case FUDaePassState::COLOR_LOGIC_OP_ENABLE:
	case FUDaePassState::CULL_FACE_ENABLE:
	case FUDaePassState::DEPTH_BOUNDS_ENABLE:
	case FUDaePassState::DEPTH_CLAMP_ENABLE:
	case FUDaePassState::DEPTH_TEST_ENABLE:
	case FUDaePassState::DITHER_ENABLE:
	case FUDaePassState::FOG_ENABLE:
	case FUDaePassState::LIGHT_MODEL_LOCAL_VIEWER_ENABLE:
	case FUDaePassState::LIGHT_MODEL_TWO_SIDE_ENABLE:
	case FUDaePassState::LINE_SMOOTH_ENABLE:
	case FUDaePassState::LINE_STIPPLE_ENABLE:
	case FUDaePassState::LOGIC_OP_ENABLE:
	case FUDaePassState::MULTISAMPLE_ENABLE:
	case FUDaePassState::NORMALIZE_ENABLE:
	case FUDaePassState::POINT_SMOOTH_ENABLE:
	case FUDaePassState::POLYGON_OFFSET_FILL_ENABLE:
	case FUDaePassState::POLYGON_OFFSET_LINE_ENABLE:
	case FUDaePassState::POLYGON_OFFSET_POINT_ENABLE:
	case FUDaePassState::POLYGON_SMOOTH_ENABLE:
	case FUDaePassState::POLYGON_STIPPLE_ENABLE:
	case FUDaePassState::RESCALE_NORMAL_ENABLE:
	case FUDaePassState::SAMPLE_ALPHA_TO_COVERAGE_ENABLE:
	case FUDaePassState::SAMPLE_ALPHA_TO_ONE_ENABLE:
	case FUDaePassState::SAMPLE_COVERAGE_ENABLE:
	case FUDaePassState::SCISSOR_TEST_ENABLE:
	case FUDaePassState::STENCIL_TEST_ENABLE:
	case FUDaePassState::COLOR_MATERIAL_ENABLE:
		CHECKBOX(enabled);
		break;
	default: 
		break;
	}

#undef END_ROW
#undef COLOR_RGBA
#undef CHECKBOX
#undef SIZESLIDER
#undef INTSLIDER2
#undef INTSLIDER
#undef FLOATSLIDER
#undef TEXTINPUT
#undef OPTION_END
#undef OPTION_ITEM_LOOP
#undef OPTION_ITEM
#undef OPTION_BEGIN
#undef LABEL
#undef BEGIN_ROW
#undef BUILD_ATTRIB

}

uint32 CFXRenderState::getTotalRenderStateCount()
{
	return sizeof(CG_RENDER_STATE_NAMES) / sizeof(*CG_RENDER_STATE_NAMES);
}

FUDaePassState::State CFXRenderState::CgToColladaTranslation(const char* cgName)
{
	// sanity checks
	if (cgName == NULL || cgName[0] == '\0') return FUDaePassState::INVALID;

	const int arrayLength = getTotalRenderStateCount();
	
	// skip to interresting part of the array
	int i = 0;
	while(i < arrayLength && tolower(CG_RENDER_STATE_NAMES[i][0]) != tolower(cgName[0])) i++;

	// try to match the string (not case sensitive)
	for (; i < arrayLength && tolower(CG_RENDER_STATE_NAMES[i][0]) == tolower(cgName[0]); i++)
	{
		if (IsEquivalentI(CG_RENDER_STATE_NAMES[i], cgName))
			return CG_RENDER_STATES_XREF[i];
	}

	// past the interresting part, we didn't find it.
	return FUDaePassState::INVALID;
}

const char* CFXRenderState::ColladaToCgTranslation(FUDaePassState::State stateType)
{
	uint32 len = getTotalRenderStateCount();
	for (uint32 i = 0; i < len; i++)
	{
		if (CG_RENDER_STATES_XREF[i] == stateType)
			return CG_RENDER_STATE_NAMES[i];
	}
	return NULL;
}

FUDaePassState::State CFXRenderState::TranslateState(_CGstate* state)
{
	const char* stateName = cgGetStateName(state);
	CGtype stateType = cgGetStateType(state); // purely for debugging purposes.

	return CgToColladaTranslation(stateName);
}

const char* CFXRenderState::GetColladaStateName() const
{
	if (colladaState == NULL) return NULL;
	return FUDaePassState::ToString(colladaState->GetType());
}

const char* CFXRenderState::GetCgStateName() const
{
	if (colladaState == NULL || colladaState->GetType() == FUDaePassState::INVALID) return NULL;
	return ColladaToCgTranslation(colladaState->GetType());
}

bool CFXRenderState::IsStateIndexed(FUDaePassState::State stateType)
{
	switch (stateType)
	{
	// lights
	case FUDaePassState::LIGHT_ENABLE:
	case FUDaePassState::LIGHT_AMBIENT:
	case FUDaePassState::LIGHT_DIFFUSE:
	case FUDaePassState::LIGHT_SPECULAR:
	case FUDaePassState::LIGHT_POSITION:
	case FUDaePassState::LIGHT_CONSTANT_ATTENUATION:
	case FUDaePassState::LIGHT_LINEAR_ATTENUATION:
	case FUDaePassState::LIGHT_QUADRATIC_ATTENUATION:
	case FUDaePassState::LIGHT_SPOT_CUTOFF:
	case FUDaePassState::LIGHT_SPOT_EXPONENT:
	case FUDaePassState::LIGHT_SPOT_DIRECTION:
	// texture units
	case FUDaePassState::TEXTURE1D_ENABLE:
	case FUDaePassState::TEXTURE2D_ENABLE:
	case FUDaePassState::TEXTURE3D_ENABLE:
	case FUDaePassState::TEXTURECUBE_ENABLE:
	case FUDaePassState::TEXTURERECT_ENABLE:
	case FUDaePassState::TEXTUREDEPTH_ENABLE:
	case FUDaePassState::TEXTURE_ENV_COLOR:
	// clip panes
	case FUDaePassState::CLIP_PLANE_ENABLE:
	case FUDaePassState::CLIP_PLANE:
		return true;

	// anything else
	default:
		return false;
	}
}

bool CFXRenderState::IsIndexed() const
{
	return IsStateIndexed(GetType());
}

MString CFXRenderState::GetIndexedName()
{
	MString name(GetColladaStateName());
	if (IsIndexed())
	{
		name += MString("-") + MString(FUStringConversion::ToString(GetIndex()).c_str());
	}
	return name;
}

uint CFXRenderState::GetIndexFromIndexedName(fm::string& indexedName)
{
	size_t sepIdx = indexedName.find_last_of("-");
	int index = -1;
	if (sepIdx != fm::string::npos)
	{
		index = FUStringConversion::ToInt32(indexedName.substr(sepIdx + 1)); 
		indexedName = indexedName.substr(0,sepIdx);	
	}
	return index;
}
