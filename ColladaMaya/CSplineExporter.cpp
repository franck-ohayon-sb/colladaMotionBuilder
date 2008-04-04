/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include <maya/MPointArray.h>
#include "FCDocument/FCDGeometrySpline.h"
#include "CSplineExporter.h"

CSplineExporter::CSplineExporter(MFnNurbsCurve& icurveFn, FCDGeometrySpline* _colladaSpline, DaeDoc* idoc)
:	curveFn(icurveFn), colladaSpline(_colladaSpline), doc(idoc)
{
}

CSplineExporter::~CSplineExporter(void)
{
	colladaSpline = NULL;
	doc = NULL;
}

void CSplineExporter::exportCurve()
{
	if (!colladaSpline->SetType(FUDaeSplineType::NURBS))
	{
		return;
	}

	FCDNURBSSpline* nurbSpline = (FCDNURBSSpline*) colladaSpline->AddSpline();

	MPointArray vertexArray;
	curveFn.getCVs(vertexArray, MSpace::kObject);
	uint cvCount = vertexArray.length();
	for (uint i = 0; i < cvCount; ++i)
	{
		nurbSpline->AddCV(MConvert::ToFMVector(vertexArray[i]), (float) vertexArray[i].w);
	}

	// set the degree
	int deg = curveFn.degree();
	nurbSpline->SetDegree((uint32)deg);

	MDoubleArray knotArray;
	curveFn.getKnots(knotArray);
	uint knotCount = knotArray.length();

	// TODO. Find out why Maya has a knot vector of length [spans + 2 * degree - 1]
	// this causes a knot length mismatch between Max and Maya, Max having a knot
	// vector length of [CVsCount + degree + 1]. The difference is 2 knots less in Maya.

	// interpolate for the first knot
	nurbSpline->AddKnot((float) (2.0 * knotArray[0] - knotArray[1]));

	for (uint i = 0; i < knotCount; i++)
	{
		nurbSpline->AddKnot((float) knotArray[i]);
	}

	// interpolate the last knot
	nurbSpline->AddKnot((float) (2.0 * knotArray[knotCount-1] - knotArray[knotCount-2]));
	nurbSpline->SetClosed(curveFn.form() != MFnNurbsCurve::kOpen);
}
