/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnCompoundAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnSet.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MFnStringArrayData.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNurbsCurveData.h>
//Hack to circumvent BAD code in Maya7.0.
class MFnNurbsSurface;
#include <maya/MFnNurbsSurface.h>
#include <maya/MTrimBoundaryArray.h>
#include "DaeGeometry.h"
#include "DaeDocNode.h"
#include "ColladaFX/CFXPasses.h"
#include "ColladaFX/CFXShaderNode.h"
#include "ColladaFX/CFXParameter.h"
#include "CMeshExporter.h"
#include "CExportOptions.h"
#include "CReferenceManager.h"
#include "CSplineExporter.h"
#include "CNURBSExporter.h"
#include "TranslatorHelpers/CShaderHelper.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDExtra.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDGeometryInstance.h"
#include "FCDocument/FCDGeometryMesh.h"
#include "FCDocument/FCDGeometryPolygons.h"
#include "FCDocument/FCDGeometryPolygonsInput.h"
#include "FCDocument/FCDGeometryPolygonsTools.h"
#include "FCDocument/FCDGeometrySource.h"
#include "FCDocument/FCDGeometrySpline.h"
#include "FCDocument/FCDGeometryNURBSSurface.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDMaterial.h"
#include "FCDocument/FCDMaterialInstance.h"
#include "FCDocument/FCDEffect.h"
#include "FCDocument/FCDTexture.h"
#include "FCDocument/FCDPlaceHolder.h"
#include "FCDocument/FCDEntityReference.h"
#include "../FColladaPlugins/FArchiveXML/FAXColladaParser.h"

//
// DaeGeometryLibrary
//

DaeGeometryLibrary::DaeGeometryLibrary(DaeDoc* doc)
:	doc(doc)
{
}


DaeGeometryLibrary::~DaeGeometryLibrary()
{
	geometryList.clear();
	doc = NULL;
}

void DaeGeometryLibrary::ExportNURBS(FCDSceneNode* sceneNode, const MDagPath& dagPath, bool isLocal)
{
}

void DaeGeometryLibrary::ExportSpline(FCDSceneNode* sceneNode, const MDagPath& dagPath, bool isLocal)
{
	FUAssert(sceneNode != NULL, return);
	
	// Add this spline to our library
	if (isLocal)
	{
		DaeEntity* entity = IncludeSpline(dagPath.node());
		if (entity != NULL)
		{
			doc->GetSceneGraph()->ExportInstance(sceneNode, entity->entity);
			doc->GetSceneGraph()->AddElement(entity);
		}
	}
	else
	{
		// Check for an entity linkage.
		DaeEntity* entity = NODE->FindEntity(dagPath.node());
		if (entity != NULL) doc->GetSceneGraph()->ExportInstance(sceneNode, entity->entity);
		else
		{
			// Generate the XRef URI.
			FCDPlaceHolder* placeHolder = CReferenceManager::GetPlaceHolder(CDOC, dagPath);
			MString name = doc->MayaNameToColladaName(MFnDagNode(dagPath).name(), true);
			FUUri uri(placeHolder->GetFileUrl());
			uri.SetFragment(MConvert::ToFChar(name));

			FCDEntityInstance* colladaInstance = sceneNode->AddInstance(FCDEntity::GEOMETRY);
			FCDEntityReference* xref = colladaInstance->GetEntityReference();
			xref->SetUri(uri);
		}
	}
}

void DaeGeometryLibrary::ExportMesh(FCDSceneNode* sceneNode, const MDagPath& dagPath, bool isLocal, 
									bool instantiate)
{
	FUAssert(sceneNode != NULL, return);

	// Add the controller and/or geometry to our library(ies)
	bool hasSkinController = CExportOptions::ExportJointsAndSkin() && 
			doc->GetControllerLibrary()->HasSkinController(dagPath.node());
	bool hasMorphController = doc->GetControllerLibrary()->HasMorphController(dagPath.node());
	if (isLocal)
	{
		DaeEntity* entity = NULL;
		if (hasSkinController || hasMorphController)
		{
			entity = doc->GetControllerLibrary()->ExportController(sceneNode, dagPath, hasSkinController, 
					instantiate);
		}
		else
		{
			entity = ExportMesh(dagPath.node());
			if (entity != NULL && instantiate) doc->GetSceneGraph()->ExportInstance(sceneNode, entity->entity);
		}
		if (entity != NULL) doc->GetSceneGraph()->AddElement(entity);
	}
	else if (instantiate)
	{
		// Check for an entity linkage.
		DaeEntity* entity = NODE->FindEntity(dagPath.node());
		if (entity != NULL) doc->GetSceneGraph()->ExportInstance(sceneNode, entity->entity);
		else
		{
			// Generate the XRef URI.
			FCDPlaceHolder* placeHolder = CReferenceManager::GetPlaceHolder(CDOC, dagPath);
			FUUri uri(placeHolder->GetFileUrl());
			MString name = doc->MayaNameToColladaName(MFnDagNode(dagPath).name(), true);
			uri.SetFragment(MConvert::ToFChar(name));

			// Create the XRef instance.
			FCDEntity::Type instanceType = 
					hasSkinController || hasMorphController ? FCDEntity::CONTROLLER : FCDEntity::GEOMETRY;
			FCDEntityInstance* colladaInstance = sceneNode->AddInstance(instanceType);
			FCDEntityReference* xref = colladaInstance->GetEntityReference();
			xref->SetUri(uri);
		}
	}
}

// Add a mesh to this library and return the export Id
DaeEntity* DaeGeometryLibrary::ExportMesh(const MObject& meshObject)
{
	MStatus status;
	MFnMesh meshFn(meshObject);
	MString name = doc->MayaNameToColladaName(meshFn.name(), true);

	FCDGeometry* colladaGeometry = CDOC->GetGeometryLibrary()->AddEntity();
	DaeGeometry* geometry = new DaeGeometry(doc, colladaGeometry, meshFn.object());
	colladaGeometry->SetName(MConvert::ToFChar(name));
	if (geometry->isOriginal) colladaGeometry->SetDaeId(TO_STRING(colladaGeometry->GetName()));

	// Don't create a new mesh if this geometry already contains one.
	FCDGeometryMesh* colladaMesh = NULL;
	if (!colladaGeometry->IsMesh()) colladaMesh = colladaGeometry->CreateMesh();
	else
	{
		// Mark all the sources and polygon sets as unused.
		colladaMesh = colladaGeometry->GetMesh();
		for (size_t s = 0; s < colladaMesh->GetSourceCount(); ++s)
		{
			colladaMesh->GetSource(s)->SetUserHandle((void*) 1);
		}
		for (size_t i = 0; i < colladaMesh->GetPolygonsCount(); ++i)
		{
			colladaMesh->GetPolygons(i)->SetUserHandle((void*) 1);
		}
	}


	// Fill up the mesh.
	meshFn.setObject(meshObject);
	CMeshExporter meshExporter(geometry, meshFn, colladaMesh, doc);
	meshExporter.ExportMesh();

	// Release all the unused sources and polygon sets.
	FCDGeometrySourceList unusedSources;
	for (size_t s = 0; s < colladaMesh->GetSourceCount(); ++s)
	{
		if (colladaMesh->GetSource(s)->GetUserHandle() == (void*) 1) unusedSources.push_back(colladaMesh->GetSource(s));
	}
	FCDGeometryPolygonsList unusedPolygons;
	for (size_t i = 0; i < colladaMesh->GetPolygonsCount(); ++i)
	{
		if (colladaMesh->GetPolygons(i)->GetUserHandle() == (void*) 1) unusedPolygons.push_back(colladaMesh->GetPolygons(i));
	}
	CLEAR_POINTER_VECTOR(unusedSources);
	CLEAR_POINTER_VECTOR(unusedPolygons);

	doc->GetEntityManager()->ExportEntity(meshObject, colladaGeometry);
	return geometry;
}

void DaeGeometryLibrary::ExportInstanceInfo(const MDagPath& sceneNodePath, FCDGeometry* colladaGeometry, FCDGeometryInstance* colladaInstance)
{
	MStatus status;
	DaeGeometry* geometry = (DaeGeometry*) colladaGeometry->GetUserHandle();

	// Mark all the existing and imported material instances as unused
	size_t materialInstanceCount = colladaInstance->GetMaterialInstanceCount();
	for (size_t m = 0; m < materialInstanceCount; ++m)
	{
		colladaInstance->GetMaterialInstance(m)->SetUserHandle((void*) 1);
	}

	if (colladaGeometry->IsMesh())
	{
		MFnMesh meshFn(geometry->GetNode(), &status);
		if (status != MStatus::kSuccess) return;

		// Retrieve a dagpath to the unique instance.
		// As this code may be called more than once to handle each visible instance.
		MDagPath meshPath = sceneNodePath;
		meshPath.push(geometry->GetNode());

		FCDGeometryMesh* colladaMesh = colladaGeometry->GetMesh();

		MObjectArray shaders;
		MIntArray shaderIndices;
		meshFn.getConnectedShaders(meshPath.instanceNumber(), shaders, shaderIndices);
		size_t shaderCount = min((size_t) shaders.length(), colladaMesh->GetPolygonsCount());
		
		for (size_t i = 0; i < shaderCount; ++i)
		{
			// Connect the polygons' material symbol to the actual material library element
			DaeMaterial* material = doc->GetMaterialLibrary()->ExportMaterial(shaders[(uint) i]);
			if (material == NULL) continue;
			FCDMaterial* colladaMaterial = (FCDMaterial*) &*(material->entity);
			FCDGeometryPolygons* colladaPolygons = colladaMesh->GetPolygons(i);

			// Look for an existing imported material instance for this polygon set, using the material semantic.
			FCDMaterialInstance* matInstance = NULL;
			const fstring& materialSemantic = colladaPolygons->GetMaterialSemantic();
			for (size_t m = 0; m < materialInstanceCount; ++m)
			{
				FCDMaterialInstance* it = colladaInstance->GetMaterialInstance(m);
				if (it->GetUserHandle() == (void*) 1 && IsEquivalent(it->GetSemantic(), materialSemantic))
				{
					matInstance = it;
					it->SetUserHandle((void*) 0);
					break;
				}
			}
			if (matInstance == NULL) matInstance = colladaInstance->AddMaterialInstance(colladaMaterial, colladaPolygons);
			else
			{
				// Assign the new material to this material instance.
				// For now: delete all the internals of the imported material instance.
				matInstance->SetMaterial(colladaMaterial);
				while (matInstance->GetBindingCount() > 0) matInstance->GetBinding(matInstance->GetBindingCount() - 1)->Release();
				while (matInstance->GetVertexInputBindingCount() > 0) matInstance->GetVertexInputBinding(matInstance->GetVertexInputBindingCount() - 1)->Release();
			}
			
			// Find the input connections to the shader
			MFnDependencyNode shaderFn(material->GetNode());

			// Export material-specific bindings
			if (shaderFn.typeName() == "colladafxShader") 
			{
				ExportVertexAttribute(matInstance, shaderFn, colladaGeometry);
				ExportObjectBinding(matInstance, shaderFn);
			}
			else if (shaderFn.typeName() == "colladafxPasses") 
			{
				CFXPasses* passes = (CFXPasses*) shaderFn.userNode();

				MStringArray techNames;
				passes->getTechniqueNames(techNames);
				for (uint tech = 0; tech < techNames.length(); ++tech)
				{
					MString techName = techNames[tech].asChar() + 3; // removes the "cfx" prefix
					uint passCount = passes->getPassCount(techName);
					for (uint pass = 0; pass < passCount; ++pass)
					{
						MFnDependencyNode shaderFn(passes->getPass(techName,pass));
						if (shaderFn.object().isNull()) continue;
						ExportVertexAttribute(matInstance, shaderFn, colladaGeometry);
						ExportObjectBinding(matInstance, shaderFn);
					}
				}
			}
			else
			{
				doc->GetMaterialLibrary()->ExportTextureBinding(colladaGeometry, matInstance, shaderFn.object());
			}
		}
	}

	// Delete all the unused imported material instances
	for (size_t m = 0; m < colladaInstance->GetMaterialInstanceCount();)
	{
		FCDMaterialInstance* materialInstance = colladaInstance->GetMaterialInstance(m);
		if (materialInstance->GetUserHandle() == (void*) 1) materialInstance->Release();
		else ++m;
	}
}

void DaeGeometryLibrary::Import()
{
	FCDGeometryLibrary* library = CDOC->GetGeometryLibrary();
	for (size_t i = 0; i < library->GetEntityCount(); ++i)
	{
		FCDGeometry* geometry = library->GetEntity(i);
		if (geometry->GetUserHandle() == NULL) ImportGeometry(geometry);
	}
}

// Create a Maya mesh from the given COLLADA geometry entity
DaeEntity* DaeGeometryLibrary::ImportGeometry(FCDGeometry* colladaGeometry)
{
	// Verify that we haven't imported this geometry already
	if (colladaGeometry->GetUserHandle() != NULL)
	{
		return (DaeEntity*) colladaGeometry->GetUserHandle();
	}

	// Create the connection object for this geometry
	DaeGeometry* geometry = new DaeGeometry(doc, colladaGeometry, MObject::kNullObj);
	geometryList.push_back(geometry);

	if (colladaGeometry->IsMesh())
	{
		ImportMesh(geometry, colladaGeometry->GetMesh());
	}
	else if (colladaGeometry->IsSpline())
	{
		size_t curSplineCount = 0;

		FCDGeometrySpline* colladaSpline = colladaGeometry->GetSpline();
		size_t splineCount = colladaSpline->GetSplineCount();
		for (size_t i = 0; i < splineCount; ++i)
		{
			FCDSpline *spline = colladaSpline->GetSpline(i);

			if (spline->GetSplineType() == FUDaeSplineType::LINEAR)
			{
				FCDBezierSpline bezier(NULL);
				FCDNURBSSplineList nurbsList;
				FCDLinearSpline* lin = (FCDLinearSpline*) spline;
				lin->ToBezier(bezier);
				bezier.ToNURBs(nurbsList);
				for (size_t j = 0; j < nurbsList.size(); j++)
				{
					FCDNURBSSpline *nurb = nurbsList[j];
					ImportSpline(geometry, nurb, curSplineCount);
					
					delete nurb;
					curSplineCount++;
				}
				nurbsList.clear();
			}
			else if (spline->GetSplineType() == FUDaeSplineType::BEZIER)
			{
				FCDNURBSSplineList nurbsList;
				FCDBezierSpline* bez = (FCDBezierSpline*) spline;
				bez->ToNURBs(nurbsList);
				for (size_t j = 0; j < nurbsList.size(); j++)
				{
					FCDNURBSSpline *nurb = nurbsList[j];
					ImportSpline(geometry, nurb, curSplineCount);
					
					delete nurb;
					curSplineCount++;
				}
				nurbsList.clear();
			}
			else if (spline->GetSplineType() == FUDaeSplineType::NURBS)
			{
				ImportSpline(geometry, (FCDNURBSSpline*)spline, curSplineCount);
				curSplineCount++;
			}
		}
	}

	CDagHelper::SetPlugValue(geometry->GetNode(), "visibility", false);
	MObject entityObject = geometry->GetNode();
	doc->GetEntityManager()->ImportEntity(entityObject, geometry);
	return geometry;
}

void DaeGeometryLibrary::InstantiateGeometry(DaeEntity* sceneNode, FCDGeometryInstance* instance, FCDGeometry* colladaGeometry)
{
	DaeGeometry* geometry = (DaeGeometry*) ImportGeometry(colladaGeometry);
	if (geometry == NULL) return;

	// Parent the geometry to this scene node
	geometry->Instantiate(sceneNode);

	// Instantiate the polygon materials
	InstantiateGeometryMaterials(instance, colladaGeometry, geometry, sceneNode);
}

void DaeGeometryLibrary::ImportMesh(DaeGeometry* geometry, FCDGeometryMesh* colladaMesh)
{
	// Preprocess the mesh polygons sets in order to triangulate the non-supported primitive types.
	size_t polygonsCount = colladaMesh->GetPolygonsCount();
	bool recalculate = false;
	for (size_t i = 0; i < polygonsCount; ++i)
	{
		FCDGeometryPolygons* colladaPolys = colladaMesh->GetPolygons(i);
		if (colladaPolys->GetPrimitiveType() != FCDGeometryPolygons::POLYGONS)
		{
			FCDGeometryPolygonsTools::Triangulate(colladaPolys, false);
			recalculate = true;
		}
	}
	if (recalculate) colladaMesh->Recalculate();

	// Create the Maya mesh object
	MObject mesh = ImportMeshObject(geometry, colladaMesh);
	if (mesh == MObject::kNullObj) return;

	// Import the actual per-vertex information according to the <polygons> inputs
	ImportMeshNormals(mesh, geometry, colladaMesh);
	ImportMeshTexCoords(mesh, geometry, colladaMesh);
	bool hasVertexColor = ImportMeshVertexColors(mesh, geometry, colladaMesh);
	hasVertexColor |= ImportColorSets(mesh, colladaMesh);
	ImportVertexBlindData(mesh, geometry);

	// Set the per-vertex color display
	MFnMesh meshFn(mesh);
	if (hasVertexColor)
	{
		MGlobal::executeCommand(MString(MGlobal::mayaVersion().asFloat() > 5.9f ? 
						"polyOptions -cm \"diffuse\" -mb \"multiply\" -cs on " : 
						"polyOptions -cm \"diffuse\" -cs on ") + meshFn.fullPathName());
	}

	// Check for the double-sidedness flag in the extra parameters
	FCDETechnique* mayaTechnique = geometry->entity->GetExtra()->GetDefaultType()->FindTechnique(DAEMAYA_MAYA_PROFILE);
	if (mayaTechnique != NULL)
	{
		FCDENode* parameter = mayaTechnique->FindParameter(DAESHD_DOUBLESIDED_PARAMETER);
		if (parameter == NULL) parameter = mayaTechnique->FindParameter(DAEMAYA_DOUBLE_SIDED_PARAMETER);
		if (parameter != NULL)
		{
			bool isDoubleSided = FUStringConversion::ToBoolean(parameter->GetContent());
			CDagHelper::SetPlugValue(mesh, "doubleSided", isDoubleSided);
			SAFE_RELEASE(parameter);
		}
	}
	meshFn.updateSurface();

	// Create holes: assumes that the hole face index list is in increasing order.
	for (size_t i = 0; i < polygonsCount; ++i)
	{
		FCDGeometryPolygons* polygons = colladaMesh->GetPolygons(i);
		size_t holeCount = polygons->GetHoleFaceCount();
		const uint32* holeFaces = polygons->GetHoleFaces();
		size_t faceOffset = polygons->GetFaceOffset();

		// Create the holes
		for (size_t h = 0; h < holeCount; ++h)
		{
			uint originalFaceIndex = (uint) (holeFaces[h] + faceOffset - h - 1);
			MString cmd = MString("polyMergeFacet -ch off -ff ") + originalFaceIndex + " -sf " + (originalFaceIndex + 1) + " " + meshFn.fullPathName();
			MGlobal::executeCommand(cmd);
			meshFn.updateSurface();
			meshFn.syncObject();
		}
	}

	// Load possible vertex position animations
	FCDGeometrySource* positionSource = colladaMesh->GetPositionSource();
	FUObjectContainer<FCDAnimated>& animatedValues = positionSource->GetAnimatedValues();
	MPlug pntsArrayPlug = meshFn.findPlug("pt");
	uint32 positionStride = positionSource->GetStride();
	for (FCDAnimatedList::iterator itA = animatedValues.begin(); itA != animatedValues.end(); ++itA)
	{
		ldiv_t d = ldiv((*itA)->GetArrayElement(), positionStride);
		if (d.rem >= 3) continue; // only 3D positions supported.
		MPlug vertexPlug = pntsArrayPlug.elementByLogicalIndex(d.quot);
		MPlug coordinatePlug = vertexPlug.child(d.rem);
		ANIM->AddPlugAnimation(coordinatePlug, (*itA), (SampleType) (kSingle | kLength)); // "pnts"
	}
}

void DaeGeometryLibrary::ImportSpline(DaeGeometry* geometry, FCDNURBSSpline* colladaSpline, size_t cur)
{
	ImportNURBS(geometry, colladaSpline, cur);
}

// Create the mesh object, importing the vertex' positions from the COLLADA entity
MObject DaeGeometryLibrary::ImportMeshObject(DaeGeometry* geometry, FCDGeometryMesh* colladaMesh)
{
	// Get the vertex position data
	FCDGeometrySource* positionSource = colladaMesh->GetPositionSource();
	MConvert::ToMPointArray(positionSource->GetData(), positionSource->GetValueCount(), positionSource->GetStride(), geometry->points);

	// Retrieve the face vertex counts and the indices from each geometry's polygons data
	size_t polygonsCount = colladaMesh->GetPolygonsCount();
	for (size_t i = 0; i < polygonsCount; ++i)
	{
		// Copy into one massive array, the face vertex counts
		FCDGeometryPolygons* polygons = colladaMesh->GetPolygons(i);
		if ((polygons->GetPrimitiveType() == FCDGeometryPolygons::LINE_STRIPS) ||
				(polygons->GetPrimitiveType() == FCDGeometryPolygons::LINES) ||
				(polygons->GetPrimitiveType() == FCDGeometryPolygons::POINTS))
		{
			continue; // don't want to add these primitive type since they are not meshes
		}
		MConvert::AppendToMIntArray(polygons->GetFaceVertexCounts(), polygons->GetFaceVertexCountCount(), geometry->faceVertexCounts);

		// Merge in the polygon connects
		FCDGeometryPolygonsInput* positionInput = polygons->FindInput(FUDaeGeometryInput::POSITION);
		uint32* positionIndices = positionInput->GetIndices();
		MConvert::AppendToMIntArray(positionIndices, positionInput->GetIndexCount(), geometry->faceConnects);
	}

	if (geometry->faceVertexCounts.length() == 0)
	{
		return MObject::kNullObj;
	}

	// Create the Maya mesh node
	MFnMesh meshFn;
	int polygonCount = (int) (colladaMesh->GetFaceCount() + colladaMesh->GetHoleCount());
	MFnDagNode dummyNodeFn;
	dummyNodeFn.create("transform", "___DummyTransform___");
	MObject mesh = meshFn.create(geometry->points.length(), polygonCount, geometry->points,
		geometry->faceVertexCounts, geometry->faceConnects, dummyNodeFn.object());
	meshFn.setName(geometry->colladaGeometry->GetName().c_str());
	geometry->SetNode(meshFn.object());

	if (!CheckMeshConsistency(mesh, colladaMesh)) return MObject::kNullObj;

	// Import Maya's note
	return mesh;

}

bool DaeGeometryLibrary::CheckMeshConsistency(MObject& mesh, FCDGeometryMesh* colladaMesh)
{
	MFnMesh meshFn(mesh);

	// Safety check: if there is bad data, Maya may have thrown out some of the faces
	// That breaks virtually everything.
	int numPoly = meshFn.numPolygons();
	if (numPoly < (int) (colladaMesh->GetFaceCount() + colladaMesh->GetHoleCount()))
	{
		// ensures Maya exports tesellation data properly; especially for certain bad triangle strip tests
		if (numPoly > 1)
		{
			MStatus status;
			int polygonVertexCount = meshFn.polygonVertexCount(0, &status);
			if (status)
			{
				MIntArray vertexList;
				meshFn.getPolygonVertices(0, vertexList);
				MPointArray vertexArray;
				meshFn.getPoints(vertexArray);

				MPointArray newVertices;
				for (int i = 0; i < polygonVertexCount; i++)
				{
					newVertices.append(MPoint(vertexArray[vertexList[i]]));
				}
				
				meshFn.deleteFace(0);
				meshFn.addPolygon(newVertices, 0, true, 0.001);
			}
		}


		MGlobal::displayError(MString("FATAL ERROR: Mesh, '") + meshFn.fullPathName() + "' has bad data.");
		return false;
	}

	return true;
}

struct DaeEdge
{
public:
	uint32 normals[2][2];
	FCDGeometrySource* sources[2];
};
typedef fm::vector<DaeEdge, true> DaeEdgeList;

void DaeGeometryLibrary::ImportMeshNormals(MObject& mesh, DaeGeometry* geometry, FCDGeometryMesh* colladaMesh)
{
	// Initialized here for performance purposes.
	MIntArray polyEdges;

	MFnMesh meshFn(mesh);
	const MIntArray& vertexTessellation = geometry->faceConnects;
	uint vertexCount = meshFn.numVertices();

	// Set-up the per-vertex buckets for the smooth/hard normals triage
	Int32List vertexNormalIndices(vertexCount, -1);
	FCDGeometrySourceList vertexHardFlags(vertexCount);

	// At the same time, generate the edge smoothness using the normals.
	DaeEdgeList edges;
	uint32 edgeCount = meshFn.numEdges();
	if (!CImportOptions::ImportNormals())
	{
		DaeEdge emptyEdge;
		emptyEdge.normals[0][0] = emptyEdge.normals[0][1] = ~(uint32)0;
		emptyEdge.normals[1][0] = emptyEdge.normals[1][1] = ~(uint32)0;
		emptyEdge.sources[0] = emptyEdge.sources[1] = NULL;
		edges.resize(edgeCount, emptyEdge);
	}

	// Find all the normal sources to fill in the triage buckets
	bool hasNormals = false;
	MItMeshPolygon polyIt(meshFn.object());
	size_t polygonsCount = colladaMesh->GetPolygonsCount();
	for (size_t i = 0; i < polygonsCount && !polyIt.isDone(); ++i)
	{
		FCDGeometryPolygons* polygons = colladaMesh->GetPolygons(i);
		FCDGeometryPolygonsInput* input = polygons->FindInput(FUDaeGeometryInput::NORMAL);
		FCDGeometrySource* source = input != NULL ? input->GetSource() : NULL;
		size_t sourceStride = source != NULL ? source->GetStride() : 0;
		uint32* sourceIndices = input != NULL ? input->GetIndices() : NULL;
		bool localHasNormals = sourceStride >= 3 && sourceIndices != NULL;
		hasNormals |= localHasNormals;

		uint32 offset = 0;
		size_t faceVertexCountCount = polygons->GetFaceVertexCountCount();
		size_t faceVertexOffset = polygons->GetFaceVertexOffset();

		const uint32* faceVertexCounts = polygons->GetFaceVertexCounts();
		for (size_t j = 0; j < faceVertexCountCount && !polyIt.isDone(); ++j)
		{
			int32 faceVertexCount = faceVertexCounts[j];

			// Retrieve the per-polygon edge information
			polyEdges.clear();
			polyIt.getEdges(polyEdges);

			if (localHasNormals)
			{
				for (int32 k = 0; k < faceVertexCount; ++k)
				{
					int vertexIndex = vertexTessellation[(uint) (faceVertexOffset + offset + k)];
					int32 normalIndex = sourceIndices[offset + k];

					if (CImportOptions::ImportNormals())
					{
						int32 currentNormalIndex = vertexNormalIndices[vertexIndex];
						if (currentNormalIndex == -1)
						{
							// First time we encounter this normal, so assign it to our per-vertex bucket
							vertexNormalIndices[vertexIndex] = normalIndex;
							vertexHardFlags[vertexIndex] = source;
						}
						else if (vertexHardFlags[vertexIndex] != NULL && (currentNormalIndex != normalIndex || vertexHardFlags[vertexIndex] != source))
						{
							// Also verify that the actual value of the normal is different..
							const float* originalNormal = source->GetValue(currentNormalIndex);
							const float* newNormal = source->GetValue(normalIndex);
							bool different = false;
							for (size_t n = 0; n < sourceStride; ++n) { different |= !IsEquivalent(newNormal[n], originalNormal[n]); }
							if (different)
							{
								// We have seen this vertex before, with a different normal, so no smoothing
								vertexHardFlags[vertexIndex] = NULL;
							}
						}
					}
					else
					{
						// Process this edge.
						int32 edgeIndex = polyEdges[k];
						DaeEdge& edge = edges[edgeIndex];
						int32 leftNormalIndex = normalIndex;
						int32 rightNormalIndex = sourceIndices[offset + ((k == faceVertexCount - 1) ? 0 : (k + 1))];
						int32 spot = (edge.normals[0][0] != ~(uint32)0);
						edge.normals[spot][spot] = leftNormalIndex;
						edge.normals[1 - spot][spot] = rightNormalIndex;
						edge.sources[spot] = source;
					}
				}
			}
			else
			{
				// COLLADA 1.3 defines hard edges as the default when there are no normals provided
				uint localEdgeCount = polyEdges.length();
				for (uint k = 0; k < localEdgeCount; ++k)
				{
					meshFn.setEdgeSmoothing(polyEdges[k], false);
				}
			}

			// Advance
			polyIt.next();
			offset += faceVertexCount;
		}
	}

	if (hasNormals)
	{
		if (CImportOptions::ImportNormals())
		{
			// Load the per-face, per-vertex normals, according to the <polygon>-specific data
			polyIt.reset();
			for (uint32 i = 0; i < polygonsCount; ++i)
			{
				if (polyIt.isDone()) break;

				FCDGeometryPolygons* polygons = colladaMesh->GetPolygons(i);
				FCDGeometryPolygonsInput* input = polygons->FindInput(FUDaeGeometryInput::NORMAL);

				size_t faceOffset = polygons->GetFaceOffset() + polygons->GetHoleOffset();
				size_t faceVertexOffset = polygons->GetFaceVertexOffset();
				size_t faceVertexCount = polygons->GetFaceVertexCountCount();

				// Retrieve the data source and the associated tessellation indices
				FCDGeometrySource* source = (input != NULL) ? input->GetSource() : NULL;
				uint32* sourceIndices = (input != NULL) ? input->GetIndices() : NULL;
				if (input == NULL || source == NULL || source->GetStride() < 3 || sourceIndices == NULL) continue;

				// Prepare the data arrays for the setFaceVertexNormals call
				uint maxNormalCount = (uint) polygons->GetFaceVertexCount();
				MIntArray faces(maxNormalCount);
				MIntArray globalVertIds(maxNormalCount);
				MVectorArray unindexedNormals(maxNormalCount);

				const uint32* faceVertexCounts = polygons->GetFaceVertexCounts();
				int32 indexedNormalCount = (int32) source->GetValueCount();

				// Unindex the normals according to the <polygons> entries to set them on the mesh.
				uint32 offset = 0, globalIndex = 0;
				for (uint32 j = 0; j < faceVertexCount; ++j)
				{
					uint32 faceIndex = (uint) (faceOffset + j);
					uint32 polyVertexCount = faceVertexCounts[j];
					for (uint32 v = 0; v < polyVertexCount; ++v)
					{
						// Only set per-face, per-vertex normals when necessary
						int32 vertexIndex = vertexTessellation[(uint) (faceVertexOffset + offset + v)];
						if (vertexHardFlags[vertexIndex] == NULL)
						{
							int32 normalIndex = sourceIndices[offset + v];
							if (normalIndex >= 0 && normalIndex < indexedNormalCount)
							{
								faces[globalIndex] = faceIndex;
								globalVertIds[globalIndex] = vertexIndex;
								unindexedNormals[globalIndex] = MConvert::ToMVector(source->GetData(), source->GetStride(), normalIndex);
								++globalIndex;
							}
						}
					}
					offset += polyVertexCount;
					polyIt.next();
				}
				if (globalIndex == 0) continue;

				// In theory, globalIndex should be smaller or equal to unindexedNormalCount
				if (globalIndex < maxNormalCount)
				{
					faces.setLength(globalIndex);
					unindexedNormals.setLength(globalIndex);
					globalVertIds.setLength(globalIndex);
				}

				if (meshFn.setFaceVertexNormals(unindexedNormals, faces, globalVertIds) != MStatus::kSuccess)
				{
					MGlobal::displayError("Error assigning face vertex normals.");
				}
			}

			// Enforce the per-vertex normals that were found to be shared accross one vertices
			MIntArray vertexNormalIds;
			MVectorArray vertexNormals;
			uint32 vertexArrayIndex = 0;
			vertexNormalIds.setLength(vertexCount);
			vertexNormals.setLength(vertexCount);
			for (uint i = 0; i < vertexCount; ++i)
			{
				int32 normalIndex = vertexNormalIndices[i];
				FCDGeometrySource* source = vertexHardFlags[i];
				if (source != NULL)
				{
					// Assign this normal to this vertex
					vertexNormalIds[vertexArrayIndex] = i;
					vertexNormals[vertexArrayIndex] = MConvert::ToMVector(source->GetData(), source->GetStride(), normalIndex);
					++vertexArrayIndex;
				}
			}
			if (vertexArrayIndex > 0)
			{
				vertexNormals.setLength(vertexArrayIndex);
				vertexNormalIds.setLength(vertexArrayIndex);
				meshFn.setVertexNormals(vertexNormals, vertexNormalIds);
			}

			meshFn.cleanupEdgeSmoothing();
		}
		else
		{
			// Need to process the edge-smoothness: must be done after any setVertexNormals..
			for (uint32 i = 0; i < edgeCount; ++i)
			{
				DaeEdge& edge = edges[i];

				// Process this edge only if two-sided.
				if (edge.sources[0] == NULL || edge.sources[1] == NULL) continue;

				// Determine the edge smoothness.
				bool smooth = true;
				if (edge.normals[0][0] != edge.normals[0][1] || edge.normals[1][0] != edge.normals[1][1]) // double-check for performance reasons.
				{
					for (int w = 0; w < 2; ++w)
					{
						if (edge.normals[w][0] != edge.normals[w][1])
						{
							// Also check the actual values of the normal vectors.
							const float* normal1 = edge.sources[w]->GetValue(edge.normals[w][0]);
							const float* normal2 = edge.sources[w]->GetValue(edge.normals[w][1]);
							for (int x = 0; x < 3; ++x) smooth &= IsEquivalent(normal1[x], normal2[x]);
						}
					}
				}
				meshFn.setEdgeSmoothing(i, smooth);
			}
		}
	}

	meshFn.cleanupEdgeSmoothing();
	meshFn.updateSurface();
	meshFn.syncObject();
}

void DaeGeometryLibrary::ImportMeshTexCoords(MObject& mesh, DaeGeometry* geometry, FCDGeometryMesh* colladaMesh)
{
	MStatus status;
	MFnMesh meshFn(mesh);

	// Gather the Texcoords inputs and count the necessary number of UV sets
	FCDGeometrySourceList sources;
	size_t polygonsCount = colladaMesh->GetPolygonsCount();
	for (size_t i = 0; i < polygonsCount; ++i)
	{
		FCDGeometryPolygons* polygons = colladaMesh->GetPolygons(i);

		// Find the texture coordinate inputs
		FCDGeometryPolygonsInputList inputs;
		polygons->FindInputs(FUDaeGeometryInput::TEXCOORD, inputs);
		if (inputs.empty()) continue;

		// Match the uvSets and create ones as necessary
		for (FCDGeometryPolygonsInputList::iterator itI = inputs.begin(); itI != inputs.end(); ++itI)
		{
			FCDGeometryPolygonsInput* input = *itI;
			FCDGeometrySource* source = input->GetSource();
			if (source == NULL) continue;

			// Keep only new sources
			FCDGeometrySourceList::iterator itS = sources.find(source);
			if (itS == sources.end()) sources.push_back(source);
//			input->set = (int32) sources.size() - 1;

			// Make sure that the source has a name
			FUStringBuilder mapName(FS("map")); mapName.append((uint32) sources.size());
			if (source->GetName().empty()) source->SetName(mapName.ToString());
		}
	}
	uint32 sourceCount = (uint32) sources.size();
	if (sourceCount == 0) return;

	// Get the default uvSets and create all the necessary uvSets
	MStringArray uvSetNames;
	meshFn.getUVSetNames(uvSetNames);
	uint32 defaultUvSetCount = (uint32) uvSetNames.length();
	uvSetNames.setLength(sourceCount);
	geometry->textureSetIndices.resize(sourceCount);
	for (uint32 s = 0; s < sourceCount; ++s)
	{
		FCDGeometrySource* source = sources[s];
		MString uvSetName = source->GetName().c_str();
		if (s < defaultUvSetCount) meshFn.renameUVSet(uvSetNames[s], uvSetName);
		else meshFn.createUVSet(uvSetName);
		uvSetNames[s] = uvSetName;

		// Load in the uvSet data
		if (source->GetStride() < 2)
		{
			MGlobal::displayError("Invalid texture mapping data in mesh: " + meshFn.name());
			continue;
		}
		else if (source->GetStride() > 2)
		{
			MGlobal::displayWarning("Only 2D texture mapping is supported, mesh: " + meshFn.name());
		}

		uint32 rawUVCount = (uint32) source->GetValueCount();
		MFloatArray us, vs; us.setLength(rawUVCount); vs.setLength(rawUVCount);
		for (uint32 v = 0; v < rawUVCount; ++v)
		{
			us[v] = source->GetValue(v)[0];
			vs[v] = source->GetValue(v)[1];
		}
		status = meshFn.setUVs(us, vs, &uvSetName);
		if (status != MStatus::kSuccess)
		{
			MGlobal::displayError(MString("Failed to set UVs on set: ") + uvSetName + MString(". Status: ") + status.errorString());
		}
	}

	// Generate the merged UV indices for the mesh
	fm::vector<MIntArray> allSetIndexCounts(sourceCount);
	fm::vector<MIntArray> allSetIndices(sourceCount);
	for (uint32 i = 0; i < polygonsCount; ++i)
	{
		FCDGeometryPolygons* polygons = colladaMesh->GetPolygons(i);

		// Find the texture coordinate inputs
		FCDGeometryPolygonsInputList inputs;
		polygons->FindInputs(FUDaeGeometryInput::TEXCOORD, inputs);
		if (inputs.empty()) continue;

		for (FCDGeometryPolygonsInputList::iterator itI = inputs.begin(); itI != inputs.end(); ++itI)
		{
			FCDGeometryPolygonsInput* input = *itI;
			FCDGeometrySource* source = input->GetSource();
			if (source == NULL) continue;
			uint32* indices = input->GetIndices();
			if (indices == NULL) continue;

			// Find the index for this source
			FCDGeometrySourceList::iterator itS = sources.find(source);
			if (itS == sources.end()) continue;
			int setIndex = (int) (itS - sources.begin());
			geometry->textureSetIndices[setIndex] = input->GetSet();

			MConvert::AppendToMIntArray(polygons->GetFaceVertexCounts(), polygons->GetFaceVertexCountCount(), allSetIndexCounts[setIndex]);
			MConvert::AppendToMIntArray(indices, input->GetIndexCount(), allSetIndices[setIndex]);
		}

		// Pad all the sources not present in this polygons with zero counts
		size_t faceOffset = polygons->GetFaceOffset() + polygons->GetHoleOffset();
		size_t faceVertexCount = polygons->GetFaceVertexCountCount();
		for (uint32 s = 0; s < sourceCount; ++s)
		{
			MIntArray& setIndexCounts = allSetIndexCounts[s];
			if (setIndexCounts.length() >= faceOffset + faceVertexCount) continue;
			setIndexCounts.setLength((uint) (faceOffset + faceVertexCount));
			for (size_t i = faceOffset; i < faceOffset + faceVertexCount; ++i) setIndexCounts[(uint) i] = 0;
		}
	}

	// Enforce the UV data in the UV sets
	for (uint i = 0; i < sourceCount; ++i)
	{
		status = meshFn.assignUVs(allSetIndexCounts[i], allSetIndices[i], &uvSetNames[i]);
		if (status != MStatus::kSuccess)
		{
			MGlobal::displayError(MString("Failed to assign UVs to set: ") + uvSetNames[i] + MString(". Status: ") + status.errorString());
		}
	}
}

bool DaeGeometryLibrary::ImportMeshVertexColors(MObject& mesh, DaeGeometry* geometry, FCDGeometryMesh* colladaMesh)
{
#if MAYA_API_VERSION >= 700
	return false;
#else

	MStatus status;
	MFnMesh meshFn(mesh);

	// Verify that there are vertex colors
	bool hasVertexColors = false;
	size_t polygonsCount = colladaMesh->GetPolygonsCount();
	for (size_t i = 0; i < polygonsCount; ++i)
	{
		FCDGeometryPolygons* polygons = colladaMesh->GetPolygons(i);
		FCDGeometryPolygonsInput* input = polygons->FindInput(FUDaeGeometryInput::COLOR);
		if (input == NULL) continue;
		FCDGeometrySource* source = input->GetSource();
		if (source == NULL) continue;
		uint32* indices = input->GetIndices();
		if (indices == NULL) continue;

		hasVertexColors = true;
	}
	if (!hasVertexColors) return false;

	// Add a "polyColorPerVertex" node in the history, where we set the vertex colors
	MObject polyColorPerVertexObject = AddPolyColorPerVertexHistoryNode(mesh, geometry);
	meshFn.setObject(mesh);

	// Ready the first level of plug information
	MFnDependencyNode polyColorPerVertexNode(polyColorPerVertexObject);
	MPlug colorPerVertexPlug = polyColorPerVertexNode.findPlug("colorPerVertex");
	MPlug vertexArrayPlug = CDagHelper::GetChildPlug(colorPerVertexPlug, "vclr"); // "vertexColor"

	// Assign the face-vertex pairs their custom colors
	for (size_t i = 0; i < polygonsCount; ++i)
	{
		FCDGeometryPolygons* polygons = colladaMesh->GetPolygons(i);
		FCDGeometryPolygonsInput* input = polygons->FindInput(FUDaeGeometryInput::COLOR);
		if (input == NULL) continue;
		FCDGeometrySource* source = input->GetSource();
		if (source == NULL) continue;
		uint32* indices = input->GetIndices();
		if (indices == NULL) continue;

		size_t faceOffset = polygons->GetFaceOffset() + polygons->GetHoleOffset();
		const uint32* faceVertexCounts = polygons->GetFaceVertexCounts();
		size_t faceCount = polygons->GetFaceVertexCountCount();
		FUObjectContainer<FCDAnimated>& animatedValues = source->GetAnimatedValues();

		// Read the data in
		uint32 offset = 0;
		for (size_t faceIndex = faceOffset; faceIndex < faceOffset + faceCount; ++faceIndex)
		{
			uint32 faceVertexCount = faceVertexCounts[faceIndex - faceOffset];
			MIntArray faceVertices;
			meshFn.getPolygonVertices((int) faceIndex, faceVertices);
			for (uint32 k = 0; k < faceVertexCount; ++k)
			{
				uint32 colorIndex = indices[offset + k];
				MColor color = MConvert::ToMColor(source->GetData(), source->GetStride(), colorIndex);
				uint vertexIndex = (uint) faceVertices[k];

				// Get the face-vertex pair color plug
				MPlug vertexPlug = vertexArrayPlug.elementByLogicalIndex(vertexIndex);
				MPlug vertexFaceArrayPlug = CDagHelper::GetChildPlug(vertexPlug, "vfcl"); // "vertexFaceColor"
				MPlug vertexFacePlug = vertexFaceArrayPlug.elementByLogicalIndex((uint) faceIndex);
				MPlug colorPlug = CDagHelper::GetChildPlug(vertexFacePlug, "frgb"); // "vertexFaceColorRGB"
				MPlug alphaPlug = CDagHelper::GetChildPlug(vertexFacePlug, "vfal"); // "vertexFaceAlpha"
				CDagHelper::SetPlugValue(colorPlug, color);
				alphaPlug.setValue(color.a);

				// Read in any animation curve for this color plug
				for (FCDAnimatedList::iterator itA = animatedValues.begin(); itA != animatedValues.end(); ++itA)
				{
					int32 arrayElement = (*itA)->GetArrayElement();
					ldiv_t d = ldiv(arrayElement, source->GetStride());
					if (d.quot == (int32) colorIndex)
					{
						if (d.rem < 3) ANIM->AddPlugAnimation(colorPlug.child(d.rem), *itA, kSingle);
						else ANIM->AddPlugAnimation(alphaPlug, *itA, kSingle);
					}
				}
			}

			offset += faceVertexCount;
		}
	}

	return true;
#endif
}

MObject DaeGeometryLibrary::AddPolyColorPerVertexHistoryNode(MObject& mesh, DaeGeometry* geometry)
{
	MStatus status;
	MFnDagNode meshFn(mesh);

	// Add a "polyColorPerVertex" node in the mesh' history
	MFnDependencyNode polyColorPerVertexNode;
	polyColorPerVertexNode.create("polyColorPerVertex");
	MString wantedName = meshFn.name();
	meshFn.setName(wantedName + "Orig");

	MFnMesh viewMesh;
	viewMesh.create(geometry->points.length(), geometry->faceVertexCounts.length(), geometry->points,
		geometry->faceVertexCounts, geometry->faceConnects, meshFn.parent(0), &status);
	viewMesh.setName(wantedName);

	CDagHelper::Connect(mesh, "outMesh", polyColorPerVertexNode.object(), "inputPolymesh");
	CDagHelper::Connect(polyColorPerVertexNode.object(), "output", viewMesh.object(), "inMesh");
	CDagHelper::SetPlugValue(mesh, "intermediateObject", true);
	geometry->SetNode(mesh = viewMesh.object());

	return polyColorPerVertexNode.object();
}

bool DaeGeometryLibrary::ImportColorSets(MObject& mesh, FCDGeometryMesh* colladaMesh)
{
#if MAYA_API_VERSION < 700
	return false;
#else

	MStatus status;
	MFnMesh meshFn(mesh);

	// Find all the vertex color sources
	FCDGeometrySourceList sources;
	size_t polygonsCount = colladaMesh->GetPolygonsCount();
	for (size_t i = 0; i < polygonsCount; ++i)
	{
		FCDGeometryPolygons* polygons = colladaMesh->GetPolygons(i);
		FCDGeometryPolygonsInputList inputs;
		polygons->FindInputs(FUDaeGeometryInput::COLOR, inputs);
		for (FCDGeometryPolygonsInputList::iterator itI = inputs.begin(); itI != inputs.end(); ++itI)
		{
			FCDGeometrySource* source = (*itI)->GetSource();
			FCDGeometrySourceList::iterator itS = sources.find(source);
			if (itS == sources.end()) sources.push_back(source);
		}
	}
	if (sources.empty()) return false;

	// Read in the color sources and enforce them onto the mesh plugs
	MStringArray setNames;
	uint32 sourceCount = (uint32) sources.size();
	setNames.setLength(sourceCount);
	MPlug basePlug = meshFn.findPlug("colorSet");
	basePlug.setNumElements(sourceCount);
	for (uint32 i = 0; i < sourceCount; ++i)
	{
		FCDGeometrySource* source = sources[i];
		FUObjectContainer<FCDAnimated>& animatedValues = source->GetAnimatedValues();

		// Name the color set
		setNames[i] = source->GetName().c_str();
		if (setNames[i].length() == 0) setNames[i] = MString("colorSet") + i;

		// Verify that the name is unique
		bool unique = false;
		while (!unique)
		{
			unique = true;
			for (uint j = 0; j < i && unique; ++j)
			{
				unique = setNames[i] != setNames[j];
			}
			if (!unique) setNames[i] += (uint) i;
		}

		// Setup the DG plug information
		MPlug elementPlug = basePlug.elementByLogicalIndex(i);
		MPlug namePlug = CDagHelper::GetChildPlug(elementPlug, "clsn"); // "colorSetName"
		namePlug.setValue(setNames[i]);

		uint32 colourCount = (uint32) source->GetValueCount();
		MPlug coloursPlug = CDagHelper::GetChildPlug(elementPlug, "clsp"); // "colorSetPoints"
		coloursPlug.setNumElements(colourCount);
		for (uint j = 0; j < colourCount; ++j)
		{
			MPlug colourPlug = coloursPlug.elementByLogicalIndex(j);
			MColor colourValue = MConvert::ToMColor(source->GetData(), source->GetStride(), j);
			CDagHelper::SetPlugValue(colourPlug, colourValue);

			// Read in any animation curve for this color plug
			// Got to be a better way to do this one.
			for (FCDAnimatedList::iterator itA = animatedValues.begin(); itA != animatedValues.end(); ++itA)
			{
				int32 arrayElement = (*itA)->GetArrayElement();
				ldiv_t d = ldiv(arrayElement, source->GetStride());
				if (d.quot == (int32) j)
				{
					if (d.rem < 4) ANIM->AddPlugAnimation(colourPlug.child(d.rem), *itA, kSingle);
				}
			}
		}
	}

	// Create an index list for each color set
	fm::vector<MIntArray> allIndices(sourceCount);
	for (uint32 i = 0; i < sourceCount; ++i)
	{
		MIntArray& indices = allIndices[i];
		indices.setLength((uint) colladaMesh->GetFaceVertexCount());
		for (uint32 j = 0; j < colladaMesh->GetFaceVertexCount(); ++j) indices[j] = 0;
	}

	// Set-up the index lists according to the tessellation information
	for (uint32 i = 0; i < polygonsCount; ++i)
	{
		FCDGeometryPolygons* polygons = colladaMesh->GetPolygons(i);
		FCDGeometryPolygonsInputList inputs;
		polygons->FindInputs(FUDaeGeometryInput::COLOR, inputs);
		for (FCDGeometryPolygonsInputList::iterator itI = inputs.begin(); itI != inputs.end(); ++itI)
		{
			// Identify the correct color set
			FCDGeometrySource* source = (*itI)->GetSource();
			FCDGeometrySourceList::iterator itS = sources.find(source);
			if (itS == sources.end()) continue;
			uint32 setIndex = (uint32) (itS - sources.begin());
			uint32* idxIndices = (*itI)->GetIndices();
			if (idxIndices == NULL) continue;

			// Overwrite the index list for this color set with the vertex position index list
			MIntArray& indices = allIndices[setIndex];
			size_t faceVertexOffset = polygons->GetFaceVertexOffset();
			size_t faceVertexCount = polygons->GetFaceVertexCount();
			for (size_t j = 0; j < faceVertexCount; ++j)
			{
				indices[(uint) (faceVertexOffset + j)] = idxIndices[j];
			}
		}
	}

	// Assign the color set indices to the mesh
	for (uint i = 0; i < sourceCount; ++i)
	{
		CHECK_MSTATUS(meshFn.assignColors(allIndices[i], &setNames[i]));
	}

	return true;
#endif
}

class DaeExtraData
{
public:
	int id;
	MStringArray typeNames;
	MStringArray shortNames;
	MStringArray longNames;
	xmlNodeList sources;
};
typedef fm::pvector<DaeExtraData> DaeExtraDataList;

void DaeGeometryLibrary::AddPolyBlindDataNode(MObject& mesh, DaeExtraData* extra, DaeGeometry* geometry)
{
	MStatus status;
	uint meshVertexCount = geometry->points.length();

	MFnDependencyNode polyBlindDataFn;
	polyBlindDataFn.create("polyBlindData");
	CDagHelper::SetPlugValue(polyBlindDataFn.object(), "typeId", extra->id);
	CDagHelper::ConnectToList(polyBlindDataFn.object(), "message", mesh, "blindDataNodes");

	// Create the main per-vertex blind data attribute
	MFnCompoundAttribute cattrCreator;
	cattrCreator.create("vertexBlindData", "vbd");
	cattrCreator.setArray(true);
	cattrCreator.setStorable(true);
	cattrCreator.setWritable(true);
	polyBlindDataFn.addAttribute(cattrCreator.object());

	// Add the blind data children attributes
    uint typeCount = extra->typeNames.length();
	for (uint j = 0; j < typeCount; ++j)
	{
#define IMPORT(typeStr, numeric_type) \
		if (extra->typeNames[j] == typeStr) { \
			MFnNumericAttribute tattrCreator; \
			tattrCreator.create(extra->longNames[j], extra->shortNames[j], numeric_type, 0, &status); \
			tattrCreator.setStorable(true); \
			tattrCreator.setWritable(true); \
			cattrCreator.addChild(tattrCreator.object()); \
			polyBlindDataFn.addAttribute(tattrCreator.object()); \
		}

		IMPORT("int", MFnNumericData::kInt)
		else IMPORT("float", MFnNumericData::kFloat)
		else IMPORT("double", MFnNumericData::kDouble)
		else IMPORT("boolean", MFnNumericData::kBoolean)

#undef IMPORT
	}

	// Enforce the data values
	for (uint j = 0; j < typeCount; ++j)
	{
#define IMPORT(typeStr, dataArrayType) \
		if (extra->typeNames[j] == typeStr) { \
			dataArrayType vertexBlindDataArray; \
			FUDaeParser::ReadSource(extra->sources[j], vertexBlindDataArray); \
			for (uint k = 0; k < meshVertexCount; ++k) { \
				MPlug p1 = polyBlindDataFn.findPlug("vertexBlindData"); \
				p1 = p1.elementByLogicalIndex(k); \
				p1 = CDagHelper::GetChildPlug(p1, extra->shortNames[j]); \
				CDagHelper::SetPlugValue(p1, vertexBlindDataArray.at(k)); \
			} \
		}

		IMPORT("int", Int32List)
		else IMPORT("float", FloatList)
		else IMPORT("double", FloatList)
		else IMPORT("boolean", Int32List)

#undef IMPORT
	}
}

void DaeGeometryLibrary::ImportVertexBlindData(MObject& mesh, DaeGeometry* geometry)
{
	/*
	MStatus status;
	MFnMesh meshFn(mesh);
	uint meshVertexCount = meshFn.numVertices();

	DaeExtraDataList extras;

	// Look for per-vertex blind data inputs
	for (DaeIndexedDataList::iterator itV = geometry->vertices.begin(); itV != geometry->vertices.end(); ++itV)
	{
		DaeIndexedData* vd = *itV;
		if (vd->semantic != kEXTRA) continue;

		// Read the information about the extra data from the XML
		xmlNode* techniqueNode = FindTechnique(vd->sourceNode, DAEMAYA_MAYA_PROFILE);
		if (techniqueNode == NULL) continue;
		xmlNode* accessorNode = FindChildByType(techniqueNode, DAE_ACCESSOR_ELEMENT);
		uint dataCount = ReadNodeCount(accessorNode);
		if (dataCount != meshVertexCount) continue;
		xmlNode* accessorParamNode = FindChildByType(accessorNode, DAE_PARAMETER_ELEMENT);
		string blindDataTypeName = ReadNodeName(accessorParamNode);
		
		int blindDataTypeId = -1;
		MString blindDataLongName;
		MString blindDataShortName;
		xmlNodeList paramNodes;
		FindChildrenByType(techniqueNode, DAE_PARAMETER_ELEMENT, paramNodes);
		for (xmlNodeList::iterator itP = paramNodes.begin(); itP != paramNodes.end(); ++itP)
		{
			xmlNode* paramNode = *itP;
			string paramName = ReadNodeName(paramNode);
			string paramType = ReadNodeProperty(paramNode, DAE_TYPE_ATTRIBUTE);
			const char *content = ReadNodeContentDirect(paramNode);

			if (paramName == DAEMAYA_BLINDTYPEID_PARAMETER)
			{
				blindDataTypeId = FUStringConversion::ToInt32(content);
			}
			else if (paramName == DAEMAYA_BLINDNAME_PARAMETER)
			{
				if (paramType == DAEMAYA_LONGNAME_TYPE) blindDataLongName = content;
				else if (paramType == DAEMAYA_SHORTNAME_TYPE) blindDataShortName = content;
			}
			else
			{
				MGlobal::displayWarning(MString("Unknown EXTRA data parameter: ") + paramName.c_str() + " in mesh: " + geometry->path.fullPathName());
			}
		}
		if (blindDataLongName.length() == 0 || blindDataShortName.length() == 0 || blindDataTypeId == -1) continue;

		// Find the corresponding entry in the extra data buffer
		DaeExtraData* extra = NULL;
		uint extraDataCount = (uint) extras.size();
		for (uint j = 0; j < extraDataCount; ++j)
		{
			if (extras[j]->id == blindDataTypeId) { extra = extras[j]; break; }
		}
		if (extra == NULL)
		{
			extra = new DaeExtraData();
			extra->id = blindDataTypeId;
			extras.push_back(extra);
		}
		extra->longNames.append(blindDataLongName);
		extra->shortNames.append(blindDataShortName);
		extra->typeNames.append(blindDataTypeName.c_str());
		extra->sources.push_back(vd->sourceNode);
	}

	// Create each blind data template
	uint extraDataCount = (uint) extras.size();
	for (int i = 0; i < (int) extraDataCount; ++i)
	{
		DaeExtraData* extra = extras[i];

		if (meshFn.isBlindDataTypeUsed(extra->id))
		{
			// Compare it with ours
			DaeExtraData existing;
			meshFn.getBlindDataAttrNames(extra->id, existing.longNames, existing.shortNames, existing.typeNames);
			bool equivalent = true;
			uint ourNameCount = extra->typeNames.length();
			uint existingNameCount = existing.longNames.length();
			for (uint j = 0; j < ourNameCount && equivalent; ++j)
			{
				bool found = false;
				for (uint k = 0; k < existingNameCount && !found; ++k)
				{
					found = (extra->longNames[j] == existing.longNames[k]
						&& extra->shortNames[j] == existing.shortNames[k]
						&& extra->typeNames[j] == existing.typeNames[k]);
				}

				if (!found) equivalent = false;
			}
			if (!equivalent) { extra->id += 5; --i; continue; }
		}
		else
		{
			status = meshFn.createBlindDataType(extra->id, extra->longNames, extra->shortNames, extra->typeNames);
			if (status != MStatus::kSuccess)
			{
				MGlobal::displayError(MString("Unable to create blind data type: ") + extra->id);
				continue;
			}
		}

		AddPolyBlindDataNode(mesh, extra, geometry);
	}

	CLEAR_POINTER_VECTOR(extras); */
}

struct ShadedComponent { MObject shader; MObject components; };
typedef fm::pvector<ShadedComponent> ShadedComponentList;

void DaeGeometryLibrary::InstantiateGeometryMaterials(FCDGeometryInstance* colladaInstance, FCDGeometry* colladaGeometry, DaeEntity* mayaInstance, DaeEntity* sceneNode)
{
	if (colladaInstance == NULL || colladaGeometry == NULL || mayaInstance == NULL) return;
	DaeGeometry* geometry = (DaeGeometry*) colladaGeometry->GetUserHandle();

	if (colladaGeometry->IsMesh())
	{
		InstantiateMeshMaterials(colladaInstance,colladaGeometry->GetMesh(),geometry,mayaInstance,sceneNode);
	}
}

void DaeGeometryLibrary::InstantiateMeshMaterials(FCDGeometryInstance* colladaInstance, FCDGeometryMesh* colladaMesh, DaeGeometry* geometry, DaeEntity* mayaInstance, DaeEntity* sceneNode)
{
	if (colladaInstance == NULL || colladaMesh == NULL || geometry == NULL || mayaInstance == NULL || sceneNode == NULL) return;
	if (mayaInstance->GetNode().isNull()) return; // something went wrong during the creation of the mesh or its controller(s).

	// Calculate the unique dag path for this instance.
	MDagPath instancePath = MDagPath::getAPathTo(sceneNode->GetNode());
	instancePath.push(mayaInstance->GetNode());

	ShadedComponentList shadedComponents;

	// First, retrieve all the shaders and assign their texture's uvSets
	size_t polygonsCount = colladaMesh->GetPolygonsCount();
	for (size_t p = 0; p < polygonsCount; ++p)
	{
		FCDGeometryPolygons* polygons = colladaMesh->GetPolygons(p);
		const fstring& materialSemantic = polygons->GetMaterialSemantic();
		if (materialSemantic.empty()) continue;

		// Create/Retrieve the correct shader
		FCDMaterialInstance* materialInstance = colladaInstance->FindMaterialInstance(materialSemantic);
		DaeMaterial* matInput = doc->GetMaterialLibrary()->InstantiateMaterial(colladaInstance, materialInstance);
		if (matInput == NULL) continue;
		
		// Assign the uvSets to the shader's inputs
		for (uint i = 0; i < matInput->textureSets.size() && i < matInput->textures.length(); ++i)
		{
			Int32List::iterator itT = geometry->textureSetIndices.find(matInput->textureSets[i]);

			if (itT == geometry->textureSetIndices.end() || itT == geometry->textureSetIndices.begin()) continue;

			// For any textures not using the first UV set, link a chooser to the file texture object
			int setIndex = (int) (itT - geometry->textureSetIndices.begin());
			CShaderHelper::AssociateUVSetWithTexture(matInput->textures[i], mayaInstance->GetNode(), setIndex);
		}

		// Process the vertex input bindings
		MObject matInputNode = matInput->GetNode();
		doc->GetMaterialLibrary()->ImportVertexInputBindings(materialInstance, polygons, matInputNode);

		MFnSet shaderGroupFn(matInput->GetShaderGroup());
		if (polygonsCount > 1)
		{
			MFnSingleIndexedComponent componentFn;
			ShadedComponent* shadedComponent = NULL;
			for (ShadedComponentList::iterator it = shadedComponents.begin(); it != shadedComponents.end(); ++it)
			{
				if ((*it)->shader == matInput->GetShaderGroup()) { shadedComponent = (*it); break; }
			}
			if (shadedComponent == NULL)
			{
				shadedComponent = new ShadedComponent();
				shadedComponents.push_back(shadedComponent);
				shadedComponent->shader = matInput->GetShaderGroup();
				shadedComponent->components = componentFn.create(MFn::kMeshPolygonComponent);
			}
			else
			{
				componentFn.setObject(shadedComponent->components);
			}

			// Assign the faces to the right shading set, for each instance
			for (size_t k = polygons->GetFaceOffset(); k < polygons->GetFaceOffset() + polygons->GetFaceCount(); ++k)
			{
				componentFn.addElement((int) k);
			}
			if (componentFn.isEmpty())
			{
				shadedComponents.pop_back();
				SAFE_DELETE(shadedComponent);
			}
		}
		else
		{
			// [Bug #244] In order to support the hardware renderer, if there is only one shader to assign:
			// assign it to the whole mesh directly.
			CShaderHelper::AttachMeshShader(shaderGroupFn.object(), mayaInstance->GetNode(), instancePath.instanceNumber());
		}
	}

	if (shadedComponents.size() == 1)
	{
		// [Bug #244] If there is only one shader assigned, even though there are multiple polygons, in order to
		// support the hardware renderer:  assign it to the whole mesh directly.
		CShaderHelper::AttachMeshShader(shadedComponents.at(0)->shader, mayaInstance->GetNode(), 
				instancePath.instanceNumber());
	}
	else
	{
		// In the case of multi-material instances, since there may be repeated materials:
		// assign the shaders only when all the component lists have been composed.
		for (ShadedComponentList::iterator it = shadedComponents.begin(); it != shadedComponents.end(); ++it)
		{
			CShaderHelper::AttachMeshShader((*it)->shader, mayaInstance->GetNode(), (*it)->components, instancePath.instanceNumber());
		}
	}
	CLEAR_POINTER_VECTOR(shadedComponents);
}


void DaeGeometryLibrary::ExportObjectBinding(FCDMaterialInstance* instance, MFnDependencyNode shaderFn)
{
	if (shaderFn.typeId() != CFXShaderNode::id) return;
	CFXShaderNode* shader = (CFXShaderNode*) shaderFn.userNode();

	size_t parameterCount = shader->GetParameters().size();
	for (size_t i = 0; i < parameterCount; ++i)
	{
		CFXParameter* parameter = shader->GetParameters()[i];
		const char* semantic = parameter->getSemanticString();
		MPlug connectedPlug;
		if (CDagHelper::GetPlugConnectedTo(parameter->getPlug(), connectedPlug) && !connectedPlug.node().isNull() && connectedPlug.node().hasFn(MFn::kTransform))
		{
			fm::string plugName = connectedPlug.name().asChar();
			while (plugName.find('.') != fm::string::npos) plugName[plugName.find('.')] = '/';
			instance->AddBinding(semantic, plugName);
		}
	}
}

// Add a spline to this library and return the export Id
DaeEntity* DaeGeometryLibrary::IncludeSpline(const MObject& splineObject)
{
	MStatus status;
	MFnNurbsCurve splineFn(splineObject);
	MString name = doc->MayaNameToColladaName(splineFn.name());
	MString libId = name + "-lib";

	// Create the COLLADA geometry entity and its ColladaMaya equivalent
	FCDGeometry* colladaGeometry = CDOC->GetGeometryLibrary()->AddEntity();
	DaeGeometry* geometry = new DaeGeometry(doc, colladaGeometry, splineObject);
	if (geometry->isOriginal) colladaGeometry->SetDaeId(libId.asChar());
	colladaGeometry->SetName(MConvert::ToFChar(name));

	// Create and fill up the curve.
	FCDGeometrySpline* colladaSpline = colladaGeometry->CreateSpline();
	CSplineExporter splineExporter(splineFn, colladaSpline, doc);
	splineExporter.exportCurve();

	doc->GetEntityManager()->ExportEntity(splineObject, colladaGeometry);
	return geometry;
}


MObject CreateNURBSCurve(FCDNURBSSpline *nurb, bool asTrimCurve)
{
	if (nurb == NULL) return MObject::kNullObj;

	const FMVector3List& cvs = nurb->GetCVs();
	const FloatList& knots = nurb->GetKnots();
	const FloatList& weights = nurb->GetWeights();

	// Create the Maya curve node
	MFnNurbsCurve curveFn;
	MObject curve;

	int idegree = nurb->GetDegree();

	MPointArray cvarray;
	MDoubleArray knotsequences;

	size_t cvsCount = cvs.size(); cvsCount = cvsCount;
	size_t knotCount = knots.size(); knotCount = knotCount;

	// sanity check
	if (knotCount != (cvsCount + idegree + 1)) return MObject::kNullObj;

	for (size_t i = 0; i < cvs.size(); ++i)
	{
		cvarray.append(MPoint(cvs[i].x, cvs[i].y, cvs[i].z, weights[i]));
	}

	for (uint i = 0; i < (uint)knots.size(); i++) 
	{
		// beginning and end were added during export
		// TODO. Figure out why Maya has (n+p-1) instead of (n+p+1) knots in its vector.
		if ((i == 0) || (i == knots.size() - 1)) continue;
		knotsequences.append(knots[i]);
	}

	MFnNurbsCurve::Form curveForm = (!nurb->IsClosed()) ? MFnNurbsCurve::kOpen : MFnNurbsCurve::kClosed;
	if (curveForm == MFnNurbsCurve::kClosed && knotsequences[0] < 0.0f) curveForm = MFnNurbsCurve::kPeriodic;

	if (asTrimCurve)
	{
		MFnNurbsCurveData curveCreator;
		MObject curveOwner = curveCreator.create();

		curveFn.create(cvarray, knotsequences, idegree, curveForm, true, false, curveOwner);
		return curveOwner;
	}
	else
	{
		MFnDagNode dummyTransform;
		dummyTransform.create("transform", nurb->GetName().empty() ? "nurbsSpline" : nurb->GetName().c_str());

		curveFn.setName(nurb->GetName().c_str());
		MObject curveTransform = dummyTransform.object();
		MObject curve = curveFn.create(cvarray, knotsequences, idegree, curveForm, false, false, curveTransform);
		return dummyTransform.object();
	}
}

void DaeGeometryLibrary::ImportNURBS(DaeGeometry* geometry, FCDNURBSSpline *nurb, size_t index)
{
	MObject curve = CreateNURBSCurve(nurb, false);

	// NURBS splines are part of a geometry set
	geometry->children.append(curve);
}

void DaeGeometryLibrary::ExportVertexAttribute(FCDMaterialInstance* instance, MFnDependencyNode shaderFn, FCDGeometry* geometry)
{
	if (!geometry->IsMesh()) return;

	FCDGeometryMesh* mesh = geometry->GetMesh();
	FCDGeometryPolygonsList sets;
	mesh->FindPolygonsByMaterial(instance->GetSemantic(), sets);
	if (sets.empty()) return;

	FCDGeometryPolygons* firstSet = sets.front();
	
	fstring previousName = FC("map1");
	const uint sourceCount = 9;
	const char* sourceNames[sourceCount] = { "COLOR0", "TEXCOORD0", "TEXCOORD1", "TEXCOORD2", "TEXCOORD3", "TEXCOORD4", "TEXCOORD5", "TEXCOORD6", "TEXCOORD7" };
	for (uint i = 0; i < sourceCount; ++i)
	{
		MString sAttr;
		CDagHelper::GetPlugValue(shaderFn.object(), sourceNames[i], sAttr);
		if (sAttr.length() == 0) continue;

		if (sAttr == "tangent" || sAttr == "binormal")
		{
			sAttr = (previousName + FC("-") + MConvert::ToFChar(sAttr)).c_str();
		}
			
		FCDGeometrySource* source = mesh->FindSourceByName(MConvert::ToFChar(sAttr));
		FCDGeometryPolygonsInput* sourceInput = firstSet->FindInput(source);
		if (source != NULL && sourceInput != NULL)
		{
			if (source->GetType() == FUDaeGeometryInput::TEXCOORD)
			{
				previousName = source->GetName();
			}
	
			instance->AddVertexInputBinding(sourceNames[i], source->GetType(), sourceInput->GetSet());
		}
	}
}

void DaeGeometryLibrary::LeanMemory()
{
	FCDGeometryLibrary* library = CDOC->GetGeometryLibrary();
	for (size_t i = 0; i < library->GetEntityCount(); ++i)
	{
		FCDGeometry* geometry = library->GetEntity(i);
		if (geometry->IsMesh()) LeanMesh(geometry->GetMesh());
		else if (geometry->IsSpline()) LeanSpline(geometry->GetSpline());
	}
}

void DaeGeometryLibrary::LeanMesh(FCDGeometryMesh* colladaMesh)
{
	// Release the polygon tessellation information
	size_t polyCount = colladaMesh->GetPolygonsCount();
	for (size_t i = 0; i < polyCount; ++i)
	{
		FCDGeometryPolygons* p = colladaMesh->GetPolygons(i);
		size_t inputCount = p->GetInputCount();
		for (size_t j = 0; j < inputCount; ++j)
		{
			FCDGeometryPolygonsInput* input = p->GetInput(j);
			input->SetIndexCount(0);
		}
	}

	// Release the vertex source data
	size_t sourceCount = colladaMesh->GetSourceCount();
	for (size_t i = 0; i < sourceCount; ++i)
	{
		FCDGeometrySource* s = colladaMesh->GetSource(i);
		s->SetDataCount(0);
	}
}

void DaeGeometryLibrary::LeanSpline(FCDGeometrySpline* colladaSpline)
{
	// Release the spline control vertices
	size_t count = colladaSpline->GetSplineCount();
	for (size_t i = 0; i < count; ++i)
	{
		FCDSpline* s = colladaSpline->GetSpline(i);
		s->GetCVs().clear();
		if (s->GetSplineType() == FUDaeSplineType::NURBS)
		{
			FCDNURBSSpline* ns = (FCDNURBSSpline*) s;
			ns->GetWeights().clear();
			ns->GetKnots().clear();
		}
	}
}
