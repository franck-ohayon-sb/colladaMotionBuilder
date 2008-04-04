/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "ColladaFX/CFXPasses.h"
#include "ColladaFX/CFXShaderNode.h"
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnPluginData.h>
#include <maya/MFnStringArrayData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MArrayDataHandle.h>
#include "TranslatorHelpers/CDagHelper.h"
#if MAYA_API_VERSION >= 700
#include <maya/MGeometryData.h>
#include <maya/MHardwareRenderer.h>
#include <maya/MHWShaderSwatchGenerator.h>
#endif

MTypeId     CFXPasses::id(0x0005052A);

// Attributes
// 
MObject		CFXPasses::aTechniqueNames;
MObject		CFXPasses::aChosenTechniqueName;

#define CFX_ATTRIB_PREFIX "cfx"
#define CFX_OUTCOLOR_SUFFIX "_outColors"

CFXPasses::CFXPasses() 
{
}

CFXPasses::~CFXPasses() 
{
}

MStatus CFXPasses::compute(const MPlug& plug, MDataBlock& data)
//
//	Description:
//		This method computes the value of the given output plug based
//		on the values of the input attributes.
//
//	Arguments:
//		plug - the plug to compute
//		data - object that provides access to the attributes for this node
//
{ 
	MStatus returnStatus;
	if (plug == outColor)
	{
		MFloatVector color(0.9f, .8f, .07f);
		MDataHandle outputHandle = data.outputValue(outColor);
		outputHandle.asFloatVector() = color;
		
		outputHandle.setClean();
	}
	else return MS::kUnknownParameter;

	return MS::kSuccess;
}

void* CFXPasses::creator()
//
//	Description:
//		this method exists to give Maya a way to create new objects
//      of this type. 
//
//	Return Value:
//		a new object of this type
//
{
	return new CFXPasses();
}

MStatus CFXPasses::initialize()
{
	MStatus stat;

#define CHECK_STAT(msg) \
	if (!stat){ stat.perror("CFXPasses::initialize ERROR: " msg); return stat; }

	MFnTypedAttribute tAttr;
	aTechniqueNames = tAttr.create("cfxTechniques", "tn", MFnData::kStringArray); 
	tAttr.setStorable(true);
	stat = addAttribute(aTechniqueNames);
	CHECK_STAT("techniqueNames");

	aChosenTechniqueName = tAttr.create("cfxChosenTechnique","ctn",MFnData::kString);
	MFnStringData defaultValue; defaultValue.set("");
	tAttr.setDefault(defaultValue.create());
	tAttr.setStorable(true);
	stat = addAttribute(aChosenTechniqueName);
	CHECK_STAT("chosenTechnique");

	stat = attributeAffects(aChosenTechniqueName, outColor);
	CHECK_STAT("attributeAffects aChosenTechniqueName");

	return MS::kSuccess;

#undef CHECK_STAT
}

MStatus CFXPasses::glBind(const MDagPath& shapePath)
{
	return MS::kSuccess;
}

MStatus CFXPasses::glUnbind(const MDagPath& shapePath)
{
	return MS::kSuccess;
}

MStatus CFXPasses::glGeometry(const MDagPath& shapePath, int prim, unsigned int writable, int indexCount, const unsigned int * indexArray, int vertexCount, const int * vertexIDs, const float * vertexArray, int normalCount, const float ** normalArrays, int colorCount, const float ** colorArrays, int texCoordCount, const float ** texCoordArrays)
{
	MStatus status;

	if (prim != GL_TRIANGLES && prim != GL_TRIANGLE_STRIP)
	{
		return MS::kFailure;
	}

	// Iterate over the connected plugs
	MObject odata;
	MPlug curTechPlug;
	if (!getCurrentTechnique(curTechPlug)) return MS::kFailure;
	MFnDependencyNode nodeFn(thisMObject());
	MPlug passPlug = nodeFn.findPlug(curTechPlug.partialName() + CFX_OUTCOLOR_SUFFIX, &status);
	FUAssert(status,return MS::kFailure);

	uint numPasses = passPlug.numElements();
	for (uint i = 0; i < numPasses; ++i) 
	{
		// bypass empty connection; retrieve the ColladaFX shader node
		MPlugArray connections;
		if (!passPlug[i].connectedTo(connections, true, false)) continue;
		MObject connection = connections[0].node();
		if (connection.isNull()) continue;
		MFnDependencyNode connectionFn(connection);
		if (connectionFn.typeId() != CFXShaderNode::id) continue;
		CFXShaderNode* shader = (CFXShaderNode*) connectionFn.userNode();
		if (shader == NULL) continue;

		// Render using the ColladaFX shader node
		shader->setBlendFlag(i > 0);
		shader->glBind(shapePath);
		shader->glGeometry(shapePath, prim, writable, indexCount, indexArray, vertexCount, vertexIDs, vertexArray, normalCount, normalArrays, colorCount, colorArrays, texCoordCount, texCoordArrays);
		shader->glUnbind(shapePath);
		shader->setBlendFlag(false);
	}

	return MS::kSuccess;
}

#if MAYA_API_VERSION == 700 || MAYA_API_VERSION > 800
MStatus CFXPasses::renderSwatchImage(MImage& outImage)
{
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
		MGeometryData* pGeomData =
			pRenderer->referenceDefaultGeometry(gshape, numberOfData, pIndexing, indexCount);
		if (!pGeomData)
		{
			return MStatus::kFailure;
		}

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
		float ** normalArrays = new float * [3];
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
		float ** texCoordArrays = new float * [1];
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
					NULL, 
					vertexData,
					normalCount,
					(const float **) normalArrays,
					0, 
					NULL, 
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
		if (width != origWidth || height != origHeight)
		{
			status = MStatus::kFailure;
		}
		else
		{
			status = MStatus::kSuccess;
		}

		pRenderer->dereferenceGeometry(pGeomData, numberOfData);
	}

	return status;
}
#endif // MAYA_API_VERSION >= 700

int CFXPasses::texCoordsPerVertex ()
{
	int returnVal = 0;

#if MAYA_API_VERSION >= 650
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


int  CFXPasses::normalsPerVertex ()
{
	return 3;
}

int  CFXPasses::colorsPerVertex ()
{
	int returnVal = 0;

#if MAYA_API_VERSION >= 700
	const MDagPath & path = currentPath();
	if (path.hasFn(MFn::kMesh)) 
	{
		MFnMesh meshf(path.node());
		returnVal = meshf.numColorSets();
	}
#endif
	return returnVal;
}

MStatus CFXPasses::addTechnique(const MString& techName)
{
	MStatus status;

	// Get the techniques string array
	MPlug techniques(thisMObject(), CFXPasses::aTechniqueNames);
	MStringArray astr;
	CDagHelper::GetPlugValue(techniques, astr, &status);

	MString newTechName = CFX_ATTRIB_PREFIX + techName;
	MString newTechNameCopy = newTechName;
	MFnDependencyNode nodeFn(thisMObject());

	// a little bit of logic to avoid extra UI needs
	bool duplicatedName;
	int suffix = 1;
	do
	{
		duplicatedName = false;
		for (uint i = 0; i < astr.length(); ++i)
		{
			if (astr[i] == newTechName)
			{
				// two passes can't have the same name
				duplicatedName = true;
				newTechName = newTechNameCopy + suffix;
				++ suffix;
				break;
			}
		}
	} while(duplicatedName);

	// create a new [techName] attribute to hold pass names
	MFnTypedAttribute tAttr;
	MFnStringArrayData attrData; attrData.set(MStringArray());
	MObject newTechAttrib = tAttr.create(newTechName,newTechName,MFnData::kStringArray,attrData.create());
	tAttr.setStorable(true);
	status = nodeFn.addAttribute(newTechAttrib);
	FUAssert(status,return MS::kFailure);

	// create a new [techName]_outColors attribute and add it
	MFnNumericAttribute nAttr;
	MString outColorsName = newTechName + CFX_OUTCOLOR_SUFFIX;
	MObject outColorsAttrib = nAttr.createColor(outColorsName, outColorsName);
	nAttr.setStorable(false);
	nAttr.setArray(true);
	status = nodeFn.addAttribute(outColorsAttrib);
	FUAssert(status,return MS::kFailure);
	
	// TODO.
	// status = attributeAffects(outColorsAttrib,outColor);
	// FUAssert(status,return MS::kFailure);

	// add the new technique to the string array attribute

	astr.append(newTechName);
	CDagHelper::SetPlugValue(techniques, astr);

	// set the current technique to the added technique
	MPlug techPlug(thisMObject(), CFXPasses::aChosenTechniqueName);
	status = techPlug.setValue(newTechName);
	FUAssert(status,return MS::kFailure);

	return status;
}

MStatus CFXPasses::getCurrentTechnique(MPlug& result)
{
	MStatus status;

	// which technique is selected?
	MPlug techPlug(thisMObject(), CFXPasses::aChosenTechniqueName);
	MString chosenTechName;
	status = techPlug.getValue(chosenTechName);
	FUAssert(status,return MS::kFailure);
	if (chosenTechName.length() == 0)
	{
		// no technique available
		return MS::kFailure;
	}

	// fetch the current technique plug
	MFnDependencyNode nodeFn(thisMObject());
	MPlug currentTechPlug = nodeFn.findPlug(chosenTechName,&status);
	if (!status) return status; // sanity check

	result = currentTechPlug;
	return status;
}

MStatus CFXPasses::getTechnique(const MString& techName, MPlug& result)
{
	MStatus status;

	// fetch the technique plug
	MFnDependencyNode nodeFn(thisMObject());
	MPlug techPlug = nodeFn.findPlug(CFX_ATTRIB_PREFIX + techName,&status);
	if (!status) return status; // sanity check

	result = techPlug;
	return status;
}

MStatus CFXPasses::removeCurrentTechnique()
{
	MStatus status = MS::kFailure;

	// fetch the current technique plug
	MPlug techPlug;
	if (!getCurrentTechnique(techPlug)) return MS::kFailure;
	
	// remove the technique from the technique names array
	MPlug techniques(thisMObject(), CFXPasses::aTechniqueNames);
	MStringArray astr;
	CDagHelper::GetPlugValue(techniques, astr, &status);
	FUAssert(status,return MS::kFailure);
	
	status = MS::kFailure;
	for (uint i = 0; i < astr.length(); ++i)
	{
		MString plugName = techPlug.partialName(), techName = astr[i];
		if (plugName == techName)
		{
			astr.remove(i);
			status = MS::kSuccess;
			break;
		}
	}

	if (status)
	{
		CDagHelper::SetPlugValue(techniques,astr);
	}

	// remove all the passes associated with that technique
	for (; removeLastPass();){}

	// fetch the associated CFX_OUTCOLOR_SUFFIX plug and remove it
	// NOTE: You MUST remove this attribute before removing the techPlug attribute itself
	//		 otherwise it throws an exception.
	MFnDependencyNode nodeFn(thisMObject());
	MPlug outColorsPlug = nodeFn.findPlug(techPlug.partialName() + CFX_OUTCOLOR_SUFFIX, &status);
	FUAssert(status,return MS::kFailure);
	status = nodeFn.removeAttribute(outColorsPlug.attribute());
	FUAssert(status,return MS::kFailure);

	// remove the current technique plug
	status = nodeFn.removeAttribute(techPlug.attribute()); // this calls glGeometry
	FUAssert(status,return MS::kFailure);

	// set the current technique to a valid value
	MPlug chosenTechPlug(thisMObject(), CFXPasses::aChosenTechniqueName);
	if (astr.length() > 0)
	{
		status = chosenTechPlug.setValue(astr[0]);
		FUAssert(status,return MS::kFailure);
	}
	else
	{
		status = chosenTechPlug.setValue("");
		FUAssert(status,return MS::kFailure);
	}

	return MS::kSuccess;
}

MStatus CFXPasses::addPass(const MString& passNameP)
{
	MStatus status;

	MPlug techPlug;
	if (!getCurrentTechnique(techPlug)) return MS::kFailure;
	
	// build a new attribute named [techName]_passNameP[numPasses]
	MStringArray astr;
	CDagHelper::GetPlugValue(techPlug, astr, &status);
	MString passname = MString(techPlug.partialName() + "_" + passNameP);
	MString passnameCopy = passname;
	// a little bit of logic to avoid extra UI needs
	bool duplicatedName;
	int suffix = 1;
	do
	{
		duplicatedName = false;
		for (uint i = 0; i < astr.length(); ++i)
		{
			if (astr[i] == passname)
			{
				// two passes can't have the same name
				duplicatedName = true;
				passname = passnameCopy + suffix;
				++ suffix;
				break;
			}
		}
	} while(duplicatedName);

	MFnTypedAttribute fnAttr;
	MObject toadd = fnAttr.create(passname, passname, MFnData::kString, MObject::kNullObj, &status);
	FUAssert(status,return MS::kFailure);
	fnAttr.setStorable(true);

	MFnDependencyNode nodeFn(thisMObject());
	status = nodeFn.addAttribute(toadd, MFnDependencyNode::kLocalDynamicAttr);
	FUAssert(status,return MS::kFailure);

	// add this new entry in the techPlug string array
	astr.append(passname);
	CDagHelper::SetPlugValue(techPlug, astr);

	return MS::kSuccess;
}

MStatus CFXPasses::removeLastPass()
{
	MPlug techPlug;
	if (!getCurrentTechnique(techPlug)) return MS::kFailure;

	MStatus status;
	MStringArray astr;
	CDagHelper::GetPlugValue(techPlug, astr, &status);
	uint lstr = astr.length();

	if (lstr > 0)
	{
		// remove the created pass attribute
		MFnDependencyNode nodeFn(thisMObject());
		MObject oldAttr = nodeFn.findPlug(astr[lstr-1]).attribute();
		nodeFn.removeAttribute(oldAttr);
		
		// remove the entry from the current technique pass array
		astr.remove(lstr - 1);
		CDagHelper::SetPlugValue(techPlug, astr);

		// disconnect the associated pass from the rendering pipeline
		MPlug techOutColors = nodeFn.findPlug(techPlug.partialName() + CFX_OUTCOLOR_SUFFIX, &status);
		if (techOutColors[lstr-1].isConnected())
		{
			MPlugArray array;
			techOutColors[lstr-1].connectedTo(array, true, false);
			
			MDGModifier modifier;
			modifier.disconnect(array[0], techOutColors[lstr-1]);
			modifier.doIt();
			
			techOutColors[lstr-1].setValue(MObject::kNullObj);
		}

		// pass removed successfully
		return MS::kSuccess;
	}
	else
	{
		// nothing to remove
		return MS::kFailure;
	}
}

uint CFXPasses::getTechniqueCount()
{
	MStringArray astr;
	getTechniqueNames(astr);
	return astr.length();
}

void CFXPasses::getTechniqueNames(MStringArray& output)
{
	MStatus status;
	MPlug techniques(thisMObject(), CFXPasses::aTechniqueNames);
	CDagHelper::GetPlugValue(techniques,output,&status);
}

uint CFXPasses::getPassCount(const MString& techName)
{
	MStatus status;
	MPlug techPlug;
	if (!getTechnique(techName,techPlug)) return 0;

	MFnDependencyNode nodeFn(thisMObject());
	MPlug outColorsPlug = nodeFn.findPlug(techPlug.partialName() + CFX_OUTCOLOR_SUFFIX, &status);
	FUAssert(status,return 0);

	return outColorsPlug.numElements();
}

uint CFXPasses::getPassCount(uint techIndex)
{
	// get decorated names
	MStringArray techNames;
	getTechniqueNames(techNames);

	if (techNames.length() <= techIndex)
		return 0;

	MString techName = techNames[techIndex].asChar() + 3; // un-decorate
	return getPassCount(techName);
}

uint CFXPasses::getChosenTechniquePassCount()
{
	MStatus status;
	MPlug techPlug;
	if (!getCurrentTechnique(techPlug)) return 0;

	MFnDependencyNode nodeFn(thisMObject());
	MPlug outColorsPlug = nodeFn.findPlug(techPlug.partialName() + CFX_OUTCOLOR_SUFFIX, &status);
	FUAssert(status,return 0);

	return outColorsPlug.numElements();
}

MObject CFXPasses::getPass(const MString& techName, uint index)
{
	MStatus status;
	MPlug techPlug;
	if (!getTechnique(techName,techPlug)) return MObject::kNullObj;

	MFnDependencyNode nodeFn(thisMObject());
	MPlug outColorsPlug = nodeFn.findPlug(techPlug.partialName() + CFX_OUTCOLOR_SUFFIX, &status);
	FUAssert(status,return MObject::kNullObj);

	MObject odata;
	MPlug shaderPlug = outColorsPlug.elementByLogicalIndex(index);
	MPlugArray connections;
	if (!shaderPlug.connectedTo(connections, true, false)) return MObject::kNullObj;
	return connections[0].node();
}

CFXShaderNode* CFXPasses::getPass(uint techIndex, uint passIndex)
{
	// get decorated names
	MStringArray techNames;
	getTechniqueNames(techNames);

	if (techNames.length() <= techIndex)
		return NULL;

	MString techName = techNames[techIndex].asChar() + 3; // un-decorate

	MObject o = getPass(techName, passIndex);
	MFnDependencyNode nodeFn(o);
	if (nodeFn.typeId() == CFXShaderNode::id)
	{
		return (CFXShaderNode*)nodeFn.userNode();
	}
	return NULL;
}

MString CFXPasses::getPassName(const MString& techName, uint index)
{
	MStatus status;
	MPlug techPlug;
	if (!getTechnique(techName,techPlug)) return "";
	MStringArray passNames;
	CDagHelper::GetPlugValue(techPlug, passNames);
	if (index < passNames.length())
	{
		return passNames[index];
	}
	else
	{
		return "";
	}
}

MObject CFXPasses::getChosenTechniquePass(uint index)
{
	MStatus status;
	MPlug techPlug;
	if (!getCurrentTechnique(techPlug)) return MObject::kNullObj;

	MFnDependencyNode nodeFn(thisMObject());
	MPlug outColorsPlug = nodeFn.findPlug(techPlug.partialName() + CFX_OUTCOLOR_SUFFIX, &status);
	FUAssert(status,return MObject::kNullObj);

	MObject odata;
	MPlug shaderPlug = outColorsPlug.elementByLogicalIndex(index);
	MPlugArray connections;
	if (!shaderPlug.connectedTo(connections, true, false)) return MObject::kNullObj;
	return connections[0].node();
}

void CFXPasses::getTechniquePasses(const MString& techName, CFXShaderNodeList& shaders)
{
	uint passCount = getPassCount(techName);
	for (uint i = 0; i < passCount; ++i)
	{
		MObject o = getPass(techName,i);
		MFnDependencyNode nodeFn(o);
		if (nodeFn.typeId() == CFXShaderNode::id)
		{
			shaders.push_back((CFXShaderNode*) nodeFn.userNode());
		}
	}
}

void CFXPasses::getChosenTechniquePasses(CFXShaderNodeList& shaders)
{
	uint passCount = getChosenTechniquePassCount();
	for (uint i = 0; i < passCount; ++i)
	{
		MObject o = getChosenTechniquePass(i);
		MFnDependencyNode nodeFn(o);
		if (nodeFn.typeId() == CFXShaderNode::id)
		{
			shaders.push_back((CFXShaderNode*) nodeFn.userNode());
		}
	}
}

MStatus CFXPasses::reloadFromCgFX(const char* fxFileName)
{
	if (fxFileName == NULL || fxFileName[0] == '\0') return MS::kFailure;

	CGcontext context = cgCreateContext();
	cgGLRegisterStates(context);
	if (!context) return MS::kFailure;

	CGeffect effect = CFXShaderNode::CreateEffectFromFile(context, fxFileName);
	if (!effect)
	{
		const char* error = cgGetLastListing(context);
		MGlobal::displayError(MString("CgFX compilation errors for file: ") + MString(fxFileName) + MString("\n") + MString(error));
		cgDestroyContext(context);
		return MS::kFailure;
	}

	// we have to remove all current techniques
	for (; removeCurrentTechnique();){}

	uint techniqueTarget = 0, passTarget = 0;

	// for each effect technique
	CGtechnique technique = cgGetFirstTechnique(effect);
	while (technique != NULL) 
	{
		// add this technique and make it current
		const char* cTechName = cgGetTechniqueName(technique);
		MString techName;
		if (!cTechName) techName = MString("technique") + MString(FUStringConversion::ToString(techniqueTarget).c_str());
		else techName = cTechName;
		addTechnique(techName);

		// for each technique pass
		CGpass pass = cgGetFirstPass(technique);
		while (pass != NULL)
		{
			const char* cPassName = cgGetPassName(pass);
			MString passName;
			if (!cPassName) passName = MString("pass") + MString(FUStringConversion::ToString(passTarget).c_str());
			else passName = cPassName;
			addPass(passName);

			// create the FX shadingEngine
			MString shaderName;
			MGlobal::executeCommand(
				"shadingNode -asShader \"colladafxShader\"",
				shaderName);

			MFnDependencyNode nodeFn(CDagHelper::GetNode(shaderName));
			CFXShaderNode* shader = (CFXShaderNode*) nodeFn.userNode();

			// populate the node
			shader->setEffectTargets(techniqueTarget,passTarget);
			shader->setEffectFilename(fxFileName);
			
			// make the connection (shader.outColor) -> (pass.outColor)
			MStatus status;
			MFnDependencyNode passFn(thisMObject());
			MString inPlugName = CFX_ATTRIB_PREFIX + techName + CFX_OUTCOLOR_SUFFIX;
			MPlug inPlug = passFn.findPlug(inPlugName,&status);
			inPlug = inPlug.elementByLogicalIndex(passTarget,&status);
			MPlug outPlug = nodeFn.findPlug("outColor",&status);
			status = CDagHelper::Connect(outPlug,inPlug) ? MS::kSuccess : MS::kFailure;

			pass = cgGetNextPass(pass);
			++ passTarget;
		}
		technique = cgGetNextTechnique(technique);
		++ techniqueTarget;
		passTarget = 0;
	}

	// Second step is to setup the shadingEngine and connect it up ...
	MString shadingGroupName;
	MString cmd = MString("sets -renderable true -noSurfaceShader true -empty -name ") + name() + "SG;";
	MGlobal::executeCommand(cmd, shadingGroupName);
	MObject shaderGroup = CDagHelper::GetNode(shadingGroupName);
	CDagHelper::Connect(thisMObject(), "outColor", shaderGroup, "surfaceShader");

	// ensure selection is still the ColladaFXPass that issued this call
	MString sSelectCmd = "select " + name();
#if MAYA_API_VERSION >= 700
	MGlobal::executeCommandOnIdle(sSelectCmd);
#else
	MGlobal::executeCommand(MString("evalDeferred \"") + sSelectCmd + MString("\""));
#endif // MAYA_API_VERSION
	cgDestroyEffect(effect);
	cgDestroyContext(context);
	return MS::kSuccess;
}

#undef CFX_OUTCOLOR_SUFFIX
#undef CFX_ATTRIB_PREFIX
