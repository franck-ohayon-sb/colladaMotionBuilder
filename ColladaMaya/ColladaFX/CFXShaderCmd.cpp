/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "ColladaFX/CFXShaderCmd.h"
#include "ColladaFX/CFXShaderNode.h"
#include "ColladaFX/CFXPasses.h"
#include "ColladaFX/CFXParameter.h"
#include <maya/MDGModifier.h>
#include <maya/MFnStringArrayData.h>
#include <maya/MSyntax.h>
#include <maya/MArgDatabase.h>
#include "TranslatorHelpers/CDagHelper.h"

CFXShaderCommand::CFXShaderCommand() {}
CFXShaderCommand::~CFXShaderCommand() {}

MStatus CFXShaderCommand::doIt(const MArgList& args)
{ 
	MString shaderName, parameterName, arg1, arg2, arg3;
	MArgDatabase argData(syntax(), args);

	// Cg commands
	if (argData.isFlagSet("-fxt"))
	{
		// FX file name
		argData.getFlagArgument("-fxt",0,arg1);

		CGcontext tempContext = cgCreateContext();
		if (!tempContext) return MS::kFailure;

		cgGLRegisterStates(tempContext);
		CGeffect effect = CFXShaderNode::CreateEffectFromFile(tempContext, arg1.asChar());
		if (!effect)
		{
			const char* error = cgGetLastListing(tempContext);
			MGlobal::displayError(MString("CgFX compilation errors for file: ") + arg1 + MString("\n") + MString(error));
			cgDestroyContext(tempContext);
			return MS::kFailure;
		}

		MStringArray results;

		CGtechnique technique = cgGetFirstTechnique(effect);
		int index = 0;
		while(technique != NULL)
		{
			const char* techName = cgGetTechniqueName(technique);
			MString t;
			if (techName == NULL)
			{
				// provide default name technique##index
				t = MString("technique") + MString(FUStringConversion::ToString(index).c_str());
			}
			else
			{
				t = MString(techName);
			}
			results.append(t);
			technique = cgGetNextTechnique(technique);
			++ index;
		}

		cgDestroyEffect(effect);
		cgDestroyContext(tempContext);

		appendToResult(results);
		return MS::kSuccess;
	}
	else if (argData.isFlagSet("-fxp"))
	{
		// 0-based tech index
		int techIndex = -1;
		argData.getFlagArgument("-fxp",0,techIndex);
		
		// FX file name
		argData.getFlagArgument("-fxp",1,arg1);

		CGcontext tempContext = cgCreateContext();
		if (!tempContext) return MS::kFailure;

		cgGLRegisterStates(tempContext);
		CGeffect effect = CFXShaderNode::CreateEffectFromFile(tempContext, arg1.asChar());
		if (!effect)
		{
			const char* error = cgGetLastListing(tempContext);
			MGlobal::displayError(MString("CgFX compilation errors for file: ") + arg1 + MString("\n") + MString(error));
			cgDestroyContext(tempContext);
			return MS::kFailure;
		}

		MStringArray results;

		CGtechnique technique = cgGetFirstTechnique(effect);
		int index = 0;
		while(technique != NULL)
		{
			if (index == techIndex)
			{
				CGpass pass = cgGetFirstPass(technique);
				MStringArray results;
				int passIndex = 0;
				while(pass != NULL)
				{
					const char* passName = cgGetPassName(pass);
					MString p;
					if (passName == NULL)
					{
						// provide default pass name pass##passIndex
						p = MString("pass") + MString(FUStringConversion::ToString(passIndex).c_str());
					}
					else
					{
						p = MString(passName);
					}
					results.append(p);
					pass = cgGetNextPass(pass);
					++ passIndex;
				}
				
				appendToResult(results);
				break; // out of outer while loop
			}
			technique = cgGetNextTechnique(technique);
			++ index;
		}

		cgDestroyEffect(effect);
		cgDestroyContext(tempContext);

		appendToResult(results);
		return MS::kSuccess;
	}

	// ColladaFX Passes commands
	if (argData.isFlagSet("-ap")) 
	{
		argData.getFlagArgument("-ap", 0, shaderName);
		MFnDependencyNode shaderNode(CDagHelper::GetNode(shaderName));
		if (shaderNode.typeId() != CFXPasses::id) return MStatus::kInvalidParameter;
		CFXPasses* passes = (CFXPasses*) shaderNode.userNode();
		argData.getFlagArgument("-ap", 1, arg1); // fetch the desired pass name
		if (arg1.length() == 0) arg1 = "pass"; // provide default name
		passes->addPass(arg1);
	}
	else if (argData.isFlagSet("-rp")) 
	{
		argData.getFlagArgument("-rp", 0, shaderName);
		MFnDependencyNode shaderNode(CDagHelper::GetNode(shaderName));
		if (shaderNode.typeId() != CFXPasses::id) return MStatus::kInvalidParameter;
		CFXPasses* passes = (CFXPasses*) shaderNode.userNode();
		passes->removeLastPass();
	}
	else if (argData.isFlagSet("-at"))
	{
		// add technique
		argData.getFlagArgument("-at", 0, shaderName);
		MFnDependencyNode shaderNode(CDagHelper::GetNode(shaderName));
		if (shaderNode.typeId() != CFXPasses::id) return MStatus::kInvalidParameter;
		argData.getFlagArgument("-at", 1, arg1);
		CFXPasses* passes = (CFXPasses*) shaderNode.userNode();
		passes->addTechnique(arg1);
	}
	else if (argData.isFlagSet("-rt"))
	{
		// remove current technique
		argData.getFlagArgument("-rt", 0, shaderName);
		MFnDependencyNode shaderNode(CDagHelper::GetNode(shaderName));
		if (shaderNode.typeId() != CFXPasses::id) return MStatus::kInvalidParameter;
		CFXPasses* passes = (CFXPasses*) shaderNode.userNode();
		passes->removeCurrentTechnique();
	}
	else if (argData.isFlagSet("-lfx"))
	{
		// load techniques and passes from a CgFX file
		argData.getFlagArgument("-lfx", 0, shaderName);
		MFnDependencyNode shaderNode(CDagHelper::GetNode(shaderName));
		if (shaderNode.typeId() != CFXPasses::id) return MStatus::kInvalidParameter;
		CFXPasses* passes = (CFXPasses*) shaderNode.userNode();
		argData.getFlagArgument("-lfx", 1, arg1);
		passes->reloadFromCgFX(arg1.asChar());
	}
	else
	{
		// Read the -s argument with the name of the colladaFX shader node
		CFXShaderNode* shader = NULL;
		if (argData.isFlagSet("-s"))
		{
			argData.getFlagArgument("-s", 0, shaderName);
			MFnDependencyNode nodeFn(CDagHelper::GetNode(shaderName));
			shader = (CFXShaderNode*) nodeFn.userNode();
		}
		if (shader == NULL) return MStatus::kInvalidParameter;
		if (argData.isFlagSet("-p")) argData.getFlagArgument("-p", 0, parameterName);

		// ColladaFX Shader node commands

		// Load vertex/fragment programs
		if (argData.isFlagSet("-vp")) 
		{
			argData.getFlagArgument("-vp", 0, arg1);
			shader->setVertexProgramFilename(arg1.asChar(),CFXShaderNode::DEFAULT_VERTEX_ENTRY);
		}
		if (argData.isFlagSet("-fp")) 
		{
			argData.getFlagArgument("-fp", 0, arg1);
			shader->setFragmentProgramFilename(arg1.asChar(),CFXShaderNode::DEFAULT_FRAGMENT_ENTRY);
		}

		// reload commands
		if (argData.isFlagSet("-rlv")) 
		{
			argData.getFlagArgument("-rlv", 0, arg1);
			argData.getFlagArgument("-rlv", 1, arg2);
			shader->setVertexProgramFilename(arg1.asChar(),arg2.asChar());
		}
		if (argData.isFlagSet("-rlf")) 
		{
			argData.getFlagArgument("-rlf", 0, arg1);
			argData.getFlagArgument("-rlf", 1, arg2);
			shader->setFragmentProgramFilename(arg1.asChar(),arg2.asChar());
		}

		// Load/reload a CgFX file
		if (argData.isFlagSet("-lfe")) 
		{
			argData.getFlagArgument("-lfe", 0, arg1);
			shader->setEffectFilename(arg1.asChar());
		}
		if (argData.isFlagSet("-rle")) 
		{
			argData.getFlagArgument("-rle", 0, arg1);
			shader->setEffectFilename(arg1.asChar());
		}

		// set program from CgFX
		if (argData.isFlagSet("-sfx"))
		{
			argData.getFlagArgument("-sfx",0,arg1);
			int tech,pass;
			argData.getFlagArgument("-sfx",1,tech);
			argData.getFlagArgument("-sfx",2,pass);

			shader->setEffectTargets(tech,pass);
			shader->setEffectFilename(arg1.asChar());
		}

		// Set/get the UI type for parameters
		if (argData.isFlagSet("-su")) 
		{
			argData.getFlagArgument("-su", 0, arg1);
			CFXParameter* attribute = shader->FindEffectParameter(parameterName.asChar());
			if (attribute == NULL) return MStatus::kInvalidParameter;
			attribute->setUIType(MConvert::ToFChar(arg1));
		}
		if (argData.isFlagSet("-gu")) 
		{
			argData.getFlagArgument("-gu", 0, arg1);
			CFXParameter* attribute = shader->FindEffectParameter(parameterName.asChar());
			if (attribute == NULL) return MStatus::kInvalidParameter;
			appendToResult(attribute->getUIType());
		}
		
		if (argData.isFlagSet("-es")) 
		{
			argData.getFlagArgument("-es", 0, arg1);
			argData.getFlagArgument("-is", 0, arg2);
			CFXParameter* attribute = shader->FindEffectParameter(arg1.asChar());
			if (attribute == NULL) return MStatus::kInvalidParameter;
			attribute->setSource(arg2.asChar());
			appendToResult(attribute->getSource());
		}
		
		if (argData.isFlagSet("-qs")) 
		{
			argData.getFlagArgument("-qs", 0, arg1);
			CFXParameter* attribute = shader->FindEffectParameter(arg1.asChar());
			if (attribute == NULL) return MStatus::kInvalidParameter;
			appendToResult(attribute->getSource());
		}

		// Parameter information iterators
		if (argData.isFlagSet("-pc"))
		{
			appendToResult((int) shader->getUIParameterCount());
		}

		if (argData.isFlagSet("-pi"))
		{
			uint index;
			argData.getFlagArgument("-pi", 0, index);
			MStringArray infos = shader->getUIParameterInfo(index);
			appendToResult(infos);
		}

		// Render state methods
		if (argData.isFlagSet("-qrs"))
		{
			MStringArray states = shader->getAvailableRenderStates();
			appendToResult(states);
		}

		if (argData.isFlagSet("-qri"))
		{
			MStringArray states = shader->getAvailableIndexedRenderStates();
			appendToResult(states);
		}

		if (argData.isFlagSet("-ars"))
		{
			argData.getFlagArgument("-ars", 0, arg1);
			bool result = shader->addRenderState(arg1.asChar());
			appendToResult(result);
		}

		if (argData.isFlagSet("-ari"))
		{
			argData.getFlagArgument("-ari", 0, arg1);
			uint index = ~0u;
			argData.getFlagArgument("-ari", 1, index);
			bool result = shader->addRenderState(arg1.asChar(), index);
			appendToResult(result);
		}

		if (argData.isFlagSet("-rrs"))
		{
			argData.getFlagArgument("-rrs", 0, arg1);
			bool result = shader->removeRenderState(arg1.asChar());
			appendToResult(result);
		}

		if (argData.isFlagSet("-rri"))
		{
			argData.getFlagArgument("-rri", 0, arg1);
			uint index = ~0u;
			argData.getFlagArgument("-rri", 1, index);
			bool result = shader->removeRenderState(arg1.asChar(),index);
			appendToResult(result);
		}

		if (argData.isFlagSet("-rsi"))
		{
			argData.getFlagArgument("-rsi", 0, arg1);
			int layout = ~0;
			argData.getFlagArgument("-rsi", 1, layout);
			MString uiCmd = shader->getRenderStateUI(arg1.asChar(), layout);
			appendToResult(uiCmd);
		}

		if (argData.isFlagSet("-amm"))
		{
			argData.getFlagArgument("-amm", 0, arg1);
			double min,max;
			argData.getFlagArgument("-amm", 1, min);
			argData.getFlagArgument("-amm", 2, max);
			shader->editParamMinMax(MConvert::ToFChar(arg1), min, max);
		}
	}

	return MS::kSuccess; 
}

void* CFXShaderCommand::creator() 
{ 
	return new CFXShaderCommand; 
} 

MSyntax CFXShaderCommand::newSyntax()
{
	MSyntax syntax;

	// NOTE: shortName < 4 characters, longName > 3 characters (otherwise it won't remember the flag).

	syntax.addFlag("-vp", "-vertprogram", MSyntax::kString);
	syntax.addFlag("-fp", "-fragprogram", MSyntax::kString);
	syntax.addFlag("-rlv", "-reloadvert", MSyntax::kString, MSyntax::kString);
	syntax.addFlag("-rlf", "-reloadfrag", MSyntax::kString, MSyntax::kString);
	syntax.addFlag("-ls", "-list", MSyntax::kString);
	syntax.addFlag("-s", "-shader", MSyntax::kString);

	syntax.addFlag("-at", "-addTechnique", MSyntax::kString, MSyntax::kString);
	syntax.addFlag("-ap", "-adpass", MSyntax::kString, MSyntax::kString);
	syntax.addFlag("-rt", "-removeCurrentTechnique", MSyntax::kString);
	syntax.addFlag("-rp", "-rmpass", MSyntax::kString);
	syntax.addFlag("-lp", "-lspass", MSyntax::kString);
	syntax.addFlag("-lfx","-loadFromCgFXFile", MSyntax::kString, MSyntax::kString);

	// Sets the Cg program used from a CgFX file
	// prototype : -sfx (FX filename) (tech index) (pass index)
	// note : pair with (-s) flag
	syntax.addFlag("-sfx","-setProgramFromEffect", MSyntax::kString, MSyntax::kLong, MSyntax::kLong);

	// Retrieves the names of the techniques in an effect file
	syntax.addFlag("-fxt","-getFXTechniques",MSyntax::kString);
	// Retrieves the names of the passes in an effect file given a technique index
	syntax.addFlag("-fxp","-getFXPasses",MSyntax::kLong, MSyntax::kString);

	syntax.addFlag("-ef", "-effectfile", MSyntax::kString);
	syntax.addFlag("-rle", "-reloadeffect", MSyntax::kString);
	syntax.addFlag("-pp", "-postproc", MSyntax::kString);
	syntax.addFlag("-su", "-setui", MSyntax::kString);
	syntax.addFlag("-gu", "-getui", MSyntax::kString);
	syntax.addFlag("-p", "-parameter", MSyntax::kString);
	syntax.addFlag("-es", "-editsource", MSyntax::kString);
	syntax.addFlag("-qs", "-querysource", MSyntax::kString);
	syntax.addFlag("-is", "-insource", MSyntax::kString);
	syntax.addFlag("-lfe", "-loadcgfromeffect", MSyntax::kString, MSyntax::kString, MSyntax::kString);
	syntax.addFlag("-pc", "-paramCount");
	syntax.addFlag("-pi", "-paramInfo", MSyntax::kLong);
	syntax.addFlag("-amm", "-editAttrMinMax", MSyntax::kString, MSyntax::kDouble, MSyntax::kDouble);

	// query the supported Cg render states
	syntax.addFlag("-qrs", "-queryRenderStates");
	syntax.addFlag("-qri", "-queryIndexedRenderStates");

	syntax.addFlag("-ars", "-addRenderState", MSyntax::kString);
	syntax.addFlag("-ari", "-addRenderStateIndexed", MSyntax::kString, MSyntax::kLong);
	syntax.addFlag("-rrs", "-removeRenderState", MSyntax::kString);
	syntax.addFlag("-rri", "-removeRenderStateIndexed", MSyntax::kString, MSyntax::kLong);

	syntax.addFlag("-rsi", "-renderStateInfoUI", MSyntax::kString, MSyntax::kLong);

	syntax.enableQuery(false);
	syntax.enableEdit(false);

   return syntax;
}
