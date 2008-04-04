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

#include "StdAfx.h"
#include "AnimationExporter.h"
#include "DocumentExporter.h"
#include "EntityExporter.h"
#include "GeometryExporter.h"
#include "MaterialExporter.h"
#include "ColladaOptions.h"
#include "XRefFunctions.h"
#include "XRefManager.h"
#include "ColladaIDCreator.h"
#include "Common/MultiMtl.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDController.h"
#include "FCDocument/FCDEntityInstance.h"
#include "FCDocument/FCDEntityReference.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDGeometryInstance.h"
#include "FCDocument/FCDGeometryMesh.h"
#include "FCDocument/FCDGeometryPolygons.h"
#include "FCDocument/FCDGeometryPolygonsInput.h"
#include "FCDocument/FCDGeometryPolygonsTools.h"
#include "FCDocument/FCDGeometrySource.h"
#include "FCDocument/FCDGeometrySpline.h"
#include "FCDocument/FCDEffect.h"
#include "FCDocument/FCDMaterial.h"
#include "FCDocument/FCDMaterialInstance.h"
#include "FCDocument/FCDLibrary.h"
#include <surf_api.h>

// ST - TODO: Check where we should be sticking these defines..

// Defined in EditTriObj, in the 3dsMax samples
#define ET_MASTER_CONTROL_REF	0
#define ET_VERT_BASE_REF		1

// Defined in PolyEdit, in the 3dsMax samples
#define EPOLY_PBLOCK 0
#define EPOLY_MASTER_CONTROL_REF  1
#define EPOLY_VERT_BASE_REF 2

inline bool IsFinite(const Point3& p) { return _finite(p.x) && _finite(p.y) && _finite(p.z); }

//
// NormalSpec
//

void NormalSpec::SetTriangles(MeshNormalSpec *t)
{ 
	triangleSpec = t;
	isTriangles = true;
	isEditablePoly = false;
}

void NormalSpec::SetPolygons(MNNormalSpec *p) 
{
	polySpec = p;
	isTriangles = false;
	isEditablePoly = true;
}

void NormalSpec::SetNull()
{
	triangleSpec = NULL;
	polySpec = NULL;
	isTriangles = isEditablePoly = false;
}

bool NormalSpec::IsNull()
{
	return (!isTriangles && !isEditablePoly);
}

// dispatchers
int NormalSpec::GetNumNormals()
{
	if (isEditablePoly) return polySpec->GetNumNormals();
	else if (isTriangles) return triangleSpec->GetNumNormals();
	else return -1;
}

Point3 NormalSpec::GetNormal(int idx)
{
	if (isEditablePoly) 
		return polySpec->Normal(idx);
	else if (isTriangles)
		return triangleSpec->Normal(idx);
	
	return Point3::Origin;
}

int NormalSpec::GetNormalIndex(int faceIdx, int cornerIdx)
{
	if (isEditablePoly) 
		return polySpec->GetNormalIndex(faceIdx, cornerIdx);
	else if (isTriangles)
		return triangleSpec->GetNormalIndex(faceIdx, cornerIdx);
	
	return -1;
}

//
// GeometryExporter
//

GeometryExporter::GeometryExporter(DocumentExporter* _document)
:	BaseExporter(_document)
,	geomObject(NULL), triObject(NULL), polyObject(NULL), deleteObject(false)
{
	currentNormals.SetNull();
}

GeometryExporter::~GeometryExporter()
{
	// Release all the ExportedMaterial information structures.
	CLEAR_POINTER_VECTOR(exportedMaterials);
}

void GetBaseObjectAndID(Object*& object, SClass_ID& sid)
{
	if (object == NULL) return;

	sid = object->SuperClassID();
	if (sid == WSM_DERIVOB_CLASS_ID || sid == DERIVOB_CLASS_ID || sid == GEN_DERIVOB_CLASS_ID)
	{
		IDerivedObject* derivedObject = (IDerivedObject*) object;
		if (derivedObject->NumModifiers() > 0)
		{
			// Remember that 3dsMax has the mod stack reversed in its internal structures.
			// So that evaluating the zero'th modifier implies evaluating the whole modifier stack.
			ObjectState state = derivedObject->Eval(0, 0);
			object = state.obj;
		}
		else
		{
			object = derivedObject->GetObjRef();
		}
		sid = object->SuperClassID();
	}
}

void GeometryExporter::ClassifyObject(Object* object, bool affectsControllers)
{
	SClass_ID sid;
	GetBaseObjectAndID(object, sid);

	Class_ID id = object->ClassID();

	if (!OPTS->ExportEPolyAsTriangles())
	{
		// Check for the possibility of an editable poly object.
		if (sid == GEOMOBJECT_CLASS_ID && (id == EPOLYOBJ_CLASS_ID || id.PartA() == POLYOBJ_CLASS_ID))
		{
			polyObject = (PolyObject*) object;
		}
	}
	if (polyObject == NULL)
	{
		// Check for the possibility of an editable mesh object.
		if (sid == GEOMOBJECT_CLASS_ID && (id.PartA() == EDITTRIOBJ_CLASS_ID || id.PartA() == TRIOBJ_CLASS_ID))
		{
			triObject = (TriObject*) object;
		}

		if (triObject == NULL)
		{
			// Check for geometry primitives (Box, Cylinder, Torus, etc.) and convert them to TriObject
			// since they don't export well as PolyObject (controllers are messed up)
			if (affectsControllers && 
				(id == Class_ID(BOXOBJ_CLASS_ID, 0) ||
				id == Class_ID(SPHERE_CLASS_ID, 0) ||
				id == Class_ID(CYLINDER_CLASS_ID, 0) ||
#if (MAX_VERSION_MAJOR >= 8)
#ifndef NO_OBJECT_STANDARD_PRIMITIVES
				id == PLANE_CLASS_ID ||
				id == PYRAMID_CLASS_ID ||
				id == GSPHERE_CLASS_ID ||
#endif // NO_OBJECT_STANDARD_PRIMITIVES
#endif // USE_MAX_8
				id == Class_ID(CONE_CLASS_ID, 0) ||
				id == Class_ID(TORUS_CLASS_ID, 0) ||
				id == Class_ID(TUBE_CLASS_ID, 0) ||
				id == Class_ID(HEDRA_CLASS_ID, 0) ||
				id == Class_ID(BOOLOBJ_CLASS_ID, 0)))
			{
				if (object->CanConvertToType(Class_ID(TRIOBJ_CLASS_ID, 0)))
				{
					triObject = (TriObject*) object->ConvertToType(TIME_EXPORT_START, Class_ID(TRIOBJ_CLASS_ID, 0));
					deleteObject = true;
				}
			}

			if (triObject == NULL && !OPTS->ExportEPolyAsTriangles() && object->CanConvertToType(Class_ID(POLYOBJ_CLASS_ID, 0)))
			{
				polyObject = (PolyObject*) object->ConvertToType(TIME_EXPORT_START, Class_ID(POLYOBJ_CLASS_ID, 0));
				deleteObject = true;
			}
			if (triObject == NULL && polyObject == NULL && object->CanConvertToType(Class_ID(TRIOBJ_CLASS_ID, 0)))
			{
				triObject = (TriObject*) object->ConvertToType(TIME_EXPORT_START, Class_ID(TRIOBJ_CLASS_ID, 0));
				// ST - Copy over animated vertices, this is not done by default
				if (id == EPOLYOBJ_CLASS_ID || id.PartA() == POLYOBJ_CLASS_ID) {
					// Copy over any animated vertices, because that is not done by default.
					int numVertControllers = object->NumRefs() - EPOLY_VERT_BASE_REF;
					if (numVertControllers > 0)
					{
						// Initialize our triObj to recieve vert controllers.
						triObject->ReplaceReference(ET_VERT_BASE_REF, NULL);

						// Assert we now have the same count
#ifdef _DEBUG
						assert((triObject->NumRefs() - ET_VERT_BASE_REF) == numVertControllers);
#endif
						for (int i = 0; i < numVertControllers; ++i)
						{
							triObject->AssignController(object->GetReference(EPOLY_VERT_BASE_REF + i), ET_VERT_BASE_REF + i);
						}
					}
				}

				deleteObject = true;
			}
			if (triObject == NULL && OPTS->ExportEPolyAsTriangles())
			{
				// Last possibility: do check for an editable poly type that does not convert to a TriObject on its own.
				//[untested].
				if (object->CanConvertToType(Class_ID(POLYOBJ_CLASS_ID, 0)))
				{
					PolyObject* tempPolyObject = (PolyObject*) object->ConvertToType(TIME_EXPORT_START, Class_ID(POLYOBJ_CLASS_ID, 0));
					triObject = (TriObject*) tempPolyObject->ConvertToType(TIME_EXPORT_START, Class_ID(TRIOBJ_CLASS_ID, 0));
					tempPolyObject->DeleteMe();
					deleteObject = true;
				}
			}
		}
	}

	geomObject = (polyObject != NULL) ? (GeomObject*) polyObject : (GeomObject*) triObject;
}

FCDEntity* GeometryExporter::ExportMesh(INode* node, Object* object, bool affectsControllers)
{
	if (node == NULL || object == NULL) return NULL;

	bool isXRef = false;
	if (XRefFunctions::IsXRefObject(object))
	{
		// Resolve the source
		object = XRefFunctions::GetXRefObjectSource(object);
		if (OPTS->ExportXRefs()) isXRef = true;
	}

	// Retrieve the TriObject or PolyObject representation of this geometric object.
	ClassifyObject(object, affectsControllers);
	if (geomObject == NULL || (triObject == NULL && polyObject == NULL)) return NULL;

	currentNormals.SetNull();

	// Create the COLLADA geometry entity and its mesh
	FCDGeometry* colladaGeometry = NULL;
	if (isXRef)
	{
		// we want this geometry, but without adding it to the library
		colladaGeometry = new FCDGeometry(CDOC); //CDOC->GetXRefManager()->CreateGeometry();
	}
	else
	{
		colladaGeometry = CDOC->GetGeometryLibrary()->AddEntity();
	}

	ENTE->ExportEntity(object, colladaGeometry, (fm::string(node->GetName()) + "-mesh").c_str());
	colladaGeometry->SetName(fm::string(node->GetName()));
	FCDGeometryMesh* colladaMesh = colladaGeometry->CreateMesh();

	// Export the vertex positions and the per-face-vertex normals
	ExportVertexPositions(colladaMesh);
	if (OPTS->ExportNormals()) ExportNormals(colladaMesh);

	// Export the map channel sources (valid indices are from -2 to 99)
	// Offset the negative map channels indices by MAX_MESHMAPS, in order to bring
	// the -2,-1 channels into non-negative range and not clash with the valid map channels.
	Tab<int> channels; 
	if (triObject != NULL) 
	{
		Mesh &triMesh = triObject->GetMesh();
		// according to SPARKS... valid maps = 0 (color) to MAX_MESHMAPS
		for (int i = -NUM_HIDDENMAPS; i < MAX_MESHMAPS; ++i)
		{
			if (triMesh.mapSupport(i) == TRUE)
				channels.Append(1, &i);
		}
	}
	else // polyObject != NULL...
	{
		MNMesh &mnMesh = polyObject->GetMesh();
		int chanNum = mnMesh.MNum();
		for (int i = -NUM_HIDDENMAPS; i < chanNum; ++i)
		{
			MNMap *map = mnMesh.M(i);
			if (map == NULL) continue;
			if (!map->GetFlag(MN_DEAD))
				channels.Append(1, &i);
		}
	}

	for (int i = 0; i < channels.Count(); ++i)
	{
		ExportTextureChannel(colladaMesh, channels[i]);
	}

	// Retrieve the list of materials assigned to the different polygons of this mesh
	Mtl* mat = node->GetMtl();
	MaterialList materials;
	if (mat != NULL)
	{
		FlattenMaterials(mat, materials);
	}

	fm::vector<size_t> matIDs;
	size_t numMaterials = materials.size();

	if (numMaterials > 1)
	{
		if (IsEditablePoly())
		{
			MNMesh &mnMesh = polyObject->GetMesh();
			int nbFaces = mnMesh.FNum();
			for (int i = 0; i < nbFaces; ++i) {
				MNFace *face = mnMesh.F(i);
				size_t matid = face->material;
				if (matIDs.find(matid) == matIDs.end())
					matIDs.push_back(matid);
			}
		}
		else
		{
			Mesh& mesh = triObject->GetMesh();
			int nbFaces = mesh.getNumFaces();
			for (int i = 0; i < nbFaces; ++i) {
				Face& face = mesh.faces[i];
				size_t matid = face.getMatID() % numMaterials;
				if (matIDs.find(matid) == matIDs.end())	
					matIDs.push_back(matid);
			}
		}
	} 
	else
		matIDs.push_back(0);

	// Create one COLLADA polygon set for each material used in the mesh
	int materialIndex = 0;
	FCDMaterial* blackMtl = NULL;
	for (size_t* it = matIDs.begin(); it != matIDs.end(); ++it, ++materialIndex)
	{
		size_t matID = *it;

		// Create the COLLADA polygon set
		FCDGeometryPolygons* polygons = colladaMesh->AddPolygons();

		// Attach these polygons to a material
		FCDMaterial* colladaMaterial = NULL;
		if (mat == NULL)
		{
			polygons->SetMaterialSemantic(FC("ColorMaterial"));
		}
		else
		{
			Mtl* subMaterial = materials[matID % numMaterials];
			if (subMaterial != NULL)
			{
				
				// check for XRefs
				if (XRefFunctions::IsXRefMaterial(subMaterial))
				{
					if (!OPTS->ExportXRefs())
					{
						// resolve the source
						subMaterial = XRefFunctions::GetXRefMaterialSource(subMaterial);
					}
					// else do nothing, this is only a material instance
				}

				// if this is a XRef it'll return NULL
				colladaMaterial = document->GetMaterialExporter()->FindExportedMaterial(subMaterial);

				if (colladaMaterial != NULL) polygons->SetMaterialSemantic(colladaMaterial->GetDaeId());
				else polygons->SetMaterialSemantic(ColladaID::Create(subMaterial));
			}
			else
			{
				if (blackMtl == NULL)
				{
					// This is possible. In this case, Max displays black, so lets do that too
					DWORD wireColor = 0;
					blackMtl = document->GetMaterialExporter()->ExportColorMaterial(wireColor);
				}
				colladaMaterial = blackMtl;
				polygons->SetMaterialSemantic(colladaMaterial->GetDaeId());
			}
		}
		ExportedMaterial* exportedMat = new ExportedMaterial(colladaMaterial, (int)matID);
		exportedMaterials.push_back(exportedMat);
		polygons->SetUserHandle(exportedMat);

		// Add the polygons set input and its tessellation indices
		ExportPolygonsSet(polygons, numMaterials <= 1 ? -1 : (int)matID, (int)numMaterials); // <input>
	}

	// For texturing map channels, export the texture tangents and bi-normals, on request
	if (OPTS->ExportNormals() && OPTS->ExportTangents())
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

	// Reset the store data structures
	currentNormals.SetNull();
	if (deleteObject)
	{
		if (triObject != NULL) triObject->DeleteMe();
		if (polyObject != NULL) polyObject->DeleteMe();
	}
	geomObject = NULL; triObject = NULL; polyObject = NULL;
	deleteObject = false;

	return colladaGeometry;
}


void GeometryExporter::ExportVertexPositions(FCDGeometryMesh* colladaMesh)
{
	// Check for vertex position animation
	MasterPointControl* masterController = NULL;
	int referenceCount = geomObject->NumRefs();
	for (int i = 0; i < referenceCount; ++i)
	{
		ReferenceTarget* reference = geomObject->GetReference(i);
		if (reference->ClassID().PartA() == MASTERPOINTCONT_CLASS_ID)
		{
			masterController = (MasterPointControl*) reference;
			break;
		}
	}

	// Create the data source
	FCDGeometrySource* source = colladaMesh->AddVertexSource(FUDaeGeometryInput::POSITION);
	source->SetStride(3);
	source->SetDaeId(colladaMesh->GetDaeId() + "-positions");

	if (!IsEditablePoly())
	{
		Mesh& triMesh = triObject->GetMesh();

		// Fill in the vertex positions
		int vertexCount = triMesh.getNumVerts();
		source->SetValueCount(vertexCount);
		float* data = source->GetData();
		for (int i = 0; i < vertexCount; ++i)
		{
			Point3& p = triMesh.getVert(i);
			(*data++) = p.x; (*data++) = p.y; (*data++) = p.z;
		}
	}
	else
	{
		// editable poly. case
		MNMesh& m = polyObject->GetMesh();

		int vertexCount = m.VNum();
		source->SetValueCount(vertexCount);
		float* data = source->GetData();
		for (int i = 0; i < vertexCount; ++i)
		{
			Point3 &p = m.V(i)->p;
			(*data++) = p.x; (*data++) = p.y; (*data++) = p.z;
		}
	}

	// Export the master controller information
	if (masterController != NULL)
	{
		ANIM->SetRelativeAnimationFlag(true);
		int vertexCount = masterController->GetNumSubControllers();
		for (int index = 0; index < vertexCount; ++index)
		{
			Control* subController = masterController->GetSubController(index);
			if (subController != NULL)
			{
				FMVector3* colladaValue = (FMVector3*) source->GetValue(index);
				for (int j = 0; j < 3; ++j)
				{
					FCDAnimated* animated = source->GetSourceData().GetAnimated(index*3+j);
					ANIM->SetControlType((ColladaMaxAnimatable::Type) (ColladaMaxAnimatable::ROTATION_X + j));
					ANIM->ExportAnimatedFloat(source->GetDaeId(), subController, animated);
					if (animated != NULL) animated->SetRelativeAnimationFlag();
				}

				// Hack in the relative-to value in the vertex position.
				Point3 p; Interval i;
				subController->GetValue(OPTS->AnimStart(), &p, i);
				colladaValue->x = p.x;
				colladaValue->y = p.y;
				colladaValue->z = p.z;
			}
		}
		ANIM->SetRelativeAnimationFlag(false);
	}
}

void GeometryExporter::ExportNormals(FCDGeometryMesh* colladaMesh)
{
	if (!IsEditablePoly())
	{
		// Retrieve the 3dsMax normals
		Mesh& triMesh = triObject->GetMesh();

		MeshNormalSpec *norms = triMesh.GetSpecifiedNormals();
		if (norms == NULL)
		{
			triMesh.SpecifyNormals();
			norms = triMesh.GetSpecifiedNormals();
		}
		if (norms->GetNumNormals() == 0)
		{
			norms->SetParent(&triMesh);
			norms->CheckNormals();
		}
		if (norms == NULL) return;
		currentNormals.SetTriangles(norms);

		if (norms->GetNumNormals() == 0)
		{
			norms->SetParent(&triMesh);
			norms->CheckNormals();
		}

	}

	// Create the data source
	FCDGeometrySource* source = colladaMesh->AddSource(FUDaeGeometryInput::NORMAL);
	source->SetStride(3);
	source->SetDaeId(colladaMesh->GetDaeId() + "-normals");

	if (IsEditablePoly())
	{
		ExportEditablePolyNormals(polyObject->GetMesh(), source);
	}
	else
	{
		// Fill in the normals source
		int normalCount = currentNormals.GetNumNormals();
		source->SetValueCount(normalCount);
		float* data = source->GetData();
		for (int i = 0; i < normalCount; ++i)
		{
			Point3 n = currentNormals.GetNormal(i);
			if (IsFinite(n))
			{
				(*data++) = n.x; (*data++) = n.y; (*data++) = n.z;
			}
			else
			{
				(*data++) = 0.0f; (*data++) = 0.0f; (*data++) = 1.0f;
			}
		}
	}
}

void GeometryExporter::ExportEditablePolyNormals(MNMesh &mesh, FCDGeometrySource* source)
{
	currentNormals.SetNull();
	MNNormalSpec *mnSpec = mesh.GetSpecifiedNormals();
	if (mnSpec == NULL)
	{
		mesh.SpecifyNormals();
		mnSpec = mesh.GetSpecifiedNormals();
	}
	if (mnSpec == NULL) return;

	// rebuild as needed
	mnSpec->CheckNormals();

	// final test
	int numNormals = mnSpec->GetNumNormals();
	if (numNormals == 0) return;

	// set the normals
	currentNormals.SetPolygons(mnSpec);

	source->SetValueCount(numNormals);
	float* data = source->GetData();

	// set the data
	for (int i = 0; i < numNormals; ++i)
	{
		Point3 &n = mnSpec->Normal(i);
		if (IsFinite(n))
		{
			(*data++) = n.x; (*data++) = n.y; (*data++) = n.z;
		}
		else
		{
			(*data++) = 1.0f; (*data++) = 0.0f; (*data++) = 0.0f;
		}
	}
}

void GeometryExporter::ExportTextureChannel(FCDGeometryMesh* colladaMesh, int channelIndex)
{
	// Create the data source
	FCDGeometrySource* source = colladaMesh->AddSource(channelIndex <= 0 ? FUDaeGeometryInput::COLOR : FUDaeGeometryInput::TEXCOORD);
	source->SetUserHandle((void*)(size_t) channelIndex);
	source->SetStride(3);
	FUSStringBuilder builder(colladaMesh->GetDaeId()); builder.append("-map-channel");
	builder.append(channelIndex >= 0 ? channelIndex : (MAX_MESHMAPS - channelIndex));
	source->SetDaeId(builder.ToCharPtr());

	if (IsEditablePoly())
	{
		MNMesh &mnMesh = polyObject->GetMesh();
		MNMap *mnMap = mnMesh.M(channelIndex);
		if (mnMap == NULL) return;

		int numV = mnMap->numv;
		source->SetValueCount(numV);
		float* data = source->GetData();
		for (int i = 0; i < numV; ++i)
		{
			UVVert tv = mnMap->V(i);
			if (IsFinite(tv))
			{
				(*data++) = tv.x; (*data++) = tv.y; (*data++) = tv.z;
			}
			else
			{
				(*data++) = 0.0f; (*data++) = 0.0f; (*data++) = 0.0f;
			}
		}
	}
	else
	{
		Mesh& mesh = triObject->GetMesh();
		if (channelIndex >= mesh.getNumMaps() || channelIndex < -NUM_HIDDENMAPS) return;
		MeshMap& tmap = mesh.Map(channelIndex);

		// Fill in the map channel source
		int mapVertexCount = tmap.getNumVerts();
		source->SetValueCount(mapVertexCount);
		float* data = source->GetData();
		for (int i = 0; i < mapVertexCount; ++i)
		{
			Point3& tv = tmap.tv[i];
			if (IsFinite(tv))
			{
				(*data++) = tv.x; (*data++) = tv.y; (*data++) = tv.z;
			}
			else
			{
				(*data++) = 0.0f; (*data++) = 0.0f; (*data++) = 0.0f;
			}
		}
	}
}

void GeometryExporter::FlattenMaterials(Mtl* material, MaterialList& materialMap, int materialIndex)
{
	// check for XRefs
	if (material != NULL && XRefFunctions::IsXRefMaterial(material))
	{
		material = XRefFunctions::GetXRefMaterialSource(material);
	}

	// KEEP THE NULL POINTER! Null pointers are actually allowed in max, and we need to 
	// maintain the material list.
	Class_ID matId = (material == NULL) ? 
						Class_ID(STANDIN_CLASS_ID, STANDIN_CLASS_ID) :
						material->ClassID();

	if (matId == Class_ID(MULTI_CLASS_ID, 0))
	{
		if (materialIndex < 0)
		{
			// Read in the sub-materials
			int nbSubMtl = material->NumSubMtls();
			for (int i = 0; i < nbSubMtl; ++i)
			{
				Mtl *subMtl = material->GetSubMtl(i);
				FlattenMaterials(subMtl, materialMap, i);
			}
		}
		else 
		{
			// If we are in a recursive multi-material, read in the correctly indexed sub-material
			Mtl* subMtl = MATE->GetSubMaterialById(material, materialIndex);
			if (subMtl == NULL) subMtl = material->GetSubMtl(0);
			if (subMtl != NULL) FlattenMaterials(subMtl, materialMap);
		}
	}
	else if (matId == Class_ID(BAKE_SHELL_CLASS_ID, 0))
	{
		// Read in the first sub-material only
		FlattenMaterials(material->GetSubMtl(1), materialMap);
	}
	/*else if (XRefFunctions::IsXRefMaterial(material))
	{
		// Add this material as a leaf
		materialMap.push_back(material);
	} */
	else
	{
		// Add this material as a leaf
		materialMap.push_back(material);
	}
}

void GeometryExporter::ExportPolygonsSet(FCDGeometryPolygons* colladaPolygons, int materialIndex, int numMaterials)
{
	// Make a list of the IDs of faces to export in this polygons set
	Int32List faceIndices;
	int faceCount = 0;
	if (materialIndex != -1)
	{
		// Export only the faces/polygons which use this material
		if (IsEditablePoly())
		{
			MNMesh &mnMesh = polyObject->GetMesh();
			int nbFaces = mnMesh.FNum();
			faceIndices.reserve(nbFaces);
			for (int i = 0; i < nbFaces; ++i)
			{
				MNFace *face = mnMesh.F(i);
				if (face->material % numMaterials == materialIndex)
				{
					faceIndices.push_back(i);
				}
			}
		}
		else
		{
			// Triangles
			Mesh& mesh = triObject->GetMesh();
			int nbFaces = mesh.getNumFaces();
			faceIndices.reserve(nbFaces);
			for (int i = 0; i < nbFaces; ++i)
			{
				if (mesh.getFaceMtlIndex(i) % numMaterials == materialIndex)
				{
					faceIndices.push_back(i);
				}
			}
		}
		
		faceCount = (int) faceIndices.size();
	}
	else
	{
		// Only retrieve the correct face/polygon count.
		if (IsEditablePoly())
		{
			faceCount = polyObject->GetMesh().FNum();
		}
		else
		{
			// Triangles
			faceCount = triObject->GetMesh().getNumFaces();
		}
	}

	int idx = 1;
	FCDGeometryMesh* colladaMesh = colladaPolygons->GetParent();
	size_t sourceCount = colladaMesh->GetSourceCount();

	// Make a list of the polygon inputs to process.
	// Idx 0 is already used by the vertex inputs
	FCDGeometryPolygonsInputList inputs;
	inputs.reserve(8);
	FCDGeometryPolygonsInput* vertexInput = colladaPolygons->GetInput(0);
	inputs.push_back(vertexInput);
	vertexInput->ReserveIndexCount(faceCount * 4);
	for (size_t i = 0; i < sourceCount; ++i)
	{
		FCDGeometrySource* source = colladaMesh->GetSource(i);
		if (colladaMesh->IsVertexSource(source)) continue;
		FCDGeometryPolygonsInput* input = colladaPolygons->AddInput(source, idx++);
		inputs.push_back(input);
		input->ReserveIndexCount(faceCount * 4);
		if (source->GetType() == FUDaeGeometryInput::COLOR || source->GetType() == FUDaeGeometryInput::TEXCOORD)
		{
			input->SetSet((int32)(size_t) source->GetUserHandle());
			if (input->GetSet() < 0)
			{
				input->SetSet(MAX_MESHMAPS - input->GetSet());
			}
		}
	}
	size_t inputCount = inputs.size();
	
	// Add the tessellation indices to the inputs
	for (int i = 0; i < faceCount; ++i)
	{
		int faceIndex = (i < (int) faceIndices.size()) ? faceIndices[i] : i;

		int vertexCount;
		Tab<int> vertexIndices, normalIndices;

		if (IsEditablePoly())
		{
			// Calculate the number of vertices in this face
			// Pre-buffer some typical polygon inputs
			MNMesh &mnMesh = polyObject->GetMesh();
			MNFace *face = mnMesh.F(faceIndex);
			if (face == NULL) continue;
			for (int d = 0; d < face->deg; d++)
			{
				vertexIndices.Append(1, &(face->vtx[d]));
			}
			vertexCount = face->deg;
		}
		else
		{
			// Triangles
			vertexCount = 3;
		}

		colladaPolygons->AddFaceVertexCount(vertexCount);

		// Write out the input indices
		for (size_t k = 0; k < inputCount; ++k)
		{
			FCDGeometryPolygonsInput* input = inputs[k];
			FCDGeometrySource* source = input->GetSource();
			switch (source->GetType())
			{
			case FUDaeGeometryInput::POSITION:
				if (!IsEditablePoly()) // Triangles
				{
					Mesh& mesh = triObject->GetMesh();
					Face& face = mesh.faces[faceIndex];
					for (int v = 0; v < vertexCount; ++v)
					{
						input->AddIndex(face.v[v]);
					}
				}
				else
				{
					MNMesh &mnMesh = polyObject->GetMesh();
					MNFace *face = mnMesh.F(faceIndex);
					for (int v = 0; v < vertexCount; ++v)
					{
						input->AddIndex(face->vtx[v]);
					}
				}
				break;

			case FUDaeGeometryInput::NORMAL:
				for (int v = 0; v < vertexCount; ++v)
				{
					input->AddIndex(currentNormals.GetNormalIndex(faceIndex, v));
				}
				break;

			case FUDaeGeometryInput::TEXCOORD:
			case FUDaeGeometryInput::COLOR: {
				int channel = (int)(size_t) source->GetUserHandle();
				if (!IsEditablePoly()) // Triangles
				{
					Mesh &mesh = triObject->GetMesh();
					if (channel < mesh.getNumMaps() && channel >= -NUM_HIDDENMAPS)
					{
						MeshMap& tmap = mesh.Map(channel);
						TVFace& face = tmap.tf[faceIndex];
						for (int v = 0; v < vertexCount; ++v)
						{
							input->AddIndex(face.t[v]);
						}
					}
				}
				else
				{
					MNMesh &mnMesh = polyObject->GetMesh();
					MNMap *map = mnMesh.M(channel);
					if (map == NULL) break;

					MNMapFace *face = map->F(faceIndex);

					FUAssert(face != NULL,);
					FUAssert(vertexCount == face->deg,);
					for (int v = 0; v < vertexCount; v++)
					{
						input->AddIndex(face->tv[v]);
					}
				}
				break; }

			default:
				for (int v = 0; v < vertexCount; ++v) input->AddIndex(0);
				break;
			}
		}
	}
}

bool GeometryExporter::IsNURB(Object* object)
{
	if (object == NULL) return false;
	Class_ID objId = object->ClassID();

	if (!(	objId == EDITABLE_CVCURVE_CLASS_ID ||
			objId == EDITABLE_FPCURVE_CLASS_ID ||
			objId == EDITABLE_SURF_CLASS_ID    ||
			objId == FITPOINT_PLANE_CLASS_ID))
	{
		return false;
	}

	return true;
}

FCDEntity* GeometryExporter::ExportSpline(INode* node, Object* object)
{
	SClass_ID sid;
	GetBaseObjectAndID(object, sid);

	if (sid != SHAPE_CLASS_ID) return NULL;
	if (XRefFunctions::IsXRefObject(object)) return NULL;

	// NURB or spline?
	// doing this test here since spline->InitializeData may CRASH on a NURB object.
	if (IsNURB(object)) return ExportNURBS (node, object);

	ShapeObject* maxobj = (ShapeObject*) object;
    if (!maxobj->CanMakeBezier()) return NULL;
	BezierShape shape;
	maxobj->MakeBezier(TIME_EXPORT_START, shape);

	// Create the COLLADA geometry entity and its mesh
	FCDGeometry* colladaGeometry = CDOC->GetGeometryLibrary()->AddEntity();
	colladaGeometry->SetDaeId(fm::string(node->GetName()) + "-spline");

	// create the Bezier collada spline
	FCDGeometrySpline* colladaGeomSpline = colladaGeometry->CreateSpline();
	colladaGeomSpline->SetType(FUDaeSplineType::BEZIER);

	// for each curve
	int splines = min(shape.SplineCount(), maxobj->NumberOfCurves());
	for (int i=0; i < splines; ++i)
	{
		Spline3D* spline = shape.GetSpline(i);
		if (spline == NULL) continue;

		FCDBezierSpline* bezierSpline = (FCDBezierSpline*) colladaGeomSpline->AddSpline(FUDaeSplineType::BEZIER);
		bezierSpline->SetClosed(maxobj->CurveClosed(0,i) == TRUE);

		// for each knot of the spline3D
		int knots = spline->KnotCount();
		for (int j = 0; j < knots; ++j)
		{
			SplineKnot k = spline->GetKnot(j);
			Point3 pos;

			// add the knot in-vector as a control vertex for all knots except
			// the first one. Include the first one if the spline is closed.
			if (bezierSpline->IsClosed() || j > 0)
			{
				pos = k.InVec();
				bezierSpline->AddCV(ToFMVector3(pos));
			}
			// always add the knot position as a control vertice
			pos = k.Knot();
			bezierSpline->AddCV(ToFMVector3(pos));

			// add the knot out-vector as a control vertex for every knot except
			// the last one. Include the last one if the spline is closed.
			if (bezierSpline->IsClosed() || j < knots - 1)
			{
				pos = k.OutVec();
				bezierSpline->AddCV(ToFMVector3(pos));
			}
		}
	}

	return colladaGeometry;
}

void FillNURBSCVCurve(NURBSCVCurve *maxNurbs, FCDNURBSSpline *colladaNURBS)
{
	if (maxNurbs == NULL || colladaNURBS == NULL)
		return;

	TimeValue coreTime = 0;

	// set the nurb degree
	colladaNURBS->SetDegree((uint32)maxNurbs->GetOrder() - 1);

	// is this NURB closed
	colladaNURBS->SetClosed(maxNurbs->IsClosed() == TRUE);

	int numCVs = maxNurbs->GetNumCVs(), 
		numKnots = maxNurbs->GetNumKnots();

	// create the control vertex
	for (int c = 0; c < numCVs; c++)
	{
		NURBSControlVertex *vertex = maxNurbs->GetCV(c);
		float x,y,z,w; x=y=z=w=0.0f;
		vertex->GetPosition(coreTime,x,y,z);
		vertex->GetWeight(coreTime,w);
		colladaNURBS->AddCV(FMVector3(x,y,z), w);
	}

	// create the knots
	for (int k = 0; k < numKnots; k++)
	{
		double knot = maxNurbs->GetKnot(k);
		colladaNURBS->AddKnot(knot);
	}
}

FCDEntity* GeometryExporter::ExportNURBS(INode* node, Object* object)
{
	if (node == NULL || !IsNURB(object)) return NULL;

	TimeValue coreTime = 0;

	// the following NURB API calls require the MAX SDK edmodel library
	NURBSSet maxNurbSet;
	BOOL success = GetNURBSSet(object, coreTime, maxNurbSet, FALSE);
	if (!success) return NULL;

	int numNurbs = maxNurbSet.GetNumObjects();
	if (numNurbs == 0) return NULL;

	FCDGeometry* colladaGeometry = NULL;
	FCDGeometrySpline* colladaSpline = NULL;
	
	// for each NURB in the Set
	for (int n = 0; n < numNurbs; n++)
	{
		NURBSObject *maxNurb = NULL;
		maxNurb = maxNurbSet.GetNURBSObject(n);
		if (maxNurb == NULL) continue;

		// single CV Curve NURB
		// this will detect CV and Point NURBS
		if (maxNurb->GetKind() == kNURBSCurve && maxNurb->GetType() == kNCVCurve)
		{
			NURBSCVCurve *maxNurbCurve = static_cast<NURBSCVCurve *>(maxNurb);

			// Create the COLLADA geometry entity and its mesh
			if (colladaGeometry == NULL)
			{
				colladaGeometry = CDOC->GetGeometryLibrary()->AddEntity();
				colladaGeometry->SetDaeId(fm::string(node->GetName()) + "-spline");
				colladaSpline = colladaGeometry->CreateSpline();
				colladaSpline->SetType(FUDaeSplineType::NURBS);
			}

			FCDNURBSSpline* nurbSpline = (FCDNURBSSpline*) colladaSpline->AddSpline(FUDaeSplineType::NURBS);
			FillNURBSCVCurve(maxNurbCurve, nurbSpline);
		}
	}

	return colladaGeometry;
}


void GeometryExporter::ExportInstance(INode* node, FCDEntityInstance* _instance)
{
	// Retrieve all the necessary COLLADA interfaces
	if (node == NULL || !_instance->GetObjectType().Includes(FCDGeometryInstance::GetClassType())) return;
	FCDGeometryInstance* instance = (FCDGeometryInstance*) _instance;
	FCDEntity* colladaEntity = instance->GetEntity();
	FCDGeometry* colladaGeometry = NULL;
	if (colladaEntity->GetObjectType().Includes(FCDGeometry::GetClassType()))
	{
		colladaGeometry = (FCDGeometry*) colladaEntity;
	}
	else if (colladaEntity->GetObjectType().Includes(FCDController::GetClassType()))
	{
		colladaGeometry = ((FCDController*) colladaEntity)->GetBaseGeometry();
	}
	if (colladaGeometry == NULL || (!colladaGeometry->IsMesh() && !colladaGeometry->IsPSurface())) return;
	
	if (colladaGeometry->IsMesh())
	{
		FCDGeometryMesh* colladaMesh = colladaGeometry->GetMesh();

		// Retrieve the list of materials assigned to the different polygons of this mesh
		Mtl* baseMaterial = node->GetMtl();
		FCDMaterial* colladaMaterial = NULL;
		if (baseMaterial == NULL)
		{
			// do not export the XRef color material unless we don't export XRefs or this object isn't an XRef
			if (!OPTS->ExportXRefs() || !XRefFunctions::IsXRefObject(node->GetObjectRef()))
			{
				DWORD wireColor = node->GetWireColor();
				colladaMaterial = document->GetMaterialExporter()->ExportColorMaterial(wireColor);
			}
		}

		size_t polygonsCount = colladaMesh->GetPolygonsCount();
		for (size_t i = 0; i < polygonsCount; ++i)
		{
			FCDGeometryPolygons* colladaPolygons = colladaMesh->GetPolygons(i);
			ExportedMaterial* exportedMaterial = (ExportedMaterial*) colladaPolygons->GetUserHandle();
			FUAssert(exportedMaterial != NULL, continue);
			Mtl *xRefMaterial = NULL;
			
			// Look for the correct material to assign to this polygons set.
			if (baseMaterial != NULL)
			{
				Mtl* polyMaterial = baseMaterial;

				// Check for XRef material to dereference
				if (polyMaterial != NULL && XRefFunctions::IsXRefMaterial(polyMaterial))
				{
					xRefMaterial = polyMaterial;
					polyMaterial = XRefFunctions::GetXRefMaterialSource(xRefMaterial);
				}

				// For multi-materials: look for the correct sub-material. Multi-material may be hierarchical...
				while (polyMaterial != NULL && polyMaterial->IsMultiMtl())
				{
					polyMaterial = MATE->GetSubMaterialById(polyMaterial, exportedMaterial->materialId);

					// Check at every level, for a XRef material..
					if (polyMaterial != NULL && XRefFunctions::IsXRefMaterial(polyMaterial))
					{
						xRefMaterial = polyMaterial;
						polyMaterial = XRefFunctions::GetXRefMaterialSource(xRefMaterial);
					}
				}

				if (polyMaterial != NULL && (!OPTS->ExportXRefs() || xRefMaterial == NULL))
				{
					colladaMaterial = document->GetMaterialExporter()->FindExportedMaterial(polyMaterial);
				}
			}
			if (colladaMaterial == NULL)
			{
				// If we have not yet found a valid material, we may have a COLLADA material with a color value
				colladaMaterial = exportedMaterial->colladaMaterial;
			}

			if (OPTS->ExportXRefs() && xRefMaterial != NULL)
			{
				FCDMaterialInstance* materialInstance = instance->AddMaterialInstance();
				materialInstance->SetMaterial(NULL);
				materialInstance->SetSemantic(colladaPolygons->GetMaterialSemantic());
				FCDEntityReference* xref = materialInstance->GetEntityReference();
				if (XRefFunctions::IsXRefCOLLADA(xRefMaterial))
				{
					FUUri uri(XRefFunctions::GetURL(xRefMaterial), ColladaID::Create(xRefMaterial, true));
					xref->SetUri(uri);
				}
				else
				{
					FUUri uri(XRefFunctions::GetURL(xRefMaterial), fstring(xRefMaterial->GetName().data()));
					xref->SetUri(uri);
				}
				document->GetMaterialExporter()->ExportMaterialInstance(materialInstance, exportedMaterial, colladaMaterial);	
			}
			else if (colladaMaterial != NULL)
			{
				FCDMaterialInstance* materialInstance = instance->AddMaterialInstance(colladaMaterial, colladaPolygons);
				document->GetMaterialExporter()->ExportMaterialInstance(materialInstance, exportedMaterial, colladaMaterial);
			}
		}
	}
}

