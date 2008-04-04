/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "ColladaEffect.h"
#include "ColladaEffectParameter.h"
#include "ColladaEffectPass.h"
#include "colladaEffectShader.h"
#include "FUtils/FUFileManager.h"

// using polymorphism to export the parameters
#include "../DocumentExporter.h"
#include "../AnimationExporter.h"
#include "../MaterialExporter.h"
#include "../DlgTemplateBuilder.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDEffectParameter.h"
#include "FCDocument/FCDEffectParameterSampler.h"
#include "FCDocument/FCDEffectParameterSurface.h"
#include "FCDocument/FCDEffectProfileFX.h"
#include "FCDocument/FCDImage.h"
#include "FCDocument/FCDMaterial.h"

/** An helper class to redraw the views when a shader parameter is changed.*/
class CFXAnimatedPBAccessor : public PBAccessor
{
	void Set(PB2Value&, ReferenceMaker*, ParamID, int, TimeValue t)
	{
		GetCOREInterface()->RedrawViews(t);
	}
};
static CFXAnimatedPBAccessor gAnimatedPBAccessor;

ColladaEffectParameter::Type ColladaEffectParameter::convertCGtype(CGtype type)
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

ColladaEffectParameter::Semantic ColladaEffectParameter::convertCGsemantic(const char* name)
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

// static singleton initialization
ColladaEffectTimeObserver ColladaEffectTimeObserver::sInstance;

ColladaEffectParameter::ColladaEffectParameter(ColladaEffectShader* parentP, CGparameter paramP, Type typeP, Semantic semanticP)
:	mParent(parentP)
,	mType(typeP)
,	mSemantic(semanticP)
,	mEntryName(FS(""))
,	mUIName(FS(""))
,	mParamID(0)
,	mParamBlock(NULL)
,	mIsDirty(true)
,	mBindTarget(kNONE)
,	mParameter(NULL)
{
	FUAssert(mParent != NULL, return);
	initialize(paramP);
}

ColladaEffectParameter::~ColladaEffectParameter()
{
	mExpandedParameters.clear();
}

bool ColladaEffectParameter::initialize(CGparameter paramP)
{
	FUAssert(paramP != NULL, return false);
	mParameter = paramP;

	// retrieve names
	mName = FS(cgGetParameterName(mParameter));
	const char* dotToken = strrchr(mName.c_str(), '.');
	mEntryName = (dotToken == NULL) ? mName : FS(dotToken + 1);
	DebugCG();

	// read annotations
	CGannotation ann = cgGetNamedParameterAnnotation(mParameter, "UIName");
	if (ann) mUIName = cgGetStringAnnotationValue(ann);
	else mUIName = mName;

	return true;
}

static const uint16 lineHeight = 11;
static const uint16 lineWidth = 216;
static const uint16 leftSectionLeft = 2;
static const uint16 leftSectionRight = 118;
static const uint16 rightSectionLeft = 124;
static const uint16 rightSectionRight = 213;
static const uint16 rightSectionWidth = rightSectionRight - rightSectionLeft;

DLGITEMTEMPLATE& ColladaEffectParameter::buildTemplateLabel(DlgTemplateBuilder& tb, WORD& dialogCount, WORD lineCount)
{
	// Create a label for the parameter control
	tb.Align();
	DLGITEMTEMPLATE& label = tb.Append<DLGITEMTEMPLATE>();
	dialogCount++;
	label.x = leftSectionLeft;
	label.y = lineCount * lineHeight + 4;
	label.id = 1024 + mParamID;
	label.cx = leftSectionRight - leftSectionLeft;
	label.cy = lineHeight - 2;
	label.style = WS_CHILD | WS_VISIBLE;
	label.dwExtendedStyle = WS_EX_RIGHT;

	tb.Append((uint32) 0x0082FFFF); // STATIC window class
	tb.AppendWString((TCHAR*)mUIName.c_str()); // Window text
	tb.Append((uint16) 0); // extra bytes count

	return label;
}

const char* ColladaEffectParameter::getSemanticString()
{ 
	switch (mSemantic)
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
		const char* s = cgGetParameterSemantic(mParameter);
		return (s != NULL) ? s : ""; }
	}
}

fstring ColladaEffectParameter::getReferenceString()
{
	if (mParent == NULL || // shader
		mParent->getParent() == NULL || // pass
		mParent->getParent()->getParent() == NULL || // technique
		mParent->getParent()->getParent()->getParent() == NULL || // effect
		mParentWithId == NULL
		) return "INCOMPLETE";

	if (!mParent->isValid())
		return "INVALID";

	// material-tech-pass-shader-name
	fstring ref = mParentWithId->GetDaeId() + "-";
	ref += mParent->getParent()->getParent()->getName(); // tech
	ref += "-";
	ref += mParent->getParent()->getName(); // pass
	ref += "-";
	ref += (mParent->getInfo().isEffect) ? "" : ((mParent->getInfo().isVertex) ? "vertex-" : "fragment-"); // shader
	ref += mName; // name
	return ref;
}

ColladaEffectParameter* ColladaEffectParameterFactory::create(ColladaEffectShader* parentP, CGparameter param)
{
	if (param == NULL) return NULL;

	if (cgGetParameterVariability(param) != CG_UNIFORM)
		return NULL;

	ColladaEffectParameter::Type type = ColladaEffectParameter::convertCGtype(cgGetParameterType(param));
	ColladaEffectParameter::Semantic semantic = ColladaEffectParameter::convertCGsemantic(cgGetParameterSemantic(param));
	ColladaEffectParameter* parameter = NULL;

	switch (type)
	{
	case ColladaEffectParameter::kBool:
		parameter = new ColladaEffectParameterBool(parentP, param, type, semantic);
		break;
	case ColladaEffectParameter::kHalf:
	case ColladaEffectParameter::kFloat:
		parameter = new ColladaEffectParameterFloat(parentP, param, type, semantic);
		break;
	case ColladaEffectParameter::kHalf2:
	case ColladaEffectParameter::kFloat2:
		// TODO: Support Vector parameters such as float2 or float3 that are not
		// node positions or colors...
		break;
	case ColladaEffectParameter::kHalf3:
	case ColladaEffectParameter::kFloat3: // RGB color or 3D position
	case ColladaEffectParameter::kHalf4:
	case ColladaEffectParameter::kFloat4: // RGBA color or 4D position
		{
			// two choices here: color or position
			bool isColor = false; // default to position

			// check for UIWidget
			CGannotation ann = cgGetNamedParameterAnnotation(param, "UIWidget");
			if (ann != NULL)
			{
				if (IsEquivalentI(cgGetStringAnnotationValue(ann), "COLOR"))
				{
					isColor = true;
				}
			}

			if (!isColor)
			{
				// also check the semantic
				switch (semantic)
				{
				case ColladaEffectParameter::kCOLOR0:
				case ColladaEffectParameter::kCOLOR1:
					{
						isColor = true;
						
					} break;
				}
			}

			// this is supported by the DirectX shader
			if (!isColor && 
				(IsEquivalentI(cgGetParameterSemantic(param), "DIFFUSE") ||
				IsEquivalentI(cgGetParameterSemantic(param), "SPECULAR") ||
				IsEquivalentI(cgGetParameterSemantic(param), "AMBIENT") ||
				IsEquivalentI(cgGetParameterSemantic(param), "LIGHTCOLOR"))
				)
			{
				isColor = true;
			}

			if (isColor)
			{
				parameter = new ColladaEffectParameterColor(parentP, param,type,semantic);
			}
			else
			{
				// this must be a position to an object, find out which kind.
				parameter = new ColladaEffectParameterNode(parentP, param, type, semantic);
				ColladaEffectParameterNode* nparam = (ColladaEffectParameterNode*)parameter;
				CGannotation ann = cgGetNamedParameterAnnotation(param, "Object");
				if (ann != NULL)
				{
					const char *aName = cgGetStringAnnotationValue(ann);
					nparam->setObjectID(aName);
					if (aName && IsEquivalentI(aName,"POINTLIGHT"))
					{
						nparam->setSuperClassID(LIGHT_CLASS_ID); break;
					}
				}
			}
		} break;
	case ColladaEffectParameter::kHalf4x4:
	case ColladaEffectParameter::kFloat4x4:
		{
			parameter = new ColladaEffectParameterMatrix(parentP, param, type, semantic);
		} break;
	case ColladaEffectParameter::kSurface:
		{
			// the texture name will be stored in the TEXMAP from the Sampler, no need to keep this attribute.
		} break;
	case ColladaEffectParameter::kSampler2D:
	case ColladaEffectParameter::kSamplerCUBE:
		{
			parameter = new ColladaEffectParameterSampler(parentP, param, type, semantic);
		} break;
	default: break;
	}

	return parameter;
}

///////////////////////////////////////////////////////////////////////////////
// ColladaEffectParameter implementations
///////////////////////////////////////////////////////////////////////////////

//
// BOOL
//

ColladaEffectParameterBool::ColladaEffectParameterBool(ColladaEffectShader* parentP, CGparameter param, Type type, Semantic semantic)
:	ColladaEffectParameter(parentP, param,type,semantic)
,	mBool(FALSE)
{}

void ColladaEffectParameterBool::addToDescription(ParamBlockDesc2 *desc)
{
	// get the default value
	BOOL def = FALSE;
	float fval;
	if (cgGetParameterValuefr(mParameter, 1, &fval))
	{
		def = (fval == 0.0f) ? FALSE : TRUE;
	}

	mParamID = desc->Count();
	TCHAR *name = (TCHAR*)mName.c_str();

	// NOT ANIMATABLE
	desc->AddParam(mParamID, name, TYPE_BOOL, 0, 0,
		p_default, def,
		p_ui, TYPE_CHECKBUTTON, mParent->getParent()->GetNextDialogID(),
		end);
}

void ColladaEffectParameterBool::buildTemplateItem(DlgTemplateBuilder& tb, WORD& dialogCount, WORD& lineCount)
{
	ParamDef& definition = mParamBlock->GetParamDef(mParamID);
	FUAssert(definition.ctrl_count == 1, return);

	DLGITEMTEMPLATE& label = buildTemplateLabel(tb,dialogCount,lineCount);

	// CUSTBUTTONWINDOWCLASS
	tb.Align();
	DLGITEMTEMPLATE& checkBox = tb.Append<DLGITEMTEMPLATE>();
	dialogCount++;
	checkBox.x = rightSectionLeft + 1;
	checkBox.y = label.y + 1;
	checkBox.id = definition.ctrl_IDs[0];
	checkBox.cx = lineHeight - 3;
	checkBox.cy = lineHeight - 3;
	checkBox.style = WS_CHILD | WS_VISIBLE | BS_CHECKBOX;
	checkBox.dwExtendedStyle = WS_EX_RIGHT;

	tb.AppendWString(CUSTBUTTONWINDOWCLASS); // Window class
	tb.AppendWString(""); // Window text
	tb.Append((uint16) 0); // extra bytes count

	++ lineCount;
}

void ColladaEffectParameterBool::update()
{
	synchronize();
	cgSetParameter1i(mParameter, mBool);
}

void ColladaEffectParameterBool::synchronize()
{
	Interval dummy;
	mParamBlock->GetValue(mParamID, GetCOREInterface()->GetTime(), mBool, dummy);
}

void ColladaEffectParameterBool::doExport(DocumentExporter* UNUSED(document), FCDEffectProfileFX* profile, FCDMaterial* instance)
{
	fstring reference = getReferenceString();

	// create generator on profile
	FCDEffectParameterBool* parameter = (FCDEffectParameterBool*) profile->AddEffectParameter(FCDEffectParameter::BOOLEAN);
	parameter->SetGenerator();
	parameter->SetSemantic(reference);
	parameter->SetValue(mBool == TRUE);
	parameter->SetReference(getReferenceString());
	parameter->AddAnnotation(FC("UIName"), FCDEffectParameter::STRING, getUIName());

	// create modifier on material
	parameter = (FCDEffectParameterBool*) instance->AddEffectParameter(FCDEffectParameter::BOOLEAN);
	parameter->SetModifier();
	parameter->SetReference(reference);
	parameter->SetValue(mBool == TRUE);
}

//
// FLOAT
//

ColladaEffectParameterFloat::ColladaEffectParameterFloat(ColladaEffectShader* parentP, CGparameter param, Type type, Semantic semantic)
:	ColladaEffectParameter(parentP, param,type,semantic)
,	mFloat(0.0f)
,	mUIMin(-99999999.0f)
,	mUIMax(99999999.0f)
{
	ColladaEffectTimeObserver::GetInstance().AddObserver();
}

ColladaEffectParameterFloat::~ColladaEffectParameterFloat()
{
	ColladaEffectTimeObserver::GetInstance().RemoveObserver();
}

void ColladaEffectParameterFloat::addToDescription(ParamBlockDesc2 *desc)
{
	// get default value
	float fval, def = 0.0f;
	if (cgGetParameterValuefr(mParameter, 1, &fval))
	{
		def = fval;
	}

	// get min max
	CGannotation annmn = cgGetNamedParameterAnnotation(mParameter, "UIMin");
	CGannotation annmx = cgGetNamedParameterAnnotation(mParameter, "UIMax");

	if (annmn && annmx)
	{
		int nvals;
		const float* fvalmn = cgGetFloatAnnotationValues(annmn, &nvals);
		if (nvals > 0) mUIMin = fvalmn[0];
		const float* fvalmx = cgGetFloatAnnotationValues(annmx, &nvals);
		if (nvals > 0) mUIMax = fvalmx[0];
	}

	mParamID = desc->Count();
	TCHAR *name = (TCHAR*)mName.c_str();

	desc->AddParam(mParamID, name, TYPE_FLOAT, P_ANIMATABLE, 0,
		p_default, def,
		p_range, mUIMin, mUIMax,
		p_accessor, &gAnimatedPBAccessor,
		p_ui, TYPE_SPINNER, EDITTYPE_FLOAT, mParent->getParent()->GetNextDialogID(), mParent->getParent()->GetNextDialogID(), SPIN_AUTOSCALE,
		end);
}

void ColladaEffectParameterFloat::buildTemplateItem(DlgTemplateBuilder& tb, WORD& dialogCount, WORD& lineCount)
{
	ParamDef& definition = mParamBlock->GetParamDef(mParamID);
	FUAssert(definition.ctrl_count == 2, return);

	DLGITEMTEMPLATE& label = buildTemplateLabel(tb,dialogCount,lineCount);
	uint32 breakX = rightSectionRight - 8;

	// CUSTEDITWINDOWCLASS
	tb.Align();
	DLGITEMTEMPLATE& custEdit = tb.Append<DLGITEMTEMPLATE>();
	dialogCount++;
	custEdit.x = rightSectionLeft;
	custEdit.y = label.y;
	custEdit.id = definition.ctrl_IDs[0];
	custEdit.cx = breakX - rightSectionLeft;
	custEdit.cy = lineHeight - 1;
	custEdit.style = WS_CHILD | WS_VISIBLE;
	custEdit.dwExtendedStyle = WS_EX_RIGHT;

	tb.AppendWString(CUSTEDITWINDOWCLASS); // Window class
	tb.AppendWString(""); // Window text
	tb.Append((uint16) 0); // extra bytes count

	// SPINNERWINDOWCLASS
	tb.Align();
	DLGITEMTEMPLATE& spinner = tb.Append<DLGITEMTEMPLATE>();
	dialogCount++;
	spinner.x = breakX + 1;
	spinner.y = label.y;
	spinner.id = definition.ctrl_IDs[1];
	spinner.cx = rightSectionRight - breakX - 1;
	spinner.cy = lineHeight - 1;
	spinner.style = WS_CHILD | WS_VISIBLE;
	spinner.dwExtendedStyle = WS_EX_RIGHT;

	tb.AppendWString(SPINNERWINDOWCLASS); // Window class
	tb.AppendWString(""); // Window text
	tb.Append((uint16) 0); // extra bytes count

	++ lineCount;
}

void ColladaEffectParameterFloat::update()
{
	synchronize();
	cgSetParameter1f(mParameter, mFloat);
}

void ColladaEffectParameterFloat::synchronize()
{
	Interval dummy;
	if (mSemantic == kTIME)
	{
		// in Max, each second is 4800 time ticks
		mFloat = static_cast<float>(GetCOREInterface()->GetTime()) / 4800.0f;
	}
	else
	{
		mParamBlock->GetValue(mParamID, GetCOREInterface()->GetTime(), mFloat, dummy);
	}
}

void ColladaEffectParameterFloat::doExport(DocumentExporter* document, FCDEffectProfileFX* profile, FCDMaterial* instance)
{
	if (profile == NULL || instance == NULL || mParamBlock == NULL)
		return;

	fstring semantic = getSemanticString();
	fstring reference = getReferenceString();

	// create animated generator on profile
	FCDEffectParameterFloat* parameter = (FCDEffectParameterFloat*) profile->AddEffectParameter(FCDEffectParameter::FLOAT);
	parameter->SetGenerator();
	parameter->SetSemantic(semantic);
	parameter->SetValue(mFloat);
	parameter->SetReference(reference);
	parameter->SetFloatType(mType == ColladaEffectParameter::kFloat ? FCDEffectParameterFloat::FLOAT : FCDEffectParameterFloat::HALF);

	parameter->AddAnnotation(FC("UIName"), FCDEffectParameter::STRING, mUIName);
	parameter->AddAnnotation(FC("UIWidget"), FCDEffectParameter::STRING, FC("Spinner"));
	parameter->AddAnnotation(FC("UIMin"), FCDEffectParameter::FLOAT, mUIMin);
	parameter->AddAnnotation(FC("UIMax"), FCDEffectParameter::FLOAT, mUIMax);
	
	if (mSemantic != ColladaEffectParameter::kTIME)
	{
		ParamDef& def = mParamBlock->GetParamDef(mParamID);
		ANIM->ExportProperty(def.int_name, mParamBlock, mParamID, 0, parameter->GetValue(), NULL);
	}

	// create modifier on instance
	parameter = (FCDEffectParameterFloat*) instance->AddEffectParameter(FCDEffectParameter::FLOAT);
	parameter->SetModifier();
	parameter->SetValue(mFloat);
	parameter->SetReference(reference);
	parameter->SetFloatType(mType == ColladaEffectParameter::kFloat ? FCDEffectParameterFloat::FLOAT : FCDEffectParameterFloat::HALF);
}

//
// MATRIX
//

ColladaEffectParameterMatrix::ColladaEffectParameterMatrix(ColladaEffectShader* parentP, CGparameter param, Type type, Semantic semantic)
:	ColladaEffectParameter(parentP, param,type,semantic)
,	mMatrix(TRUE)
{}

void ColladaEffectParameterMatrix::addToDescription(ParamBlockDesc2 *desc)
{
	// get its value
	float mat[16];
	Matrix3 def(TRUE); // identity
	if (!cgGetParameterValuefr(mParameter, 16, mat))
	{
		def = ToMatrix3(FMMatrix44(mat));
	}

	mParamID = desc->Count();
	TCHAR *name = (TCHAR*)mName.c_str();

	// not animatable, no UI
	desc->AddParam(mParamID, name, TYPE_MATRIX3, 0, 0,
		p_default, def,
		end);
}

void ColladaEffectParameterMatrix::buildTemplateItem(DlgTemplateBuilder& UNUSED(tb), WORD& UNUSED(dialogCount), WORD& UNUSED(lineCount))
{
	// NO UI
}

void ColladaEffectParameterMatrix::update()
{
	// check for window
	ID3D9GraphicsWindow *d3d9 = mParent->getParent()->getGraphicsWindow();
	if (d3d9 == NULL)
		return;

	// check for device
	LPDIRECT3DDEVICE9 pd3dDevice = d3d9->GetDevice();
	if (pd3dDevice == NULL)
		return;

	// check for object
	INode* objectNode = mParent->getParent()->getObjectNode();
	if (objectNode == NULL)
		return;

	// optional, but since we do not support any other kind of matrix
	// than float4x4 I guess we need to test this.
	CGtype type = cgGetParameterType(mParameter);
	CGparameterclass clas = cgGetParameterClass(mParameter);
	FUAssert(type == CG_FLOAT4x4, return);
	FUAssert(clas == CG_PARAMETERCLASS_MATRIX, return);

	switch (mSemantic)
	{
	case kUNKNOWN:
	{	
		// maybe a constant matrix, so just set it to its default value
		synchronize();
		FMMatrix44 mat = ToFMMatrix44(mMatrix);
		cgSetMatrixParameterfr(mParameter, (float*)mat);
		break;
	}
	case kVIEWPROJ:
	{
		D3DXMATRIX view, proj, viewproj;
		pd3dDevice->GetTransform(D3DTS_VIEW, &view);
		pd3dDevice->GetTransform(D3DTS_PROJECTION, &proj);
		D3DXMatrixMultiply(&viewproj, &view, &proj);
		D3DXMatrixTranspose(&viewproj, &viewproj);
		cgSetMatrixParameterfr(mParameter, (float*)viewproj);
		break;
	}
	case kVIEWPROJI:
	{
		D3DXMATRIX view, proj, viewproji;
		pd3dDevice->GetTransform(D3DTS_VIEW, &view);
		pd3dDevice->GetTransform(D3DTS_PROJECTION, &proj);
		D3DXMatrixMultiply(&viewproji, &view, &proj);
		D3DXMatrixInverse(&viewproji, NULL, &viewproji);
		D3DXMatrixTranspose(&viewproji, &viewproji);
		cgSetMatrixParameterfr(mParameter, (float*)viewproji);
		break;
	}
	case kVIEWPROJIT:
	{
		D3DXMATRIX view, proj, viewprojit;
		pd3dDevice->GetTransform(D3DTS_VIEW, &view);
		pd3dDevice->GetTransform(D3DTS_PROJECTION, &proj);
		D3DXMatrixMultiply(&viewprojit, &view, &proj);
		D3DXMatrixInverse(&viewprojit, NULL, &viewprojit);
		cgSetMatrixParameterfr(mParameter, (float*)viewprojit);
		break;
	}
	case kWORLDVIEWPROJ:
	{
		D3DXMATRIX world, view, proj, worldviewproj;
		pd3dDevice->GetTransform(D3DTS_WORLD, &world);
		pd3dDevice->GetTransform(D3DTS_VIEW, &view);
		pd3dDevice->GetTransform(D3DTS_PROJECTION, &proj);
		D3DXMatrixMultiply(&worldviewproj, &world, &view);
		D3DXMatrixMultiply(&worldviewproj, &worldviewproj, &proj);
		D3DXMatrixTranspose(&worldviewproj, &worldviewproj);
		cgSetMatrixParameterfr(mParameter, (float*)worldviewproj);
		break;
	}
	case kWORLDVIEWPROJI:
	{
		D3DXMATRIX world, view, proj, worldviewproj;
		pd3dDevice->GetTransform(D3DTS_WORLD, &world);
		pd3dDevice->GetTransform(D3DTS_VIEW, &view);
		pd3dDevice->GetTransform(D3DTS_PROJECTION, &proj);
		D3DXMatrixMultiply(&worldviewproj, &world, &view);
		D3DXMatrixMultiply(&worldviewproj, &worldviewproj, &proj);
		D3DXMatrixInverse(&worldviewproj, NULL, &worldviewproj);
		D3DXMatrixTranspose(&worldviewproj, &worldviewproj);
		cgSetMatrixParameterfr(mParameter, (float*)worldviewproj);
		break;
	}
	case kWORLDVIEWPROJIT:
	{
		D3DXMATRIX world, view, proj, worldviewproj;
		pd3dDevice->GetTransform(D3DTS_WORLD, &world);
		pd3dDevice->GetTransform(D3DTS_VIEW, &view);
		pd3dDevice->GetTransform(D3DTS_PROJECTION, &proj);
		D3DXMatrixMultiply(&worldviewproj, &world, &view);
		D3DXMatrixMultiply(&worldviewproj, &worldviewproj, &proj);
		D3DXMatrixInverse(&worldviewproj, NULL, &worldviewproj);
		cgSetMatrixParameterfr(mParameter, (float*)worldviewproj);
		break;
	}
	case kWORLDVIEW:
	{
		D3DXMATRIX world, view, worldview;
		pd3dDevice->GetTransform(D3DTS_WORLD, &world);
		pd3dDevice->GetTransform(D3DTS_VIEW, &view);
		D3DXMatrixMultiply(&worldview, &world, &view);
		D3DXMatrixTranspose(&worldview, &worldview);
		cgSetMatrixParameterfr(mParameter, (float*)worldview);
		break;
	}
	case kWORLDVIEWI:
	{
		D3DXMATRIX world, view, worldview;
		pd3dDevice->GetTransform(D3DTS_WORLD, &world);
		pd3dDevice->GetTransform(D3DTS_VIEW, &view);
		D3DXMatrixMultiply(&worldview, &world, &view);
		D3DXMatrixInverse(&worldview, NULL, &worldview);
		D3DXMatrixTranspose(&worldview, &worldview);
		cgSetMatrixParameterfr(mParameter, (float*)worldview);
		break;
	}
	case kWORLDVIEWIT:
	{
		D3DXMATRIX world, view, worldview;
		pd3dDevice->GetTransform(D3DTS_WORLD, &world);
		pd3dDevice->GetTransform(D3DTS_VIEW, &view);
		D3DXMatrixMultiply(&worldview, &world, &view);
		D3DXMatrixInverse(&worldview, NULL, &worldview);
		cgSetMatrixParameterfr(mParameter, (float*)worldview);
		break;
	}
	case kOBJECT:
	{
		// not too sure about this... do we use NodeTM, ObjectTM, ObjTMBeforeWSM or ObjTMAfterWSM?
		Matrix3 object = objectNode->GetObjectTM(GetCOREInterface()->GetTime());
		FMMatrix44 mat = ToFMMatrix44(object);
		cgSetMatrixParameterfr(mParameter, (float*)mat);
		break;
	}
	case kOBJECTI:
	{
		Matrix3 object = objectNode->GetObjectTM(GetCOREInterface()->GetTime());
		FMMatrix44 mat = ToFMMatrix44(object);
		mat = mat.Inverted();
		cgSetMatrixParameterfr(mParameter, (float*)mat);
		break;
	}
	case kOBJECTIT:
	{
		Matrix3 object = objectNode->GetObjectTM(GetCOREInterface()->GetTime());
		FMMatrix44 mat = ToFMMatrix44(object);
		mat = mat.Inverted();
		mat = mat.Transposed();
		cgSetMatrixParameterfr(mParameter, (float*)mat);
		break;
	}
	case kWORLD:
	{	
		D3DXMATRIX world;
		pd3dDevice->GetTransform(D3DTS_WORLD, &world);
		D3DXMatrixTranspose(&world, &world);
		cgSetMatrixParameterfr(mParameter, (float*)world);
		break;
	}
	case kWORLDI:
	{
		D3DXMATRIX world;
		pd3dDevice->GetTransform(D3DTS_WORLD, &world);
		D3DXMatrixInverse(&world, NULL, &world);
		D3DXMatrixTranspose(&world, &world);
		cgSetMatrixParameterfr(mParameter, (float*)world);
		break;
	}
	case kWORLDIT:
	{
		D3DXMATRIX world;
		pd3dDevice->GetTransform(D3DTS_WORLD, &world);
		D3DXMatrixInverse(&world, NULL, &world);
		cgSetMatrixParameterfr(mParameter, (float*)world);
		break;
	}
	case kVIEW:
	{
		D3DXMATRIX view;
		pd3dDevice->GetTransform(D3DTS_VIEW, &view);
		D3DXMatrixTranspose(&view, &view);
		cgSetMatrixParameterfr(mParameter, (float*)view);
		break;
	}
	case kVIEWI:
	{	
		D3DXMATRIX view;
		pd3dDevice->GetTransform(D3DTS_VIEW, &view);
		D3DXMatrixInverse(&view, NULL, &view);
		D3DXMatrixTranspose(&view, &view);
		cgSetMatrixParameterfr(mParameter, (float*)view);
		break;
	}
	case kVIEWIT:
	{	
		D3DXMATRIX view;
		pd3dDevice->GetTransform(D3DTS_VIEW, &view);
		D3DXMatrixInverse(&view, NULL, &view);
		cgSetMatrixParameterfr(mParameter, (float*)view);
		break;
	}
	case kPROJ:
	{	
		D3DXMATRIX proj;
		pd3dDevice->GetTransform(D3DTS_PROJECTION, &proj);
		D3DXMatrixTranspose(&proj, &proj);
		cgSetMatrixParameterfr(mParameter, (float*)proj);
		break;
	}
	case kPROJI:
	{	
		D3DXMATRIX proj;
		pd3dDevice->GetTransform(D3DTS_PROJECTION, &proj);
		D3DXMatrixInverse(&proj, NULL, &proj);
		D3DXMatrixTranspose(&proj, &proj);
		cgSetMatrixParameterfr(mParameter, (float*)proj);
		break;
	}
	case kPROJIT:
	{	
		D3DXMATRIX proj;
		pd3dDevice->GetTransform(D3DTS_PROJECTION, &proj);
		D3DXMatrixInverse(&proj, NULL, &proj);
		cgSetMatrixParameterfr(mParameter, (float*)proj);
		break;
	}
	default: 
		break;
	} // end switch (semantic)
	
	DebugCG();
}

void ColladaEffectParameterMatrix::synchronize()
{
	Interval dummy;
	mParamBlock->GetValue(mParamID, GetCOREInterface()->GetTime(), mMatrix, dummy);
}

void ColladaEffectParameterMatrix::doExport(DocumentExporter* UNUSED(document), FCDEffectProfileFX* profile, FCDMaterial* instance)
{
	if (profile == NULL || instance == NULL)
		return;

	fstring semantic = getSemanticString();
	fstring reference = getReferenceString();

	FCDEffectParameterMatrix* parameter = (FCDEffectParameterMatrix*) profile->AddEffectParameter(FCDEffectParameter::MATRIX);
	parameter->SetGenerator();
	parameter->SetSemantic(semantic);
	parameter->SetValue(FMMatrix44::Identity);
	parameter->SetReference(reference);
	parameter->SetFloatType(mType == ColladaEffectParameter::kFloat4x4 ? FCDEffectParameterMatrix::FLOAT : FCDEffectParameterMatrix::HALF);
	parameter->AddAnnotation(FC("UIName"), FCDEffectParameter::STRING, mUIName);
	
	parameter = (FCDEffectParameterMatrix*) instance->AddEffectParameter(FCDEffectParameter::MATRIX);
	parameter->SetModifier();
	parameter->SetValue(FMMatrix44::Identity);
	parameter->SetReference(reference);
	parameter->SetFloatType(mType == ColladaEffectParameter::kFloat4x4 ? FCDEffectParameterMatrix::FLOAT : FCDEffectParameterMatrix::HALF);
}

//
// COLOR
//

ColladaEffectParameterColor::ColladaEffectParameterColor(ColladaEffectShader* parentP, CGparameter param, Type type, Semantic semantic)
:	ColladaEffectParameter(parentP, param,type,semantic)
,	mColor()
{
	ColladaEffectTimeObserver::GetInstance().AddObserver();
}

ColladaEffectParameterColor::~ColladaEffectParameterColor()
{
	ColladaEffectTimeObserver::GetInstance().RemoveObserver();
}

void ColladaEffectParameterColor::addToDescription(ParamBlockDesc2 *desc)
{
	// get default value (we lose the alpha component TYPE_RGBA needs a Color...)
	Color def(0.0f,0.0f,0.0f);
	float a[4];
	switch (mType)
	{
	case kHalf3:
	case kFloat3:
		if (cgGetParameterValuefr(mParameter, 3, a)) def = Color(a[0],a[1],a[2]); break;
	default:
		if (cgGetParameterValuefr(mParameter, 4, a)) def = Color(a[0],a[1],a[2]); break;
	}

	mParamID = desc->Count();
	TCHAR *name = (TCHAR*)mName.c_str();

	// TYPE RGBA is a Point3 ...
	desc->AddParam(mParamID, name, TYPE_RGBA, P_ANIMATABLE, 0,
		p_default, def,
		p_accessor, &gAnimatedPBAccessor,
		p_ui, TYPE_COLORSWATCH, mParent->getParent()->GetNextDialogID(),
		end);
}

void ColladaEffectParameterColor::buildTemplateItem(DlgTemplateBuilder& tb, WORD& dialogCount, WORD& lineCount)
{
	ParamDef& definition = mParamBlock->GetParamDef(mParamID);
	FUAssert(definition.ctrl_count == 1, return);

	DLGITEMTEMPLATE& label = buildTemplateLabel(tb,dialogCount,lineCount);
	
	// COLORSWATCHWINDOWCLASS
	tb.Align();
	DLGITEMTEMPLATE& colorSwatch = tb.Append<DLGITEMTEMPLATE>();
	dialogCount++;
	colorSwatch.x = rightSectionLeft + 1;
	colorSwatch.y = label.y;
	colorSwatch.id = definition.ctrl_IDs[0];
	colorSwatch.cx = rightSectionWidth - 10;
	colorSwatch.cy = lineHeight - 1;
	colorSwatch.style = WS_CHILD | WS_VISIBLE;
	colorSwatch.dwExtendedStyle = WS_EX_RIGHT;

	tb.AppendWString(COLORSWATCHWINDOWCLASS); // Window class
	tb.AppendWString(""); // Window text
	tb.Append((uint16) 0); // extra bytes count

	++ lineCount;
}

void ColladaEffectParameterColor::update()
{
	synchronize();
	switch (mType){
		case kHalf3:
		case kFloat3: cgSetParameter3fv(mParameter, &(mColor.r)); break;
		case kHalf4:
		case kFloat4: {
			AColor col(mColor);
			col.a = 1.0f;
			cgSetParameter4fv(mParameter, &(mColor.r)); break; }
		default: break;
	}
	DebugCG();
}

void ColladaEffectParameterColor::synchronize()
{
	Interval dummy;
	mParamBlock->GetValue(mParamID, GetCOREInterface()->GetTime(), mColor, dummy);
}

void ColladaEffectParameterColor::doExport(DocumentExporter* document, FCDEffectProfileFX* profile, FCDMaterial* instance)
{
	if (profile == NULL || instance == NULL || mParamBlock == NULL)
		return;

	fstring semantic = getSemanticString();
	fstring reference = getReferenceString();

	switch (mType)
	{
	case kHalf3:
	case kFloat3:
		{
			FCDEffectParameterFloat3* parameter = (FCDEffectParameterFloat3*) profile->AddEffectParameter(FCDEffectParameter::FLOAT3);
			parameter->SetGenerator();
			parameter->SetSemantic(semantic);
			parameter->SetValue(ToFMVector3(mColor));
			parameter->SetReference(reference);
			parameter->AddAnnotation(FC("UIName"), FCDEffectParameter::STRING, mUIName);
			parameter->AddAnnotation(FC("UIWidget"), FCDEffectParameter::STRING, FC("Color"));
			parameter->SetFloatType(mType == ColladaEffectParameter::kFloat3 ? FCDEffectParameterFloat3::FLOAT : FCDEffectParameterFloat3::HALF);

			ParamDef& def = mParamBlock->GetParamDef(mParamID);
			ANIM->ExportProperty(def.int_name, mParamBlock, mParamID, 0, parameter->GetValue(), NULL);

			parameter = (FCDEffectParameterFloat3*) instance->AddEffectParameter(FCDEffectParameter::FLOAT3);
			parameter->SetModifier();
			parameter->SetValue(ToFMVector3(mColor));
			parameter->SetReference(reference);
			parameter->SetFloatType(mType == ColladaEffectParameter::kFloat3 ? FCDEffectParameterFloat3::FLOAT : FCDEffectParameterFloat3::HALF);
		} break;
	case kHalf4:
	case kFloat4:
	default:
		{
			FCDEffectParameterVector* parameter = (FCDEffectParameterVector*) profile->AddEffectParameter(FCDEffectParameter::VECTOR);
			parameter->SetGenerator();
			parameter->SetSemantic(semantic);
			parameter->SetValue(FMVector4(ToFMVector3(mColor), 1.0f)); // we don't support colors with alpha
			parameter->SetReference(reference);
			parameter->AddAnnotation(FC("UIName"), FCDEffectParameter::STRING, mUIName);
			parameter->AddAnnotation(FC("UIWidget"), FCDEffectParameter::STRING, FC("Color"));
			parameter->SetFloatType(mType == ColladaEffectParameter::kFloat4 ? FCDEffectParameterVector::FLOAT : FCDEffectParameterVector::HALF);

			ParamDef& def = mParamBlock->GetParamDef(mParamID);
			// we really should support animations in 4D...
			ANIM->ExportProperty(def.int_name, mParamBlock, mParamID, 0, parameter->GetValue(), NULL);

			parameter = (FCDEffectParameterVector*) instance->AddEffectParameter(FCDEffectParameter::VECTOR);
			parameter->SetModifier();
			parameter->SetValue(FMVector4(ToFMVector3(mColor), 1.0f)); // we don't support colors with alpha
			parameter->SetReference(reference);
			parameter->SetFloatType(mType == ColladaEffectParameter::kFloat4 ? FCDEffectParameterVector::FLOAT : FCDEffectParameterVector::HALF);
		} break;
	}
}

//
// NODE
//

ColladaEffectParameterNode::ColladaEffectParameterNode(ColladaEffectShader* parentP, CGparameter param, Type type, Semantic semantic)
:	ColladaEffectParameter(parentP, param,type,semantic)
,	mNode(NULL)
,	mSuperClassID(0) // a 0 valued super class ID means "any node"
{}

void ColladaEffectParameterNode::addToDescription(ParamBlockDesc2 *desc)
{
	mParamID = desc->Count();
	TCHAR *name = (TCHAR*)mName.c_str();

	// not animatable since the node is
	if (mSuperClassID)
	{
		desc->AddParam(mParamID, name, TYPE_INODE, 0, 0,
			p_ui, TYPE_PICKNODEBUTTON, mParent->getParent()->GetNextDialogID(),
			p_sclassID, mSuperClassID,
			end);
	}
	else // any node
	{
		desc->AddParam(mParamID, name, TYPE_INODE, 0, 0,
			p_ui, TYPE_PICKNODEBUTTON, mParent->getParent()->GetNextDialogID(),
			end);
	}
}

void ColladaEffectParameterNode::buildTemplateItem(DlgTemplateBuilder& tb, WORD& dialogCount, WORD& lineCount)
{
	ParamDef& definition = mParamBlock->GetParamDef(mParamID);
	FUAssert(definition.ctrl_count == 1, return);

	DLGITEMTEMPLATE& label = buildTemplateLabel(tb,dialogCount,lineCount);

	// CUSTBUTTONWINDOWCLASS
	tb.Align();
	DLGITEMTEMPLATE& checkBox = tb.Append<DLGITEMTEMPLATE>();
	dialogCount++;
	checkBox.x = rightSectionLeft + 1;
	checkBox.y = label.y + 1;
	checkBox.id = definition.ctrl_IDs[0];
	checkBox.cx = rightSectionWidth - 10;
	checkBox.cy = lineHeight - 3;
	checkBox.style = WS_CHILD | WS_VISIBLE;
	checkBox.dwExtendedStyle = WS_EX_RIGHT;

	tb.AppendWString(CUSTBUTTONWINDOWCLASS); // Window class
	tb.AppendWString(""); // Window text
	tb.Append((uint16) 0); // extra bytes count

	++ lineCount;
}

void ColladaEffectParameterNode::update()
{
	synchronize();
	Point4 pos(0.0f,0.0f,0.0f,1.0f);
	if (mNode != NULL)
	{
		// get the node position
		pos = mNode->GetNodeTM(GetCOREInterface()->GetTime()).GetTrans();
	}
	else
	{
		// get the viewport's camera position
		ID3D9GraphicsWindow* d3d9 = mParent->getParent()->getGraphicsWindow();
		D3DXMATRIX view = d3d9->GetViewXform();
		D3DXMatrixInverse(&view, NULL, &view);
		pos = Point4(view._41, view._42, view._43, 1.0f); // get the translation components
	}

	if (mType == kFloat3 || mType == kHalf3)
	{
		cgSetParameter3fv(mParameter, &(pos.x));
	}
	else
	{
		cgSetParameter4fv(mParameter, &(pos.x));
	}
	DebugCG();
}

void ColladaEffectParameterNode::synchronize()
{
	Interval dummy;
	mParamBlock->GetValue(mParamID, GetCOREInterface()->GetTime(), mNode, dummy);
}

void ColladaEffectParameterNode::doExport(DocumentExporter* UNUSED(document), FCDEffectProfileFX* profile, FCDMaterial* instance)
{
	if (profile == NULL || instance == NULL)
		return;

	fstring semantic = getSemanticString();
	fstring reference = getReferenceString();

	Point4 pos(0.0f,0.0f,0.0f,1.0f);
	if (mNode != NULL)
	{
		// fetch the node position at time 0
		pos = mNode->GetNodeTM(0).GetTrans();
	}

	if (mType == kFloat3 || mType == kHalf3)
	{
		FCDEffectParameterFloat3* parameter = (FCDEffectParameterFloat3*) profile->AddEffectParameter(FCDEffectParameter::FLOAT3);
		parameter->SetGenerator();
		parameter->SetSemantic(semantic);
		parameter->SetValue(ToFMVector3(pos));
		parameter->SetReference(reference);
		parameter->AddAnnotation(FC("UIName"), FCDEffectParameter::STRING, mUIName);
		parameter->AddAnnotation(FC("Object"), FCDEffectParameter::STRING, mObjectID);
		parameter->AddAnnotation(FC("Space"), FCDEffectParameter::STRING, FC("World"));
		parameter->SetFloatType(mType == ColladaEffectParameter::kFloat3 ? FCDEffectParameterFloat3::FLOAT : FCDEffectParameterFloat3::HALF);

		parameter = (FCDEffectParameterFloat3*) profile->AddEffectParameter(FCDEffectParameter::FLOAT3);
		parameter->SetModifier();
		parameter->SetValue(ToFMVector3(pos));
		parameter->SetReference(reference);
		parameter->SetFloatType(mType == ColladaEffectParameter::kFloat3 ? FCDEffectParameterFloat3::FLOAT : FCDEffectParameterFloat3::HALF);
	}
	else
	{
		FCDEffectParameterVector* parameter = (FCDEffectParameterVector*) profile->AddEffectParameter(FCDEffectParameter::VECTOR);
		parameter->SetGenerator();
		parameter->SetSemantic(semantic);
		parameter->SetValue(ToFMVector4(pos));
		parameter->SetReference(reference);
		parameter->AddAnnotation(FC("UIName"), FCDEffectParameter::STRING, mUIName);
		parameter->AddAnnotation(FC("Object"), FCDEffectParameter::STRING, mObjectID);
		parameter->AddAnnotation(FC("Space"), FCDEffectParameter::STRING, FC("World"));
		parameter->SetFloatType(mType == ColladaEffectParameter::kFloat4 ? FCDEffectParameterVector::FLOAT : FCDEffectParameterVector::HALF);

		parameter = (FCDEffectParameterVector*) instance->AddEffectParameter(FCDEffectParameter::VECTOR);
		parameter->SetModifier();
		parameter->SetValue(ToFMVector4(pos));
		parameter->SetReference(reference);
		parameter->SetFloatType(mType == ColladaEffectParameter::kFloat4 ? FCDEffectParameterVector::FLOAT : FCDEffectParameterVector::HALF);
	}
}

//
// SAMPLER
//

ColladaEffectParameterSampler::ColladaEffectParameterSampler(ColladaEffectShader* parentP, CGparameter param, Type type, Semantic semantic)
:	ColladaEffectParameter(parentP, param,type,semantic)
,	mBitmap(NULL)
,	mTexture(NULL)
,	mStage(-1)
,	mDefaultResourceName("")
,	mSamplerType(kSAMPLER_2D)
{

	//// initialize default sampler states
	//mStates[D3DSAMP_MAGFILTER] = D3DTEXF_LINEAR;
	//mStates[D3DSAMP_MINFILTER] = D3DTEXF_LINEAR;

	// ensure sampler type
	if (mType == kSampler2D) mSamplerType = kSAMPLER_2D;
	else if (mType == kSamplerCUBE) mSamplerType = kSAMPLER_CUBE;

	// CgFX specific logic
	CGstateassignment textureAssign = cgGetNamedSamplerStateAssignment(mParameter, "texture");
    if (textureAssign)
    {
        CGparameter textureParam = cgGetTextureStateAssignmentValue(textureAssign);

		// check for default resource
		CGannotation ann = cgGetNamedParameterAnnotation(textureParam, "ResourceName");
        if (ann) mDefaultResourceName = cgGetStringAnnotationValue(ann);

		// check for resource type
		ann = cgGetNamedParameterAnnotation(textureParam, "ResourceType");
		if (ann && IsEquivalentI(cgGetStringAnnotationValue(ann), "CUBE")) mSamplerType = kSAMPLER_CUBE;
		else mSamplerType = kSAMPLER_2D;

		// check for UIName (overides the Sample parameter UIName if any)
		ann = cgGetNamedParameterAnnotation(textureParam, "UIName");
		if (ann) mUIName = cgGetStringAnnotationValue(ann);
	}

	//// find out about other sampler states
	//CGstateassignment samplerState;
	//samplerState = cgGetNamedSamplerStateAssignment(mParameter, "MinFilter");
	//samplerState = cgGetNamedSamplerStateAssignment(mParameter, "MagFilter");
	//samplerState = cgGetNamedSamplerStateAssignment(mParameter, "WrapS");
	//samplerState = cgGetNamedSamplerStateAssignment(mParameter, "WrapT");
}

void ColladaEffectParameterSampler::addToDescription(ParamBlockDesc2 *desc)
{
	mParamID = desc->Count();
	TCHAR *name = (TCHAR*)mName.c_str();

	// not animatable
	desc->AddParam(mParamID, name, TYPE_BITMAP, P_SHORT_LABELS, 0,
		p_ui, TYPE_BITMAPBUTTON, mParent->getParent()->GetNextDialogID(),
		end);
}

void ColladaEffectParameterSampler::buildTemplateItem(DlgTemplateBuilder& tb, WORD& dialogCount, WORD& lineCount)
{
	ParamDef& definition = mParamBlock->GetParamDef(mParamID);
	FUAssert(definition.ctrl_count == 1, return);

	DLGITEMTEMPLATE& label = buildTemplateLabel(tb,dialogCount,lineCount);

	// CUSTBUTTONWINDOWCLASS
	tb.Align();
	DLGITEMTEMPLATE& checkBox = tb.Append<DLGITEMTEMPLATE>();
	dialogCount++;
	checkBox.x = rightSectionLeft + 1;
	checkBox.y = label.y + 1;
	checkBox.id = definition.ctrl_IDs[0];
	checkBox.cx = rightSectionWidth - 10;
	checkBox.cy = lineHeight - 3;
	checkBox.style = WS_CHILD | WS_VISIBLE;
	checkBox.dwExtendedStyle = WS_EX_RIGHT;

	tb.AppendWString(CUSTBUTTONWINDOWCLASS); // Window class
	tb.AppendWString(""); // Window text
	tb.Append((uint16) 0); // extra bytes count

	++ lineCount;
}

void ColladaEffectParameterSampler::update()
{
	synchronize();

	// Cg adds a reference (AddRef()) to the texture
	// in a CgFX shader, the parameter we have may reference multiple D3D-enabled parameters
	for (size_t i = 0; i < getExpandedParameterCount(); ++i)
	{
		CGparameter param = getExpandedParameter(i);
		if (param == 0) continue;
		cgD3D9SetTexture(param, mTexture);
		mStage = cgGetParameterResourceIndex(param); // not used yet
		DebugCG();
	}
}

void ColladaEffectParameterSampler::synchronize()
{
	PBBitmap* oldValue = mBitmap;
	Interval dummy;
	mParamBlock->GetValue(mParamID, GetCOREInterface()->GetTime(), mBitmap, dummy);
	if (oldValue != mBitmap || (mBitmap == NULL && !mDefaultResourceName.empty()))
	{
		rebuildTexture(cgD3D9GetDevice());
	}
}

void ColladaEffectParameterSampler::releaseTexture()
{
	// Cg adds a reference (AddRef()) to the texture
	// in a CgFX shader, the parameter we have may reference multiple D3D-enabled parameters
	for (size_t i = 0; i < getExpandedParameterCount(); ++i)
	{
		CGparameter param = getExpandedParameter(i);
		if (param == 0) continue;
		cgD3D9SetTexture(param, NULL);
		DebugCG();
	}

	if (mTexture)
		mTexture->Release();
}

void ColladaEffectParameterSampler::rebuildTexture(IDirect3DDevice9* pNewDevice)
{
	if (pNewDevice == NULL)
		return;

	fstring textureFile;
	if (mBitmap == NULL)
		textureFile = mDefaultResourceName;
	else
	{
		textureFile = TO_FSTRING(mBitmap->bi.Name());
	}

	FUFileManager manager;
	manager.PushRootFile(mParent->getInfo().filename);
	textureFile = manager.GetCurrentUri().MakeAbsolute(textureFile);

	if (!textureFile.empty())
	{
		HRESULT hr = D3D_OK;
		
		if (mSamplerType == kSAMPLER_2D)
			hr = D3DXCreateTextureFromFile(pNewDevice, textureFile.c_str(), (LPDIRECT3DTEXTURE9*)&mTexture);
		else if (mSamplerType == kSAMPLER_CUBE)
			hr = D3DXCreateCubeTextureFromFile(pNewDevice, textureFile.c_str(), (LPDIRECT3DCUBETEXTURE9*)&mTexture);

		if (hr == D3D_OK && mTexture != NULL)
		{
			PBBitmap newBitmap;
			newBitmap.bi.SetName(const_cast<TCHAR*>(textureFile.c_str()));
			// set the default param block value
			Interval dummy;
			mParamBlock->SetValue(mParamID, GetCOREInterface()->GetTime(), &newBitmap);
			mParamBlock->GetValue(mParamID, GetCOREInterface()->GetTime(), mBitmap, dummy);
		}
		else
		{
			mBitmap = NULL;
			mParamBlock->SetValue(mParamID, GetCOREInterface()->GetTime(), (PBBitmap*)NULL);
		}
	}
}

void ColladaEffectParameterSampler::doExport(DocumentExporter* document, FCDEffectProfileFX* profile, FCDMaterial* instance)
{
	if (profile == NULL || instance == NULL)
		return;

	fstring semantic = getSemanticString();
	fstring reference = getReferenceString();

	FCDEffectParameterSampler* parameter1 = (FCDEffectParameterSampler*) profile->AddEffectParameter(FCDEffectParameter::SAMPLER);
	parameter1->SetGenerator();
	parameter1->SetSemantic(semantic);
	parameter1->SetReference(reference);
	parameter1->AddAnnotation(FC("UIName"), FCDEffectParameter::STRING, mUIName);
	parameter1->SetSamplerType(mSamplerType == kSAMPLER_2D ? FCDEffectParameterSampler::SAMPLER2D : FCDEffectParameterSampler::SAMPLERCUBE);

	FCDEffectParameterSampler* parameter2 = (FCDEffectParameterSampler*) instance->AddEffectParameter(FCDEffectParameter::SAMPLER);
	parameter2->SetModifier();
	parameter2->SetReference(reference);
	parameter2->SetSamplerType(mSamplerType == kSAMPLER_2D ? FCDEffectParameterSampler::SAMPLER2D : FCDEffectParameterSampler::SAMPLERCUBE);

	FCDEffectParameterSurface* surfaceParameter = exportSurface(document, profile, instance);
	parameter1->SetSurface(surfaceParameter);
	parameter2->SetSurface(surfaceParameter);
}

FCDEffectParameterSurface* ColladaEffectParameterSampler::exportSurface(DocumentExporter* document, FCDEffectProfileFX* profile, FCDMaterial* instance)
{
	if (mBitmap == NULL)
		return NULL;

	// export or locate the texture
	FCDImage* image = document->GetMaterialExporter()->ExportImage(mBitmap->bi);
	if (image != NULL)
	{
		fm::string surfaceId = image->GetDaeId() + "Surface";

		FCDEffectParameterSurfaceInit* initMethod = NULL;
		if (mSamplerType == kSAMPLER_2D)
		{
			FCDEffectParameterSurfaceInitFrom* init2D = new FCDEffectParameterSurfaceInitFrom();
			init2D->mip.push_back("0");
			init2D->slice.push_back("0");
			initMethod = init2D;
		}
		else if (mSamplerType == kSAMPLER_CUBE)
		{
			// DDS-like cube map
			FCDEffectParameterSurfaceInitCube* initCube = new FCDEffectParameterSurfaceInitCube();
			initCube->cubeType = FCDEffectParameterSurfaceInitCube::ALL;
			initMethod = initCube;
		}

		FCDEffectParameterSurface* parameter1 = (FCDEffectParameterSurface*) profile->AddEffectParameter(FCDEffectParameter::SURFACE);
		parameter1->SetGenerator();
		parameter1->SetReference(surfaceId);
		parameter1->AddImage(image);
		parameter1->SetSurfaceType(mSamplerType == kSAMPLER_2D ? "2D" : "CUBE");
		parameter1->SetInitMethod(initMethod);

		if (initMethod != NULL)
			initMethod = initMethod->Clone(NULL);

		FCDEffectParameterSurface* parameter2 = (FCDEffectParameterSurface*) instance->AddEffectParameter(FCDEffectParameter::SURFACE);
		parameter2->SetModifier();
		parameter2->AddImage(image);
		parameter2->SetReference(surfaceId);
		parameter2->SetSurfaceType(mSamplerType == kSAMPLER_2D ? "2D" : "CUBE");
		parameter2->SetInitMethod(initMethod);
		
		return parameter1;
	}
	else
	{
		return NULL;
	}
}
