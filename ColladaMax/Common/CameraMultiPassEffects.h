/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef CAMERA_MULTI_PASS_EFFECTS_H
#define CAMERA_MULTI_PASS_EFFECTS_H

/**
*	Information about the Max GenCamera's MultiPassEffect found in the SDK samples.
*/

#define FMULTI_PASS_MOTION_BLUR_CLASS_ID Class_ID(0xd481518, 0x687d7c99)

namespace MultiPassEffectMB
{
	// parameter blocks IDs
	enum
	{
		multiPassMotionBlur_params,
	};

	// parameters for multiPassMotionBlur_params
	enum
	{
		prm_displayPasses,
		prm_totalPasses,
		prm_duration,
		prm_bias,
		prm_normalizeWeights,
		prm_ditherStrength,
		prm_tileSize,
		prm_disableFiltering,
		prm_disableAntialiasing,
	};
}

// Again from the samples...
#define FMULTI_PASS_DOF_CLASS_ID Class_ID(0xd481815, 0x687d799c)

namespace MultiPassEffectDOF
{
	// parameter blocks IDs
	enum
	{
		multiPassDOF_params,
	};

	// parameters for multiPassDOF_params
	enum
	{
		prm_useTargetDistance,
		prm_focalDepth,
		prm_displayPasses,
		prm_totalPasses,
		prm_sampleRadius,
		prm_sampleBias,
		prm_normalizeWeights,
		prm_ditherStrength,
		prm_tileSize,
		prm_disableFiltering,
		prm_disableAntialiasing,
		prm_useOriginalLocation,
	};
}

#endif // CAMERA_MULTI_PASS_EFFECTS_H
