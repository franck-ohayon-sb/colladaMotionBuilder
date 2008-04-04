/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __SHADER_HELPER_INCLUDED__
#define __SHADER_HELPER_INCLUDED__

#include <maya/MFnDependencyNode.h>

class DaeTexture
{
public:
	const FCDTexture* colladaTexture;
	MObject textureObject;
	MObject frontObject;
	int blendMode;

	DaeTexture(const FCDTexture* _colladaTexture)
		: colladaTexture(_colladaTexture), blendMode(-1)
	{
	}
};
typedef fm::vector<DaeTexture> DaeTextureList;

class CShaderHelper
{
public:
	// Get the UV set for a procedural/file texture
	static unsigned int	GetAssociatedUVSet(const MObject& shape, const MObject& textureNode);

	// Get a list of file texture node associated with the given layeredTexture node
	// Note: the returned MObject is always kNullObj.
	static MObject GetLayeredTextures(const MObject& layeredTexture, MObjectArray& fileTextures, MIntArray& blendModes);

	// Create a new shader
	static MObject CreateShader(MFn::Type type, const MString& name);

	// Attach a shading network to a shading engine
	static MStatus AttachShadingNetwork(MObject shadingEngine, MObject shadingNetwork, const MString& type);

	// Attach a list of mesh components to a shading group.
	static MStatus AttachMeshShader(const MObject& shadingGroup, const MObject& mesh, const MObject& components, uint instanceNumber);
	static MStatus AttachMeshShader(const MObject& shadingGroup, const MObject& mesh, uint instanceNumber);

	// Attach a list of NURBS components to a shading group.
	static MStatus AttachNURBSSurfaceShader(const MObject& shadingGroup, const MObject& nurbsSurface, uint instanceNumber);

	// Get the shadingEngine used by a specific shader (the inverse of the above)
	static MObject GetShadingEngine(MObject shadingNetwork);

	// Find the shaders connected to this dependency node
	// Used for shader retrieval in NURBS surfaces
	static MStatus GetConnectedShaders(const MFnDependencyNode& node, MObjectArray& shaders);

	// Attach a place2dTexture node to a texture image
	static MObject Create2DPlacementNode(MObject texture);

	// Attach a bump2d node to a bump file texture node
	static MObject CreateBumpNode(MObject texture);

	// Attach all the fileTexture nodes together into a layeredTexture node
	static MObject CreateLayeredTexture(const DaeTextureList& textures);

	// Attach a uvChooser node between a uvSet and a texture image
	static MObject AssociateUVSetWithTexture(MObject texture, MObject mesh, int uvSetIndex);

	// Find a connected/non-connected plug from a node
	static MPlug FindPlug(const MFnDependencyNode& node, const MString& plugName, MStatus* rc=NULL); 
	static uint FindArrayPlugEmptyIndex(const MPlug& plug);

	// Find/Create a display layer node
	static MObject FindDisplayLayer(const char* name);

	// Handles projection type
	static int ToProjectionType(const fchar* type);
	static int DefaultProjectionType();
	static const fchar* ProjectionTypeToString(int type);
};

//
// Helper Macro
//
#define CHECK_STATUS(rc, errorString) \
	if ((rc) != MStatus::kSuccess)\
	{\
		MGlobal::displayError(MString(errorString));\
		return MObject::kNullObj;\
	}\

#endif
