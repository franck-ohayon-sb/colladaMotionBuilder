/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/*
	Helper class to export COLLADA mesh and derived elements (e.g. skin).
*/

#include "StdAfx.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericData.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>

#include "CMeshExporter.h"
#include "CExportOptions.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDExtra.h"
#include "FCDocument/FCDGeometryMesh.h"
#include "FCDocument/FCDGeometryPolygons.h"
#include "FCDocument/FCDGeometryPolygonsInput.h"
#include "FCDocument/FCDGeometryPolygonsTools.h"
#include "FCDocument/FCDGeometrySource.h"
#include "TranslatorHelpers/CMeshHelper.h"
#include "TranslatorHelpers/CAnimationHelper.h"
#include "TranslatorHelpers/CDagHelper.h"

#define VALIDATE_DATA

CMeshExporter::CMeshExporter(DaeEntity* _element, MFnMesh& _meshFn, FCDGeometryMesh* _colladaMesh, DaeDoc* _doc)
 :	meshFn(_meshFn), colladaMesh(_colladaMesh), doc(_doc)
{
	element = _element;
}

CMeshExporter::~CMeshExporter()
{
	accumulator.clear();
}

// Export the vertex positions.
// 
void CMeshExporter::ExportVertexPositions()
{
	// Retrieves the vertex positions.
	MPointArray vertexArray;
	meshFn.getPoints(vertexArray, MSpace::kObject);
	uint vertexCount = vertexArray.length();

	// Look for an existing position source
	FCDGeometrySource* vertexSource = colladaMesh->FindSourceByType(FUDaeGeometryInput::POSITION);
	if (vertexSource != NULL)
	{
		vertexSource->SetDataCount(0);
		vertexSource->SetUserHandle((void*) 0);
	}
	else
	{
		vertexSource = colladaMesh->AddVertexSource(FUDaeGeometryInput::POSITION);
		vertexSource->SetDaeId(colladaMesh->GetDaeId() + "-positions");
	}

	// Export the vertex positions
	vertexSource->SetStride(3);
	vertexSource->SetName(FC("position"));
	vertexSource->SetValueCount(vertexCount);
	for (uint i = 0; i < vertexCount; ++i)
	{
		vertexSource->SetValue(i, MConvert::ToFMVector(vertexArray[i]));
	}

	// Remove the vertex position tweaks that will be exported as animations
	MPlug positionTweakArrayPlug = meshFn.findPlug("pt");
	uint positionTweakCount = positionTweakArrayPlug.numElements();
	for (uint i = 0; i < positionTweakCount; ++i)
	{
		MPlug positionTweakPlug = positionTweakArrayPlug.elementByPhysicalIndex(i);
		MPlug positionTweakPlugX = positionTweakPlug.child(0);
		MPlug positionTweakPlugY = positionTweakPlug.child(1);
		MPlug positionTweakPlugZ = positionTweakPlug.child(2);
		bool xAnimated = CAnimationHelper::IsAnimated(doc->GetAnimationCache(), positionTweakPlugX) != kISANIM_None;
		bool yAnimated = CAnimationHelper::IsAnimated(doc->GetAnimationCache(), positionTweakPlugY) != kISANIM_None;
		bool zAnimated = CAnimationHelper::IsAnimated(doc->GetAnimationCache(), positionTweakPlugZ) != kISANIM_None;
		if (!xAnimated && !yAnimated && !zAnimated) continue;

		uint logicalIndex = positionTweakPlug.logicalIndex();
		if (logicalIndex >= vertexCount) continue;
		FMVector3 pointData(vertexSource->GetValue(logicalIndex));

		MObject vectorObject;
		positionTweakPlug.getValue(vectorObject);
		MFnNumericData data(vectorObject);
		MFloatPoint positionTweak;
		data.getData(positionTweak.x, positionTweak.y, positionTweak.z);

		pointData -= MConvert::ToFMVector(positionTweak);
		vertexSource->SetValue(logicalIndex, pointData);
        
        // Export the Point3 animation - note that this is a relative animation.
		for (size_t k = 0; k < 3; ++k)
		{
			FCDAnimated* animated = vertexSource->GetSourceData().GetAnimated(3 * logicalIndex + k);
	        animated->SetRelativeAnimationFlag();
			ANIM->AddPlugAnimation(positionTweakPlug.child((uint) k), animated, kSingle | kLength);
		}
	}

	// Add input to the mesh <vertices> node
	accumulator.push_back(DaeInput(vertexSource, -1));
}

// Export the vertex normals.
bool CMeshExporter::ExportVertexNormals()
{
	if (!CExportOptions::ExportNormals()) return false;

	// Look for an existing normal source
	FCDGeometrySource* normalSource = colladaMesh->FindSourceByType(FUDaeGeometryInput::NORMAL);
	if (normalSource != NULL)
	{
		normalSource->SetDataCount(0);
		normalSource->SetUserHandle((void*) 0);
	}
	else
	{
		normalSource = colladaMesh->AddSource(FUDaeGeometryInput::NORMAL);
		normalSource->SetDaeId(colladaMesh->GetDaeId() + "-normals");
	}

	// Check for all smooth normals
	uint normalCount = meshFn.numNormals();
	normalSource->SetName(FC("normal"));
	normalSource->SetStride(3);
	normalSource->SetValueCount(normalCount);
	bool perVertexNormals = normalCount == (uint) meshFn.numVertices();

	MFloatVectorArray normals(normalCount);
	if (perVertexNormals)
	{
		// Get the unindexed normals in a separate buffer
		MFloatVectorArray unindexedNormals;
		meshFn.getNormals(unindexedNormals, MSpace::kObject);

		// Index the normals to match the vertex indices
		for (MItMeshPolygon polyIt(meshFn.object()); !polyIt.isDone(); polyIt.next())
		{
			uint vertexCount = polyIt.polygonVertexCount();
			for (uint i = 0; i < vertexCount; ++i)
			{
				int normalIndex = polyIt.normalIndex(i);
				int vertexIndex = polyIt.vertexIndex(i);
				normals[vertexIndex] = unindexedNormals[normalIndex];
			}
		}

		// Add the source to the COLLADA mesh
		if (!colladaMesh->IsVertexSource(normalSource)) colladaMesh->AddVertexSource(normalSource);
	}
	else
	{
		// Retrieve the per-face, per-vertex normals
		meshFn.getNormals(normals, MSpace::kObject);
		accumulator.push_back(DaeInput(normalSource, -1));

		if (colladaMesh->IsVertexSource(normalSource)) colladaMesh->RemoveVertexSource(normalSource);
	}

	// Copy the normals to the geometry source
	for (uint i = 0; i < normalCount; ++i)
	{
		normalSource->SetValue(i, MConvert::ToFMVector(normals[i]));
	}

	if (CExportOptions::ExportTangents())
	{
		// Look for an existing tangent source
		FCDGeometrySource* tangentSource = colladaMesh->FindSourceByType(FUDaeGeometryInput::GEOTANGENT);
		if (tangentSource != NULL)
		{
			tangentSource->SetDataCount(0);
			tangentSource->SetUserHandle((void*) 0);
		}
		else
		{
			tangentSource = colladaMesh->AddSource(FUDaeGeometryInput::GEOTANGENT);
			tangentSource->SetDaeId(colladaMesh->GetDaeId() + "-geotangent");
		}

		// Look for an existing binormal source
		FCDGeometrySource* binormalSource = colladaMesh->FindSourceByType(FUDaeGeometryInput::GEOBINORMAL);
		if (binormalSource != NULL)
		{
			binormalSource->SetDataCount(0);
			binormalSource->SetUserHandle((void*) 0);
		}
		else
		{
			binormalSource = colladaMesh->AddSource(FUDaeGeometryInput::GEOBINORMAL);
			binormalSource->SetDaeId(colladaMesh->GetDaeId() + "-geobinormal");
		}

		// Fill in the basic source data informations
		tangentSource->SetName(FC("geotangent"));
		tangentSource->SetStride(3);
		binormalSource->SetName(FC("geobinormal"));
		binormalSource->SetStride(3);

		MVectorArray tangents(normalCount), binormals(normalCount);
		if (perVertexNormals)
		{
			// Calculate and export the geometric tangents and binormals(T/Bs)
			// Retrieve all the vertex positions for our calculations
			MPointArray vertexPositions;
			meshFn.getPoints(vertexPositions);
			uint vertexCount = vertexPositions.length();
			MObject meshObject(meshFn.object());
			for (MItMeshVertex vertexIt(meshObject); !vertexIt.isDone(); vertexIt.next())
			{
				MIntArray vertexNeighbors;
				int vertexIndex = vertexIt.index();
				vertexIt.getConnectedVertices(vertexNeighbors);
				if (vertexNeighbors.length() == 0 || vertexNeighbors[0] >= (int) vertexCount || vertexIndex >= (int) vertexCount)
				{
					tangents[vertexIndex] = MFloatVector::yAxis;
					binormals[vertexIndex] = MFloatVector::zAxis;
					continue;
				}

				// Calculate the T/Bs (code repeated below)
				MPoint& neighborPosition = vertexPositions[vertexNeighbors[0]];
				MVector directionV = neighborPosition - vertexPositions[vertexIndex];
				tangents[vertexIndex] = (directionV ^ normals[vertexIndex]).normal();
				binormals[vertexIndex] = (normals[vertexIndex] ^ tangents[vertexIndex]).normal();
			}

			if (!colladaMesh->IsVertexSource(tangentSource)) colladaMesh->AddVertexSource(tangentSource);
			if (!colladaMesh->IsVertexSource(binormalSource)) colladaMesh->AddVertexSource(binormalSource);
		}
		else
		{
			// Calculate and export the geometric tangents and binormals(T/Bs)
			// Retrieve all the vertex positions for our calculations
			MPointArray vertexPositions;
			meshFn.getPoints(vertexPositions);
			uint vertexCount = vertexPositions.length();

			for (uint i = 0; i < normalCount; ++i) { binormals[i] = tangents[i] = MFloatVector::zero; }
			for (MItMeshPolygon faceIt(meshFn.object()); !faceIt.isDone(); faceIt.next())
			{
				int faceVertexCount = faceIt.polygonVertexCount();
				for (int i = 0; i < faceVertexCount; ++i)
				{
					int normalIndex = faceIt.normalIndex(i);
					if (normalIndex >= (int) normalCount) continue;

					// Don't recalculate T/Bs
					if (!tangents[normalIndex].isEquivalent(MFloatVector::zero)) continue;
					
					// Retrieve the vertex position and the position of its closest neighbor within the face
					int vertexIndex = faceIt.vertexIndex(i);
					int neighborVertexIndex = faceIt.vertexIndex((i == 0) ? faceVertexCount - 1 : i - 1);
					if (neighborVertexIndex >= (int) vertexCount || vertexIndex >= (int) vertexCount) continue;
					MPoint& vertexPosition = vertexPositions[vertexIndex];
					MPoint& neighborPosition = vertexPositions[neighborVertexIndex];

					// Calculate the T/Bs (code repeated above)
					MFloatVector directionV = MFloatVector(neighborPosition - vertexPosition);
					tangents[normalIndex] = (directionV ^ normals[normalIndex]).normal();
					binormals[normalIndex] = (normals[normalIndex] ^ tangents[normalIndex]).normal();
				}
			}

			if (colladaMesh->IsVertexSource(tangentSource)) colladaMesh->RemoveVertexSource(tangentSource);
			if (colladaMesh->IsVertexSource(binormalSource)) colladaMesh->RemoveVertexSource(binormalSource);

			accumulator.push_back(DaeInput(tangentSource, -1));
			accumulator.push_back(DaeInput(binormalSource, -1));
		}

		uint tangentCount = tangents.length();
		tangentSource->SetValueCount(tangentCount);
		float* tdata = tangentSource->GetData();
		for (uint i = 0; i < tangentCount; ++i)
		{
			(*tdata++) = (float) tangents[i].x; (*tdata++) = (float) tangents[i].y; (*tdata++) = (float) tangents[i].z;
		}

		uint binormalCount = binormals.length();
		binormalSource->SetValueCount(binormalCount);
		float* bdata = binormalSource->GetData();
		for (uint i = 0; i < binormalCount; ++i)
		{
			(*bdata++) = (float) binormals[i].x; (*bdata++) = (float) binormals[i].y; (*bdata++) = (float) binormals[i].z;
		}
	}

	return !perVertexNormals;
}

// Export the color sets
// Returns true if we should proceed to export the given color set Ids.
void CMeshExporter::ExportColorSets(DaeColourSetList& colorSets)
{
	if (!CExportOptions::ExportVertexColors()) return;

	// Retrieve all the color set header information
	CMeshHelper::GetMeshValidColorSets(meshFn.object(), colorSets);

	// Process the color sets
	uint colorSetCount = (uint) colorSets.size();
	for (uint i = 0; i < colorSetCount; ++i)
	{
		DaeColourSet& colorSet = *colorSets[i];
		if (colorSet.name.length() == 0) continue;

		// Create the source
		FCDGeometrySource* source = colladaMesh->AddSource(FUDaeGeometryInput::COLOR);
		source->SetDaeId((meshFn.name() + colorSet.name).asChar());
		source->SetName(MConvert::ToFChar(colorSet.name));
		source->SetStride(4);

		// Retrieve the colour set data
		CMeshHelper::GetMeshColorSet(doc, meshFn.object(), source, colorSet);
		if (source->GetDataCount() == 0)
		{
			// Delete the empty color set
			SAFE_RELEASE(source);
		}
		else if (colorSet.isVertexColor)
		{
			// Insert a per-vertex color set input
			colladaMesh->AddVertexSource(source);
		}
		else
		{
			// Insert a per-face-vertex color set input
			accumulator.push_back(DaeInput(source, i));
		}
	}
}

// Export per-vertex blind data
void CMeshExporter::ExportVertexBlindData()
{
	uint meshVertexCount = meshFn.numVertices();
	MIntArray blindDataTemplateIndices;
	meshFn.getBlindDataTypes(MFn::kMeshVertComponent, blindDataTemplateIndices);

	uint blindDataTemplateCount = blindDataTemplateIndices.length();
	for (uint i = 0; i < blindDataTemplateCount; ++i)
	{
		uint blindDataTemplateIndex = blindDataTemplateIndices[i];

		MStringArray longNames, shortNames, typeNames;
		meshFn.getBlindDataAttrNames(blindDataTemplateIndex, longNames, shortNames, typeNames);

		uint valueCount = typeNames.length();
		for (uint j = 0; j < valueCount; ++j)
		{
			MString typeName = typeNames[j];

			#define EXPORT_TYPE(typeStr, meshFunction, dataArrayType) \
			if (typeName == typeStr) \
			{ \
				FCDGeometrySource* source = colladaMesh->AddSource(FUDaeGeometryInput::EXTRA); \
				source->SetName(MConvert::ToFChar(longNames[j])); \
				source->SetStride(1); \
				FUSStringBuilder sourceId(colladaMesh->GetDaeId()); sourceId.append("-blind"); sourceId.append(blindDataTemplateIndex); sourceId.append("-"); sourceId.append(shortNames[j].asChar()); \
				source->SetDaeId(colladaMesh->GetDaeId() + "-blind"); \
				\
				MIntArray indices; dataArrayType indexedData; \
				source->SetDataCount(meshVertexCount); \
				float* data = source->GetData(); \
				memset(data, 0, meshVertexCount * sizeof(float)); \
				meshFn.meshFunction(MFn::kMeshVertComponent, blindDataTemplateIndex, longNames[j], indices, indexedData); \
				uint indexCount = indices.length(); \
				for (uint k = 0; k < indexCount; ++k) data[indices[k]] = (float) indexedData[k]; \
				FCDExtra* extra = source->GetExtra(); \
				FCDETechnique* technique = extra->GetDefaultType()->AddTechnique(DAEMAYA_MAYA_PROFILE); \
				technique->AddParameter(DAEMAYA_BLINDTYPEID_PARAMETER, blindDataTemplateIndex); \
				technique->AddParameter(DAEMAYA_LONGNAME_PARAMETER, MConvert::ToFChar(longNames[j])); \
				technique->AddParameter(DAEMAYA_SHORTNAME_PARAMETER, MConvert::ToFChar(shortNames[j])); \
			}

			EXPORT_TYPE("int", getIntBlindData, MIntArray)
			else EXPORT_TYPE("float", getFloatBlindData, MFloatArray)
			else EXPORT_TYPE("double", getDoubleBlindData, MDoubleArray)
			else EXPORT_TYPE("boolean", getBoolBlindData, MIntArray)

			#undef EXPORT_TYPE
		}
	}
}


// Export the texture coordinates listed in the
// two arrays given in argument, that correspond respectively
// to the Maya uv set name and the collada texcoord id.
//
void CMeshExporter::ExportTexCoords(const MStringArray& uvSetNames, const StringList& texcoordIds)
{
	if (!CExportOptions::ExportTexCoords()) return;

	int texCoordsCount = uvSetNames.length();

	for (int i = 0; i < texCoordsCount; i++)
	{
		MFloatArray uArray, vArray;
		MString uvSetName = uvSetNames[i];
		meshFn.getUVs(uArray, vArray, &uvSetName);
		uint uvCount = uArray.length();
		if (uvCount == 0 || vArray.length() != uvCount) continue;
		
		FCDGeometrySource* source = colladaMesh->AddSource(FUDaeGeometryInput::TEXCOORD);
		source->SetStride(2);
		source->SetName(MConvert::ToFChar(uvSetName));
		source->SetDaeId(texcoordIds[i]);
		source->SetValueCount(uvCount);
		float* data = source->GetData();
		for (uint j = 0; j < uvCount; ++j)
		{
			*(data++) = uArray[j];
			*(data++) = vArray[j];
		}

		// Figure out the real index for this texture coordinate set
		MPlug uvSetPlug = meshFn.findPlug("uvSet");
		uint realIndex;
		for (realIndex = 0; realIndex < uvSetPlug.numElements(); ++realIndex)
		{
			// get uvSet[<index>] and uvSet[<index>].uvSetName
			MPlug uvSetElememtPlug = uvSetPlug.elementByPhysicalIndex(realIndex);
			MPlug uvSetNamePlug = uvSetElememtPlug.child(0);

			// get value of plug (uvSet's name)
			MString uvSetNamePlugValue;
			uvSetNamePlug.getValue(uvSetNamePlugValue);
			if (uvSetName == uvSetNamePlugValue) break;
		}
		accumulator.push_back(DaeInput(source, realIndex));
	}
}

// Return a list of names for each texcoord id
// that corresponds to the equivalent Maya uv set name,
// as returned by MFnMesh.getUVSetNames().
// 
StringList CMeshExporter::GenerateTexCoordIds(const MStringArray& uvSetNames)
{
	int texCoordsCount = uvSetNames.length();
	StringList texcoordIds;
	texcoordIds.resize(texCoordsCount);
	for (int i = 0; i < texCoordsCount; i++)
	{
		texcoordIds[i] = colladaMesh->GetDaeId() + "-" + uvSetNames[i].asChar();
	}

	return texcoordIds;
}

// Are the faces double-sided?
//
bool CMeshExporter::IsDoubleSided()
{
	MPlug doubleSidedPlug = meshFn.findPlug("doubleSided");
	bool doubleSided; 
	doubleSidedPlug.getValue(doubleSided);
	if (doubleSided)
	{
		// Also check the backfaceCulling plug
		MPlug backfaceCullingPlug = meshFn.findPlug("backfaceCulling");
		int enumValue = 0;
		backfaceCullingPlug.getValue(enumValue);
		if (enumValue != 0) doubleSided = false;
	}
	return doubleSided;
}

void CMeshExporter::ExportMesh()
{
	MStatus status;

	// Retrieve all uv set names for this mesh,
	// then generate corresponding textureCoordinateIds.
	MStringArray uvSetNames;
	MPlug uvSetPlug = meshFn.findPlug("uvSet");
	for (uint i = 0; i < uvSetPlug.numElements(); i++)
	{
		// get uvSet[<index>] and uvSet[<index>].uvSetName
		MPlug uvSetElememtPlug = uvSetPlug.elementByPhysicalIndex(i);
		MPlug uvSetNamePlug = uvSetElememtPlug.child(0);

		// get value of plug (uvSet's name)
		MString uvSetName;
		uvSetNamePlug.getValue(uvSetName);
		uvSetNames.append(uvSetName);
	}
	StringList texcoordIds = GenerateTexCoordIds(uvSetNames);
	DaeColourSetList colorSets;

	// Retrieve all the per-vertex & per-vertex/per-face inputs
	ExportVertexPositions();
	bool hasFaceVertexNormals = ExportVertexNormals();
	ExportTexCoords(uvSetNames, texcoordIds);
	ExportVertexBlindData();
	ExportColorSets(colorSets);

	// If triangulation is requested, verify that it is feasible by checking
	// with all the mesh polygons
	bool triangulated = false;
	if (CExportOptions::ExportTriangles())
	{
		triangulated = true;
		for (MItMeshPolygon polyIt(meshFn.object()); triangulated && !polyIt.isDone(); polyIt.next())
		{
			triangulated = polyIt.hasValidTriangulation();
		}
	}

	// Get hole information from the mesh node.
	// The format for the holes information is explained in the MFnMesh documentation.
	MIntArray holesInformation, holesVertices;
	meshFn.getHoles(holesInformation, holesVertices, &status);
	uint holeCount = (status != MStatus::kSuccess) ? 0 : (holesInformation.length() / 3);
	
	// Find how many shaders are used by this instance of the mesh
	MObjectArray shaders;
	MIntArray shaderIndices;
	meshFn.getConnectedShaders(0, shaders, shaderIndices);

	// Find the polygons that correspond to each materials and export them
	MItMeshPolygon polyIt(meshFn.object());
	uint realShaderCount = (uint) shaders.length();
	uint numShaders = (uint) max((size_t) 1, (size_t) shaders.length());
	for (uint ii = 0; ii < numShaders; ++ii)
	{
		// Even if there are no shaded faces, export an empty <polygons> so that the instance_geometry export works.
		FCDGeometryPolygons* colladaPoly = colladaMesh->AddPolygons();
		if (ii < realShaderCount)
		{
			// Add shader-specific parameters (TexCoords sets)
			MFnDependencyNode shaderFn(shaders[ii]);
			colladaPoly->SetMaterialSemantic(MConvert::ToFChar(doc->MayaNameToColladaName(shaderFn.name())));
		}

		// Generate the polygon set inputs.
		fm::vector<DaeInput> vertexAttributes;
		int nextIdx = 1, normalsIdx = -1;
		size_t inputCount = accumulator.size();
		for (size_t p = 0; p < inputCount; ++p)
		{
			DaeInput& param = accumulator[p];
			FCDGeometrySource* source = param.source;
			FUDaeGeometryInput::Semantic type = source->GetType();

			// Figure out which idx this parameter will use
			//
   			int foundIdx = -1;

			// For geometric tangents and binormals, use the same idx as the normals.
			// For texture tangents and binormals, group together for each UV set.
			if (type == FUDaeGeometryInput::NORMAL || type == FUDaeGeometryInput::GEOTANGENT || type == FUDaeGeometryInput::GEOBINORMAL)
			{
				foundIdx = normalsIdx;
			}

			// Check for duplicate vertex attributes to use their idx
			if (foundIdx == -1)
			{
				size_t vertexAttributeCount = vertexAttributes.size();
   				for (size_t v = 0; v < vertexAttributeCount; ++v)
				{
					if (vertexAttributes[v].source->GetType() == type && vertexAttributes[v].idx == param.idx)
					{
						foundIdx = (int) v;
						break;
					}
				}
			}

			bool isVertexSource = colladaMesh->IsVertexSource(source);

			// New vertex attribute, so generate a new idx
			bool newIdx = foundIdx == -1;
			if (newIdx)
			{
				// Assign it the correct offset/idx.
				foundIdx = (!isVertexSource) ? nextIdx++ : 0;
				if (type == FUDaeGeometryInput::NORMAL || type == FUDaeGeometryInput::GEOTANGENT || type == FUDaeGeometryInput::GEOBINORMAL)
				{
					normalsIdx = foundIdx;
				}
			}

   			// Add the per-face, per-vertex input to the polygons description
			FCDGeometryPolygonsInput* input = (!isVertexSource) ? colladaPoly->AddInput(source, foundIdx) : colladaPoly->FindInput(source);

			if (newIdx)
			{
				// Retrieve the index buffer to fill in.
				param.input = input;
				param.input->ReserveIndexCount(meshFn.numFaceVertices() / numShaders);
				// vertexAttributes is a fm::vector (not pvector), so push_back after you modified
				// param for the last time.
				vertexAttributes.push_back(param);
			}

			// For texture coordinate-related inputs: set the 'set' attribute.
			if (type == FUDaeGeometryInput::TEXCOORD)
			{
				input->SetSet(param.idx);
			}
		}

		// Dump the indices
		size_t numAttributes = vertexAttributes.size();
		polyIt.reset();
		for (polyIt.reset(); !polyIt.isDone(); polyIt.next())
		{
			// Is this polygon shaded by this shader?
			int polyIndex = polyIt.index();
			if (ii < realShaderCount && (uint) shaderIndices[polyIndex] != ii) continue;
			if (ii >= realShaderCount && (shaderIndices[polyIndex] >= 0 && shaderIndices[polyIndex] < (int)realShaderCount)) continue;

			// Collect data in order to handle triangle-only export option
			int numPolygons = 0, numVertices = 0;
			int polygonVertexCount = polyIt.polygonVertexCount();
			MIntArray vertexIndices;
			if (triangulated && polygonVertexCount > 3)
			{
				polyIt.numTriangles(numPolygons);
				if (numPolygons > 0)
				{
					numVertices = 3;
					MPointArray vertexPositions;
					MIntArray meshVertexIndices;
					polyIt.getTriangles(vertexPositions, meshVertexIndices);
					vertexIndices.setLength(meshVertexIndices.length());
					numPolygons = meshVertexIndices.length() / 3;

					// Map the vertex indices to iterator vertex indices so that we can
					// get information from the iterator about normals and such.
					//
					uint meshVertexIndexCount = meshVertexIndices.length();
					for (uint mvi = 0; mvi < meshVertexIndexCount; ++mvi)
					{
						int meshVertexIndex = meshVertexIndices[mvi];
						int polygonVertexCount = polyIt.polygonVertexCount();
						int iteratorVertexIndex = 0;
						for (int pv = 0; pv < polygonVertexCount; ++pv)
						{
							if ((int) polyIt.vertexIndex(pv) == meshVertexIndex)
							{
								iteratorVertexIndex = pv;
							}
						}
						vertexIndices[mvi] = iteratorVertexIndex;
					}
				}
				else numPolygons = 0;
			}
			else if (polygonVertexCount >= 3)
			{
				numPolygons = 1;
				vertexIndices.setLength(polygonVertexCount);
				for (int pv = 0; pv < polygonVertexCount; ++pv)
				{
					vertexIndices[pv] = pv;
				}

				#ifdef VALIDATE_DATA
				// Skip over any duplicate vertices in this face.
				// Very rarely, a cunning user manages to corrupt
				// a face entry on the mesh and somehow configure
				// a face to include the same vertex multiple times.
				// This will cause the read-back of this data to
				// reject the face, and can crash other COLLADA
				// consumers, so better to lose the data here
				//
				for (uint n = 0; n < vertexIndices.length() - 1; ++n)
				{
					for (uint m = n + 1; m < vertexIndices.length();)
					{
						if (vertexIndices[n] == vertexIndices[m])
						{
							vertexIndices.remove(m);
						}
						else ++m;
					}
				}
				#endif

				numVertices = (int) vertexIndices.length();
			}

			for (int p = 0; p < numPolygons; ++p)
			{
				size_t faceIndex = colladaPoly->GetFaceVertexCountCount();
				colladaPoly->AddFaceVertexCount((uint32) numVertices);

				// Buffer the face normal indices
				MIntArray normalIndices;
				if (hasFaceVertexNormals)
				{
					meshFn.getFaceNormalIds(polyIndex, normalIndices);
				}

				for (int j = 0; j < numVertices; j++)
				{
                    // Handle front face vs back face by walking
					// the vertices backward on the backface
					//
					int iteratorVertexIndex = vertexIndices[p * numVertices + j];
					int vertexIndex = polyIt.vertexIndex(iteratorVertexIndex);

					// Look for holes in this polygon
					// ASSUMPTION: Holes are automatically removed by triangulation.
					// ASSUMPTION: The iterator gives the hole vertices at the end of the enumeration.
					// ASSUMPTION: Hole vertices are never used as surface vertices or repeated between
					// holes or inside a hole.
					//
					if (polyIt.isHoled() && !triangulated)
					{
						for (uint h = 0; h < holeCount; ++h)
						{
							if (holesInformation[h*3] == polyIndex)
							{
								uint holeVertexOffset = holesInformation[h*3+2];
								if (holeVertexOffset <= holesVertices.length() && holesVertices[holeVertexOffset] == vertexIndex)
								{
									// Bad cast!!
									const_cast<uint32*>(colladaPoly->GetFaceVertexCounts())[faceIndex] -= (numVertices - iteratorVertexIndex);
									faceIndex = colladaPoly->GetFaceVertexCountCount();
									colladaPoly->AddHole((uint32) faceIndex);
									colladaPoly->AddFaceVertexCount(numVertices - iteratorVertexIndex);
								}
							}
						}
					}

					// Output each vertex attribute we need
					for (size_t kk = 0; kk < numAttributes; kk++)
					{
						DaeInput& attr = vertexAttributes[kk];
						switch (attr.source->GetType())
						{
						case FUDaeGeometryInput::VERTEX:
						case FUDaeGeometryInput::POSITION:
							attr.input->AddIndex(vertexIndex);
							break;

						case FUDaeGeometryInput::NORMAL:
						case FUDaeGeometryInput::GEOTANGENT:
						case FUDaeGeometryInput::GEOBINORMAL:
							attr.input->AddIndex(normalIndices[iteratorVertexIndex]);
							break;

						case FUDaeGeometryInput::TEXCOORD: {
							int uvIndex = 0;
							polyIt.getUVIndex(iteratorVertexIndex, uvIndex, &uvSetNames[attr.idx]);
							attr.input->AddIndex(uvIndex);
							break; }

						case FUDaeGeometryInput::COLOR: {
							DaeColourSet& set = *colorSets[attr.idx];
							int colorIndex = 0;
							if (set.indices.length() > 0)
							{
								meshFn.getFaceVertexColorIndex(polyIndex, iteratorVertexIndex, colorIndex);
								if (colorIndex >= 0 && colorIndex < (int) set.indices.length()) colorIndex = set.indices[colorIndex];
							}
							else
							{
#if MAYA_API_VERSION >= 700
								meshFn.getColorIndex(polyIndex, iteratorVertexIndex, colorIndex, &set.name);
#else
								meshFn.getFaceVertexColorIndex(polyIndex, iteratorVertexIndex, colorIndex);
#endif
							}

							// Its possible for colorIndex to be -1, however COLLADA doesnt support
							// non-coloured vertices, so simply use the white color index
							if (colorIndex < 0) colorIndex = set.whiteColorIndex;
							attr.input->AddIndex(colorIndex);
							break; }

						case FUDaeGeometryInput::UNKNOWN:
						case FUDaeGeometryInput::UV:
						case FUDaeGeometryInput::EXTRA:
						default: break; // Not exported/supported
						}
					}
				}
			}
		}
	}

	// For texturing map channels, export the texture tangents and bi-normals, on request
	if (CExportOptions::ExportTexTangents())
	{
		size_t sourceCount = colladaMesh->GetSourceCount();
		for (size_t sourceIndex = 0; sourceIndex < sourceCount; ++sourceIndex)
		{
			FCDGeometrySource* source = colladaMesh->GetSource(sourceIndex);
			if (source->GetType() == FUDaeGeometryInput::TEXCOORD)
			{
				FCDGeometryPolygonsTools::GenerateTextureTangentBasis(colladaMesh, source, true);
			}
		}
	}

	// Export the double-sidedness of this mesh
	FCDETechnique* mayaTechnique = element->entity->GetExtra()->GetDefaultType()->AddTechnique(DAEMAYA_MAYA_PROFILE);
	mayaTechnique->AddParameter(DAESHD_DOUBLESIDED_PARAMETER, IsDoubleSided());
}
