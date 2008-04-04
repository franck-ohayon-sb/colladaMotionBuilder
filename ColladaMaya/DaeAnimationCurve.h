/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/*
* Contains one animation curve for export and allows for the interleaving of animation curves.
*/

#ifndef _DAE_ANIMATION_CURVE_H_
#define _DAE_ANIMATION_CURVE_H_

#include "DaeAnimationTypes.h"

class FCDAnimationCurve;
class FCDAnimationChannel;

class DaeAnimationCurve
{
public:
	// Create a single-dimensional curve from the given Maya 'animCurveXX' node
	static FCDAnimationCurve* CreateFromMayaNode(DaeDoc* doc, const MObject& curveNode, FCDAnimationChannel* channel);

	// Import Parasitic
	// Create a Maya 'animCurveXX' node for the given dimension of a COLLADA animation curve
	static MObject CreateMayaNode(DaeDoc* doc, FCDAnimationCurve* curve, SampleType typeFlag);
};

#endif // _DAE_ANIMATION_CURVE_H_
