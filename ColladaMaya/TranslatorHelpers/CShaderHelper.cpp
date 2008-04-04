/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"

#include <maya/MFnAttribute.h>
#include <maya/MFnMesh.h>
//Hack to circumvent bad code in Maya7.0
class MFnNurbsSurface;
#include <maya/MFnNurbsSurface.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MSelectionList.h>
#include <maya/MFnComponentListData.h>

#include "CDagHelper.h"
#include "CShaderHelper.h"

// Get the UV set for a procedural/file texture
//
uint CShaderHelper::GetAssociatedUVSet(const MObject& shape, const MObject& textureNode)
{
	// Rather than trying to find all the node types which may be derived
	// off texture2d, then handling optionally handling placement nodes,
	// we can simply walk backwards along the uvCoord attributes until
	// we find a uvChooser.
	MStatus status;
	MObject node = textureNode;
	while(node.apiType() != MFn::kUvChooser)
	{
		MFnDependencyNode dgFn(node);
		MPlug plug = dgFn.findPlug(MString("uvCoord"), &status);
		if (status == MS::kSuccess && plug.isConnected())
		{
			// Get the connection - there can be at most one input to a plug
			//
			MPlugArray connections;
			plug.connectedTo(connections, true, false);
			if (connections.length() > 0)
			{
				node = connections[ 0].node();
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
	}

	// Did we find a uvChooser?
	//
	if (node.apiType() == MFn::kUvChooser)
	{
		// Find the uvSet connection to this shape
		//
		MFnDependencyNode dgFn(node);
		MPlug plug = dgFn.findPlug(MString("uvSets"), &status);
		if (status == MS::kSuccess && plug.isArray())
		{
			// Iterate over all the elements in this array looking for
			// one which connects to our shape
			//
			uint numUVSets = plug.evaluateNumElements();
			for (uint i = 0; i < numUVSets; i++)
			{
				MPlug uvSetPlug = plug.elementByPhysicalIndex(i, &status);
				if (status && uvSetPlug.isConnected())
				{
					// Get the connection - there can be at most one input to a plug
					//
					MPlugArray connections;
					uvSetPlug.connectedTo(connections, true, false);
					if (connections.length() > 0 && connections[0].node() == shape)
					{
						// connected through name element
						MPlug uvSetElement= connections[0].parent();
						MPlug uvSetArray = uvSetElement.array();
						if (uvSetArray.isArray())
						{
							uint logicalIndex = uvSetElement.logicalIndex();
							for (uint child = 0; child < uvSetArray.numElements(); ++child)
							{
								MPlug childPlug = uvSetArray.elementByPhysicalIndex(child);
								if (childPlug.logicalIndex() == logicalIndex)
								{
									return child;
								}
							}
						} 
					}
				}
			}
		}
	}


	// [KThacker] Another weird thing. MFnMesh keeps an internal list of uvSet names.
	// This is not necessarily in the physical order of the plugs under uvSet (elementByPhysicalIndex),
	// or the logical order (through MEL uvSet[<logical index>]).
	// When no uvChooser is present it uses the first uvSet name from the list
	// to define which uvSet to use as default, if the name is different it uses a uvChooser.
	// Got this problem after importing a file from FBX.
	//
	// So we get the list of names and search through the plugs in physical order
	// to get the correct physical index to return.

	MFnMesh mesh(shape);
	MStringArray setNames;
	mesh.getUVSetNames(setNames);

	MPlug uvSetPlug = mesh.findPlug("uvSet");

	for (uint i = 0; i < uvSetPlug.numElements(); ++i)
	{
		// get uvSet[<index>]
		MPlug uvSetElememtPlug = uvSetPlug.elementByPhysicalIndex(i);

		// get uvSet[<index>].uvSetName
		MPlug uvSetNamePlug = uvSetElememtPlug.child(0);

		// get value of plug (uvSet's name)
		MString uvSetName;

		uvSetNamePlug.getValue(uvSetName);

		if (uvSetName==setNames[0])
			return i;
	
	}

	// we don't want to get here, because it'll end up with wrong results.
	return 0;
}

MObject CShaderHelper::GetLayeredTextures(const MObject& layeredTexture, MObjectArray& fileTextures, MIntArray& blendModes)
{
	MFnDependencyNode layeredTextureFn(layeredTexture);
	MStatus rc;

	MPlug inputs = FindPlug(layeredTextureFn, "inputs", &rc);
	CHECK_STATUS(rc, MString("Unable to locate inputs array plug on layeredTexture: ") + layeredTextureFn.name());

	uint elementCount = inputs.numElements();
	for (int i = (int) (elementCount - 1); i >= 0; --i)
	{
		MPlug input = inputs.elementByPhysicalIndex(i, &rc);
		CHECK_STATUS(rc, MString("Unable to locate input#") + i + MString(" on layeredTexture: ") + layeredTextureFn.name());

		MPlug colorInput = CDagHelper::GetChildPlug(input, "c", &rc); // "color"
		CHECK_STATUS(rc, MString("Unable to locate input#") + i + MString("'s color child plug on layeredTexture: ") + layeredTextureFn.name());

		MObject connection = CDagHelper::GetNodeConnectedTo(colorInput);
		if (connection != MObject::kNullObj)
		{
			fileTextures.append(connection);

			MPlug blendMode = CDagHelper::GetChildPlug(input, "bm", &rc); // "blendMode"
			CHECK_STATUS(rc, MString("Unable to locate input#") + i + MString("'s blend mode child plug on layeredTexture: ") + layeredTextureFn.name());
			
			int mode; blendMode.getValue(mode);
			blendModes.append(mode);
		}
	}

	return layeredTexture;
}

//
// Create a new shader
//
MObject CShaderHelper::CreateShader(MFn::Type type, const MString& name)
{
	// Given shadingEngines are quite tricky to create and setup, the
	// simplest creation method is to re-use the existing MEL creation
	// command. There's obviously some overhead, but it's a LOT easier =)
	//
	MString cmd = "shadingNode -asShader ";
	MString strType;
	switch (type)
	{
	case MFn::kLambert:		strType = "lambert";		break; 
	case MFn::kPhong:		strType = "phong";		break;
	case MFn::kBlinn:		strType = "blinn";		break;
	case MFn::kSurfaceShader:	strType = "surfaceShader";	break;
	default:
		MGlobal::displayError(MString("Shader type not supported by CShaderHelper::createShader: ") + type);
		return MObject::kNullObj;
	}
	cmd += strType + " -name \"" + name + "\";";
	MString result;
	MGlobal::executeCommand(cmd, result);
	MObject depNode = CDagHelper::GetNode(result);

	// If the name clashes with an existing default object,
	// the creation will fail
	if (depNode == MObject::kNullObj)
	{
		MGlobal::displayError("Name clash when trying to create new shader.");
	}

	// Second step is to setup the shadingEngine and connect it up ...
	cmd = "string $sg = `sets -renderable true -noSurfaceShader true -empty -name " + result 
		+ "SG`; defaultNavigation -connectToExisting -source " + result
		+ " -destination $sg;";
	MGlobal::executeCommand(cmd, result);

	return depNode;
}

MStatus CShaderHelper::AttachMeshShader(const MObject& shadingGroup, const MObject& mesh, const MObject& components, uint instanceNumber)
{
	MFnMesh meshFn(mesh);
	MFnDependencyNode sgFn(shadingGroup);
	MFnComponentListData componentListFn;

	// Retrieve the object group plug
	MPlug objectGroupsInstancesPlug = meshFn.findPlug("iog");
	MPlug objectGroupsInstancePlug = objectGroupsInstancesPlug.elementByLogicalIndex(instanceNumber);
	MPlug objectGroupsPlug = CDagHelper::GetChildPlug(objectGroupsInstancePlug, "og");

	// Check if a group already exists between this mesh and the shading group.
	uint numGroups = objectGroupsPlug.numElements();
	uint groupIndex = 0;
	for (; groupIndex < numGroups; ++groupIndex)
	{
		MPlug headPlug = objectGroupsPlug.elementByPhysicalIndex(groupIndex);
		MObject connection = CDagHelper::GetNodeConnectedTo(headPlug);

		// Retrieve the component list for this group.
		MObject componentList;
		MPlug componentListPlug = CDagHelper::GetChildPlug(headPlug, "gcl");
		componentListPlug.getValue(componentList);
		componentListFn.setObject(componentList);

		if (connection != shadingGroup)
		{
			// Check if the wanted components are included in this group.
			// Strangely, that happens often with materials.
			// Also: this could break skinning, need testing!!
			if (componentListFn.has(components)) break;
		}
		else
		{
			// Re-use this group, since the faces end up on the same material.
			componentListFn.add(const_cast<MObject&>(components));
			componentListPlug.setValue(componentList);
			return MStatus::kSuccess;
		}
	}

	MPlug objectGroupPlug = (groupIndex >= numGroups) ? objectGroupsPlug.elementByLogicalIndex(groupIndex) : objectGroupsPlug.elementByPhysicalIndex(groupIndex);

	MFnDependencyNode groupIdFn;
	if (groupIndex >= numGroups)
	{

		// Create the group id node
		groupIdFn.create("groupId");
		CDagHelper::SetPlugValue(groupIdFn.object(), "isHistoricallyInteresting", 0);

		// Assign the components
		MPlug componentListPlug = CDagHelper::GetChildPlug(objectGroupPlug, "gcl");
		MObject componentList = componentListFn.create();
		componentListFn.add(const_cast<MObject&>(components));
		componentListPlug.setValue(componentList);
		
		// Attach the group id node to the mesh
		MPlug groupIdPlug = CDagHelper::GetChildPlug(objectGroupPlug, "objectGroupId");
		CDagHelper::Connect(groupIdFn.object(), "groupId", groupIdPlug);

		MPlug inMeshPlug = meshFn.findPlug("inMesh");
		if (CDagHelper::HasConnection(inMeshPlug, false, true))
		{
			// Need to add group parts node
			MPlug oldConnection;
			CDagHelper::GetPlugConnectedTo(inMeshPlug, oldConnection);
			MFnDependencyNode groupPartsFn;
			groupPartsFn.create("groupParts");
			MPlug groupPartsComponentPlug = groupPartsFn.findPlug("ic");
			groupPartsComponentPlug.setValue(componentList);
			CDagHelper::SetPlugValue(groupPartsFn.object(), "isHistoricallyInterested", 0);
			CDagHelper::Connect(oldConnection, groupPartsFn.object(), "inputGeometry");
			CDagHelper::Connect(groupIdFn.object(), "groupId", groupPartsFn.object(), "groupId");
	 		CDagHelper::Disconnect(oldConnection, inMeshPlug);
			CDagHelper::Connect(groupPartsFn.object(), "outputGeometry", inMeshPlug);
		}
	}
	else
	{
		// Retrieve the group id node.
		MPlug groupIdPlug = CDagHelper::GetChildPlug(objectGroupPlug, "objectGroupId");
		groupIdFn.setObject(CDagHelper::GetNodeConnectedTo(groupIdPlug));
	}

	// Attach the shading group's color to the object group
	MPlug colorPlug = CDagHelper::GetChildPlug(objectGroupPlug, "objectGrpColor");
	CDagHelper::Disconnect(colorPlug);
	CDagHelper::Connect(sgFn.object(), "mwc", colorPlug);

	// Attach the group id node and the object group to the shading group.
	MPlug messagePlug = groupIdFn.findPlug("message");
	CDagHelper::Disconnect(messagePlug);
	CDagHelper::Disconnect(objectGroupPlug);

	// Figure out the first available indices in the two array plugs.
	int index = -1;
	index = CDagHelper::GetNextAvailableIndex(shadingGroup, "groupNodes", index);
	index = CDagHelper::GetNextAvailableIndex(shadingGroup, "dagSetMembers", index);

	if (index > -1)
	{
		CDagHelper::ConnectToList(messagePlug, shadingGroup, "groupNodes", &index);
		CDagHelper::ConnectToList(objectGroupPlug, shadingGroup, "dagSetMembers", &index);
	}

	return MStatus::kSuccess;
}

MStatus CShaderHelper::AttachMeshShader(const MObject& shadingGroup, const MObject& mesh, uint instanceNumber)
{
	MFnMesh meshFn(mesh);
	MFnDependencyNode sgFn(shadingGroup);
	MFnComponentListData componentListFn;

	// Retrieve the object group plug
	MPlug objectGroupsInstancesPlug = meshFn.findPlug("iog");
	MPlug objectGroupsInstancePlug = objectGroupsInstancesPlug.elementByLogicalIndex(instanceNumber);

	// Attach the group id node and the object group to the shading group.
	int index = -1;
	CDagHelper::Disconnect(objectGroupsInstancePlug);
	CDagHelper::ConnectToList(objectGroupsInstancePlug, shadingGroup, "dagSetMembers", &index);

	return MStatus::kSuccess;
}

MStatus CShaderHelper::AttachNURBSSurfaceShader(const MObject& shadingGroup, const MObject& nurbsSurface, uint instanceNumber)
{
	MStatus ret;
	MFnNurbsSurface nurbsFn(nurbsSurface, &ret);
	if (!ret) return ret;

	MFnDependencyNode sgFn(shadingGroup);
	MFnComponentListData componentListFn;

	// Retrieve the object group plug
	MPlug objectGroupsInstancesPlug = nurbsFn.findPlug("iog");
	MPlug objectGroupsInstancePlug = objectGroupsInstancesPlug.elementByLogicalIndex(instanceNumber);

	// Attach the group id node and the object group to the shading group.
	int index = -1;
	CDagHelper::Disconnect(objectGroupsInstancePlug);
	CDagHelper::ConnectToList(objectGroupsInstancePlug, shadingGroup, "dagSetMembers", &index);

	return MStatus::kSuccess;
}

//
// Get the shadingEngine used by a specific shader (the inverse of the above)
//
MObject CShaderHelper::GetShadingEngine(MObject shadingNetwork)
{
	MStatus rc = MStatus::kSuccess;
	MItDependencyGraph it(shadingNetwork, 
			MFn::kShadingEngine, 
			MItDependencyGraph::kDownstream, 
			MItDependencyGraph::kBreadthFirst, 
			MItDependencyGraph::kNodeLevel, &rc);
	return (rc == MStatus::kSuccess ? it.thisNode() : MObject::kNullObj);
}

MStatus CShaderHelper::GetConnectedShaders(const MFnDependencyNode& node, MObjectArray& shaders)
{
	MPlugArray nodePlugs;
	node.getConnections(nodePlugs);

	for (uint i = 0; i < nodePlugs.length(); i++)
	{
		const MPlug& nodePlug = nodePlugs[i];
		MPlugArray nodeDestinations;
		nodePlug.connectedTo(nodeDestinations, false, true);

		for (uint j = 0; j < nodeDestinations.length(); j++)
		{
			if (nodeDestinations[j].node().apiType() == MFn::kShadingEngine)
			{
				shaders.append(nodeDestinations[j].node());
			}
		}
	}

	return (shaders.length() > 0 ? MStatus::kSuccess : MStatus::kFailure);
}


//
// Attach a shading network to a shading engine
//
MStatus CShaderHelper::AttachShadingNetwork(MObject shadingEngine, MObject shadingNetwork, const MString& type)
{
	MStatus status;

	MFnDependencyNode engineFn(shadingEngine);
	MFnDependencyNode networkFn(shadingNetwork);
	
	MPlug enginePlug = engineFn.findPlug(type, &status);
	if (status != MStatus::kSuccess)
	{
		MGlobal::displayError(MString("Unable to locate shadingEngine plug: ") + type);
		return status;
	}

	const MString networkAttr = "outColor";
	MPlug networkPlug = networkFn.findPlug(networkAttr, &status);
	if (status != MStatus::kSuccess)
	{
		MGlobal::displayError(MString("Unable to locate shadingNetwork plug: ") + networkAttr);
		return status;
	}

	MDGModifier dgMod;
	dgMod.connect(networkPlug, enginePlug);
	return dgMod.doIt();
}


//
// Attach a place2dTexture node to a texture image
//
MObject CShaderHelper::Create2DPlacementNode(MObject texture)
{
	MStatus rc;
	MDGModifier dgModifier;
	MPlug src, dest;

	MFnDependencyNode textureFn(texture);
	MFnDependencyNode place2dTextureFn;
	MObject place2dTexture = place2dTextureFn.create("place2dTexture", &rc);
	CHECK_STATUS(rc, "Unable to create place2dTexture node");

	#define CONNECT(srcAttr, destAttr) \
	src = place2dTextureFn.findPlug(srcAttr, &rc); \
	CHECK_STATUS(rc, MString("Unable to locate ") + srcAttr + " attribute on new place2dTexture node."); \
	dest = textureFn.findPlug(destAttr, &rc); \
	CHECK_STATUS(rc, MString("Unable to locate ") + destAttr + " attribute on file texture node: " + textureFn.name()); \
	dgModifier.connect(src, dest);

	CONNECT("outUV", "uvCoord");
	CONNECT("outUvFilterSize", "uvFilterSize");
	CONNECT("vertexUvOne", "vertexUvOne");
	CONNECT("vertexUvTwo", "vertexUvTwo");
	CONNECT("vertexUvThree", "vertexUvThree");
	CONNECT("vertexCameraOne", "vertexCameraOne");
	CONNECT("offset", "offset");
	CONNECT("stagger", "stagger");
	CONNECT("coverage", "coverage");
	CONNECT("translateFrame", "translateFrame");
	CONNECT("mirrorV", "mirrorV");
	CONNECT("mirrorU", "mirrorU");
	CONNECT("wrapU", "wrapU");
	CONNECT("wrapV", "wrapV");
	CONNECT("noiseUV", "noiseUV");
	CONNECT("rotateUV", "rotateUV");
	CONNECT("repeatUV", "repeatUV");
	#undef CONNECT

	dgModifier.doIt();
	return place2dTexture;
}

//
// Attach a bump2d node to a shader
//
MObject CShaderHelper::CreateBumpNode(MObject texture)
{
	MFnDependencyNode textureFn(texture);
	MDGModifier dgModifier;
	MStatus rc;

	MPlug src = FindPlug(textureFn, "outAlpha", &rc);
	CHECK_STATUS(rc, "Unable to locate outAlpha attribute on texture node for bump connect.");

	if (src.isConnected())
	{
		MGlobal::displayWarning(MString("Texture node: ") + textureFn.name() + ", already has a node associated with outAlpha!");
		return CDagHelper::GetNodeConnectedTo(src);
	}

	const char* bumpType;
	if (texture.hasFn(MFn::kTexture3d)) bumpType = "bump3d";
	else bumpType = "bump2d";

	MFnDependencyNode bumpFn;
	MObject bumpNode = bumpFn.create(bumpType, &rc);
	CHECK_STATUS(rc, MString("Unable to create new bump node for texture: ") + textureFn.name());

	MPlug dest = FindPlug(bumpFn, "bumpValue", &rc);
	CHECK_STATUS(rc, MString("Unable to locate outNormal attribute on new bump node"));

	dgModifier.connect(src, dest);
	dgModifier.doIt();

	return bumpNode;
}

MObject CShaderHelper::CreateLayeredTexture(const DaeTextureList& textures)
{
	MFnDependencyNode layeredTextureFn;
	MObject layeredTexture = layeredTextureFn.create("layeredTexture");
	MDGModifier dgModifier;
	MStatus rc;

    MPlug inputs = FindPlug(layeredTextureFn, "inputs", &rc);
	CHECK_STATUS(rc, "Unable to locate inputs plug array on new layeredTexture node.");

	uint textureCount = (uint) textures.size();
	for (int i = (int) textureCount - 1; i >= 0; --i)
	{
		const DaeTexture& tx = textures[i];
		const MObject& frontObject = tx.frontObject;
		MFnDependencyNode frontFn(frontObject);
		MPlug src = FindPlug(frontFn, "outColor", &rc);
		CHECK_STATUS(rc, MString("Unable to locate outColor node on texture: ") + frontFn.name());

		MPlug input = inputs.elementByLogicalIndex(textureCount - i - 1, &rc);
		CHECK_STATUS(rc, MString("Unable to create new plug element input on new layeredTexture node."));

		MPlug dest = CDagHelper::GetChildPlug(input, "c", &rc); // "color"
		CHECK_STATUS(rc, MString("Unable to locate color input's child plug on new layeredTexture node."));

		dgModifier.connect(src, dest);

		MPlug blendMode = CDagHelper::GetChildPlug(input, "bm", &rc); // "blendMode"
		CHECK_STATUS(rc, "Unable to locate blendMode input's child plug on new layeredTexture node.");

		// Figure out the right blend mode, if none was imported:
		// 0 -> None. 6 -> Multiply, for all but the first texture of a layeredTexture
		int mode = tx.blendMode;
		if (mode == -1) mode = (i > 0) ? 6 : 0;
		blendMode.setValue(mode);
	}

	dgModifier.doIt();

	return layeredTexture;
}

MObject CShaderHelper::AssociateUVSetWithTexture(MObject texture, MObject mesh, int uvSetIndex)
{
	MStatus rc;
	MPlug p;
	MPlugArray plugs;
	MFnDependencyNode meshFn(mesh);
	MFnDependencyNode textureFn(texture);
	MDGModifier dgModifier;

	// Find the texture node's place2dTexture input.
	//
	MObject place2dTexture;
	p = FindPlug(textureFn, "uvCoord", &rc);
	CHECK_STATUS(rc, "Unable to find file texture uvCoord plug.");

	if (!p.isConnected()) place2dTexture = Create2DPlacementNode(texture);
	else
	{
		plugs.clear();
		p.connectedTo(plugs, true, false, &rc);
		place2dTexture = plugs[0].node();
	}

	MFnDependencyNode placerFn(place2dTexture);
	p = FindPlug(placerFn, "uvCoord", &rc);
	CHECK_STATUS(rc, "Unable to get the place2dTexture uvCoord plug");
	MObject uvChooser;
	if (!p.isConnected())
	{
		// Create the UV chooser node
		//
		MPlug src, dest;
		MFnDependencyNode uvChooserFn;
		uvChooser = uvChooserFn.create("uvChooser", &rc);
		CHECK_STATUS(rc, "Unable to create uvChooser node");

		// Link the place2dTexture node with the uvChooser
		//
		#define CONNECT(srcAttr, destAttr) \
		src = uvChooserFn.findPlug(srcAttr, &rc); \
		CHECK_STATUS(rc, MString("Unable to locate ") + srcAttr + " attribute on new uvChooser node."); \
		dest = placerFn.findPlug(destAttr, &rc); \
		CHECK_STATUS(rc, MString("Unable to locate ") + destAttr + " attribute on place2dTexture node: " + placerFn.name()); \
		dgModifier.connect(src, dest);

		CONNECT("outUv", "uvCoord");
		CONNECT("outVertexUvOne", "vertexUvOne");
		CONNECT("outVertexUvTwo", "vertexUvTwo");
		CONNECT("outVertexUvThree", "vertexUvThree");
		CONNECT("outVertexCameraOne", "vertexCameraOne");
		#undef CONNECT

		dgModifier.doIt();
	}
	else
	{
		plugs.clear();
		p.connectedTo(plugs, true, false, &rc);
		CHECK_STATUS(rc, "Unable to get the place2dTexture uvCoord plug connections.");
		uvChooser = plugs[0].node();
	}

	// Link the UV chooser to the mesh
	//
	MPlug src = meshFn.findPlug(MString("uvSet"), &rc);
	CHECK_STATUS(rc, "Unable to locate mesh uvSet attribute");
	src = src.elementByPhysicalIndex(uvSetIndex, &rc);
	CHECK_STATUS(rc, "Unable to locate specific mesh uvSet attribute");
	src = CDagHelper::GetChildPlug(src, "uvSetName");

	MFnDependencyNode chooserFn(uvChooser);
	p = FindPlug(chooserFn, "uvSets", &rc);
	CHECK_STATUS(rc, "Unable to locate uvChooser uvSets attribute");
	unsigned int elementCount = p.numConnectedElements();
	MPlug dest = p.elementByLogicalIndex(elementCount, &rc);
	CHECK_STATUS(rc, "Unable to create array element plug for uvChooser uvSets attribute");

	dgModifier.connect(src, dest);
	dgModifier.doIt();
	return uvChooser;
}

MPlug CShaderHelper::FindPlug(const MFnDependencyNode& node, const MString& plugName, MStatus* rc)
{
	MStatus st;
	MPlug p = node.findPlug(plugName, &st);
	if (st == MStatus::kSuccess) { if (rc != NULL) *rc = MStatus::kSuccess; return p; }

	MPlugArray plugs;
	st = node.getConnections(plugs);
	if (st != MStatus::kSuccess) { if (rc != NULL) *rc = st; return p; }

	for (unsigned int i = 0; i < plugs.length(); ++i)
	{
		p = plugs[i];
		MFnAttribute attribute(p.attribute());
		if (attribute.name() ==  plugName)
		{
			if (rc != NULL) *rc = MStatus::kSuccess;
			return p;
		}
	}

	if (rc != NULL) *rc = MStatus::kNotFound;
	return p;
}

uint CShaderHelper::FindArrayPlugEmptyIndex(const MPlug& plug)
{
	MStatus rc;
	uint index;

	uint count = plug.numElements();
	for (index = 0; index < count; ++index)
	{
		MPlug connection = plug.elementByLogicalIndex(index, &rc);
		if (rc == MStatus::kSuccess && !connection.isConnected()) break;
	}

	return index;
}

MObject CShaderHelper::FindDisplayLayer(const char* name)
{
	MStatus rc;
	MDGModifier dgModifier;
	MFnDependencyNode layerNode;

	// Find the layer manager
	MFnDependencyNode layerManager;
	for (MItDependencyNodes dependencyIt; !dependencyIt.isDone(); dependencyIt.next())
	{
		MFnDependencyNode node(dependencyIt.item());
		if (node.name() == "layerManager")
		{
			layerManager.setObject(node.object());
			break;
		}
	}

	if (layerManager.object() == MObject::kNullObj)
	{
		MGlobal::displayError("Unable to find default layer manager.");
		return MObject::kNullObj;
	}

	// Look for a display layer with this name
	MPlug layerListPlug = FindPlug(layerManager, "displayLayerId", &rc);
	CHECK_STATUS(rc, "Unable to find default layer manager's id plug array.");
	uint layerListPlugCount = layerListPlug.numElements();
	for (uint i = 0; i < layerListPlugCount; ++i)
	{
		MPlug layerPlug = layerListPlug.elementByPhysicalIndex(i, &rc);
		CHECK_STATUS(rc, "Unable to find default layer manager's physical id plug.");

		MObject connection = CDagHelper::GetNodeConnectedTo(layerPlug);
		MFnDependencyNode connectedNode(connection);
		if (connectedNode.name() ==  name)
		{
			return connection;
		}
	}

	// Create a new layer node and get its "identification" plug
	layerNode.create("displayLayer", name, &rc);
	CHECK_STATUS(rc, MString("Unable to create new display layer: ") + name);

	MPlug dst = FindPlug(layerNode, "identification", &rc);
	CHECK_STATUS(rc, MString("Unable to get new display layer's identification plug: ") + name);

	// Connect the layer node to the layer manager's list of layers
	uint logicalIndex = FindArrayPlugEmptyIndex(layerListPlug);
	MPlug src = layerListPlug.elementByLogicalIndex(logicalIndex, &rc);
	CHECK_STATUS(rc, "Unable to find default layer manager's correct id plug.");

	dgModifier.connect(src, dst);
	dgModifier.doIt();

	// Set the ID value unto the display manager child plug.
	// Fixes SourceForge 1425745: Layer information is missing from the UI on import.
	CDagHelper::SetPlugValue(src, (int) logicalIndex); // Arbitary size

	return layerNode.object();
}

static const int g_projectionTypeValueCount = 9;
static const fchar* g_projectionTypeValues[g_projectionTypeValueCount] = { FC("NONE"), FC("PLANAR"), FC("SPHERICAL"), FC("CYLINDRICAL"), FC("BALL"), FC("CUBIC"), FC("TRIPLANAR"), FC("CONCENTRIC"), FC("PERSPECTIVE") };
int CShaderHelper::ToProjectionType(const fchar* type)
{
	for (int i = 0; i < g_projectionTypeValueCount; ++i)
	{
		if (IsEquivalent(type, g_projectionTypeValues[i])) return i;
	}

	// According to the documentation, "PLANAR" is the default value
	return DefaultProjectionType();
}

const fchar* CShaderHelper::ProjectionTypeToString(int type)
{
	if (type >= 0 && type < g_projectionTypeValueCount)
	{
		return g_projectionTypeValues[type];
	}
	else
	{
		return g_projectionTypeValues[DefaultProjectionType()];
	}
}

int CShaderHelper::DefaultProjectionType()
{
	// According to the documentation, "PLANAR" is the default value
	return 1;
}
