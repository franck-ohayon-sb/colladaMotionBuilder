/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "MayaConvert.h"

bool operator< (const MString& a, const MString& b)
{
	return strcmp(a.asChar(), b.asChar()) < 0;
}

bool operator< (const MDagPath& a, const MDagPath& b)
{
	return a.fullPathName() < b.fullPathName();
}

bool operator< (const MObject& a, const MObject& b)
{
	return (*((const uint**)&a)) < (*((const uint**)&b));
}

namespace MConvert
{
	void ToFloatList(MFloatArray& a, FloatList& b)
	{
		uint count = a.length(); b.resize(count);
		for (uint i = 0; i < count; ++i) b[i] = a[i];
	}

	void ToFloatList(MPointArray& a, FloatList& b)
	{
		uint count = a.length(); b.resize(3*count);
		FloatList::iterator itF = b.begin();
		for (uint i = 0; i < count; ++i)
		{ *(itF++) = (float) a[i].x; *(itF++) = (float) a[i].y; *(itF++) = (float) a[i].z; }
	}

	void ToFloatList(MVectorArray& a, FloatList& b)
	{
		uint count = a.length(); b.resize(3*count);
		FloatList::iterator itF = b.begin();
		for (uint i = 0; i < count; ++i)
		{ *(itF++) = (float) a[i].x; *(itF++) = (float) a[i].y; *(itF++) = (float) a[i].z; }
	}

	void ToFloatList(MFloatVectorArray& a, FloatList& b)
	{
		uint count = a.length(); b.resize(3*count);
		FloatList::iterator itF = b.begin();
		for (uint i = 0; i < count; ++i)
		{ *(itF++) = a[i].x; *(itF++) = a[i].y; *(itF++) = a[i].z; }
	}

	void ToFloatList(MColorArray& a, FloatList& b)
	{
		uint count = a.length(); b.resize(4*count);
		FloatList::iterator itF = b.begin();
		for (uint i = 0; i < count; ++i)
		{ *(itF++) = a[i].r; *(itF++) = a[i].g; *(itF++) = a[i].b; *(itF++) = a[i].a; }
	}

	void ToMPointArray(const float* a, size_t _count, uint32 stride, MPointArray& b)
	{
		if (stride < 3) return;
		b.setLength((uint)_count);
		for (uint i = 0; i < _count; ++i) { b[i].x = a[i * stride + 0]; b[i].y = a[i * stride + 1]; b[i].z = a[i * stride + 2]; }
	}

	void AppendToMIntArray(const uint32* a, size_t count, MIntArray& b)
	{
		uint offset = (uint) b.length();
		b.setLength((uint) (offset + count));
		for (uint j = 0; j < count; ++j) b[offset + j] = (*a++);
	}

	MVector ToMVector(const float* a, uint32 stride, uint32 index)
	{
		index *= stride;
		return MVector(a[index], a[index + 1], a[index + 2]);
	}

	MColor ToMColor(const float* a, uint32 stride, uint32 index)
	{
		index *= stride;
		return MColor(a[index], a[index + 1], a[index + 2], (stride >= 4) ? a[index + 3] : 1.0f);
	}

	MMatrix ToMMatrix(const FMMatrix44& m)
	{
#define V(i, j) mx[i][j] = (double) m[i][j]
		MMatrix mx;
		V(0,0); V(0,1); V(0,2); V(0,3);
		V(1,0); V(1,1); V(1,2); V(1,3);
		V(2,0); V(2,1); V(2,2); V(2,3);
		V(3,0); V(3,1); V(3,2); V(3,3);
		return mx;
#undef V
	}

	FMMatrix44 ToFMatrix(const MMatrix& m)
	{
#define V(i, j) mx[i][j] = (float) m[i][j]
		FMMatrix44 mx;
		V(0,0); V(0,1); V(0,2); V(0,3);
		V(1,0); V(1,1); V(1,2); V(1,3);
		V(2,0); V(2,1); V(2,2); V(2,3);
		V(3,0); V(3,1); V(3,2); V(3,3);
		return mx;
#undef V
	}

	const fchar* ToFChar(const MString& m)
	{
#if MAYA_API_VERSION >= 850
		return (fchar*)m.asWChar();
#else // MAYA_API_VERSION >= 850
		return m.asChar();
#endif // MAYA_API_VERSION >= 850
	}
}

MString operator+(const MString& m, const fm::string& s)
{
	return m + MString(s.c_str());
}
