/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "ColladaEffectShader.h"
#include "ColladaEffectPass.h"
#include "ColladaEffectParameter.h"
#include "ColladaCgWrapper.h"

///////////////////////////////////////////////////////////////////////////////
// CLASS DESCRIPTION
///////////////////////////////////////////////////////////////////////////////

class ColladaEffectShaderClassDesc : public ClassDesc2
{
public:
	int				IsPublic() {return FALSE; }
	void*			Create(BOOL isLoading)
	{
		return new ColladaEffectShader(isLoading, NULL);
	}

	const TCHAR*	ClassName() {return _T("ColladaEffectShader");}
	SClass_ID		SuperClassID() {return REF_TARGET_CLASS_ID;}
	Class_ID 		ClassID() {return COLLADA_EFFECT_SHADER_ID;}
	const TCHAR* 	Category() {return _T("");}
	const TCHAR*	InternalName() { return _T("ColladaEffectShader"); }
	HINSTANCE		HInstance() { return hInstance; }
};

static ColladaEffectShaderClassDesc gColladaEffectShaderCD;
ClassDesc2* GetColladaEffectShaderClassDesc()
{
	return &gColladaEffectShaderCD;
}

///////////////////////////////////////////////////////////////////////////////
// CLASS IMPLEMENTATION
///////////////////////////////////////////////////////////////////////////////

ColladaEffectShaderInfo::ColladaEffectShaderInfo()
:	filename("UNKNOWN")
,	entry("")
,	cgProgram(NULL)
,	isVertex(true)
,	isEffect(false)
,	cgEffect(NULL)
,	techniqueIndex(0)
,	passIndex(0)
,	cgVertexProgram(NULL)
,	cgFragmentProgram(NULL)
{}

ColladaEffectShader::ColladaEffectShader(BOOL UNUSED(isLoading), ColladaEffectPass* parentP)
:	mParent(parentP)
,	mInfo()
,	mErrorString("")
,	mIsValid(false)
,	mParameters()
{
	
}

ColladaEffectShader::~ColladaEffectShader()
{
	CLEAR_POINTER_VECTOR(mParameters);
}

static void safeDestroyProgram(CGprogram& prog)
{
	if (prog)
	{
		cgDestroyProgram(prog);
		DebugCG();
		prog = NULL;
	}
}

static void safeDestroyEffect(CGeffect& effect)
{
	if (effect)
	{
		cgDestroyEffect(effect);
		DebugCG();
		effect = NULL;
	}
}

bool ColladaEffectShader::compileCg()
{
	FUAssert(!mInfo.isEffect, return false);

	safeDestroyProgram(mInfo.cgProgram);

	// use the latest profile available
	CGprofile profile = mInfo.isVertex 
		? cgD3D9GetLatestVertexProfile() : cgD3D9GetLatestPixelProfile();
	DebugCG();

	// with the best compiler options
	const char **options[] =
	{
		cgD3D9GetOptimalOptions(profile),
		NULL,
	};
	DebugCG();
	
	// to create the program
	CGcontext context = mParent->getCgContext();

	mInfo.cgProgram = scg::CreateProgramFromFile(
		context, 
		CG_SOURCE, 
		mInfo.filename.c_str(), 
		profile, 
		mInfo.entry.c_str(),
		*options);

	DebugCGC(context);

	if (mInfo.cgProgram == NULL)
	{
		mErrorString = FS(cgGetLastListing(context));
		cgGetError(); // invalidate the error
		mIsValid = false;
		return false;
	}
	else
	{
		HRESULT hr = cgD3D9LoadProgram(mInfo.cgProgram, 0, 0);
		(void)hr;
		DebugCG();
	}

	mErrorString.clear();
	mIsValid = true;
	return true;
}

bool ColladaEffectShader::compileCgFX()
{
	FUAssert(mInfo.isEffect, return false);
	safeDestroyEffect(mInfo.cgEffect);

	// profile and options are given in the effect file
	CGcontext context = mParent->getCgContext();
	
	mInfo.cgEffect = scg::CreateEffectFromFile(
		context, 
		mInfo.filename.c_str(), 
		NULL);

	DebugCGC(context);
	//DebugCG();

	if (mInfo.cgEffect == NULL)
	{
		mErrorString = FS(cgGetLastListing(context));
		cgGetError(); // invalidate the error
		mIsValid = false;
		return false;
	}

	safeDestroyProgram(mInfo.cgVertexProgram);
	safeDestroyProgram(mInfo.cgFragmentProgram);

	// heuristic: if the given technique and pass targets are of greater numbers than
	// what the CgFx shader actually holds, use the nearest numbers available
	CGtechnique technique = cgGetFirstTechnique(mInfo.cgEffect);
	CGpass pass = NULL;
	uint32 techIndex = 0, passIndex = 0;
	while(technique != NULL)
	{
		if (techIndex == mInfo.techniqueIndex)
		{
			// use this one
			break;
		}
		if (cgGetNextTechnique(technique) == NULL)
		{
			mInfo.techniqueIndex = techIndex;
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
			if (passIndex == mInfo.passIndex)
			{
				// use this one
				break;
			}
			if (cgGetNextPass(pass) == NULL)
			{
				// use the last one
				mInfo.passIndex = passIndex;
				break;
			}
			pass = cgGetNextPass(pass);
			++ passIndex;
		}
	}

	if (technique != NULL && pass != NULL)
	{
		mInfo.techniqueName = TO_FSTRING(cgGetTechniqueName(technique));
		if (mInfo.techniqueName.empty())
			mInfo.techniqueName = fstring(FC("technique")) + FUStringConversion::ToFString(techIndex);

		mInfo.passName = TO_FSTRING(cgGetPassName(pass));
		if (mInfo.passName.empty())
			mInfo.passName = fstring(FC("pass")) + FUStringConversion::ToFString(passIndex);

		// Get the VertexProgram
		CGstateassignment assign = cgGetNamedStateAssignment(pass, "VertexProgram"); DebugCG();
		CGprogram prog = cgGetProgramStateAssignmentValue(assign);
		CGprofile vertexProfile = cgGetProfile(cgGetProgramString(prog, CG_PROGRAM_PROFILE));
		if (!cgD3D9IsProfileSupported(vertexProfile))
			vertexProfile = cgD3D9GetLatestVertexProfile();
		mInfo.cgVertexProgram = cgCreateProgramFromEffect(mInfo.cgEffect, vertexProfile, cgGetProgramString(prog, CG_PROGRAM_ENTRY), NULL);
		DebugCG();

		// Get the FragmentProgram
		assign = cgGetNamedStateAssignment(pass, "FragmentProgram"); DebugCG();
		prog = cgGetProgramStateAssignmentValue(assign);
		CGprofile fragmentProfile = cgGetProfile(cgGetProgramString(prog, CG_PROGRAM_PROFILE));
		if (!cgD3D9IsProfileSupported(fragmentProfile))
			fragmentProfile = cgD3D9GetLatestPixelProfile();
		mInfo.cgFragmentProgram = cgCreateProgramFromEffect(mInfo.cgEffect, fragmentProfile, cgGetProgramString(prog, CG_PROGRAM_ENTRY), NULL);
		DebugCG();
	}

	if (!mInfo.cgVertexProgram || !mInfo.cgFragmentProgram)
	{
		mErrorString = FS(cgGetLastListing(context));
		safeDestroyProgram(mInfo.cgVertexProgram);
		safeDestroyProgram(mInfo.cgFragmentProgram);
		mIsValid = false;
		return false;
	}
	else
	{
		// cgD3D9LoadProgram(prog, bUseShadowing, D3DXAssembleShader::flags)
		HRESULT hr = cgD3D9LoadProgram(mInfo.cgVertexProgram, 0, 0);
		DebugCG();
		hr = cgD3D9LoadProgram(mInfo.cgFragmentProgram, 0, 0);
		DebugCG();
	}

	mErrorString.clear();
	mIsValid = true;
	return true;
}

bool ColladaEffectShader::initialize()
{
	CLEAR_POINTER_VECTOR(mParameters);
	if (compile())
	{
		return loadParameters();
	}
	return false;
}

bool ColladaEffectShader::compile()
{
	// TODO. Add other formats here
	if (mInfo.isEffect) return compileCgFX();
	else return compileCg();
}

bool ColladaEffectShader::loadCgParameters(CGprogram program)
{
	// sanity check: we register parameters on a pass
	FUAssert(mParent != NULL, return false);
	if (program == NULL) return false;

	for (int i = 0; i < 2; ++i)
	{
		CGenum name_space = (i) ? CG_GLOBAL : CG_PROGRAM;
		CGparameter param = cgGetFirstParameter(program, name_space);
		while (param != NULL)
		{
			if (cgGetNumConnectedToParameters(param) == 0)
			{
				ColladaEffectParameter* cfxParam = ColladaEffectParameterFactory::create(this, param);
				if (!cfxParam)
				{
					param = cgGetNextParameter(param);
					continue;
				}
				if (!mParent->addParameter(*cfxParam))
				{
					SAFE_DELETE(cfxParam);
					return false;
				}

				if (mInfo.isVertex)
					cfxParam->addBinding(ColladaEffectParameter::kVERTEX);
				else
					cfxParam->addBinding(ColladaEffectParameter::kFRAGMENT);

				// Parameters from a Cg (not CgFX) are by default expanded by Cg's D3D interface
				cfxParam->addExpandedParameter(param);

				mParameters.push_back(cfxParam);
			}
			param = cgGetNextParameter(param);
		}
	}
	return true;
}

bool ColladaEffectShader::loadCgFXParameters(CGeffect effect)
{
	// sanity check: we register parameters on a pass
	FUAssert(mParent != NULL, return false);
	if (effect == NULL) return false;

	CGparameter param = cgGetFirstEffectParameter(effect);
	while (param != NULL)
	{
		ColladaEffectParameter* cfxParam = ColladaEffectParameterFactory::create(this, param);
		if (!cfxParam)
		{
			param = cgGetNextParameter(param);
			continue;
		}
		if (!mParent->addParameter(*cfxParam))
		{
			SAFE_DELETE(cfxParam);
			return false;
		}
		mParameters.push_back(cfxParam);
		param = cgGetNextParameter(param);
	}
	return true;
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
		
void ColladaEffectShader::connectParameters(CGprogram prog, bool isFrag)
{
	if (!prog) return;

	for (ColladaEffectParameterList::iterator it = mParameters.begin(); it != mParameters.end(); ++it)
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
			(*it)->addExpandedParameter(param);
				
			// set program entry
			(*it)->setProgramEntry(cgGetParameterName(param));

			// add program binding
			(*it)->addBinding(isFrag ? ColladaEffectParameter::kFRAGMENT : ColladaEffectParameter::kVERTEX);
		}
	}
}

bool ColladaEffectShader::loadParameters()
{
	if (mInfo.isEffect)
	{
		bool success = loadCgFXParameters(mInfo.cgEffect);
		if (success)
		{
			connectParameters(mInfo.cgVertexProgram, false);
			connectParameters(mInfo.cgFragmentProgram, true);
		}
		return success;
	}
	else
	{
		return loadCgParameters(mInfo.cgProgram);
	}
	return false;
}

void ColladaEffectShader::prepare()
{
	if (!mIsValid) return;

	// update the parameters
	size_t pCount = mParameters.size();
	for (size_t i = 0; i < pCount; ++i)
	{
		mParameters[i]->update();
		DebugCG();
	}

	if (mInfo.isEffect)
	{
		HRESULT hr = cgD3D9BindProgram(mInfo.cgVertexProgram);
		DebugCG();

		hr = cgD3D9BindProgram(mInfo.cgFragmentProgram);
		DebugCG();
	}
	else
	{
		HRESULT hr = cgD3D9BindProgram(mInfo.cgProgram);
		(void)hr;
		DebugCG();
	}
}

void ColladaEffectShader::setParamBlock(IParamBlock2* pb)
{
	for (size_t i = 0; i < mParameters.size(); ++i)
	{
		mParameters[i]->setParamBlock(pb);
		mParameters[i]->synchronize();
	}
}

#define INFO_FILENAME_CHUNK		5000
#define INFO_ENTRY_CHUNK		5001
#define INFO_FLAGS_CHUNK		5002

#define CHECK_IOR(cmd) {\
	IOResult ret; \
	FUAssert((ret = cmd) == IO_OK, return ret); }

IOResult ColladaEffectShader::Save(ISave* isave)
{
	unsigned long Nb;

	isave->BeginChunk(INFO_FILENAME_CHUNK);
	CHECK_IOR(isave->WriteWString(mInfo.filename.c_str()));
	isave->EndChunk();
	
	isave->BeginChunk(INFO_ENTRY_CHUNK);
	CHECK_IOR(isave->WriteWString(mInfo.entry.c_str()));
	isave->EndChunk();

	isave->BeginChunk(INFO_FLAGS_CHUNK);
	CHECK_IOR(isave->Write(&mInfo.isVertex, sizeof(bool), &Nb));
	CHECK_IOR(isave->Write(&mInfo.isEffect, sizeof(bool), &Nb));
	CHECK_IOR(isave->Write(&mInfo.passIndex, sizeof(int), &Nb));
	CHECK_IOR(isave->Write(&mInfo.techniqueIndex, sizeof(int), &Nb));
	isave->EndChunk();

	CHECK_IOR(ReferenceTarget::Save(isave));

	return IO_OK;
}

IOResult ColladaEffectShader::Load(ILoad* iload)
{
	TCHAR* buf;
	IOResult ret;
	unsigned long Nb;

	while(IO_OK == (ret = iload->OpenChunk())) 
	{
		switch (iload->CurChunkID()) 
		{
		case INFO_FILENAME_CHUNK:
			CHECK_IOR(iload->ReadWStringChunk(&buf));
			mInfo.filename = TO_FSTRING(buf);
			break;
		case INFO_ENTRY_CHUNK:
			CHECK_IOR(iload->ReadWStringChunk(&buf));
			mInfo.entry = TO_FSTRING(buf);
			break;
		case INFO_FLAGS_CHUNK:
			CHECK_IOR(iload->Read(&mInfo.isVertex, sizeof(bool), &Nb));
			CHECK_IOR(iload->Read(&mInfo.isEffect, sizeof(bool), &Nb));
			CHECK_IOR(iload->Read(&mInfo.passIndex, sizeof(int), &Nb));
			CHECK_IOR(iload->Read(&mInfo.techniqueIndex, sizeof(int), &Nb));
			break;
		}
		if (ret != IO_OK)
			return ret;

		iload->CloseChunk();
	}

	// do NOT initialize the shader here, it still needs pass information
	// the initialization is done in the PostLoadCallback of the ColladaEffectPass class

	CHECK_IOR(ReferenceTarget::Load(iload));

	return IO_OK;
}

void ColladaEffectShader::GetEffectInfo(const fstring& fileName, StringList& techNames, UInt32List& passCounts, StringList& passNames, fstring& errorString)
{
	fm::string f = TO_STRING(fileName);
	CGcontext ctx = cgCreateContext();
	cgD3D9RegisterStates(ctx);
	DebugCG();

	CGeffect effect = scg::CreateEffectFromFile(ctx, f.c_str(), NULL);
	if (effect == NULL)
	{
		errorString = FS(cgGetLastListing(ctx));
		cgGetError(); // invalidate the error
		return;
	}

	CGtechnique tech = cgGetFirstTechnique(effect);
	uint32 currTech = 0;
	while(tech != NULL)
	{
		// retrieve name
		const char* techName = cgGetTechniqueName(tech);
		fm::string techNameStr = techName;
		if (techNameStr.empty()) techNameStr = "tech" + FUStringConversion::ToString(currTech++);
		techNames.push_back(techNameStr);

		// retrive number of passes
		passCounts.push_back(0);
		uint32& passCount = passCounts.back();

		// retrieve pass names
		CGpass pass = cgGetFirstPass(tech);
		uint32 currPass = 0;
		while(pass != NULL)
		{
			++ passCount;
			const char* passName = cgGetPassName(pass);
			fm::string passNameStr = passName;
			if (passNameStr.empty()) passNameStr = "pass" + FUStringConversion::ToString(currPass++);
			passNames.push_back(passNameStr);
			pass = cgGetNextPass(pass);
		}
		tech = cgGetNextTechnique(tech);
	}

	cgDestroyEffect(effect);
	cgDestroyContext(ctx);
}
