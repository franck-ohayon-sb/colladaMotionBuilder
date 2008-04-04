/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MGlobal.h>
#include <maya/MImage.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnPluginData.h>
#include <maya/MFnStringArrayData.h>
#include <maya/MFnTypedAttribute.h>
#include "TranslatorHelpers/CDagHelper.h"
#include "ColladaFX/CFXShaderNode.h"
#include "ColladaFX/CFXParameter.h"
#include "ColladaFX/CFXRenderState.h"
#if MAYA_API_VERSION >= 700
#include <maya/MGeometryData.h>
#include <maya/MHardwareRenderer.h>
#include <maya/MHWShaderSwatchGenerator.h>
#endif

#ifdef MAC_TIGER
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif // MAC_TIGER

#include "FUtils/FUDaeEnum.h"
#include "FUtils/FUFileManager.h"
#include <algorithm>

//
// Debugging macro
//

#define ENTRYPOINT(functionName) /* OutputDebugStringA(#functionName) */

//
// OpenGL extensions
//

using namespace std;

const char* CFXShaderNode::DEFAULT_FRAGMENT_ENTRY = "main";
const char* CFXShaderNode::DEFAULT_VERTEX_ENTRY = "main";
const char* CFXShaderNode::DEFAULT_FRAGMENT_EFFECT_ENTRY = "FragmentProgram";
const char* CFXShaderNode::DEFAULT_VERTEX_EFFECT_ENTRY = "VertexProgram";
const char* CFXShaderNode::DEFAULT_TECHNIQUE_NAME = "technique0";
const char* CFXShaderNode::DEFAULT_PASS_NAME = "pass0";

MTypeId CFXShaderNode::id(0x0005050A);
MObject CFXShaderNode::aVertexShader;
MObject CFXShaderNode::aVertexShaderEntry;
MObject	CFXShaderNode::aFragmentShader;
MObject	CFXShaderNode::aFragmentShaderEntry;
MObject	CFXShaderNode::aColorSet0;
MObject	CFXShaderNode::aTex0;
MObject	CFXShaderNode::aTex1;
MObject	CFXShaderNode::aTex2;
MObject	CFXShaderNode::aTex3;
MObject	CFXShaderNode::aTex4;
MObject	CFXShaderNode::aTex5; 
MObject	CFXShaderNode::aTex6;
MObject	CFXShaderNode::aTex7;
MObject CFXShaderNode::aVertParam;
MObject CFXShaderNode::aFragParam;
MObject CFXShaderNode::aEffectPassDec;
MObject CFXShaderNode::aRenderStates;
MObject CFXShaderNode::aDynRenderStates;
MObject CFXShaderNode::aDirtyAttrib;
MObject	CFXShaderNode::aTechTarget;
MObject CFXShaderNode::aPassTarget;

static CGeffect myCreateEffectFromFile(CGcontext ctx, const char* filename, const char** args)
{
#ifdef WIN32
	__try
#endif // WIN32
	{
		return cgCreateEffectFromFile(ctx, filename, args);
	}
#ifdef WIN32
	__except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
		EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		return NULL;
	}
#endif // WIN32
}

CGeffect CFXShaderNode::CreateEffectFromFile(CGcontext ctx, const char* filename)
{
	MGlobal::displayInfo(MString("CG: Creating effect from file " + MString(filename)));
	// bug 421: extract the file directory, and feed it to the compiler
	// this is due to 
	fstring wdirname = FUFileManager::StripFileFromPath(TO_FSTRING(filename));
	fm::string dirname = TO_STRING(wdirname);
	dirname = "-I" + dirname;
	const char* args[] = { dirname.c_str(), NULL };
	return myCreateEffectFromFile(ctx, filename, args);
}

static CGprogram myCreateProgramFromFile(CGcontext ctx, const char* filename, CGprofile profile, const char* entry, const char** args)
{
#ifdef WIN32
	__try
#endif // WIN32
	{
		return cgCreateProgramFromFile(ctx, CG_SOURCE, filename, profile, entry, args);
	}
#ifdef WIN32
	__except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
		EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		return NULL;
	}
#endif // WIN32
}

CGprogram CFXShaderNode::CreateProgramFromFile(CGcontext ctx, const char* filename, CGprofile profile, const char* entry)
{
	// bug 421: extract the file directory, and feed it to the compiler
	MGlobal::displayInfo(MString("CG: Creating program from file " + MString(filename)));
	fstring wdirname = FUFileManager::StripFileFromPath(TO_FSTRING(filename));
	fm::string dirname = TO_STRING(wdirname);
	dirname = "-I" + dirname;
	const char* args[] = { dirname.c_str(), NULL };
	return myCreateProgramFromFile(ctx, filename, profile, entry, args);
}

CFXShaderNode::CFXShaderNode()
:	effect(NULL)
,	mTechTarget(0)
,	mPassTarget(0)
,	mTechniqueName(DEFAULT_TECHNIQUE_NAME)
,	mPassName(DEFAULT_PASS_NAME)
,	mForcedOnce(false)
,	vertexProgram(NULL)
,	vertexProfile(CG_PROFILE_UNKNOWN)
,	fragmentProgram(NULL)
,	fragmentProfile(CG_PROFILE_UNKNOWN)
,	context(NULL)
,	mCgBound(false)
{
	ENTRYPOINT(CFXShaderNode);

	// Hack so that in batch mode, cgGLGetLatestProfile does not crash in 
	// setVertexProgramFilename and setFragmentProgramFilename.
	glutDummyId = 0;
	if (MGlobal::mayaState() == MGlobal::kBatch)
	{
#ifdef WIN32
		glutDummyId = glutCreateWindow("dummy");
#endif // WIN32
	}

	// init Cg
	context = cgCreateContext();
	cgGLRegisterStates(context);
	cgGLSetManageTextureParameters(context, CG_TRUE);

	channelNames[0] = "map1";
	channelNames[1] = "tangent";
	channelNames[2] = "binormal";

	vpEntry = DEFAULT_VERTEX_ENTRY;
	fpEntry = DEFAULT_FRAGMENT_ENTRY;
	
	channelIndexes[0] = channelIndexes[1] = channelIndexes[2] = channelIndexes[3] = channelIndexes[4] = channelIndexes[5] = channelIndexes[6] = channelIndexes[7] = channelIndexes[8] = -1;
	channelTypes[0] = channelTypes[1] = channelTypes[2] = channelTypes[3] = channelTypes[4] = channelTypes[5] = channelTypes[6] = channelTypes[7] = channelTypes[8] = 0;
	m_uvSetId = 0;
	isLoaded = blendFlag = false;
}

CFXShaderNode::~CFXShaderNode() 
{
	ENTRYPOINT(~CFXShaderNode);

	// Release all the attributes
	for (CFXParameterList::iterator itE = effectParameters.begin(); itE != effectParameters.end(); ++itE)
	{
		delete *itE;
	}
	effectParameters.clear();

	// Release the CG handles
	clearVertexProgram();
	clearFragmentProgram();
	clearEffect();
	cgDestroyContext(context);

	if (MGlobal::mayaState() == MGlobal::kBatch)
	{
#ifdef WIN32
		glutDestroyWindow(glutDummyId);
#endif // WIN32
	}
}

void CFXShaderNode::clearVertexProgram()
{
	if (vertexProgram)
	{
		cgDestroyProgram(vertexProgram);
		vertexProgram = NULL;
		vertexProfile = CG_PROFILE_UNKNOWN;
	}
}

void CFXShaderNode::clearFragmentProgram()
{
	if (fragmentProgram) 
	{
		cgDestroyProgram(fragmentProgram);
		fragmentProgram = NULL;
		fragmentProfile = CG_PROFILE_UNKNOWN;
	}
}

void CFXShaderNode::clearEffect()
{
	if (effect)
	{
		cgDestroyEffect(effect);
		effect = NULL;
	}
}

MStatus CFXShaderNode::compute(const MPlug& plug, MDataBlock& data)
{
	ENTRYPOINT(compute);
	MStatus stat;

	if (plug == outColor || plug.parent() == outColor)
	{
		// The outColor should never be used directly: it should be used to link the shader around, though.
		MDataHandle outPassHandle = data.outputValue(CFXShaderNode::outColor);
		outPassHandle.set(1.0f, 1.0f, 0.0f);
		data.setClean(plug);
	}
	return MS::kSuccess;
}

bool CFXShaderNode::isCgFX()
{
	fstring fileName = MConvert::ToFChar(vpfile);
	fstring ext = FUFileManager::GetFileExtension(fileName);
	return IsEquivalent(ext, FC("cgfx"));
}

void CFXShaderNode::forceLoad()
{
	// double check entry points
	if (isCgFX())
	{
		fpfile = vpfile;
		vpEntry = DEFAULT_VERTEX_EFFECT_ENTRY;
		fpEntry = DEFAULT_FRAGMENT_EFFECT_ENTRY;
	}
	else 
	{
		if (vpEntry.length() == 0)
		{
			vpEntry = DEFAULT_VERTEX_ENTRY;
		}

		if (fpEntry.length() == 0)
		{
			fpEntry = DEFAULT_FRAGMENT_ENTRY;
		}
	}

	// load Cg/CgFX programs
	if (!isLoaded) 
	{
		if (isCgFX())
		{
			setEffectFilename(vpfile.asChar(), mForcedOnce);
		}
		else
		{
			setVertexProgramFilename(vpfile.asChar(),vpEntry.asChar());
			setFragmentProgramFilename(fpfile.asChar(),fpEntry.asChar());
		}
	}

	mForcedOnce = true;
}

void* CFXShaderNode::creator()
{
	ENTRYPOINT(creator);
	return new CFXShaderNode();
}

// static
void CFXShaderNode::CgErrorCallback()
{
	CGerror errCode = cgGetError();
	if (errCode != CG_NO_ERROR)
	{
		WARNING_OUT("CG ERROR[%d] %s\n", (int)errCode, cgGetErrorString(errCode));
	}
}

MStatus CFXShaderNode::initialize()		
{
	ENTRYPOINT(initialize);

	// Set Cg error callback
	cgSetErrorCallback(&CFXShaderNode::CgErrorCallback);

	MFnTypedAttribute typedAttr;
	MFnNumericAttribute numAttr;
	
	aVertexShader = typedAttr.create("vertexProgram", "vp", MFnData::kString);
	typedAttr.setStorable(true);
	typedAttr.setInternal(true);
	addAttribute(aVertexShader);

	aVertexShaderEntry = typedAttr.create("vertexProgramEntry", "vpe", MFnData::kString);
	typedAttr.setStorable(true);
	typedAttr.setInternal(true);
	addAttribute(aVertexShaderEntry);
	
	aFragmentShader = typedAttr.create("fragmentProgram", "fp", MFnData::kString);
	typedAttr.setStorable(true);
	typedAttr.setInternal(true);
	addAttribute(aFragmentShader);

	aFragmentShaderEntry = typedAttr.create("fragmentProgramEntry", "fpe", MFnData::kString);
	typedAttr.setStorable(true);
	typedAttr.setInternal(true);
	addAttribute(aFragmentShaderEntry);
	
	aColorSet0 = typedAttr.create("COLOR0", "c0", MFnData::kString);
	typedAttr.setStorable(true);
	typedAttr.setInternal(true);
	addAttribute(aColorSet0);
	
	aTex0 = typedAttr.create("TEXCOORD0", "t0", MFnData::kString);
	typedAttr.setStorable(true);
	typedAttr.setInternal(true);
	addAttribute(aTex0);
	
	aTex1 = typedAttr.create("TEXCOORD1", "t1", MFnData::kString);
	typedAttr.setStorable(true);
	typedAttr.setInternal(true);
	addAttribute(aTex1);
	
	aTex2 = typedAttr.create("TEXCOORD2", "t2", MFnData::kString);
	typedAttr.setStorable(true);
	typedAttr.setInternal(true);
	addAttribute(aTex2);
	
	aTex3 = typedAttr.create("TEXCOORD3", "t3", MFnData::kString);
	typedAttr.setStorable(true);
	typedAttr.setInternal(true);
	addAttribute(aTex3);
	
	aTex4 = typedAttr.create("TEXCOORD4", "t4", MFnData::kString);
	typedAttr.setStorable(true);
	typedAttr.setInternal(true);
	addAttribute(aTex4);
	
	aTex5 = typedAttr.create("TEXCOORD5", "t5", MFnData::kString);
	typedAttr.setStorable(true);
	typedAttr.setInternal(true);
	addAttribute(aTex5);
	
	aTex6 = typedAttr.create("TEXCOORD6", "t6", MFnData::kString);
	typedAttr.setStorable(true);
	typedAttr.setInternal(true);
	addAttribute(aTex6);
	
	aTex7 = typedAttr.create("TEXCOORD7", "t7", MFnData::kString);
	typedAttr.setStorable(true);
	typedAttr.setInternal(true);
	addAttribute(aTex7);
	
	aVertParam = typedAttr.create("vertParam", "vpm", MFnData::kStringArray);
	typedAttr.setStorable(false);
	typedAttr.setWritable(false);
	addAttribute(aVertParam);
	
	aFragParam = typedAttr.create("fragParam", "fpm", MFnData::kStringArray);
	typedAttr.setStorable(false);
	typedAttr.setWritable(false);
	addAttribute(aFragParam);
	
	aEffectPassDec = typedAttr.create("passDec", "psd", MFnData::kStringArray);
	typedAttr.setStorable(false);
	typedAttr.setWritable(false);
	addAttribute(aEffectPassDec);

	aRenderStates = typedAttr.create("renderStates","rs",MFnData::kStringArray);
	typedAttr.setStorable(true); // write to file
	typedAttr.setInternal(true); // notifies us when `getAttr` or `setAttr` are called.
	addAttribute(aRenderStates);

	aDynRenderStates = typedAttr.create("dynRenderStates","drs",MFnData::kString);
	typedAttr.setStorable(true);
	typedAttr.setInternal(true);
	typedAttr.setHidden(true);
	addAttribute(aDynRenderStates);

	aDirtyAttrib = numAttr.create("isDirtyAttrib","dty",MFnNumericData::kBoolean);
	numAttr.setStorable(false);
	numAttr.setInternal(true);
	addAttribute(aDirtyAttrib);

	aTechTarget = numAttr.create("techTarget","tta",MFnNumericData::kLong);
	numAttr.setStorable(true);
	numAttr.setInternal(true);
	addAttribute(aTechTarget);

	aPassTarget = numAttr.create("passTarget","pta",MFnNumericData::kLong);
	numAttr.setStorable(true);
	numAttr.setInternal(true);
	addAttribute(aPassTarget);
	
	attributeAffects(aVertexShader, outColor);
	attributeAffects(aFragmentShader, outColor);
	attributeAffects(aColorSet0, outColor);
	attributeAffects(aTex0, outColor);
	attributeAffects(aTex1, outColor);
	attributeAffects(aTex2, outColor);
	attributeAffects(aTex3, outColor);
	attributeAffects(aTex4, outColor);
	attributeAffects(aTex5, outColor);
	attributeAffects(aTex6, outColor);
	attributeAffects(aTex7, outColor);
	attributeAffects(aDirtyAttrib, outColor);

	return MS::kSuccess;
}

void CFXShaderNode::BindUVandColorSets(const MDagPath& shapePath)
{
	if (shapePath.hasFn(MFn::kMesh)) 
	{
		MFnMesh meshf(shapePath.node());
		
		// Bind the texture coordinate sets
		MString useSetName;
		MStringArray setNames;
		meshf.getUVSetNames(setNames);
		meshf.getCurrentUVSetName(useSetName);
		uint num_uvSets = meshf.numUVSets();
		
		// if no uvSet is defined, use the active set
		if (channelNames[0].length() == 0) 
		{
			for (uint i = 0 ; i < num_uvSets ; i++) 
			{
				if (useSetName == setNames[i]) m_uvSetId = i;
			}
			// use the default one
			MPlug p1(thisMObject(), aTex0);
			p1.setValue(useSetName);
		}
		// if a uvSet is defined, look up it in the name array
		else 
		{
			int hasSet = 0;
			for (uint i = 0 ; i < num_uvSets; i++) 
			{
				for (int iter=0 ; iter < channelCount; iter++) 
				{
					if (channelNames[iter] == setNames[i]) 
					{	
						channelIndexes[iter] = i;
						channelTypes[iter] = 1;
						if (iter == 2) m_uvSetId = i;
						hasSet = 1;
					}
				}
			}
			// use the default one
			if (hasSet == 0)
			{
				MPlug p(thisMObject(), aTex0);
				p.setValue(useSetName);
			}
		}

#if MAYA_API_VERSION >= 700
		// Bind the color sets
		setNames.clear();
		meshf.getColorSetNames(setNames);
		meshf.getCurrentColorSetName(useSetName);
		uint num_colorSets = meshf.numColorSets();

		// find the active set
		for (uint i = 0 ; i < num_colorSets ; i++) 
		{
			if (useSetName == setNames[i]) channelIndexes[8] = i;
		}

		// if a colorSet is defined, look up it in the name array
		for (uint i = 0 ; i < num_colorSets ; i++) 
		{
			for (int iter=0 ; iter < channelCount; iter++) 
			{
				if (channelNames[iter] == setNames[i]) 
				{
					channelIndexes[iter] = i;
					channelTypes[iter] = 0;
				}
			}	
		}
#endif
	}
}

void CFXShaderNode::BindGLStates()
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
	glDisable(GL_LIGHTING);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	// Enable the alpha test
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.05f);
}

void CFXShaderNode::BindCgStates()
{
	if (vertexProgram && fragmentProgram)
	{
		cgGLLoadProgram(vertexProgram);
		cgGLEnableProfile(vertexProfile);
		cgGLBindProgram(vertexProgram);

		cgGLLoadProgram(fragmentProgram);
		cgGLEnableProfile(fragmentProfile);
		cgGLBindProgram(fragmentProgram);
		mCgBound = true;
	}
}

void CFXShaderNode::BindRenderStates()
{
	// Synchronize and set all the render states
	for (CFXRenderStateList::iterator it = renderStates.begin(); it != renderStates.end();)
	{
		if ((*it)->IsDead())
		{
			(*it)->Restore();
			delete *it;
			it = renderStates.erase(it);
		}
		else if ((*it)->IsDirty())
		{
			(*it)->Synchronize();
			(*it)->Use();
			++it;
		}
		else
		{
			(*it)->Use();
			++it;
		}
	}
}

void CFXShaderNode::UnbindGLStates()
{
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	
	glPopClientAttrib();
	glPopAttrib();
}

void CFXShaderNode::UnbindRenderStates()
{
	// Synchronize and set all the render states
	for (CFXRenderStateList::iterator it = renderStates.begin(); it != renderStates.end(); ++it)
	{
		(*it)->Reset();
	}
}

void CFXShaderNode::UnbindCgStates()
{
	if (mCgBound)
	{
		cgGLDisableProfile(vertexProfile);
		cgGLDisableProfile(fragmentProfile);
		mCgBound = false;
	}
}

MStatus CFXShaderNode::glBind(const MDagPath& UNUSED(shapePath))
{
	ENTRYPOINT(glBind);

	if (!mForcedOnce)
		forceLoad();

	BindGLStates();
	BindRenderStates();
	BindCgStates();

	return MS::kSuccess;
}

MStatus CFXShaderNode::glUnbind(const MDagPath& UNUSED(shapePath))
{
	UnbindCgStates();
	UnbindRenderStates();
	UnbindGLStates();
	return MS::kSuccess;
}

MStatus CFXShaderNode::glGeometry(const MDagPath& shapePath, int prim, unsigned int writable, int indexCount, const unsigned int * indexArray, int vertexCount, const int * vertexIDs, const float * vertexArray, int normalCount, const float ** normalArrays, int colorCount, const float ** colorArrays, int texCoordCount, const float ** texCoordArrays)
{
	ENTRYPOINT(glGeometry);

	if (prim != GL_TRIANGLES && prim != GL_TRIANGLE_STRIP) return MS::kFailure;

	// whether we're batching or not, we must bind the UV and Color sets once per geometry
	BindUVandColorSets(shapePath);

	MStatus stat;
 	MFnDagNode fdag(shapePath.node(), &stat);
 	MObject parentObj = fdag.parent(0, &stat);
 	
 	fdag.setObject(parentObj);
 	MMatrix shaderSpace = fdag.transformationMatrix(&stat);
 	
 	fdag.setObject(shapePath.node());
 	MMatrix worldSpace = shapePath.inclusiveMatrix();

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glVertexPointer (3, GL_FLOAT, 0, &vertexArray[0]);
	glNormalPointer (GL_FLOAT, 0, &normalArrays[0][0]);

	// color0
	if (channelIndexes[8] != -1 && channelTypes[8] == 0)
	{
		if (channelIndexes[8] < colorCount && colorArrays[ channelIndexes[8] ])
		{
			glColorPointer(4, GL_FLOAT, 0, colorArrays[ channelIndexes[8] ]);
			glEnableClientState(GL_COLOR_ARRAY);
		}
	}

	// texcoord0-7
	for (int i=0; i<8; i++)
	{
		glClientActiveTexture(GL_TEXTURE0_ARB + i);//:TEXCOORDi

		int normalId = m_uvSetId*3;
		if (channelNames[i] == "tangent")
		{
			if (normalCount>(1+normalId) && normalArrays[ 1+normalId ])
			{
				glTexCoordPointer(3, GL_FLOAT, 0, normalArrays[ 1+normalId ]);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			}
			continue;
		}
		
		if (channelNames[i] == "binormal")
		{
			if (normalCount>(2+normalId) && normalArrays[ 2+normalId ])
			{
				glTexCoordPointer(3, GL_FLOAT, 0, normalArrays[ 2+normalId ]);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			}
			continue;
		}
		
		
		if (channelIndexes[i] != -1)
		{
			if (channelTypes[i] == 1)
			{
				if (channelIndexes[i]< texCoordCount && texCoordArrays[ channelIndexes[i] ])
				{
					glTexCoordPointer(2, GL_FLOAT, 0, texCoordArrays[ channelIndexes[i] ]);
					glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				}
			}
			else
			{
				if (channelIndexes[i]< colorCount && colorArrays[ channelIndexes[i] ])
				{
					glTexCoordPointer(4, GL_FLOAT, 0, colorArrays[ channelIndexes[i] ]);
					glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				}
			}
		}
	}

	// Synchronize and set all the effect parameters
	for (CFXParameterList::iterator it = effectParameters.begin(); it != effectParameters.end(); ++it)
	{
		if ((*it)->IsDirty()) (*it)->Synchronize();
		(*it)->updateAttrValue(shaderSpace, worldSpace);
	}
	
	// draw geometry
	glDrawElements(prim, indexCount, GL_UNSIGNED_INT, indexArray);

	return MS::kSuccess;
}

#if MAYA_API_VERSION == 700 || MAYA_API_VERSION > 800
MStatus CFXShaderNode::renderSwatchImage(MImage & outImage)
{
	ENTRYPOINT(renderSwatchImage);

	MStatus status = MStatus::kFailure;
	
	// Get the hardware renderer utility class
	MHardwareRenderer *pRenderer = MHardwareRenderer::theRenderer();
	
	if (pRenderer)
	{
		const MString& backEndStr = pRenderer->backEndString();

		// Get geometry
		// ============
		unsigned int* pIndexing = 0;
		unsigned int  numberOfData = 0;
		unsigned int  indexCount = 0;

		MHardwareRenderer::GeometricShape gshape = MHardwareRenderer::kDefaultSphere;
		MGeometryData* pGeomData = pRenderer->referenceDefaultGeometry(gshape, numberOfData, pIndexing, indexCount);
		if (!pGeomData) return MStatus::kFailure;

		// Make the swatch context current
		// ===============================
		//
		unsigned int width, height;
		outImage.getSize(width, height);
		unsigned int origWidth = width;
		unsigned int origHeight = height;

		MStatus status2 = pRenderer->makeSwatchContextCurrent(backEndStr, width, height);

		if (status2 != MS::kSuccess) 
		{
			pRenderer->dereferenceGeometry(pGeomData, numberOfData);
			return MStatus::kFailure;
		}

		// Get the light direction from the API, and use it
		// =============================================
		{
			float light_pos[4];
			pRenderer->getSwatchLightDirection(light_pos[0], light_pos[1], light_pos[2], light_pos[3]);
		}

		// Get camera
		// ==========
		{
			// Get the camera frustum from the API
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			double l, r, b, t, n, f;
			pRenderer->getSwatchPerspectiveCameraSetting(l, r, b, t, n, f);
			glFrustum(l, r, b, t, n, f);

			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			float x, y, z, w;
			pRenderer->getSwatchPerspectiveCameraTranslation(x, y, z, w);
			glTranslatef(x, y, z);
		}

		// Get the default background color and clear the background
		//
		float r, g, b, a;
		MHWShaderSwatchGenerator::getSwatchBackgroundColor(r, g, b, a);
		glClearColor(r, g, b, a);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glShadeModel(GL_SMOOTH);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

		// Draw The Swatch
		// ===============
		//drawTheSwatch(pGeomData, pIndexing, numberOfData, indexCount);
		MDagPath dummyPath;
		glBind(dummyPath);

		float *vertexData = (float *)(pGeomData[0].data());
		float *normalData = (float *)(pGeomData[1].data());
		float *uvData = (float *)(pGeomData[2].data());
		float *tangentData = (float *)(pGeomData[3].data());
		float *binormalData = (float *)(pGeomData[4].data());
		unsigned int normalCount = 0;

		// Stick normal, tangent, binormals into ptr array
		float ** normalArrays = new float *[3];
		if (normalData) 
		{
			normalArrays[0] = normalData;
			normalCount++;
		}
		else
			normalArrays[0] = NULL;
		if (tangentData) 
		{
			normalArrays[1] = tangentData;
			normalCount++;
		}
		else
			normalArrays[1] = NULL;
		if (binormalData)
		{
			normalArrays[2] = binormalData;
			normalCount++;
		}
		else
			normalArrays[2] = NULL;

		// Stick uv data into ptr array
		unsigned int uvCount = 0;
		float ** texCoordArrays = new float *[1];
		if (uvData)
		{
			texCoordArrays[0] = uvData;
			uvCount = 1;
		}
		else
			texCoordArrays[0] = NULL;

		glGeometry(dummyPath,
					GL_TRIANGLES,
					false,
					indexCount,
					pIndexing,
					0, 
					NULL, /* no vertex ids */
					vertexData,
					normalCount,
					(const float **) normalArrays,
					0, 
					NULL, /* no colours */
					uvCount,
					(const float **) texCoordArrays);

		glUnbind(dummyPath);

		normalArrays[0] = NULL;
		normalArrays[1] = NULL;
		delete[] normalArrays;
		texCoordArrays[0] = NULL;
		delete[] texCoordArrays;

		// Read pixels back from swatch context to MImage
		// ==============================================
		pRenderer->readSwatchContextPixels(backEndStr, outImage);

		// Double check the outing going image size as image resizing
		// was required to properly read from the swatch context
		outImage.getSize(width, height);
		if (width != origWidth || height != origHeight) status = MStatus::kFailure;
		else status = MStatus::kSuccess;

		pRenderer->dereferenceGeometry(pGeomData, numberOfData);
	}
	return status;
}
#endif // MAYA_API_VERSION >= 700


bool CFXShaderNode::setInternalValueInContext(const MPlug& plug, const MDataHandle& handle, MDGContext&)
// set internal values
// vpfile
{
	ENTRYPOINT(setInternalValueInContext);

	if (plug == aVertexShader) { vpfile = handle.asString(); }
	else if (plug == aVertexShaderEntry) { vpEntry = handle.asString(); }
	else if (plug == aFragmentShader) { fpfile = handle.asString(); }
	else if (plug == aFragmentShaderEntry) { fpEntry = handle.asString(); }
	else if (plug == aColorSet0) channelNames[8] = handle.asString();
	else if (plug == aTex0) channelNames[0] = handle.asString();
	else if (plug == aTex1) channelNames[1] = handle.asString();
	else if (plug == aTex2) channelNames[2] = handle.asString();
	else if (plug == aTex3) channelNames[3] = handle.asString();
	else if (plug == aTex4) channelNames[4] = handle.asString();
	else if (plug == aTex5) channelNames[5] = handle.asString();
	else if (plug == aTex6) channelNames[6] = handle.asString();
	else if (plug == aTex7) channelNames[7] = handle.asString();
	else if (plug == aDynRenderStates)
	{
		MString namesList = handle.asString();
		StringList splitNames;
		FUStringConversion::ToStringList(namesList.asChar(),splitNames);

		// add the render states saved by the user.
		for (StringList::iterator it = splitNames.begin(); it != splitNames.end(); ++it)
		{
			fm::string& name = (*it);
			uint index = CFXRenderState::GetIndexFromIndexedName(name);
			FUDaePassState::State type = FUDaePassState::FromString(name);
			addRenderState(type,index);
		}
	}
	else if (plug == aTechTarget) 
		mTechTarget = (uint32)handle.asLong();
	else if (plug == aPassTarget) 
		mPassTarget = (uint32)handle.asLong();
	else return false;
	
	return true;
}

bool CFXShaderNode::getInternalValueInContext(const MPlug& plug, MDataHandle& handle, MDGContext&)
// get and print internal values
//
{
	ENTRYPOINT(getInternalValueInContext);

	if (plug == aVertexShader) handle.set(vpfile);
	else if (plug == aVertexShaderEntry) handle.set(vpEntry);
	else if (plug == aFragmentShader) handle.set(fpfile);
	else if (plug == aFragmentShaderEntry) handle.set(fpEntry);
	else if (plug == aColorSet0) handle.set(channelNames[8]);
	else if (plug == aTex0) handle.set(channelNames[0]);
	else if (plug == aTex1) handle.set(channelNames[1]);
	else if (plug == aTex2) handle.set(channelNames[2]);
	else if (plug == aTex3) handle.set(channelNames[3]);
	else if (plug == aTex4) handle.set(channelNames[4]);
	else if (plug == aTex5) handle.set(channelNames[5]);
	else if (plug == aTex6) handle.set(channelNames[6]);
	else if (plug == aTex7) handle.set(channelNames[7]);
	else if (plug == aRenderStates)
	{
		// getAttr called on the "renderStates" attribute
		// build a sorted MStringArray for each render state assigned
		// the user should use theses strings to query the -renderStateInfoUI command.
		StringList stateNames;
		for (CFXRenderStateList::iterator it = renderStates.begin(); it != renderStates.end(); it++)
		{
			const CFXRenderState* rs = *it;
			if (rs == NULL) continue;
			if (rs->IsDead()) continue;
			const char* stateName = rs->GetColladaStateName();
			if (stateName == NULL) continue;

			fm::string name(stateName);
			if ((*it)->IsIndexed())
			{
                name += "-";
				name += FUStringConversion::ToString((*it)->GetIndex());
			}

			stateNames.push_back(name);
		}
		fm::comparator<fm::string> comp;
		stateNames.sort(comp);
		//std::sort(stateNames.begin(), stateNames.end());

		MStringArray result;
		for (StringList::iterator it = stateNames.begin(); it != stateNames.end(); it++)
		{
			result.append((*it).c_str());
		}

		MFnStringArrayData nameData;
		MObject objData = nameData.create(result);
		handle.set(objData);
	}
	else if (plug == aDynRenderStates)
	{
		// the only reason this attribute exists is because string arrays are not
		// supported correctly (it works in MEL (getAttr)), but the API simply refuses
		// to handle MFnStringArrayData (file read error...).
		MString rsNames("");

		for (CFXRenderStateList::iterator it = renderStates.begin(); it != renderStates.end(); ++it)
		{
			if ((*it)->IsDead()) continue;
			rsNames += MString(" ") + (*it)->GetIndexedName();
		}
		handle.set(rsNames);
	}
	else if (plug == aTechTarget) handle.set((int)mTechTarget);
	else if (plug == aPassTarget) handle.set((int)mPassTarget);
	else return false;

	return true;
}

int CFXShaderNode::texCoordsPerVertex ()
{
	ENTRYPOINT(texCoordsPerVertex);

	int returnVal = 0;
	
#if MAYA_API_VERSION >= 700
	const MDagPath& path = currentPath();
	if (path.hasFn(MFn::kMesh)) 
	{
		MFnMesh meshf(path.node());
		returnVal = meshf.numUVSets();
	}
#else
	returnVal = 1;
#endif

	return returnVal;
}

int  CFXShaderNode::colorsPerVertex ()
{
	ENTRYPOINT(colorsPerVertex);
	int returnVal = 0;
	
#if MAYA_API_VERSION >= 700
	const MDagPath& path = currentPath();
	if (path.hasFn(MFn::kMesh)) 
	{
		MFnMesh meshf(path.node());
		returnVal = meshf.numColorSets();
	}
#endif

	return returnVal;
}

MStatus CFXShaderNode::setDependentsDirty (const MPlug &plugBeingDirtied, MPlugArray &affectedPlugs) 
{	
	ENTRYPOINT(setDependentsDirty);
	MObject thisNode = thisMObject();
	MPlug pB(thisNode, CFXShaderNode::outColor);
	
	MFnDependencyNode fnode(thisMObject());

	// Check for the attribute within the effect parameters.
	for (CFXParameterList::iterator it= effectParameters.begin(); it != effectParameters.end();++it)
	{
		if (plugBeingDirtied.attribute() == (*it)->getAttribute() || plugBeingDirtied.parent().attribute() == (*it)->getAttribute()) 
		{
			affectedPlugs.append(pB);
			(*it)->SetDirty();
		}
	}

	// Check for the attribute within the render states
	for (CFXRenderStateList::iterator it= renderStates.begin(); it != renderStates.end();++it)
	{
		if ((*it)->IsDead()) continue;
		if (plugBeingDirtied.attribute() == (*it)->GetAttribute() || plugBeingDirtied.parent().attribute() == (*it)->GetAttribute()) 
		{
			affectedPlugs.append(pB);
			(*it)->SetDirtyFlag();
		}
	}

	// The general-purpose "dirty" plug.
	if (plugBeingDirtied.attribute() == aDirtyAttrib)
	{
		affectedPlugs.append(pB);
	}

	return MS::kSuccess;
}

CFXParameter* CFXShaderNode::FindEffectParameter(const char* name)
{
	for (CFXParameterList::iterator it= effectParameters.begin(); it != effectParameters.end();++it) 
	{
		if ((*it)->getProgramEntry() == name || (*it)->getAttributeName() == name) return *it;
	}
	return NULL;
}

CFXParameter* CFXShaderNode::FindEffectParameterBySemantic(const char* _semantic)
{
	for (CFXParameterList::iterator it = effectParameters.begin(); it != effectParameters.end();++it) 
	{
		const char* parameterSemantic = (*it)->getSemanticString();
		if (parameterSemantic != NULL && *parameterSemantic != 0 && IsEquivalent(parameterSemantic, _semantic)) return (*it);
	}
	return NULL;
}

CFXRenderState* CFXShaderNode::FindRenderState(uint32 type)
{
	for (CFXRenderStateList::iterator it = renderStates.begin(); it != renderStates.end(); ++it)
	{
		CFXRenderState* state = (*it);
		if (!state->IsDead() && state->GetType() == type) return state;
	}
	return NULL;
}


void CFXShaderNode::setVertexProgramFilename(const char* filename, const char* entry)
{
	if (isCgFX())
	{
		// from CgFX to plain Cg :
		// Put some default entry points
		vpEntry = DEFAULT_VERTEX_ENTRY;
		fpEntry = DEFAULT_FRAGMENT_ENTRY;

		// reset fragment program
		fpfile = "";
		clearFragmentProgram();

		// clear CgFX related members
		clearEffect();
		mTechTarget = 0;
		mPassTarget = 0;

		// revert names
		mTechniqueName = DEFAULT_TECHNIQUE_NAME;
		mPassName = DEFAULT_PASS_NAME;
	}
	else
	{
		vpEntry = entry;
	}

	clearVertexProgram();
	
	vpfile = filename;
	if (vpfile.length() == 0) return;

	vertexProfile = cgGLGetLatestProfile(CG_GL_VERTEX);
	CGprogram prog = compileProgram(vpfile.asChar(), vpEntry.asChar(), vertexProfile);
	if (prog == NULL) return;

	vertexProgram = prog;

	loadParameters();
	loadRenderStates();
}

void CFXShaderNode::setFragmentProgramFilename(const char* filename, const char* entry)
{
	if (isCgFX())
	{
		// from CgFX to plain Cg :
		// Put some default entry points
		vpEntry = DEFAULT_VERTEX_ENTRY;
		fpEntry = DEFAULT_FRAGMENT_ENTRY;

		// reset fragment program
		vpfile = "";
		clearVertexProgram();

		// clear CgFX related members
		clearEffect();
		mTechTarget = 0;
		mPassTarget = 0;

		// revert names
		mTechniqueName = DEFAULT_TECHNIQUE_NAME;
		mPassName = DEFAULT_PASS_NAME;
	}
	else
	{
		fpEntry = entry;
	}

	clearFragmentProgram();
	fpfile = filename;
	if (fpfile.length() == 0) return;

	fragmentProfile = cgGLGetLatestProfile(CG_GL_FRAGMENT);
	CGprogram prog = compileProgram(fpfile.asChar(), fpEntry.asChar(), fragmentProfile);
	if (prog == NULL) return;

	fragmentProgram = prog;

	loadParameters();
	loadRenderStates();
}

void CFXShaderNode::setEffectFilename(const char* filename, bool reloadRenderStates)
{	
	if (filename == NULL || filename[0] == '\0') return;

	if (!isCgFX())
	{
		// from Cg to CgFX:
		vpEntry = DEFAULT_VERTEX_EFFECT_ENTRY;
		fpEntry = DEFAULT_FRAGMENT_EFFECT_ENTRY;
	}

	vpfile = filename;
	clearVertexProgram();

	fpfile = filename;
	clearFragmentProgram();

	// clear names 
	mTechniqueName = DEFAULT_TECHNIQUE_NAME;
	mPassName = DEFAULT_PASS_NAME;

	// Compile the CgFX file
	effect = CreateEffectFromFile(context, filename);
	if (effect == NULL)
	{
		const char* error = cgGetLastListing(context);
		MGlobal::displayError(MString("CgFX compilation errors for file: ") + MString(filename) + MString("\n") + MString(error));
		return;
	}
	MGlobal::displayInfo(MString("CgFX compilation successful for file: ") + MString(filename));

	// Read in the vertex/fragment programs from this effect
	CGtechnique technique = cgGetFirstTechnique(effect);
	CGpass pass = NULL;
	uint32 techIndex = 0, passIndex = 0;
	while(technique != NULL)
	{
		if (techIndex == mTechTarget)
		{
			// use this one
			break;
		}
		if (cgGetNextTechnique(technique) == NULL)
		{
			mTechTarget = techIndex;
			break; // use the last one
		}
		technique = cgGetNextTechnique(technique); // continue looking
		++ techIndex;
	}

	if (technique != NULL)
	{
		pass = cgGetFirstPass(technique);
		while(pass != NULL)
		{
			if (passIndex == mPassTarget)
			{
				// use this one
				break;
			}
			if (cgGetNextPass(pass) == NULL)
			{
				// use the last one
				mPassTarget = passIndex;
				break;
			}
			pass = cgGetNextPass(pass);
			++ passIndex;
		}
	}

	if (technique != NULL && pass != NULL)
	{
		// Get the VertexProgram
		CGstateassignment assign = cgGetNamedStateAssignment(pass, vpEntry.asChar());
		CGprogram prog = cgGetProgramStateAssignmentValue(assign);

		vertexProfile = cgGetProfile(cgGetProgramString(prog, CG_PROGRAM_PROFILE));
		if (!cgGLIsProfileSupported(vertexProfile))
			vertexProfile = cgGLGetLatestProfile(CG_GL_VERTEX);
		vertexProgram = cgCreateProgramFromEffect(effect, vertexProfile, cgGetProgramString(prog, CG_PROGRAM_ENTRY), NULL);

		// Get the FragmentProgram
		assign = cgGetNamedStateAssignment(pass, fpEntry.asChar());
		prog = cgGetProgramStateAssignmentValue(assign);

		fragmentProfile = cgGetProfile(cgGetProgramString(prog, CG_PROGRAM_PROFILE));
		if (!cgGLIsProfileSupported(fragmentProfile))
			fragmentProfile = cgGLGetLatestProfile(CG_GL_FRAGMENT);
		fragmentProgram = cgCreateProgramFromEffect(effect, fragmentProfile, cgGetProgramString(prog, CG_PROGRAM_ENTRY), NULL);

		// set names
		const char* techName = cgGetTechniqueName(technique);
		mTechniqueName = techName ? techName : "technique";
		const char* passName = cgGetPassName(pass);
		mPassName = passName ? passName : "pass";
	}

	// Re/load the parameters & render states
	loadParameters();
	if (reloadRenderStates)
		loadRenderStates();
}

const char* CFXShaderNode::getVertexEntry()
{
	if (vertexProgram)
	{
		return cgGetProgramString(vertexProgram, CG_PROGRAM_ENTRY);
	}
	else
	{
		return "";
	}
}

const char* CFXShaderNode::getFragmentEntry()
{
	if (fragmentProgram)
	{
		return cgGetProgramString(fragmentProgram, CG_PROGRAM_ENTRY);
	}
	else
	{
		return "";
	}
}

void CFXShaderNode::loadParameters()
{
	MFnDependencyNode nodeFn(thisMObject());
	
	// Iterate over all the CgFX parameters: to expose these parameters
	CFXParameterList deleteParameters = effectParameters;
	CFXParameterList addParameters;

	if (isCgFX())
	{
		int textureCount = 0;
		CGparameter param = cgGetFirstEffectParameter(effect);
		while (param != NULL)
		{
			CFXParameter::Type paramType = CFXParameter::convertCGtype(cgGetParameterType(param));
			if (paramType != CFXParameter::kUnknown && paramType != CFXParameter::kStruct && cgGetParameterVariability(param) == CG_UNIFORM)
			{
				// ignore all existing parameters when appending to new parameter list
				bool isNew = false;
				const char* paramName = cgGetParameterName(param);
				CFXParameter* attribute = FindEffectParameter(paramName);
				if (attribute != NULL)
				{
					attribute->overrideParameter(param);
					deleteParameters.erase(attribute);
				}
				else
				{
					attribute = new CFXParameter(this);
					isNew = attribute->create(param);
					effectParameters.push_back(attribute);
				}

				//[glaforte] not sure how that works for effects
				if (attribute->isTexture())
				{
					attribute->setTextureOffset(textureCount++);
					attribute->SetDirty();
				}

				// Look for default values and known annotations
				if (isNew)
				{
					// Look for parameter UI name
					CGannotation annName = cgGetNamedParameterAnnotation(param, CFX_ANNOTATION_UINAME);
					if (annName)
					{
						attribute->setUIName(cgGetStringAnnotationValue(annName));
					}

					switch (attribute->getType())
					{
					case CFXParameter::kFloat:
					case CFXParameter::kHalf: {

						// get default value
						float fval;
						if (cgGetParameterValuefc(param, 1, &fval))
						{
							attribute->setDefaultValue(fval);
							attribute->getPlug().setValue(fval);
						}

						// get min max
						CGannotation annmn = cgGetNamedParameterAnnotation(param, CFX_ANNOTATION_UIMIN);
						CGannotation annmx = cgGetNamedParameterAnnotation(param, CFX_ANNOTATION_UIMAX);
						if (annmn && annmx)
						{
							int nvals;
							float minval = 0.0f;
							float maxval = 1.0f;
							const float* fvalmn = cgGetFloatAnnotationValues(annmn, &nvals);
							if (nvals > 0) minval = fvalmn[0];
							const float* fvalmx = cgGetFloatAnnotationValues(annmx, &nvals);
							if (nvals > 0) maxval = fvalmx[0];
							attribute->setBounds(minval, maxval);
						}
						break; }

					case CFXParameter::kFloat3:
					case CFXParameter::kHalf3: {
						// get default value
						float fval[3];
						if (cgGetParameterValuefc(param, 3, fval))
						{
							attribute->setDefaultValue(fval[0], fval[1], fval[2]);
							MPlug p = attribute->getPlug();
							CDagHelper::SetPlugValue(p, MColor(fval));
						}
						break; }

					case CFXParameter::kFloat4:
					case CFXParameter::kHalf4: {
						// get default value
						float fval[4];
						if (cgGetParameterValuefc(param, 4, fval))
						{
							attribute->setDefaultValue(fval[0], fval[1], fval[2]);
							MPlug p = attribute->getPlug();
							CDagHelper::SetPlugValue(p, MColor(fval));
						}
						break; }
					}

					addParameters.push_back(attribute);
				}
			}

			param = cgGetNextParameter(param);
		}

		// connect effect parameters to program parameters
		connectParameters(vertexProgram, false);
		connectParameters(fragmentProgram, true);
	}
	else
	{
		for (int i = 0; i < 2; ++i)
		{
			CGprogram program = (i == 0) ? vertexProgram : fragmentProgram;
			if (program == NULL)
				continue;

			CFXParameter::cxBindTarget bindTarget = (i == 0) ? CFXParameter::kVERTEX : CFXParameter::kFRAGMENT;
			int textureCount = 0;

			CGparameter param = cgGetFirstParameter(program, CG_PROGRAM);
			while (param != NULL)
			{
				if (cgGetNumConnectedToParameters(param) == 0)
				{
					CFXParameter::Type paramType = CFXParameter::convertCGtype(cgGetParameterType(param));
					if (paramType != CFXParameter::kUnknown && paramType != CFXParameter::kStruct && cgGetParameterVariability(param) == CG_UNIFORM)
					{
						// ignore all existing parameters when appending to new parameter list
						const char* paramName = cgGetParameterName(param);
						CFXParameter* attribute = FindEffectParameter(paramName);
						if (attribute != NULL)
						{
							attribute->overrideParameter(param);
							deleteParameters.erase(attribute);
						}
						else
						{
							attribute = new CFXParameter(this);
							bool isNew = attribute->create(param);
							effectParameters.push_back(attribute);
							if (isNew) addParameters.push_back(attribute);
						}

						if (attribute->isTexture())
						{
							attribute->setTextureOffset(textureCount++);
							attribute->SetDirty();
						}
						attribute->setBindTarget(bindTarget);
					}
				}
				param = cgGetNextParameter(param);
			}
		}
	} // end isCgFX
	
	// Remove the old attributes
	for (CFXParameterList::iterator it = deleteParameters.begin(); it != deleteParameters.end(); ++it)
	{
		CFXParameter* attribute = (*it);
		effectParameters.erase(attribute);
		attribute->removeAttribute();
		SAFE_DELETE(attribute);
	}
	deleteParameters.clear();

	// Add the new attributes
	for (CFXParameterList::iterator it = addParameters.begin(); it != addParameters.end(); ++it)
	{
		CFXParameter* attribute = (*it);
		attribute->addAttribute();
	}
	addParameters.clear();

	isLoaded = true;
}

void CFXShaderNode::loadRenderStates()
{
	CLEAR_POINTER_VECTOR(renderStates);

	if (isCgFX())
	{
		// get the right pass
		CGtechnique technique = cgGetFirstTechnique(effect);
		CGpass pass = NULL;
		bool found = false;
		int tIndex = 0;
		while(technique != NULL)
		{
			if (tIndex == mTechTarget)
			{
				pass = cgGetFirstPass(technique);
				int pIndex = 0;
				while(pass != NULL)
				{
					if (pIndex == mPassTarget)
					{
						found = true;
						break; // out of inner while loop
					}
					pass = cgGetNextPass(pass);
					++ pIndex;
				}
				break; // out of outer while loop
			}
			technique = cgGetNextTechnique(technique);
			++ tIndex;
		}

		if (!found)
			return;

		_CGstateassignment* assignment = cgGetFirstStateAssignment(pass);

		while (assignment != NULL)
		{
			// Create a render state structure to hold this CgFX state assignment.
			CFXRenderState* state = new CFXRenderState(this);
			if (state->Initialize(assignment))
			{
				renderStates.push_back(state);
			}
			else
			{
				// Not a valid state.
				SAFE_DELETE(state);
			}
			assignment = cgGetNextStateAssignment(assignment);
		}
	}
}

CGprogram CFXShaderNode::compileProgram(const char* filename, const char* entry, CGprofile profile)
{
	CGprogram prog = CreateProgramFromFile(context, filename, profile, entry);
	if (prog == NULL)
	{
		const char* error = cgGetLastListing(context);
		MGlobal::displayError(MString("CG compilation errors for file:") + MString(filename) + MString("\n") + MString(error));
	}
	else
	{
		MGlobal::displayInfo(MString("CG compilation successful for file:") + MString(filename));
	}
	return prog;
}

static void findParams(CGparameter parameter, CGprogram program, fm::vector<CGparameter> &parameters)
{
	if (program == cgGetParameterProgram(parameter))
	{
		// one CGparameter instance belongs to only one CGprogram
		if (cgIsParameterReferenced(parameter))
		{
			// the parameter is used by the program, keep it
			parameters.push_back(parameter);
		}
	}

	int paramCount = cgGetNumConnectedToParameters(parameter);
	if (paramCount)
	{
		for (int i = 0; i < paramCount; ++i)
		{
			// recursively look for matches in connected parameters
			findParams(cgGetConnectedToParameter(parameter, i), program, parameters);
		}
	}
}
		
void CFXShaderNode::connectParameters(CGprogram prog, bool isFrag)
{
	if (!prog) return;

	for (CFXParameterList::iterator it = effectParameters.begin(); it != effectParameters.end(); ++it)
	{
		CGparameter currentParam = (*it)->getCGParameter();
		fm::vector<CGparameter> referencedList;
		findParams(currentParam,prog,referencedList);
		int paramCount = (int)referencedList.size();

		for (int i = 0; i < paramCount; ++i)
		{
			CGparameter param = referencedList[i];

			// create a connection from our parameter to the used Cg parameter
			cgConnectParameter(currentParam, param);
				
			// set program entry
			(*it)->setProgramEntry(cgGetParameterName(param));

			// set bind target
			if (isFrag) // fragment/pixel shader
			{
				switch ((*it)->getBindTarget())
				{
				case CFXParameter::kVERTEX:
				case CFXParameter::kBOTH:
					(*it)->setBindTarget(CFXParameter::kBOTH);
					break;
				case CFXParameter::kNONE:
				case CFXParameter::kFRAGMENT:
				default:
					(*it)->setBindTarget(CFXParameter::kFRAGMENT);
					break;
				}
			}
			else // vertex shader
			{
				switch ((*it)->getBindTarget())
				{
				case CFXParameter::kFRAGMENT:
				case CFXParameter::kBOTH:
					(*it)->setBindTarget(CFXParameter::kBOTH);
					break;
				case CFXParameter::kNONE:
				case CFXParameter::kVERTEX:
				default:
					(*it)->setBindTarget(CFXParameter::kVERTEX);
					break;
				}
			}
		}
	}	
}

CFXParameterList CFXShaderNode::collectUIParameters()
{
	CFXParameterList uiParameters;
	for (CFXParameterList::iterator it = effectParameters.begin(); it != effectParameters.end(); ++it)
	{
		CFXParameter* parameter = (*it);

		// Check the validity of this parameter for the UI.
		if (parameter->getSemantic() != CFXParameter::kUNKNOWN && 
			parameter->getSemantic() != CFXParameter::kEMPTY) 
			continue;

		const fchar* name = parameter->getName();
		if (fstrlen(name) > 2 && name[0] == '$' && name[1] == 'C') continue; // skip the strange '$C[0-9*]' parameters.

		// Order the parameters by semantic, by type and by name.
		const char* semantic = parameter->getSemanticString();
		if (semantic == NULL) semantic = "";
		int type = (int) parameter->getType();
		size_t currentParameterCount = uiParameters.size();
		size_t orderedIndex = 0;
		for (; orderedIndex < currentParameterCount; ++orderedIndex)
		{
			const char* otherSemantic = uiParameters[orderedIndex]->getSemanticString();
			if (otherSemantic == NULL) otherSemantic = "";

			// Sort by semantic first.
			if (*semantic == 0 && *otherSemantic != 0) continue;
			else if (*semantic != 0 && *otherSemantic == 0) break;
			int comparison = strcmp(semantic, otherSemantic);
			if (comparison > 0) continue;
			else if (comparison < 0) break;

			// Sort by type second
			int otherType = (int) uiParameters[orderedIndex]->getType();
			if (type < otherType) break;
			else if (type > otherType) continue;

			// Finally, sort by name
			if (fstrcmp(name, uiParameters[orderedIndex]->getName()) < 0) break;
		}

		// Collect this parameter
		uiParameters.insert(uiParameters.begin() + orderedIndex, parameter);
	}
	return uiParameters;
}

uint CFXShaderNode::getUIParameterCount()
{
	return (uint) collectUIParameters().size();
}

MStringArray CFXShaderNode::getUIParameterInfo(uint index)
{
	MStringArray infos;
	CFXParameterList uiParameters = collectUIParameters();
	if (index < uiParameters.size())
	{
		CFXParameter* parameter = uiParameters[index];
		infos.append(parameter->getName());
		infos.append(parameter->getTypeString());
		infos.append(parameter->getSemanticString());
		infos.append(parameter->getUIName());
	}
	return infos;
}

void CFXShaderNode::editParamMinMax(const fchar* paramName, double min, double max)
{
	for (CFXParameterList::iterator it = effectParameters.begin(); it != effectParameters.end(); ++it)
	{
		CFXParameter* param = (*it);
		if (!IsEquivalent(param->getName(), paramName))
			continue;

		if (param->getAttribute().hasFn(MFn::kNumericAttribute))
		{
			param->setBounds((float)min,(float)max);
		}
		break;
	}
}

MStringArray CFXShaderNode::getAvailableRenderStates()
{
	MStringArray result;
	for (uint32 i = 0; i < CFXRenderState::getTotalRenderStateCount(); i++)
	{
		FUDaePassState::State state = CFXRenderState::CG_RENDER_STATES_XREF[i];
		if (state != FUDaePassState::INVALID && !CFXRenderState::IsStateIndexed(state))
			result.append(CFXRenderState::CG_RENDER_STATE_NAMES[i]);
	}
	return result;
}

MStringArray CFXShaderNode::getAvailableIndexedRenderStates()
{
	MStringArray result;
	for (uint32 i = 0; i < CFXRenderState::getTotalRenderStateCount(); i++)
	{
		FUDaePassState::State state = CFXRenderState::CG_RENDER_STATES_XREF[i];
		if (CFXRenderState::IsStateIndexed(state))
			result.append(CFXRenderState::CG_RENDER_STATE_NAMES[i]);
	}
	return result;
}

bool CFXShaderNode::addRenderState(const char* cgStateName, int index /* = -1*/)
{
	FUDaePassState::State stateType = CFXRenderState::CgToColladaTranslation(cgStateName);
	return addRenderState(stateType,index);
}

bool CFXShaderNode::addRenderState(FUDaePassState::State stateType, int index /* = -1*/)
{
	if (stateType == FUDaePassState::INVALID) return false;

	// can't add a state if already present
	for (CFXRenderStateList::iterator it = renderStates.begin(); it != renderStates.end(); it++)
	{
		if ((*it)->GetType() == stateType &&
			(*it)->GetIndex() == index) 
			return false;
	}

	// create the new state and push it at the end of the list
	CFXRenderState* state = new CFXRenderState(this);
	state->SetIndex(index);
	state->Initialize(stateType);
	renderStates.push_back(state);
	refresh();

	return true;
}

bool CFXShaderNode::removeRenderState(const char* cgStateName, int index /* = -1*/)
{
	FUDaePassState::State stateType = CFXRenderState::CgToColladaTranslation(cgStateName);
	if (stateType == FUDaePassState::INVALID) return false;

	for (CFXRenderStateList::iterator it = renderStates.begin(); it != renderStates.end(); it++)
	{
		if ((*it)->GetType() == stateType &&
			(*it)->GetIndex() == index)
		{
			(*it)->SetDeadFlag();
			refresh();
			return true;
		}
	}

	return false;
}

MString CFXShaderNode::getRenderStateUI(const char* colladaStateName, int layout)
{
	fm::string indexedName(colladaStateName);
	uint index = CFXRenderState::GetIndexFromIndexedName(indexedName);

	FUDaePassState::State stateType = FUDaePassState::FromString(indexedName.c_str());

	for (CFXRenderStateList::iterator it = renderStates.begin(); it != renderStates.end(); ++it)
	{
		if ((*it)->GetType() == stateType && (*it)->GetIndex() == index)
		{
			FUStringBuilder builder;
			(*it)->GenerateUICommand(this, builder);
			return MString(builder.ToCharPtr());
		}
	}
	
	return "";
}

void CFXShaderNode::refresh()
{
	MPlug dirtyPlug(thisMObject(),aDirtyAttrib);
	bool val = false; dirtyPlug.getValue(val);
	dirtyPlug.setValue(!val); // after this call, Maya will call its refresh methods.
}

void CFXShaderNode::setEffectTargets(uint techniqueTarget,uint passTarget)
{
	mTechTarget = techniqueTarget; mPassTarget = passTarget;
	MPlug tta(thisMObject(), aTechTarget);
	tta.setValue((int)mTechTarget);
	MPlug pta(thisMObject(), aPassTarget);
	pta.setValue((int)mPassTarget);
}
