/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"

#include <maya/MDagPathArray.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnDirectionalLight.h>
#include <maya/MFnLight.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTransform.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MSelectionMask.h>
#include <maya/MSelectionList.h>

#include "CMeshHelper.h"
#include "CDagHelper.h"
#include "../CExportOptions.h"

#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDGeometrySource.h"

// Find all the history nodes of a specific type
void CMeshHelper::GetHistoryNodes(const MObject& mesh, MFn::Type nodeType, MObjectArray& historyNodes)
{
	MFnDagNode meshFn(mesh);

	// Look for the given node type in the mesh's history.
	MStringArray historyNodeNames;
	MGlobal::executeCommand(MString("listHistory  -bf -pdo true ") + meshFn.fullPathName() + ";", historyNodeNames);
	uint historyNodeCount = historyNodeNames.length();
	for (uint i = 0; i < historyNodeCount; ++i)
	{
		MObject historyNode = CDagHelper::GetNode(historyNodeNames[i]);
		if (historyNode.hasFn(nodeType)) historyNodes.append(historyNode);
	}
}

// Retrieve the color set information for this mesh
void CMeshHelper::GetMeshValidColorSets(const MObject& mesh, DaeColourSetList& colorSets)
{
	if (!mesh.hasFn(MFn::kMesh)) return;
	MFnMesh meshFn(mesh);

	uint colorSetCount = 0;
	MStringArray colorSetNames;

	// Starting from Maya 7.0, Color sets are the way to go!
#if MAYA_API_VERSION >= 700

	// Collect the color set names;
	meshFn.getColorSetNames(colorSetNames);
	colorSetCount = colorSetNames.length();

	// Fill in the return information
	if (colorSetCount > 0) colorSets.reserve(colorSetCount + 1);
	for (uint i = 0; i < colorSetCount; ++i)
	{
		DaeColourSet* set = new DaeColourSet();
		set->name = colorSetNames[i];
		set->isBlankSet = false;
		colorSets.push_back(set);
	}

#endif

	// Look for polyColorPerVertex nodes
	// Match them with the color set names.
	DaeColourSet* blankSet = NULL;
	MObjectArray polyColorPerVertexNodes;
	GetHistoryNodes(mesh, MFn::kPolyColorPerVertex, polyColorPerVertexNodes);
	for (uint i = 0; i < polyColorPerVertexNodes.length(); ++i)
	{
		MFnDependencyNode nodeFn(polyColorPerVertexNodes[i]);
		MPlug colorSetNamePlug = nodeFn.findPlug("cn"); // "colorSetName"
		MString nameString; colorSetNamePlug.getValue(nameString);
		if (nameString.length() == 0 && blankSet == NULL)
		{
			// Support finding/exporting one blank polyColorPerVertex node
			blankSet = new DaeColourSet();
			blankSet->name = "_blank-color-set";
			blankSet->polyColorPerVertexNode = polyColorPerVertexNodes[i];
			blankSet->isBlankSet = true;
			colorSets.push_back(blankSet);
		}

		uint colorSetIndex;
		for (colorSetIndex = 0; colorSetIndex < colorSetCount; ++colorSetIndex)
		{
			if (colorSetNames[colorSetIndex] == nameString)
			{
				colorSets[colorSetIndex]->polyColorPerVertexNode = polyColorPerVertexNodes[i];
				break;
			}
		}
	}

#if MAYA_API_VERSION < 700

	if (blankSet == NULL)
	{
		// Support checking the mesh directly for a color set, as it
		// has the same interface as the polyColorPerVertex node for pre-Maya 7.0 vertex colors
		blankSet = new DaeColourSet();
		blankSet->name = "_blank-color-set";
		blankSet->polyColorPerVertexNode = mesh;
		blankSet->isBlankSet = true;
		colorSets.push_back(blankSet);
	}

#else

	// Get the base plug for the color sets information
	// and figure out the logical index for each color set name
	MPlug basePlug = meshFn.findPlug("colorSet");
	uint basePlugElementCount = basePlug.numElements();
	for (uint i = 0; i < basePlugElementCount; ++i)
	{
		MPlug elementPlug = basePlug.elementByPhysicalIndex(i);
		MPlug namePlug = CDagHelper::GetChildPlug(elementPlug, "clsn"); // "colorSetName"
		MString nameString;
		namePlug.getValue(nameString);
		if (nameString.length() == 0) continue;

		for (uint j = 0; j < colorSetCount; ++j)
		{
			if (colorSetNames[j] == nameString)
			{
				MPlug p = CDagHelper::GetChildPlug(elementPlug, "clsp"); // "colorSetPoints"
				colorSets[j]->arrayPlug = p;
			}
		}
	}
#endif
}

void CMeshHelper::GetMeshColorSet(DaeDoc* doc, const MObject& mesh, FCDGeometrySource* source, DaeColourSet& colorSet)
{
	// Is this a mesh?
	if (!mesh.hasFn(MFn::kMesh)) return;
	MFnMesh meshFn(mesh);

	colorSet.isVertexColor = false;

#if MAYA_API_VERSION >= 700
	// Fill out the color set information list
	uint setPlugElementCount = colorSet.arrayPlug.numElements();
	if (CExportOptions::ExportVertexColorAnimations()) source->GetAnimatedValues().reserve(setPlugElementCount);
	if (setPlugElementCount > 0)
	{
		bool opaqueWhiteFound = false;
		source->SetValueCount(setPlugElementCount + 1); // +1 to account for the potential white color.
		float* data = source->GetData();
		for (uint j = 0; j < setPlugElementCount; ++j)
		{
			MPlug colorPlug = colorSet.arrayPlug.elementByPhysicalIndex(j);
			MColor c; CDagHelper::GetPlugValue(colorPlug, c);
			(*data++) = c.r; (*data++) = c.g; (*data++) = c.b; (*data++) = c.a; 
			if (c == MColor(1.0f, 1.0f, 1.0f, 1.0f))
			{
				colorSet.whiteColorIndex = j;
				opaqueWhiteFound = true;
			}

			// Add the animation curve, if there's any (not supported according to the API docs)
			if (CExportOptions::ExportVertexColorAnimations())
			{
				ANIM->AddPlugAnimation(colorPlug.child(0), source->GetSourceData().GetAnimated(4*j+0), kSingle);
				ANIM->AddPlugAnimation(colorPlug.child(1), source->GetSourceData().GetAnimated(4*j+1), kSingle);
				ANIM->AddPlugAnimation(colorPlug.child(2), source->GetSourceData().GetAnimated(4*j+2), kSingle);
				ANIM->AddPlugAnimation(colorPlug.child(3), source->GetSourceData().GetAnimated(4*j+3), kSingle);
			}
		}

		if (!opaqueWhiteFound)
		{
			// Push white at the end of the color set.
			(*data++) = 1.0f; (*data++) = 1.0f; (*data++) = 1.0f; (*data++) = 1.0f;
			colorSet.whiteColorIndex = setPlugElementCount;
		}
		else source->SetValueCount(setPlugElementCount);
	}
	else
#endif
	{
		source->GetAnimatedValues().reserve((size_t) meshFn.numVertices());

		// If requested, bake lighting information into per-vertex colors.
		MFnDependencyNode bakeLightingFn;
		if (colorSet.isBlankSet && CExportOptions::BakeLighting())
		{
			MGlobal::executeCommand("undoInfo -state on -infinity on", false, true);

			bakeLightingFn.create("vertexBakeSet");

			MObjectArray shaderGroups;
			MObjectArray compositions;
			meshFn.getConnectedSetsAndMembers(0, shaderGroups, compositions, true);

			MString melCommand("convertLightmap -vm -sh -co 1 -bo ");
			melCommand += bakeLightingFn.name();
			melCommand += " ";

			uint shaderCount = shaderGroups.length();
			for (uint i = 0; i < shaderCount; ++i)
			{
				MFnDependencyNode fn(shaderGroups[i]);
				melCommand += fn.name();
				melCommand += " ";
				melCommand += meshFn.fullPathName();
				melCommand += " ";
			}
			MGlobal::executeCommand(melCommand, false, true);
			meshFn.syncObject();
		}

		if (CExportOptions::ExportVertexColorAnimations())
		{
			// Prepare an array for the vertex colours, in that we use those instead
			MColorArray vertexColours;
			uint vertexCount = meshFn.numVertices();
			vertexColours.setLength(vertexCount);
			for (uint i = 0; i < vertexCount; ++i) { MColor& c = vertexColours[i]; c.r = 1.0f; c.g = 1.0f; c.b = 1.0f; c.a = -1.0f; }
			const int invalidColorIndex = -1;

			// Gather the per-face-vertex information
			uint polygonCount = meshFn.numPolygons();
			fm::vector<MIntArray> polygonVertexIndices;
			MIntArray polygonOffsets(polygonCount);
			polygonVertexIndices.resize(polygonCount);
			uint faceVertexCount = 0;
			for (uint i = 0; i < polygonCount; ++i)
			{
				polygonOffsets[i] = faceVertexCount;
				MIntArray& faceVertices = polygonVertexIndices[i];
				meshFn.getPolygonVertices(i, faceVertices);
				faceVertexCount += faceVertices.length();
			}

			// Initialize the output index array
			colorSet.indices.setLength(faceVertexCount);
			for (uint i = 0; i < faceVertexCount; ++i)
			{
				colorSet.indices[i] = invalidColorIndex;
			}

			// Grab all the color information from the vertex color node
			MFnDependencyNode nodeFn(colorSet.polyColorPerVertexNode);
			MPlug globalPlug = nodeFn.findPlug("colorPerVertex");
			globalPlug = CDagHelper::GetChildPlug(globalPlug, "vclr"); // "vertexColor"
			uint elementCount = globalPlug.numElements();
			if (elementCount == 0) { colorSet.indices.clear(); return; }

			colorSet.isVertexColor = true; // assume true then get proved wrong..
			size_t maxValueCount = max((size_t) elementCount, (size_t) vertexCount) * 16; // * 16 to give enough space and avoid a resize that would break the export of animations..
			source->SetDataCount(maxValueCount); 
			size_t valueCount = 0;
			for (uint i = 0; i < elementCount; ++i)
			{
				// The logical index may be out-of-order.
				MPlug vertexPlug = globalPlug.elementByPhysicalIndex(i);
				int vertexId = vertexPlug.logicalIndex();
				if (vertexId < 0 || vertexId >= (int) vertexCount) continue;

				MColor& vertexColor = vertexColours[vertexId];

				// Read the face's per-vertex color values
				MPlug perFaceVertexPlug = CDagHelper::GetChildPlug(vertexPlug, "vfcl"); // "vertexFaceColor"
				uint childElementCount = perFaceVertexPlug.numElements();
				for (uint j = 0; j < childElementCount; ++j)
				{
					// Get the information about this polygon
					MPlug faceVertexColorPlug = perFaceVertexPlug.elementByPhysicalIndex(j);
					uint polyIndex = faceVertexColorPlug.logicalIndex();
					if (polyIndex >= polygonCount) continue;
					MIntArray& faceVertices = polygonVertexIndices[polyIndex];
					uint offset = polygonOffsets[polyIndex];

					// Look for the vertex index in the known mesh face-vertex array
					uint vertexOffset = 0;
					for (; vertexOffset < polygonCount; ++vertexOffset)
					{
						if (vertexId == faceVertices[vertexOffset]) break;
					}
					if (vertexOffset == polygonCount) continue;
					offset += vertexOffset;

					// Now that the vertex-face pair is identified, get its color
					MColor faceVertexColor;
					MPlug actualColorPlug = CDagHelper::GetChildPlug(faceVertexColorPlug, "frgb"); // "vertexFaceColorRGB"
					MPlug actualAlphaPlug = CDagHelper::GetChildPlug(faceVertexColorPlug, "vfal"); // "vertexFaceAlpha"
					CDagHelper::GetPlugValue(actualColorPlug, faceVertexColor);
					CDagHelper::GetPlugValue(actualAlphaPlug, faceVertexColor.a);

					if (valueCount >= maxValueCount) break;
					source->SetValue(valueCount, MConvert::ToFMVector4(faceVertexColor));

					ANIM->AddPlugAnimation(actualColorPlug.child(0), source->GetSourceData().GetAnimated(4 * valueCount + 0), kSingle);
					ANIM->AddPlugAnimation(actualColorPlug.child(1), source->GetSourceData().GetAnimated(4 * valueCount + 1), kSingle);
					ANIM->AddPlugAnimation(actualColorPlug.child(2), source->GetSourceData().GetAnimated(4 * valueCount + 2), kSingle);
					ANIM->AddPlugAnimation(actualAlphaPlug, source->GetSourceData().GetAnimated(4 * valueCount + 3), kSingle);
					colorSet.indices[offset] = (int) valueCount++;

					// Check if per-face, per-vertex color is necessary
					if (vertexColor.a == -1.0f) vertexColor = faceVertexColor;
					else colorSet.isVertexColor &= vertexColor == faceVertexColor;
					colorSet.isVertexColor &= !CDagHelper::HasConnection(actualColorPlug, false, true) && !CDagHelper::HasConnection(actualAlphaPlug, false, true);
				}
			}

			if (colorSet.isVertexColor)
			{
				colorSet.indices.clear();

				// Use per-vertex color instead
				const MColor invalid(MColor::kRGB, 0.0f, 0.0f, 0.0f, 1.0f);
				for (uint i = 0; i < elementCount; ++i)
				{
					MPlug vertexPlug = globalPlug.elementByPhysicalIndex(i);
					if (vertexPlug.numChildren() > 0)
					{
						int vertexId = vertexPlug.logicalIndex();
						if (vertexId >= (int) vertexCount) continue;

						// Get the color plug, don't use its color (SourceForge #1283335) if it is (0,0,0,1).
						// but always check for an animation.
						MColor c;
						MPlug colorPlug = CDagHelper::GetChildPlug(vertexPlug, "vrgb"); // "vertexColorRGB"
						MPlug alphaPlug = CDagHelper::GetChildPlug(vertexPlug, "vxal"); // "vertexAlpha"
						CDagHelper::GetPlugValue(colorPlug, c);
						CDagHelper::GetPlugValue(alphaPlug, c.a);

						if (c != invalid || vertexColours[vertexId].a == -1.0f)
						{
							vertexColours[vertexId] = c;

							ANIM->AddPlugAnimation(colorPlug.child(0), source->GetSourceData().GetAnimated(4 * vertexId + 0), kColour);
							ANIM->AddPlugAnimation(colorPlug.child(1), source->GetSourceData().GetAnimated(4 * vertexId + 1), kColour);
							ANIM->AddPlugAnimation(colorPlug.child(2), source->GetSourceData().GetAnimated(4 * vertexId + 2), kColour);
							ANIM->AddPlugAnimation(alphaPlug, source->GetSourceData().GetAnimated(4 * vertexId + 3), kSingle);
						}
					}
				}

				valueCount = vertexCount;
				colorSet.whiteColorIndex = 0;
				for (uint cc = 0; cc < valueCount; ++cc)
				{
					MColor& c = vertexColours[cc];
					if (c.a < 0.0f)
					{
						c.a = 1.0f; // This is triggered only for missing vertex colors. Use white.
						colorSet.whiteColorIndex = cc;
					}
					source->SetValue(cc, MConvert::ToFMVector4(c));
				}
			}
			else
			{
				// Check all the per-vertex colors for valid ones.
				// If we have invalid per-vertex-face colors, we'll assign them a valid per-vertex colors.

				// Re-index any invalid colors into white
				if (valueCount < maxValueCount)
				{
					uint indexCount = colorSet.indices.length();
					uint whiteColorIndex = (uint) valueCount;
					bool needsWhiteColor = false;
					for (uint i = 0; i < indexCount; ++i)
					{
						if (colorSet.indices[i] == invalidColorIndex)
						{
							colorSet.indices[i] = whiteColorIndex;
							needsWhiteColor = true;
						}
					}
					if (needsWhiteColor)
					{
						source->SetValue(whiteColorIndex, FMVector4::One);
						colorSet.whiteColorIndex = whiteColorIndex;
						++valueCount;
					}
				}
			}

			source->SetValueCount(valueCount);
		}
		else
		{
			MColorArray colours;
#if MAYA_API_VERSION >= 700
			if (!colorSet.isBlankSet)
			{
				meshFn.getColors(colours, &colorSet.name);
			}
			else
#endif
			{
				meshFn.getFaceVertexColors(colours);
			}

			if (colours.length() > 0)
			{
				source->SetStride(4);
				source->SetValueCount(colours.length() + 1);

				bool opaqueWhiteFound = false;
				for (uint j = 0; j < colours.length(); ++j)
				{
					FMVector4 v = MConvert::ToFMVector4(colours[j]);
					if (IsEquivalent(v, FMVector4::One))
					{
						opaqueWhiteFound = true;
						colorSet.whiteColorIndex = j;
					}
					source->SetValue(j, v);
				}
				if (!opaqueWhiteFound)
				{
					source->SetValue(colours.length(), FMVector4::One); // put white at the end.
					colorSet.whiteColorIndex = colours.length();
				}
				else source->SetValueCount(colours.length());
			}
		}

		if (colorSet.isBlankSet && CExportOptions::BakeLighting())
		{
			// Delete the bake Lighting temporary per-vertex color node
			MGlobal::executeCommand(MString("delete ") + bakeLightingFn.name());
		}
	}
}
