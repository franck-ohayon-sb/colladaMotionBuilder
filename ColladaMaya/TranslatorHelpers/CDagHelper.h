/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAG_HELPER_INCLUDED__
#define __DAG_HELPER_INCLUDED__

#ifndef _MFnNumericData
#include <maya/MFnNumericData.h>
#endif // _MFnNumericData

class CDagHelper
{
public:
	// Handle node connections
	static MObject		GetNodeConnectedTo(const MObject& node, const MString& attribute);
	static MObject		GetSourceNodeConnectedTo(const MObject& node, const MString& attribute);
	static bool			GetPlugConnectedTo(const MObject& node, const MString& attribute, MPlug& plug);
	static MObject		GetSourceNodeConnectedTo(const MPlug& inPlug);
	static bool			GetPlugConnectedTo(const MPlug& inPlug, MPlug& plug);
	static bool			GetPlugArrayConnectedTo(const MObject& node, const MString& attribute, MPlug& plug);
	static MObject		GetNodeConnectedTo(const MPlug& plug);
	static int			GetNextAvailableIndex(const MObject& object, const MString& attribute, int startIndex = 0);
	static int			GetNextAvailableIndex(const MPlug& p, int startIndex = 0);
	static bool			Connect(const MObject& source, const MString& sourceAttribute, const MObject& destination, const MString& destinationAttribute);
	static bool			Connect(const MObject& source, const MString& sourceAttribute, const MPlug& destination);
	static bool			Connect(const MPlug& source, const MObject& destination, const MString& destinationAttribute);
	static bool			Connect(const MPlug& source, const MPlug& destination);
	static bool			ConnectToList(const MObject& source, const MString& sourceAttribute, const MObject& destination, const MString& destinationAttribute, int* index = NULL);
	static bool			ConnectToList(const MPlug& source, const MObject& destination, const MString& destinationAttribute, int* index = NULL);
	static bool			HasConnection(const MPlug& plug, bool asSource = true, bool asDestination = true);
	static bool			Disconnect(const MPlug& source, const MPlug& destination);
	static bool			Disconnect(const MPlug& plug, bool sources = true, bool destinations = true);

	// Handle mesh-related node connections that use grouped components
	static void			GroupConnect(MPlug& source, const MObject& destination, const MObject& finalMesh);

	// Get/set the bind pose for a transform
	static MMatrix		GetBindPoseInverse(const MObject& controller, const MObject& influence);
	static MStatus		SetBindPoseInverse(const MObject& node, const MMatrix& bindPoseInverse);

	// Find a child plug
	static MPlug		GetChildPlug(const MPlug& parent, const MString& name, MStatus* rc=NULL);
	static int			GetChildPlugIndex(const MPlug& parent, const MString& name, MStatus* rc=NULL); // Useful for performance reasons when iterating over many plugs

	// Get/set a plug's value
	static bool			GetPlugValue(const MObject& node, const char* attributeName, double &value);
	static bool			GetPlugValue(const MObject& node, const char* attributeName, float &value);
	static bool			GetPlugValue(const MObject& node, const char* attributeName, MEulerRotation& value);
	static bool			GetPlugValue(const MObject& node, const char* attributeName, bool& value);
	static bool			GetPlugValue(const MObject& node, const char* attributeName, int& value);
	static bool			GetPlugValue(const MObject& node, const char* attributeName, MMatrix& value);
	static bool			GetPlugValue(const MObject& node, const char* attributeName, MColor& value);
	static bool			GetPlugValue(const MObject& node, const char* attributeName, MString& value);
	static bool			GetPlugValue(const MObject& node, const char* attributeName, MVector& value);
	static bool			GetPlugValue(const MPlug& plug, bool& value);
	static bool			GetPlugValue(const MPlug& plug, MMatrix& value);
	static bool			GetPlugValue(const MPlug& plug, MColor& value);
	static bool			GetPlugValue(const MPlug& plug, int& value);
	static bool			GetPlugValue(const MPlug& plug, uint32& value);
	static bool			GetPlugValue(const MPlug& plug, short& value);
	static bool			GetPlugValue(const MPlug& plug, uint16& value);
	static bool			GetPlugValue(const MPlug& plug, char& value);
	static bool			GetPlugValue(const MPlug& plug, uint8& value);
	static bool			GetPlugValue(const MPlug& plug, float& x);
	static bool			GetPlugValue(const MPlug& plug, float& x, float& y);
	static bool			GetPlugValue(const MPlug& plug, float& x, float& y, float& z);
	static bool			GetPlugValue(const MPlug& plug, MVector& value);
	static void			GetPlugValue(const MObject& node, const char* attributeName, MStringArray& output, MStatus* ReturnStatus = NULL);
	static void			GetPlugValue(const MPlug& plug, MStringArray& output, MStatus* ReturnStatus = NULL);

	static bool			SetPlugValue(MPlug& plug, const MVector& value);
	static bool			SetPlugValue(MPlug& plug, const MMatrix& value);
	static bool			SetPlugValue(MPlug& plug, const MColor& value);
	static bool			SetPlugValue(MPlug& plug, const fm::string& value);
#ifdef UNICODE
	static bool			SetPlugValue(MPlug& plug, const fstring& value);
#endif // UNICODE
	static bool			SetPlugValue(MPlug& plug, const MString& value) { return plug.setValue(const_cast<MString&>(value)) == MStatus::kSuccess; }
	static bool			SetPlugValue(MPlug& plug, float value);
	static bool			SetPlugValue(MPlug& plug, double value);
	static bool			SetPlugValue(MPlug& plug, float x, float y);
	static bool			SetPlugValue(MPlug& plug, int value);
#if defined(WIN32)
	static bool			SetPlugValue(MPlug& plug, int32 value) { return SetPlugValue(plug, (int) value); }
#endif // windows-only.
	static bool			SetPlugValue(MPlug& plug, bool value);
	static bool			SetPlugValue(MPlug& plug, MStringArray& stringArray);
	
	// Helper to avoid the findPlug.
	template <class ValueType>
	static bool			SetPlugValue(const MObject& node, const char* attributeName, const ValueType& value)
	{ MPlug p = MFnDependencyNode(node).findPlug(attributeName); return SetPlugValue(p, value); }

	// set an array plug size before creating the element plugs
	static void			SetArrayPlugSize(MPlug& plug, uint size);

	// Attempt to translate a string path/name into a dag path or node
	static MDagPath		GetShortestDagPath(const MObject& node);
	static MObject		GetNode(const MString& name);

	// Create an animation curve for the given plug
	static MObject		CreateAnimationCurve(const MObject& node, const char* attributeName, const char* curveType);
	static MObject		CreateAnimationCurve(const MPlug& plug, const char* curveType);
	static bool			PlugHasAnimation(const MPlug& plug);
	static MObject		CreateExpression(const MPlug& plug, const MString& expression);

	// Create a string-typed attribute on a given node
	static MObject		CreateAttribute(const MObject& node, const char* attributeName, const char* attributeShortName, MFnData::Type type, const char *value);
	static MObject		CreateAttribute(const MObject& node, const char* attributeName, const char* attributeShortName, MFnNumericData::Type type, const char *value);
	static MPlug		AddAttribute(const MObject& node, const MObject& attribute);
	
	// get the first empty item in the named array plug
	static unsigned GetFirstEmptyElementId(const MPlug& parent);
};

#endif
