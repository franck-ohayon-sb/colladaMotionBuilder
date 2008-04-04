/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/
/*
	Based on the 3dsMax COLLADA Tools:
	Copyright (C) 2005-2006 Autodesk Media Entertainment
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

// Static Geometry Importer Class

#include "StdAfx.h"
#include "AnimationImporter.h"
#include "ColladaIDCreator.h"
#include "DocumentImporter.h"
#include "GeometryImporter.h"
#include "MaterialImporter.h"
#include "NodeImporter.h"
#include "XRefManager.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDAnimationCurve.h"
#include "FCDocument/FCDAnimationCurveTools.h"
#include "FCDocument/FCDAnimationMultiCurve.h"
#include "FCDocument/FCDEntityInstance.h"
#include "FCDocument/FCDEntityReference.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDGeometryInstance.h"
#include "FCDocument/FCDGeometryMesh.h"
#include "FCDocument/FCDGeometrySpline.h"
#include "FCDocument/FCDGeometryPolygons.h"
#include "FCDocument/FCDGeometryPolygonsInput.h"
#include "FCDocument/FCDGeometryPolygonsTools.h"
#include "FCDocument/FCDGeometrySource.h"
#include "FCDocument/FCDMaterial.h"
#include "FCDocument/FCDMaterialInstance.h"
#include <surf_api.h>

// Defined in EditTriObj, in the 3dsMax samples
#define ET_MASTER_CONTROL_REF	0
#define ET_VERT_BASE_REF		1

// Defined in PolyEdit, in the 3dsMax samples
#define EPOLY_PBLOCK 0
#define EPOLY_MASTER_CONTROL_REF  1
#define EPOLY_VERT_BASE_REF 2

GeometryImporter::GeometryImporter(DocumentImporter* document, NodeImporter* parent)
:	EntityImporter(document, parent)
{
	obj = NULL;
	colladaGeometry = NULL;
	hasMapChannel0 = hasMapChannel1 = false;
	currentImport = NULL;
	isEditablePoly = false;
}

GeometryImporter::~GeometryImporter()
{
	obj = NULL;
	colladaGeometry = NULL;
	textureChannels.clear();
}

Point3 GeometryImporter::ToPoint3(float* floatSource, size_t index, uint32 stride)
{
	float* p = floatSource + index * stride;
	if (stride >= 3) return Point3(p);
	else if (stride == 2) return Point3(p[0], p[1], 0.0f);
	else if (stride == 1) return Point3(p[0], 0.0f, 0.0f);
	else return Point3::Origin;
}

GeomObject* GeometryImporter::GetMeshObject()
{
	if (colladaGeometry->IsMesh()) return (GeomObject*) obj;
	return NULL;
}

Object* GeometryImporter::ImportGeometry(FCDGeometry* _colladaGeometry)
{
	if (_colladaGeometry->IsSpline())
	{
		FCDGeometrySpline* spline = _colladaGeometry->GetSpline();
		Object* splineObj = NULL;

		splineObj = ImportSpline(spline);

		obj = splineObj;
	}
	else if (_colladaGeometry->IsMesh())
	{
		colladaGeometry = _colladaGeometry;
		FCDGeometryMesh* colladaMesh = colladaGeometry->GetMesh();

		// Check all the polygons sets for unsupported primitive types.
		bool hasOnlyTriangles = true, recalculate = false;
		size_t polygonsCount = colladaMesh->GetPolygonsCount();
		for (size_t i = 0; i < polygonsCount; ++i)
		{
			FCDGeometryPolygons* colladaPolys = colladaMesh->GetPolygons(i);
			if (colladaPolys->GetPrimitiveType() != FCDGeometryPolygons::POLYGONS)
			{
				FCDGeometryPolygonsTools::Triangulate(colladaPolys, false);
				recalculate = true;
				if (colladaPolys->GetPrimitiveType() != FCDGeometryPolygons::POLYGONS) return NULL;
			}
			else hasOnlyTriangles &= colladaPolys->TestPolyType() == 3;
		}
		if (recalculate) colladaMesh->Recalculate();

		// Create the poly object, whether we keep it or not: we must import into it.
		PolyObject* polyObject = CreateEditablePolyObject();
		MNMesh& mesh = polyObject->GetMesh();
		isEditablePoly = !hasOnlyTriangles;

		// Import the vertex values and the tessellation.
		if (ImportVertexPositions(mesh))
		{
			ImportNormals(mesh);
			ImportMapChannels(mesh);

			mesh.InvalidateGeomCache();
		}

		if (hasOnlyTriangles)
		{
			// Convert the poly mesh back into a triangle mesh.
			TriObject* triObject = CreateNewTriObject();
			obj = triObject;
			mesh.OutToTri(triObject->GetMesh());
			polyObject->DeleteMe();
			ImportVertexAnimations(triObject, ET_MASTER_CONTROL_REF, ET_VERT_BASE_REF);
		}
		else
		{
			obj = polyObject;
			ImportVertexAnimations(polyObject, EPOLY_MASTER_CONTROL_REF, EPOLY_VERT_BASE_REF);
		}
	}

	return obj;
}

Object* GeometryImporter::ImportSpline(FCDGeometrySpline *spline)
{
	if (spline == NULL) return NULL;

	if (spline->GetType() == FUDaeSplineType::BEZIER ||
		spline->GetType() == FUDaeSplineType::LINEAR)
	{
		SplineShape* splineObject = (SplineShape*) GetDocument()->MaxCreate(SHAPE_CLASS_ID, splineShapeClassID);
		size_t count = spline->GetSplineCount();

		for (size_t i = 0; i < count; i++)
		{
			// add a spline
			Spline3D*	spl = splineObject->shape.NewSpline();

			// collada linear or bezier spline reference
			FCDSpline *colladaSpline = spline->GetSpline(i);

			// add its CVs
			size_t cvCount = colladaSpline->GetCVCount();

			if (spline->GetType() == FUDaeSplineType::LINEAR)
			{
				// for each CV add 3 knots at the same position.
				for (size_t i = 0; i < cvCount; i++)
				{
					FMVector3& cv = *colladaSpline->GetCV(i);
					Point3 knot = Point3(cv.x,cv.y,cv.z);

					spl->AddKnot(SplineKnot(KTYPE_CORNER, LTYPE_CURVE, knot, knot, knot));
					spl->AddKnot(SplineKnot(KTYPE_CORNER, LTYPE_CURVE, knot, knot, knot));
					spl->AddKnot(SplineKnot(KTYPE_CORNER, LTYPE_CURVE, knot, knot, knot));
				}
			}
			else if (spline->GetType() == FUDaeSplineType::BEZIER)
			{
				// 3 CVs per knot, except for first and last having 2 CVs.
				size_t knotCount = (cvCount + 2) / 3;
				size_t curCV = 0;
				for (size_t i = 0; i < knotCount; i++)
				{
					FMVector3 *inVec, *knotVec, *outVec;

					// first knot has no in tangent only if it is not closed
					if (!colladaSpline->IsClosed() && i == 0)
					{
						knotVec = colladaSpline->GetCV(curCV++);
						inVec = knotVec;
						outVec = colladaSpline->GetCV(curCV++);
					}
					// last knot has no out tangent only if it is not closed
					else if (!colladaSpline->IsClosed() && i == knotCount - 1)
					{
						inVec = colladaSpline->GetCV(curCV++);
						knotVec = colladaSpline->GetCV(curCV++);
						outVec = knotVec;
					}
					// other knots have both tangents
					else
					{
						inVec = colladaSpline->GetCV(curCV++);
						knotVec = colladaSpline->GetCV(curCV++);
						outVec = colladaSpline->GetCV(curCV++);
					}

					bool InEqualsKnot = IsEquivalent(*inVec, *knotVec);
					bool KnotEqualsOut = IsEquivalent(*knotVec, *outVec);

					int type = KTYPE_BEZIER_CORNER;
					if (InEqualsKnot && KnotEqualsOut) type = KTYPE_CORNER;
					else if (!InEqualsKnot && !KnotEqualsOut)
					{
						FMVector3 v1 = (*inVec - *knotVec);
						FMVector3 v2 = (*knotVec - *outVec); // inverse

						// v1 is parallel with v2
						bool isC1 = IsEquivalent(FMVector3::Zero, (v1 ^ v2));
						if (isC1) type = KTYPE_BEZIER;
					}

					Point3 ptIn = Point3(inVec->x, inVec->y, inVec->z);
					Point3 ptKnot = Point3(knotVec->x, knotVec->y, knotVec->z);
					Point3 ptOut = Point3(outVec->x, outVec->y, outVec->z);

					spl->AddKnot(SplineKnot(type, LTYPE_CURVE, ptKnot, ptIn, ptOut));
				}
			}

			// set the closed attribute
			spl->SetClosed(colladaSpline->IsClosed() ? TRUE:FALSE);

			// updates the information internal to the spline
			spl->ComputeBezPoints();
		}

		// Make sure it readies the selection set info
		splineObject->shape.UpdateSels();

		// clear the caches
		splineObject->shape.InvalidateGeomCache();

		return splineObject;
	}
	else if (spline->GetType() == FUDaeSplineType::NURBS)
	{
// sorry about that, but I can't think of anything else since the delete operator
// doesn't work on NURBSCVCurve. Do we have stack-based containers? -Etienne.
#define MAX_NURBS_COUNT 100
		
		// create a NURB set
		NURBSSet nset;
		NURBSCVCurve cArray[MAX_NURBS_COUNT];

		size_t count = spline->GetSplineCount();
		if (count > MAX_NURBS_COUNT) return NULL;

#undef MAX_NURBS_COUNT

		for (size_t i = 0; i < count; i++)
		{

			// get the collada Nurb reference
			FCDNURBSSpline *nurbSpline = (FCDNURBSSpline *)spline->GetSpline(i);

			// set the name
			cArray[i].SetName(_T((char*)spline->GetParent()->GetName().c_str()));

			cArray[i].SetNumCVs((int)nurbSpline->GetCVCount());

			// set the order (degree + 1)
			cArray[i].SetOrder(nurbSpline->GetDegree() + 1);

			cArray[i].SetNumKnots((int)nurbSpline->GetKnotCount());

			for (size_t j = 0; j < nurbSpline->GetKnotCount(); j++)
			{
				cArray[i].SetKnot((int)j, *(nurbSpline->GetKnot(j)));
			}

			NURBSControlVertex cv;
			for (size_t j = 0; j < nurbSpline->GetCVCount(); j++)
			{
				FMVector3 *pos = nurbSpline->GetCV(j);
				float *weight = nurbSpline->GetWeight(j);
				cv.SetPosition(0, pos->x, pos->y, pos->z);
				cv.SetWeight(0, *weight);
				cArray[i].SetCV((int)j, cv);
			}

			// close it if necessary
			if (nurbSpline->IsClosed())
			{
				cArray[i].Close();
			}

			// add the REFERENCE of the curve to the set
			nset.AppendObject(&(cArray[i]));
		}

		Matrix3 mat; mat.IdentityMatrix();

		// create a NURB object
		Object *obj = CreateNURBSObject(NULL, &nset, mat);

		// REMOVE every object inserted in the NURBSet
		// this is a WEIRD bug in the NURBSCVCurve class:
		// you CAN'T call the delete operator on it, it crashes...
		for (int i = (int)count - 1; i >= 0; i--)
		{
			nset.RemoveObject(i);
		}

		// now you can return safely, the NURBSet will be destroyed
		// without any crash.
		return obj->ConvertToType(TIME_INITIAL_POSE, EDITABLE_CVCURVE_CLASS_ID);

	}

	return NULL;
}


bool GeometryImporter::ImportVertexPositions(MNMesh& meshObject)
{
	FCDGeometryMesh* mesh = colladaGeometry->GetMesh();
	size_t polygonDivisionCount = mesh->GetPolygonsCount();
	if (polygonDivisionCount == 0) return false;

	if (!meshObject.CheckAllData()) return false;

	FCDGeometrySource* positionSource = mesh->GetPositionSource();
	if (positionSource == NULL) return false;
	size_t vertexCount = positionSource->GetValueCount();

	if (!meshObject.CheckAllData()) return false;

	// Create the appropriate number of vertices and assign them positions
	meshObject.setNumVerts((int)vertexCount);
	for (size_t i = 0; i < vertexCount; ++i)
	{
		MNVert *v = meshObject.V((int)i);
		v->p = ToPoint3(positionSource->GetData(), i, positionSource->GetStride());
	}

	if (!meshObject.CheckAllData()) return false;

	// Set the face count and organize the vertices in faces
	size_t faceCount = mesh->GetFaceCount();
	meshObject.setNumFaces((int)faceCount);
	size_t polygonsCount = mesh->GetPolygonsCount();

	for (size_t i = 0; i < polygonsCount; ++i)
	{
		FCDGeometryPolygons* colladaPolygons = mesh->GetPolygons(i);
		FCDGeometryPolygonsInput* positionInput = colladaPolygons->FindInput(FUDaeGeometryInput::POSITION);
		uint32* positionIndices = positionInput->GetIndices();
		size_t positionIndexCount = positionInput->GetIndexCount();
		const uint32* localFaceVertexCounts = colladaPolygons->GetFaceVertexCounts();
		uint32 faceOffset = (uint32)colladaPolygons->GetFaceOffset();

		uint32 localFaceIndex = 0;
		size_t localFaceCount = colladaPolygons->GetFaceVertexCountCount();
		uint32 localOffset = 0;
		for (size_t j = 0; j < localFaceCount; ++j)
		{
			uint32 faceVertexCount = localFaceVertexCounts[j];
			if (!colladaPolygons->IsHoleFaceHole(j))
			{
				if (localOffset + faceVertexCount > positionIndexCount) break;

				MNFace* face = meshObject.F(faceOffset + (localFaceIndex++));
				face->MakePoly(faceVertexCount, (int*) (positionIndices + localOffset));
			}
			localOffset += faceVertexCount;
		}
	}

	// possible side-effects
	if (!meshObject.CheckAllData()) return false;
	return true;
	
}

static float offsetValue = 0.0f;
float OffsetConversion(float value) { return value + offsetValue; }

bool GeometryImporter::ImportVertexAnimations(GeomObject* object, int masterControllerReferenceID, int vertexControllerBaseID)
{
	if (colladaGeometry == NULL || !colladaGeometry->IsMesh()) return false;

	// Retrieve the vertex position source, where the animated values are stored
	FCDGeometryMesh* mesh = colladaGeometry->GetMesh();
	FCDGeometrySource* positionSource = mesh->GetPositionSource();
	if (positionSource == NULL) return false;
	size_t animatedValueCount = positionSource->GetAnimatedValues().size();
	if (animatedValueCount == 0) return true;
	size_t vertexCount = positionSource->GetValueCount();
	
	// Retrieve Max' master point controller
	RefTargetHandle reference = object->GetReference(masterControllerReferenceID);
	MasterPointControl* masterController = NULL;
	if (reference == NULL)
	{
		masterController = (MasterPointControl*) CreateMasterPointControl();
		object->AssignController(masterController, masterControllerReferenceID);
	}
	else if (reference->ClassID().PartA() == MASTERPOINTCONT_CLASS_ID)
	{
		masterController = (MasterPointControl*) reference;
	}
	if (masterController == NULL) return false;
	
	int subControllerCount = masterController->GetNumSubControllers();
	if (subControllerCount < (int) vertexCount)
	{
		// ST 25 Sep 2006: Fixing False Assertion
		// The following block of code would raise an assertion error in
		// Max, because a Tab obj is not initialized on the PolyObj.  Force
		// initialization with following SetReference call

		// GLaforte: relevant only for EditablePoly?
		if (vertexControllerBaseID == EPOLY_VERT_BASE_REF) object->SetReference(EPOLY_VERT_BASE_REF, NULL);
		else masterController->SetNumSubControllers((int) vertexCount);
	}

	// Prepare the buffer to put together the animation curves to merge.
	typedef FCDAnimationCurve* FCDAnimationCurvePtr;
	typedef FCDAnimationCurve** FCDAnimationCurve2DPtr;
	FCDAnimationCurve2DPtr* curves = new FCDAnimationCurve2DPtr[vertexCount];
	memset(curves, 0, sizeof(FCDAnimationCurve2DPtr) * vertexCount);

	// Process the individual animation curves, for each animated value
	for (size_t i = 0; i < animatedValueCount; ++i)
	{
		FCDAnimated* animated = positionSource->GetAnimatedValues()[i];
		if (!animated->HasCurve()) continue;
		ldiv_t vi = div(animated->GetArrayElement(), positionSource->GetStride());
		if (vi.quot < 0 || vi.quot > (int) vertexCount) continue;
		if (vi.rem >= 3) continue; // only 3D per-vertex-position animations supported in 3dsMax.

		if (curves[vi.quot] == NULL)
		{
			curves[vi.quot] = new FCDAnimationCurvePtr[3];
			memset(curves[vi.quot], 0, sizeof(FCDAnimationCurvePtr) * 3);
		}
		curves[vi.quot][vi.rem] = animated->GetCurve(0);
	}

	// Process all the vertices with animation curves
	for (int i = 0; i < (int) vertexCount; ++i)
	{
		if (curves[i] == NULL) continue;

		// COLLADA vertex position curves are relative, to support vertex position animations on
		// skinned or morphed objects. Max doesn't care for this, so pre-offset the component curves.
		Point3 p = ToPoint3(positionSource->GetData(), i, positionSource->GetStride());
		for (int j = 0; j < 3; ++j)
		{
			if (curves[i][j] == NULL) continue;
			offsetValue = p[j];
			curves[i][j]->ConvertValues(OffsetConversion, OffsetConversion);
		}

		// [AZadorozhny] You have to make reference to Subcontrollers.  
		Control* point3Controller = NewDefaultPoint3Controller();
		object->AssignController(point3Controller, i + vertexControllerBaseID);

		Control* reference = masterController->GetSubController(i);
		if (reference == NULL) masterController->SetSubController(i, point3Controller);
		IKeyControl* keyControl = (IKeyControl*) point3Controller->GetInterface(I_KEYCONTROL);
		if (keyControl == NULL) continue;

		FCDAnimationCurveList toMerge((const FCDAnimationCurve**) curves[i], 3);
		FloatList defaultValues(&p[0], 3);
		FCDAnimationMultiCurve* curve = FCDAnimationCurveTools::MergeCurves(toMerge, defaultValues);
		ANIM->FillController<FCDAnimationMultiCurve, 3>(keyControl, curve, true);
		curve->Release();
		SAFE_DELETE_ARRAY(curves[i]);
	}
	SAFE_DELETE_ARRAY(curves);

	object->NotifyDependents(FOREVER, 0, REFMSG_SUBANIM_STRUCTURE_CHANGED);

	return true;
}

typedef fm::map<void*, size_t> SourceOffsetMap;

bool GeometryImporter::ImportNormals(MNMesh& meshObject)
{
	if (colladaGeometry == NULL || !colladaGeometry->IsMesh()) return false;
	FCDGeometryMesh* mesh = colladaGeometry->GetMesh();

	meshObject.SpecifyNormals();
	MNNormalSpec* normalizer = meshObject.GetSpecifiedNormals();
	normalizer->ClearNormals();
	normalizer->SetNumFaces(meshObject.FNum());

	// To ensure that normals are built per source, use this map
	size_t globalOffset = 0;
	SourceOffsetMap sourceNormalOffsets;

	// Enforce the indexed normals
	size_t polygonDivisionCount = mesh->GetPolygonsCount();
	for (size_t i = 0; i < polygonDivisionCount; ++i)
	{
		FCDGeometryPolygons* polygons = mesh->GetPolygons(i);

		// Retrieve the per-vertex-face normal information.
		FCDGeometryPolygonsInput* input = polygons->FindInput(FUDaeGeometryInput::NORMAL);
		FCDGeometrySource* source = (input != NULL) ? input->GetSource() : NULL;
		if (source != NULL && source->GetStride() < 3) { source = NULL; input = NULL; }
		uint32* indices = (source != NULL) ? input->GetIndices() : NULL;

		// Retrieve the vertex positions, in case we need to generate normals.
		FCDGeometrySource* positionSource = mesh->GetPositionSource();
		if (positionSource == NULL || positionSource->GetStride() < 3) continue;
		FCDGeometryPolygonsInput* positionInput = polygons->FindInput(positionSource);
		if (positionInput == NULL) continue;
		uint32* positionIndices = positionInput->GetIndices();
		if (positionIndices == NULL) continue;

		// Find the mesh indices for this source's normals
		size_t sourceOffset = 0;
		size_t normalCount = source != NULL ? source->GetValueCount() : 0;
		if (indices != NULL)
		{
			SourceOffsetMap::iterator it = sourceNormalOffsets.find(source);
			if (it != sourceNormalOffsets.end()) sourceOffset = (*it).second;
			else
			{
				sourceNormalOffsets[source] = sourceOffset = globalOffset;
				globalOffset += normalCount;
				normalizer->SetNumNormals((int) globalOffset);

				// Build this source's normals into the normalizer
				for (size_t j = 0; j < normalCount; ++j)
				{
					Point3 normal = ToPoint3(source->GetData(), j, source->GetStride());
					normalizer->Normal((int) (sourceOffset + j)) = normal.Normalize();
					normalizer->SetNormalExplicit((int) (sourceOffset + j), true);
				}
			}
		}

		// Assign the normals
		const uint32* faceVertexCounts = polygons->GetFaceVertexCounts();
		size_t polygonsFaceOffset = polygons->GetFaceOffset();
		size_t polyFaceCount = polygons->GetFaceVertexCountCount();

		size_t offset = 0, faceIndex = 0;
		for (size_t j = 0 ; j < polyFaceCount; ++j)
		{
			size_t faceVertexCount = faceVertexCounts[j];
			if (!polygons->IsHoleFaceHole(j) && faceVertexCount >= 3)
			{
				MNNormalFace& face = normalizer->Face((int) (polygonsFaceOffset + (faceIndex++)));
				face.SetDegree((int)faceVertexCount);
				face.SpecifyAll();

				// Build a list of normal IDs for this face
				int normalIndex = 0;
				if (indices == NULL)
				{
					// Generate this face's normal.
					FMVector3 first(positionSource->GetValue(positionIndices[offset]));
					FMVector3 second(positionSource->GetValue(positionIndices[offset + 1]));
					FMVector3 third(positionSource->GetValue(positionIndices[offset + 2]));
					normalIndex = normalizer->NewNormal(::ToPoint3((first - second) ^ (first - third)), true);
				}
				for (uint32 k = 0; k < faceVertexCount; ++k)
				{
					if (indices != NULL)
					{
						normalIndex = (int) indices[offset + k];
						if (normalIndex >= (int) normalCount) continue;
						normalIndex += (int) sourceOffset;
					}
					face.SetNormalID(k, normalIndex);
				}
			}
			offset += faceVertexCount;
		}
	}

	normalizer->CheckNormals();

	if (!meshObject.CheckAllData()) return false;
	return true;
}

typedef fm::map<FCDGeometrySource*, int> SourceChannelMap;

bool GeometryImporter::ImportMapChannels(MNMesh& meshObject)
{
	if (colladaGeometry == NULL || !colladaGeometry->IsMesh()) return false;
	FCDGeometryMesh* mesh = colladaGeometry->GetMesh();

	// Map channels includes the COLOR and TEXCOORD semantics

	// Find the vertex color sources/texture coordinate sets to import
	bool usedMapChannels[MAX_MESHMAPS + NUM_HIDDENMAPS];
	memset(usedMapChannels, FALSE, sizeof(bool) * MAX_MESHMAPS + NUM_HIDDENMAPS);
	int maxMapChannelIndex = 0;
	SourceChannelMap sources;
	size_t polygonDivisionCount = mesh->GetPolygonsCount();
	for (size_t i = 0; i < polygonDivisionCount; ++i)
	{
		FCDGeometryPolygons* polygons = mesh->GetPolygons(i);
		FCDGeometryPolygonsInputList inputs;
		polygons->FindInputs(FUDaeGeometryInput::COLOR, inputs);
		polygons->FindInputs(FUDaeGeometryInput::TEXCOORD, inputs);
		size_t inputCount = inputs.size();
		if (inputCount == 0) continue;

		// Attempt to preserve the 'set' values for the channel indexation.
		// For 3dsMax, there may be 102 maps with indexes in the range: [-2,99].
		for (size_t j = 0; j < inputCount; ++j)
		{
			FCDGeometryPolygonsInput* input = inputs[j];
			FCDGeometrySource* source = input->GetSource();
			if (source == NULL) continue;
			bool isColor = source->GetType() == FUDaeGeometryInput::COLOR;

			// Figure out the map channel for this data set
			int mapChannelIndex = INT_MAX;
			SourceChannelMap::iterator it = sources.find(source);
			if (it == sources.end())
			{
				int wantedMapChannelIndex = input->GetSet();

				// map channel bug.
				if (wantedMapChannelIndex == -1)
				{
					// initialisation value? provide a default "0|1" value
					wantedMapChannelIndex = (isColor) ? 0 : 1;
				}

				for (it = sources.begin(); it != sources.end(); ++it)
				{
					if ((*it).second == wantedMapChannelIndex) break;
				}
				if (it == sources.end())
				{
					// Wanted map channel is not taken: use it, if it fits within the 3dsMax range
					if (wantedMapChannelIndex > MAX_MESHMAPS) wantedMapChannelIndex = MAX_MESHMAPS - wantedMapChannelIndex;
					if (wantedMapChannelIndex == 0 && !isColor) wantedMapChannelIndex = 1;
					else if (wantedMapChannelIndex == 1 && isColor) wantedMapChannelIndex = 0;
					if (wantedMapChannelIndex >= -NUM_HIDDENMAPS && wantedMapChannelIndex < MAX_MESHMAPS)
					{
						if (!usedMapChannels[wantedMapChannelIndex + NUM_HIDDENMAPS]) 
							mapChannelIndex = wantedMapChannelIndex;
					}
				}
				if (mapChannelIndex == INT_MAX)
				{
					// Check for the next available map channel index (should be a second step!)
					int startIndex = NUM_HIDDENMAPS; // Skip the hidden channels
					if (!isColor) startIndex = NUM_HIDDENMAPS + 1; // All the negative channels and channel 0 are always considered color channels
					for (; startIndex < MAX_MESHMAPS + NUM_HIDDENMAPS; ++startIndex)
					{
						if (isColor && startIndex == NUM_HIDDENMAPS + 1) continue; // Skip channel 1 for color channels.
						if (!usedMapChannels[startIndex])
						{
							mapChannelIndex = startIndex - NUM_HIDDENMAPS;
							break;
						}
					}
				}

				if (mapChannelIndex == INT_MAX) 
					continue; // No more free map channels
				else if (mapChannelIndex == 0) hasMapChannel0 = true;
				else if (mapChannelIndex == 1) hasMapChannel1 = true;
				if (mapChannelIndex > maxMapChannelIndex) maxMapChannelIndex = mapChannelIndex;
				usedMapChannels[mapChannelIndex + NUM_HIDDENMAPS] = true;

				// Assign this map channel index to this data source
				sources[source] = mapChannelIndex;
			}
			else
			{
				mapChannelIndex = (*it).second;
			}

			if (!isColor)
			{
				// Record the map channel assigned to this texture input for multi-textured materials
				textureChannels.insert(input, mapChannelIndex);
			}
		}
	}
	if (sources.empty()) return false;

	// Build all the map channels, from the data sources
	meshObject.SetMapNum(maxMapChannelIndex + 1);
	for (SourceChannelMap::iterator it = sources.begin(); it != sources.end(); ++it)
	{
		FCDGeometrySource* source = it->first;
		int mapChannelIndex = it->second;
		size_t sourceElementCount = source->GetValueCount();

		// enable / initialize the Map
		meshObject.InitMap(mapChannelIndex);

		MNMap* mapChannel = meshObject.M(mapChannelIndex);
		if (mapChannel == NULL) continue;

		// enable it (unset the MN_DEAD flag) needed?
		//mapChannel->ClearAllFlags();

		mapChannel->setNumVerts((int)sourceElementCount);

		// Assign to the channel vertices the source data.
		for (uint32 i = 0; i < sourceElementCount; ++i)
		{
			Point3 p = ToPoint3(source->GetData(), i, source->GetStride());
			mapChannel->v[i] = p;
		}

		mapChannel->setNumFaces((int) mesh->GetFaceCount());
	}

	// Assign the faces with the correct map channel vertices
	for (uint32 i = 0; i < polygonDivisionCount; ++i)
	{
		Int32List processedChannels;
		FCDGeometryPolygons* polygons = mesh->GetPolygons(i);
		FCDGeometryPolygonsInputList inputs;
		polygons->FindInputs(FUDaeGeometryInput::COLOR, inputs);
		polygons->FindInputs(FUDaeGeometryInput::TEXCOORD, inputs);
		size_t inputCount = inputs.size();
		if (inputCount == 0) continue;

		const uint32* faceVertexCounts = polygons->GetFaceVertexCounts(); 
		uint32 faceHoleCount = (uint32) polygons->GetFaceVertexCountCount();
		uint32 faceOffset = (uint32) polygons->GetFaceOffset();

		for (uint32 j = 0; j < inputCount; ++j)
		{
			FCDGeometryPolygonsInput* input = inputs[j];
			FCDGeometrySource* source = input->GetSource();
			SourceChannelMap::iterator it = sources.find(source);
			if (it == sources.end()) continue;
			uint32* indices = input->GetIndices();
			if (indices == NULL) continue;

			// Process this channel's indexed map vertices
			int mapChannelIndex = it->second;
			processedChannels.push_back(mapChannelIndex);
			MNMap* mapChannel = meshObject.M(mapChannelIndex);
			if (mapChannel == NULL) continue;

			uint32 localOffset = 0, faceIndex = 0;
			for (uint32 k = 0; k < faceHoleCount; ++k)
			{
				int32 faceVertexCount = faceVertexCounts[k];
				if (!polygons->IsHoleFaceHole(k))
				{
					MNMapFace* mapFace = mapChannel->F(faceOffset + (faceIndex++));
					mapFace->MakePoly(faceVertexCount, (int*) (indices + localOffset));
				}
				localOffset += faceVertexCount;
			}
		}

		// For unprocessed channels, zero out the map face vertex indices
		int *zeroes = NULL;
		for (SourceChannelMap::iterator it = sources.begin(); it != sources.end(); ++it)
		{
			Int32List::iterator itC = processedChannels.find(it->second);
			if (itC == processedChannels.end())
			{
				MNMap* mapChannel = meshObject.M(it->second);
				uint32 localOffset = 0, faceIndex = 0;
				for (uint32 k = 0; k < faceHoleCount; ++k)
				{
					int32 faceVertexCount = faceVertexCounts[k];
					if (!polygons->IsHoleFaceHole(k))
					{
						MNMapFace* mapFace = mapChannel->F(faceOffset + (faceIndex++));
						zeroes = new int[faceVertexCount];
						memset(zeroes, 0, faceVertexCount);
						mapFace->MakePoly(faceVertexCount, zeroes);
						delete [] zeroes; zeroes = NULL;
					}
					localOffset += faceVertexCount;
				}
			}
		}

	}

	// Set the display of the vertex colors in the viewport
	if (hasMapChannel0)
	{
		meshObject.SetDisplayVertexColors(0);
	}

	if (!meshObject.CheckAllData()) return false;
	return true;
}

void GeometryImporter::AssignMaterials(const FCDEntityInstance* _instance, MaterialImporterList& nodeMaterials)
{
	FUAssert(_instance->HasType(FCDGeometryInstance::GetClassType()), return);
	FCDGeometryInstance* instance = (FCDGeometryInstance*) _instance;
	if (colladaGeometry == NULL) return;

	if (colladaGeometry->IsMesh())
	{
		FCDGeometryMesh* mesh = colladaGeometry->GetMesh();

		// Process the per-polygons materials
		MapChannelMap colladaSetsMap;
		MaterialIndexMap materialIndices;
		size_t polygonDivisionCount = mesh->GetPolygonsCount(), offset = 0;
		for (size_t i = 0; i < polygonDivisionCount; ++i)
		{
			FCDGeometryPolygons* polygons = mesh->GetPolygons(i);
			FCDMaterialInstance* materialInstance = instance->FindMaterialInstance(polygons->GetMaterialSemantic());
			if (materialInstance == NULL) continue;
			
			FCDMaterial* materialCopy = NULL;
			uint16 materialIndex = 0;
			if (materialInstance->IsExternalReference())
			{
				// needs an absolute URI
				//Mtl *xMat = GetXRefMgr()->RetrieveXRefMaterial(materialInstance->GetExternalReference()->GetUri(), materialInstance->GetDocument()->GetFileManager());
				Mtl *xMat = (Mtl*)GetDocument()->GetXRefManager()->GetXRefItem(materialInstance->GetEntityReference()->GetUri(), FCDEntity::MATERIAL);
				if (xMat == NULL) continue;
				FUCrc32::crc32 crcId = FUCrc32::CRC32(ColladaID::Create(xMat));

				// Retrieve this material's index or assign it a new index
				MaterialIndexMap::iterator it = materialIndices.find(crcId);
				if (it == materialIndices.end())
				{
					materialIndices[crcId] = materialIndex = (uint16) nodeMaterials.size();
					nodeMaterials.push_back(new XRefMaterialImporter(xMat));
				}
				else materialIndex = (*it).second;
			}
			else
			{
				materialCopy = materialInstance->GetMaterial();
				if (materialCopy == NULL) continue;

				const fm::string& materialId = materialCopy->GetDaeId();
				FUCrc32::crc32 crcId = FUCrc32::CRC32(materialId);

				// Retrieve this material's index or assign it a new index
				MaterialIndexMap::iterator it = materialIndices.find(crcId);
				if (it == materialIndices.end())
				{
					materialIndices[crcId] = materialIndex = (uint16) nodeMaterials.size();
					EntityImporter* entityImporter = GetDocument()->FindInstance(materialCopy->GetDaeId());
					if (entityImporter == NULL || entityImporter->GetType() != EntityImporter::MATERIAL) continue;
					nodeMaterials.push_back((MaterialImporter*) entityImporter);
				}
				else materialIndex = (*it).second;
			}
			
			// Set the material index on the faces belonging to this polygon list
			size_t faceCount = polygons->GetFaceCount();
			if (isEditablePoly)
			{
				MNMesh &meshObject = ((PolyObject*)obj)->GetMesh();
				for (size_t j = 0; j < faceCount; ++j)
				{
					meshObject.F((int)(offset + j))->material = materialIndex;
				}
			}
			else
			{
				Mesh &meshObject = ((TriObject*)obj)->GetMesh();
				for (size_t j = 0; j < faceCount; ++j)
				{
					meshObject.faces[offset + j].setMatID(materialIndex);
				}
			}
			offset += faceCount;

			// Process all the TEXCOORD inputs to gather the ordered map channels
			FCDGeometryPolygonsInputList texCoordInputs;
			polygons->FindInputs(FUDaeGeometryInput::TEXCOORD, texCoordInputs);
			size_t texCoordInputCount = texCoordInputs.size();

			colladaSetsMap.clear();
			for (size_t i = 0; i < texCoordInputCount; ++i)
			{
				// Find this TEXCOORD input and record the assigned map channel
				FCDGeometryPolygonsInput* input = texCoordInputs[i];
				TextureChannelMap::iterator itChannel = textureChannels.find(input);
				if (itChannel == textureChannels.end())
				{
					colladaSetsMap[input->GetSet()] = 1;
				}
				else
				{
					colladaSetsMap[input->GetSet()] = (*itChannel).second;
				}
			}

			// Enforce unto the material the list of map channels
			// This will not work well for re-used materials!
			nodeMaterials.back()->AssignMapChannels(materialInstance, colladaSetsMap);

			// Import material instance bindings
			nodeMaterials.back()->ImportMaterialInstanceBindings(materialInstance);
		}
	}
}


Mtl* GeometryImporter::CreateMaterial(const FCDEntityInstance* instance)
{
	MaterialImporterList materials;
	AssignMaterials(instance, materials);

	Mtl* material = NULL;
	int subMaterialCount = (int)materials.size();
	if (subMaterialCount == 1)
	{
		// Only one material is used, so assign it directly to this node
		material = materials.front()->GetMaterial();
	}
	else if (!materials.empty())
	{
		// Multiple materials are necessary, so create a MultiMtl object.
		MultiMtl* multiMaterial = NewDefaultMultiMtl();
		material = multiMaterial;

		// Assign each sub-material, according to the buffered list
		multiMaterial->SetNumSubMtls(subMaterialCount);
		for (int i = 0; i < subMaterialCount; ++i) multiMaterial->SetSubMtl(i, materials[i]->GetMaterial());
	}

	// safely delete the XREFs in the materials list
	for (int i = 0; i < subMaterialCount; ++i)
	{
		if (materials[i]->GetType() == EntityImporter::XREF)
		{
			SAFE_DELETE(materials[i]);
			materials[i] = NULL;
		}
	}

	return material;
}


// Assign the materials of this geometry to the parent Max node
void GeometryImporter::Finalize(FCDEntityInstance* instance)
{
	Mtl* material = CreateMaterial(instance);

	if (currentImport != NULL)
	{
		INode* inode = currentImport;

		if (material != NULL)
		{
			inode->SetMtl(material);
			// Add to the list of scene materials.
			MtlBaseLib* sceneMtls = GetCOREInterface()->GetSceneMtls();
			sceneMtls->Add(material);
		}

		// If the mesh has vertex coloration, display the colors
		if (hasMapChannel0)
		{
			inode->SetCVertMode(TRUE);
			inode->SetShadeCVerts(TRUE);
		}
	}

	EntityImporter::Finalize(instance);
}

int GeometryImporter::GetMaterialIndex(const FCDMaterialInstance* materialInstance, MaterialImporterList& nodeMaterials, MaterialIndexMap& materialIndices)
{
	FUAssert(materialInstance != NULL, return -1);
	if (materialInstance->IsExternalReference())
	{
		// needs an absolute URI
		Mtl *xMat = NULL; //GetDocument()->GetXRefManager()->RetrieveXRefMaterial(materialInstance->GetExternalReference()->GetUri());
		if (xMat == NULL) return -1;
		FUCrc32::crc32 crcId = FUCrc32::CRC32(ColladaID::Create(xMat));

		// Retrieve this material's index or assign it a new index
		MaterialIndexMap::iterator it = materialIndices.find(crcId);
		if (it == materialIndices.end())
		{
			int val = materialIndices[crcId] = (uint16) nodeMaterials.size();
			nodeMaterials.push_back(new XRefMaterialImporter(xMat));
			return val;
		}
		else return (*it).second;
	}
	else
	{
		const FCDMaterial* materialCopy = materialInstance->GetMaterial();
		if (materialCopy == NULL) return -1;

		const fm::string& materialId = materialCopy->GetDaeId();
		FUCrc32::crc32 crcId = FUCrc32::CRC32(materialId);

		// Retrieve this material's index or assign it a new index
		MaterialIndexMap::iterator it = materialIndices.find(crcId);
		if (it == materialIndices.end())
		{
			int val = materialIndices[crcId] = (uint16) nodeMaterials.size();
			EntityImporter* entityImporter = GetDocument()->FindInstance(materialCopy->GetDaeId());
			if (entityImporter == NULL || entityImporter->GetType() != EntityImporter::MATERIAL) return -1;
			nodeMaterials.push_back((MaterialImporter*) entityImporter);
			return val;
		}
		else return (*it).second;
	}
}
