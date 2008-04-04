/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __SPLINE_EXPORTER_INCLUDED__
#define __SPLINE_EXPORTER_INCLUDED__

#include <maya/MFnNurbsCurve.h>

class FCDGeometrySpline;

class CSplineExporter
{
public:
	CSplineExporter(MFnNurbsCurve& icurveFn, FCDGeometrySpline* colladaSpline, DaeDoc* idoc);
	~CSplineExporter(void);
	
	// export curve info
	void exportCurve();
	
private:
	MFnNurbsCurve& curveFn;
	FCDGeometrySpline* colladaSpline;
	DaeDoc* doc;
};

#endif
