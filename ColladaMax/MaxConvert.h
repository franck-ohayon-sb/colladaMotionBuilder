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

// Conversion classes/functions between Max's API and the FS Import classes

inline Matrix3 ToMatrix3(const FMMatrix44& mx)
{
	Point3 u(mx[0][0], mx[0][1], mx[0][2]);
	Point3 v(mx[1][0], mx[1][1], mx[1][2]);
	Point3 n(mx[2][0], mx[2][1], mx[2][2]);
	Point3 t(mx[3][0], mx[3][1], mx[3][2]);
	return Matrix3(u, v, n, t);
}

inline FMMatrix44 ToFMMatrix44(const Matrix3& mx)
{
	FMMatrix44 fm;
	fm[0][0] = mx[0][0]; fm[1][0] = mx[1][0]; fm[2][0] = mx[2][0]; fm[3][0] = mx[3][0];
	fm[0][1] = mx[0][1]; fm[1][1] = mx[1][1]; fm[2][1] = mx[2][1]; fm[3][1] = mx[3][1];
	fm[0][2] = mx[0][2]; fm[1][2] = mx[1][2]; fm[2][2] = mx[2][2]; fm[3][2] = mx[3][2];
	fm[0][3] = fm[1][3] = fm[2][3] = 0.0f; fm[3][3] = 1.0f;
	return fm;
}

inline Point3 ToPoint3(const FMVector3& p) { return Point3(p.x, p.y, p.z); }
inline Point4 ToPoint4(const FMVector4& p) { return Point4(p.x, p.y, p.z, p.w); }
inline Color ToColor(const FMVector3& p) { return Color(p.x, p.y, p.z); }
inline Color ToColor(const FMVector4& p) { return Color(p.x, p.y, p.z); }
inline AColor ToAColor(const FMVector4& p) { return AColor(p.x, p.y, p.z, p.w); }

inline bool IsEquivalent(const FMVector3& p1, const Point3& p2) { return IsEquivalent(p1.x, p2.x) && IsEquivalent(p1.y, p2.y) && IsEquivalent(p1.z, p2.z); }

inline FMVector3 ToFMVector3(const Point3& p) { return FMVector3(p.x, p.y, p.z); }
inline FMVector3 ToFMVector3(const Point4& p) { return FMVector3(p.x, p.y, p.z); }
inline FMVector3 ToFMVector3(const Color& c) { return FMVector3(c.r, c.g, c.b); }
inline FMVector4 ToFMVector4(const Point4& p) { return FMVector4(p.x, p.y, p.z, p.w); }
inline FMVector4 ToFMVector4(const Color& c) { return FMVector4(c.r, c.g, c.b, 1.0f); }
inline FMVector4 ToFMVector4(const AColor& c) { return FMVector4(c.r, c.g, c.b, c.a); }

// added for Point3 properties being converted to FMVector4 in ColladaFX
inline FMVector4 ToFMVector4(const Point3& p) { return FMVector4(p.x, p.y, p.z, 1.0f); }
