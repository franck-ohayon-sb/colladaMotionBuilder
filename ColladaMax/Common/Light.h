/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COMMON_LIGHT_H_
#define _COMMON_LIGHT_H_

// extra class ID from the samples (systems/sunlight/natLight.cpp)
#define SKY_LIGHT_CLASS_ID			Class_ID(0x7bf61478, 0x522e4705)
#define SKY_LIGHT_CLASS_ID_PART_A	0x7bf61478

// Information found in the 3dsMax SDK from the 'light' object sample.
namespace MaxLight
{
	// reference #s
	const uint32 PBLOCK_REF = 0;		// This is a IParamBlock
	const uint32 PBLOCK_REF_SKY = 0;	// This is a IParamBlock2
	const uint32 PROJMAP_REF = 1;
	const uint32 SHADPROJMAP_REF = 2;
	const uint32 SHADTYPE_REF = 3;

	// Parameter block indices common to all lights (except SkyLight)
	const uint32 PB_COLOR = 0;
	const uint32 PB_INTENSITY = 1;
	const uint32 PB_CONTRAST = 2;
	const uint32 PB_DIFFSOFT = 3;

	// Parameter block indices for non-OMNI lights.
	const uint32 PB_HOTSIZE = 4;
	const uint32 PB_FALLSIZE = 5;
	const uint32 PB_ASPECT = 6;
	const uint32 PB_ATTENSTART1 = 7;
	const uint32 PB_ATTENEND1 = 8;
	const uint32 PB_ATTENSTART = 9;
	const uint32 PB_ATTENEND = 10;
	const uint32 PB_DECAY = 11;
	const uint32 PB_SHADCOLOR = 12;
	const uint32 PB_ATMOS_SHAD = 13;
	const uint32 PB_ATMOS_OPACITY = 14;
	const uint32 PB_ATMOS_COLAMT = 15;
	const uint32 PB_SHADMULT = 16;
	const uint32 PB_SHAD_COLMAP = 17;
	const uint32 PB_TDIST = 18;

	// Parameter block indices for OMNI only
	const uint32 PB_OMNIATSTART1 = 4;
	const uint32 PB_OMNIATEND1 = 5;
	const uint32 PB_OMNIATSTART = 6;
	const uint32 PB_OMNIATEND = 7;
	const uint32 PB_OMNIDECAY = 8;
	const uint32 PB_OMNISHADCOLOR = 9;
	const uint32 PB_OMNIATMOS_SHAD = 10;
	const uint32 PB_OMNIATMOS_OPACITY = 11;
	const uint32 PB_OMNIATMOS_COLAMT = 12;
	const uint32 PB_OMNISHADMULT = 13;
	const uint32 PB_OMNISHAD_COLMAP = 14;

	// Parameter block indices for SkyLights (by looking at the ParamBlockDesc)
	const uint32 PB_SKY_COLOR = 0;
	const uint32 PB_SKY_SKY_COLOR_MAP_AMOUNT = 1;
	const uint32 PB_SKY_COLOR_MAP = 2; // TYPE_TEXMAP
	const uint32 PB_SKY_SKY_COLOR_MAP_ON = 3;
	const uint32 PB_SKY_RAYS_PER_SAMPLE = 4;
	// baad food
	const uint32 PB_SKY_MODE = 7; // ??
	// baad food
	const uint32 PB_SKY_INTENSITY = 0xa;
	const uint32 PB_SKY_RAY_BIAS = 0xb;
	const uint32 PB_SKY_CAST_SHADOWS = 0xc;
	const uint32 PB_SKY_MULTIPLIER_ON = 0xd;
};

#endif // _COMMON_CAMERA_H_