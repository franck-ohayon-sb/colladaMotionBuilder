/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __SKEW_HELPER_INCLUDED__
#define __SKEW_HELPER_INCLUDED__

/*
	Important assumptions on skew and shears:

	1) COLLADA uses the RenderMan standard:
	[ 1+s*dx*ex   s*dx*ey   s*dx*ez  0 ]
	[   s*dy*ex 1+s*dy*ey   s*dy*ez  0 ]
	[   s*dz*ex   s*dz*ey 1+s*dz*ez  0 ]
	[         0         0         0  1 ]
	where s = tan(skewAngle), if the axises are normalized

	2) COLLADA and Maya use different matrix row/column ordering.

	3) Maya uses the following shear transform:
	[  1  0  0  0 ]
	[ xy  1  0  0 ]
	[ xz yx  1  0 ]
	[  0  0  0  1 ]
*/

class FCDSceneNode;

class CSkewHelper
{
public:
	// Export the shear transform. "shear" input is a double[3].
	static void ExportSkew(FCDSceneNode* parent, double* shear);
};

#endif
