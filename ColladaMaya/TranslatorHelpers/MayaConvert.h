/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/*
* Interfaces between the FCDocument classes and the Maya compatible structures
*/

#ifndef _MAYA_CONVERT_H_
#define _MAYA_CONVERT_H_

#ifndef _MFnAnimCurve
#include <maya/MFnAnimCurve.h>
#endif // _MFnAnimCurve

namespace MConvert
{
	void ToFloatList(MFloatArray& a, FloatList& b);
	void ToFloatList(MPointArray& a, FloatList& b);
	void ToFloatList(MVectorArray& a, FloatList& b);
	void ToFloatList(MFloatVectorArray& a, FloatList& b);
	void ToFloatList(MColorArray& a, FloatList& b);
	void ToMPointArray(const float* a, size_t count, uint32 stride, MPointArray& b);
	void AppendToMIntArray(const uint32* a, size_t count, MIntArray& b);
	MVector ToMVector(const float* a, uint32 stride, uint32 index);
	MColor ToMColor(const float* a, uint32 stride, uint32 index);
	MMatrix ToMMatrix(const FMMatrix44& m);
	FMMatrix44 ToFMatrix(const MMatrix& m);

	inline FMVector3 ToFMVector(const MVector& v) { return FMVector3((float) v.x, (float) v.y, (float) v.z); }
	inline FMVector3 ToFMVector(const MFloatVector& v) { return FMVector3(v.x, v.y, v.z); }
	inline FMVector3 ToFMVector(const MPoint& p) { return FMVector3((float) p.x, (float) p.y, (float) p.z); }
	inline FMVector3 ToFMVector(const MFloatPoint& p) { return FMVector3(p.x, p.y, p.z); }
	inline FMVector3 ToFMVector(const MColor& c) { return FMVector3(c.r, c.g, c.b); }

	inline FMVector4 ToFMVector4(const MColor& c) { return FMVector4(c.r, c.g, c.b, c.a); }
	
	inline MColor ToMColor(const FMVector3& p) { return MColor(p.x, p.y, p.z); }
	inline MColor ToMColor(const FMVector4& p) { return MColor(p.x, p.y, p.z, p.w); }
	inline MVector ToMVector(const FMVector3& p) { return MVector(p.x, p.y, p.z); }
	inline MPoint ToMPoint(const FMVector3& p) { return MPoint(p.x, p.y, p.z); }
	
	inline const char* floatToChar(float& val) { return (MString("")+val).asChar(); }
	inline const char* float2ToChar(float& x, float& y) { return (MString("")+x+" "+y).asChar(); }
	inline const char* float3ToChar(float& x, float& y, float& z) { return (MString("")+x+" "+y+" "+z).asChar(); }
	inline const char* float4ToChar(float& x, float& y, float& z, float& a) { return (MString("")+x+" "+y+" "+z+" "+a).asChar(); }

	const fchar* ToFChar(const MString& m);
}

bool operator< (const MDagPath& a, const MDagPath& b);
bool operator< (const MObject& a, const MObject& b);
bool operator< (const MString& a, const MString& b);

MString operator+(const MString& m, const fm::string& s);

#endif // _MAYA_HELPER_H_
