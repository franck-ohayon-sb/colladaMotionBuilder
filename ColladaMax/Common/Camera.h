/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COMMON_CAMERA_H_
#define _COMMON_CAMERA_H_

// Information found in the 3dsMax SDK from the 'camera' object sample.
namespace MaxCamera
{
	// reference #s
	const uint32 PBLOCK_REF = 0;   // This is a IParamBlock, not a IParamBlock2.
	const uint32 DOF_REF = 1;
	const uint32 MP_EFFECT_REF = 2;

	// Parameter block indices
	const uint32 PB_FOV = 0;
	const uint32 PB_TDIST = 1;
	const uint32 PB_HITHER = 2; // Near-clip.
	const uint32 PB_YON = 3; // Far-clip
	const uint32 PB_NRANGE = 4;
	const uint32 PB_FRANGE = 5;
	const uint32 PB_MP_EFFECT_ENABLE = 6;
	const uint32 PB_MP_EFF_REND_EFF_PER_PASS = 7;
};

#endif // _COMMON_CAMERA_H_