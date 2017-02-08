/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "ColladaExporter.h"
#include "GeometryExporter.h"
#include "MaterialExporter.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDController.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDGeometryInstance.h"
#include "FCDocument/FCDGeometryMesh.h"
#include "FCDocument/FCDGeometryPolygons.h"
#include "FCDocument/FCDGeometryPolygonsInput.h"
#include "FCDocument/FCDGeometrySource.h"
#include "FCDocument/FCDMaterialInstance.h"
#include "FCDocument/FCDLibrary.h"

//
// GeometryExporter
//

GeometryExporter::GeometryExporter(ColladaExporter* base)
:	EntityExporter(base)
,	positionSource(NULL), normalSource(NULL), texCoordSource(NULL)
,	positionCount(0), normalCount(0), texCoordCount(0)
{
}

GeometryExporter::~GeometryExporter()
{
}

FCDEntity* GeometryExporter::ExportGeometry(FBGeometry* geometry)
{
	if (geometry->Is(FBMesh::TypeInfo)) return ExportMesh((FBMesh*) geometry);
	//else if (geometry->Is(FBNurbs::TypeInfo)) return ExportNurbs((FBNurbs*) geometry); 
	//else if (geometry->Is(FBPatch::TypeInfo)) return ExportPatch((FBPatch*) geometry);
	else FUFail(return NULL); // Unknown geometry type. Need to add processing function.
}

FCDEntity* GeometryExporter::ExportMesh(FBMesh* mesh)
{
	// Create the FCollada entity.
	FCDGeometry* colladaGeometry = CDOC->GetGeometryLibrary()->AddEntity();
	FCDGeometryMesh* colladaMesh = colladaGeometry->CreateMesh();
	ExportEntity(colladaGeometry, mesh);

	// Create the three sources: these are always present.
	positionSource = colladaMesh->AddVertexSource(FUDaeGeometryInput::POSITION);
	normalSource = colladaMesh->AddSource(FUDaeGeometryInput::NORMAL);
	texCoordSource = colladaMesh->AddSource(FUDaeGeometryInput::TEXCOORD);

	// Export the tessellation first, then the data.
//	ExportMeshTessellation(mesh, colladaMesh);
	ExportMeshVertices(mesh, colladaMesh);

	// Reset the buffered structures.
	positionSource = normalSource = texCoordSource = NULL;
	positionCount = normalCount = texCoordCount = 0;
	return colladaGeometry;
}

/*FCDEntity* GeometryExporter::ExportNurbs(FBNurbs* nurbs)
{
	// NURBS and patches are not yet supported.
	// Tessellate into a mesh.
	return ExportMesh(nurbs);
}
*/

/*FCDEntity* GeometryExporter::ExportPatch(FBPatch* patch)
{
	// NURBS and patches are not yet supported.
	// Tessellate into a mesh.
	return ExportMesh(patch);
}
*/

void GeometryExporter::ExportGeometryInstance(FBModel* node, FCDGeometryInstance* colladaInstance, FCDEntity* colladaEntity)
{
	// Retrieve the COLLADA geometry/mesh entity.
	FCDGeometry* colladaGeometry = NULL;
	if (colladaEntity->HasType(FCDGeometry::GetClassType()))
	{
		colladaGeometry = (FCDGeometry*) colladaEntity;
	}
	else if (colladaEntity->HasType(FCDController::GetClassType()))
	{
		colladaGeometry = ((FCDController*) colladaEntity)->GetBaseGeometry();
	}
	if (colladaGeometry == NULL || !colladaGeometry->IsMesh()) return;
	FCDGeometryMesh* colladaMesh = colladaGeometry->GetMesh();
	if (colladaMesh->GetPolygonsCount() == 0) return;

	// Retrieve the Motion Builder mesh.
	FBGeometry* geometry = node->Geometry;
	// FBMesh* mesh = NULL;
	// if (geometry->Is(FBMesh::TypeInfo)) mesh = (FBMesh*) geometry;
	// else if (geometry->Is(FBPatch::TypeInfo)) patch = (FBPatch*) node->Geometry;
	// else if (geometry->Is(FBNurbs::TypeInfo)) nurbs = (FBNurbs*) node->Geometry;
	if (geometry == NULL) return;

	// Process the Motion Builder material bindings.
	if (geometry->MaterialMappingMode == /*kFBMappingModeSurface*/kFBGeometryMapping_BY_POLYGON)
	{
		int materialCount = node->Materials.GetCount();
		FUStringBuilder builder;
		for (int i = 0; i < materialCount; ++i)
		{
			builder.set("material"); builder.append(i);
			fstring semantic = builder.ToString();
			FCDGeometryPolygonsList polygons;
			colladaMesh->FindPolygonsByMaterial(semantic, polygons);
			if (!polygons.empty())
			{
				FCDGeometryPolygons* colladaPolys = polygons.front();
				int textureId = (int) (size_t) colladaPolys->GetUserHandle();

				// Process the texture(s) attached to this polygons set.
				FBTextureList textures;
				if (textureId < node->Textures.GetCount())
				{
					FBTexture* texture = node->Textures[textureId];
					if (strlen(texture->Name.AsString()) > 0) textures.push_back(texture);
				}

				// Export this material instance.
				FCDMaterial* colladaMaterial = MATL->ExportMaterial(node->Materials[i], textures);
				FCDMaterialInstance* colladaMaterialInstance = colladaInstance->AddMaterialInstance(colladaMaterial, polygons.front());
				ExportMaterialInstance(colladaMaterialInstance, colladaPolys, textures);
			}
		}
	}
	else // kFBMappingModeUnique || kFBMappingModeVertex
	{
		FUAssert(colladaMesh->GetPolygonsCount() == 1, return);

		// Consider all the textures.
		FBTextureList textures;
		for (int i = 0; i < node->Textures.GetCount(); ++i)
		{
			FBTexture* texture = node->Textures[i];
			if (strlen(texture->Name.AsString()) > 0) textures.push_back(texture);
		}

		// Export this material and link it with the geometry.
		FBMaterial* material = (node->Materials.GetCount() > 0) ? node->Materials[node->Materials.GetCount() - 1] : NULL;
		FCDMaterial* colladaMaterial = MATL->ExportMaterial(material, textures); // material == NULL and textures.empty() is a valid case!
		FCDGeometryPolygons* colladaPolys = colladaMesh->GetPolygons(0);
		FCDMaterialInstance* colladaMaterialInstance = colladaInstance->AddMaterialInstance(colladaMaterial, colladaPolys);
		ExportMaterialInstance(colladaMaterialInstance, colladaPolys, textures);
	}
}

void GeometryExporter::ExportMaterialInstance(FCDMaterialInstance* colladaInstance, FCDGeometryPolygons* colladaPolygons, const FBTextureList& textures)
{
	if (!textures.empty())
	{
		FCDGeometryPolygonsInput* texCoordInput = colladaPolygons->FindInput(FUDaeGeometryInput::TEXCOORD);
		if (texCoordInput != NULL)
		{
			int32 set = texCoordInput->GetSet();
			colladaInstance->AddVertexInputBinding("TEX0", FUDaeGeometryInput::TEXCOORD, max(set, (int32) 0));
		}
	}	
}

/*
void GeometryExporter::ExportMeshTessellation(FBGeometry* geometry, FCDGeometryMesh* colladaMesh)
{
	FBFastTessellator* tessellator = GetTessellator(geometry);
	bool isStrips = tessellator == geometry->FastTessellatorStrip;
	float* positionData = tessellator->GetPositionArray();
	float* normalData = tessellator->GetNormalArray();

	// Figure out the number of FCDGeometryPolygons we'll need.
	size_t materialCount;
	if (geometry->MaterialMappingMode == kFBMappingModeSurface)
	{
		FUAssert(tessellator->MaterialId.GetCount() == tessellator->NormalIndex.GetCount(), "");
		materialCount = geometry->Materials.GetCount();
		if (materialCount == 0) materialCount = 1; // Can MotionBuilder have material-less polygons?
	}
	else // kFBMappingModeUnique || kFBMappingModeVertex
	{
		materialCount = 1;
	}

	// Retrieve some information about the Motion Builder polygons.
	int polygonCount = tessellator->PrimitiveCount;

	FUAssert(tessellator->UVIndex.GetCount() == tessellator->NormalIndex.GetCount(), "");
	FUAssert(tessellator->VertexIndex.GetCount() == tessellator->NormalIndex.GetCount(), "");

	// Track whether per-face-vertex normals/texture coordinates exist.
	bool hasVertexFaceNormals = false;
	bool hasVertexFaceTexCoords = false;
	
	// Create the neccessary polygons sets.
	FUSStringBuilder builder;
	for (size_t i = 0; i < materialCount; ++i)
	{
		FCDGeometryPolygons* colladaPolys = colladaMesh->AddPolygons();
		colladaPolys->SetPrimitiveType(isStrips ? FCDGeometryPolygons::TRIANGLE_STRIPS : FCDGeometryPolygons::POLYGONS);

		// Set an increasing material semantic.
		builder.set("material"); builder.append(i);
		colladaPolys->SetMaterialSemantic(builder.ToString());

		// In order to avoid the memory resizes: pre-count the number of indices.
		size_t indexCount = 0;
		uint32 offset = 0;
		bool* reverseOrders = new bool[polygonCount];
		memset(reverseOrders, 0, sizeof(bool) * polygonCount);
		for (int j = 0; j < polygonCount; ++j)
		{
			int faceVertexCount = tessellator->PrimitiveVertexCount[j];
			if (materialCount < 2 || tessellator->MaterialId[offset] == (int) i)
			{
				if (isStrips)
				{
					// Check whether the winding is correct.
					uint32 index[3] = { tessellator->VertexIndex[offset], tessellator->VertexIndex[offset + 1], tessellator->VertexIndex[offset + 2] };
					FMVector3 positions[3] = { positionData + (index[0] * 4), positionData + (index[1] * 4), positionData + (index[2] * 4) };
					FMVector3 realNormal = normalData + (tessellator->NormalIndex[offset] * 4);
					FMVector3 calculatedNormal = (positions[0] - positions[1]) ^ (positions[1] - positions[2]);
					reverseOrders[j] = calculatedNormal * realNormal < 0.0f;
					if (reverseOrders[j] && faceVertexCount > 3) indexCount += 2;
				}

				indexCount += faceVertexCount;
			}
			offset += faceVertexCount;
		}
		if (materialCount != 0 && indexCount == 0)
		{
			colladaPolys->Release();
			SAFE_DELETE_ARRAY(reverseOrders);
			continue;
		}

		// Retrieve the only polygons set input: since everything is per-vertex.
		FCDGeometryPolygonsInput* positionInput = colladaPolys->GetInput(0);
		FCDGeometryPolygonsInput* normalInput = colladaPolys->AddInput(normalSource, 1);
		FCDGeometryPolygonsInput* texCoordInput = colladaPolys->AddInput(texCoordSource, 2);
		positionInput->SetIndexCount(indexCount);
		normalInput->SetIndexCount(indexCount);
		texCoordInput->SetIndexCount(indexCount);
		texCoordInput->SetSet(0);
		uint32* positionIndices = positionInput->GetIndices();
		uint32* normalIndices = normalInput->GetIndices();
		uint32* texCoordIndices = texCoordInput->GetIndices();

		offset = 0;
		for (int j = 0; j < polygonCount; ++j)
		{
			uint32 faceVertexCount = tessellator->PrimitiveVertexCount[j];
			FUAssert(faceVertexCount > 2, continue);

			if (materialCount < 2 || tessellator->MaterialId[offset] == (int) i)
			{
				if ((uint32) tessellator->TextureId.GetCount() > offset)
				{
					// This is necessary to support different color textures for different materials.
					colladaPolys->SetUserHandle((void*) (size_t) tessellator->TextureId[offset]);
				}

#define PROCESS_VERTEX(index) \
	(*positionIndices) = tessellator->VertexIndex[index]; \
	(*normalIndices) = tessellator->NormalIndex[index]; \
	(*texCoordIndices) = tessellator->UVIndex[index]; \
	hasVertexFaceNormals |= !IsEquivalent(*positionIndices, *normalIndices); \
	hasVertexFaceTexCoords |= !IsEquivalent(*positionIndices, *texCoordIndices); \
	if (positionCount <= (*positionIndices)) positionCount = (*positionIndices) + 1; \
	if (normalCount <= (*normalIndices)) normalCount = (*normalIndices) + 1; \
	if (texCoordCount <= (*texCoordIndices)) texCoordCount = (*texCoordIndices) + 1; \
	++positionIndices; ++normalIndices; ++texCoordIndices;

				bool reverseOrder = reverseOrders[j];
				if (isStrips && reverseOrder)
				{
					// Extract and reverse the first polygon from the strip.
					colladaPolys->AddFaceVertexCount(3);
					for (uint32 k = 0; k < 3; ++k)
					{
						PROCESS_VERTEX(offset + 2 - k)
					}
				}
				if (!isStrips || !reverseOrder || faceVertexCount > 3)
				{
					// Create the face and fill it with the index data.
					colladaPolys->AddFaceVertexCount(faceVertexCount - (reverseOrder ? 1 : 0));
					for (uint32 k = reverseOrder ? 1 : 0; k < faceVertexCount; ++k)
					{
						PROCESS_VERTEX(offset + k);
					}
				}
			}

			offset += faceVertexCount;
		}

		SAFE_DELETE_ARRAY(reverseOrders);
	}

	// Check whether we can downgrade the normals or texture coordinate to a per-vertex source.
	if (!hasVertexFaceTexCoords)
	{
		size_t colladaPolygonsCount = colladaMesh->GetPolygonsCount();
		for (size_t i = 0; i < colladaPolygonsCount; ++i)
		{
			FCDGeometryPolygons* colladaPolys = colladaMesh->GetPolygons(i);
			colladaPolys->FindInput(texCoordSource)->Release();
		}
		colladaMesh->AddVertexSource(texCoordSource);
	}

	if (!hasVertexFaceNormals)
	{
		size_t colladaPolygonsCount = colladaMesh->GetPolygonsCount();
		for (size_t i = 0; i < colladaPolygonsCount; ++i)
		{
			FCDGeometryPolygons* colladaPolys = colladaMesh->GetPolygons(i);
			FCDGeometryPolygonsInput* texCoordInput = colladaPolys->FindInput(texCoordSource);
			colladaPolys->FindInput(normalSource)->Release();
			if (texCoordInput->GetOffset() == 2)
			{
				// Need to bring this input back to offset 1.
				FCDGeometryPolygonsInput* backedInput = colladaPolys->AddInput(texCoordSource, 1);
				backedInput->SetSet(0);
				backedInput->SetIndices(texCoordInput->GetIndices(), texCoordInput->GetIndexCount());
				SAFE_RELEASE(texCoordInput);
			}
		}
		colladaMesh->AddVertexSource(normalSource);
	}
}
*/


void GeometryExporter::ExportMeshVertices(FBMesh* geometry, FCDGeometryMesh* colladaMesh)
{
//	FBFastTessellator* tessellator = GetTessellator(geometry);
	const int vectorStride = GetOptions()->Export3dData() ? 3 : 4;
	static const int texCoordStride = 2;

	// Fill in the source buffers individually
	// Positions are 4D in Motion Builder, but we support 3D export.
	positionSource->SetDaeId(colladaMesh->GetParent()->GetDaeId() + "-positions");
	positionSource->SetStride(vectorStride);
	positionSource->SetValueCount(positionCount);
	float* positions = positionSource->GetData();

//	float* positionData = tessellator->GetPositionArray();
    int pOutArrayCount;
	FBGeometry* geom = ((FBGeometry*) geometry);
	FBVertex* positionData   = geom->GetPositionsArray(pOutArrayCount);

	for (size_t i = 0; i < positionCount; ++i)
	{
		for (int j = 0; j < vectorStride; ++j) 
		{
			
			(*positions++) = positionData->operator[](j);
		}

		positionData += 4;
	}

	// Normals are 4D in Motion Builder, but we support 3D export.
	normalSource->SetDaeId(colladaMesh->GetParent()->GetDaeId() + "-normals");
	normalSource->SetStride(vectorStride);
	normalSource->SetValueCount(normalCount);
	float* normals = normalSource->GetData();


//	float* normalData = tessellator->GetNormalArray();
	geom = ((FBGeometry*) geometry);
	FBVertex* normalData   = geom->GetNormalsDirectArray(pOutArrayCount);

	for (size_t i = 0; i < normalCount; ++i)
	{
		for (int j = 0; j < vectorStride; ++j)
		{

			(*normals++) = normalData->operator[](j);
		}
		normalData += 4;
	}

	// Texcoords are always 2D.
	texCoordSource->SetDaeId(colladaMesh->GetParent()->GetDaeId() + "-texcoords");
	texCoordSource->SetStride(texCoordStride);
	texCoordSource->SetValueCount(texCoordCount);
	float* texCoords = texCoordSource->GetData();


	//float* uvData = tessellator->GetUVArray();
	geom = ((FBGeometry*) geometry);
	FBUV* uvData   = geom->GetUVSetDirectArray(pOutArrayCount);
	
	for (size_t i = 0; i < texCoordCount; ++i)
	{
		(*texCoords++) = uvData++->operator[](i);
		(*texCoords++) = uvData++->operator[](i);
	//	(*texCoords++) = (*uvData++);
	//	(*texCoords++) = (*uvData++);
	}
}


/*
FBFastTessellator* GeometryExporter::GetTessellator(FBGeometry* geometry)
{
	if (!GetOptions()->ExportTriangleStrips())
	{
		return geometry->FastTessellatorPolygon;
	}
	
	FBFastTessellator* stripTessellator = geometry->FastTessellatorStrip;
	if (stripTessellator->VertexIndex.GetCount() == 0)
	{
		// The strip tessellator is empty, so don't use it.
		return geometry->FastTessellatorPolygon;
	}
	else
	{
		return stripTessellator;
	}
}
*/
