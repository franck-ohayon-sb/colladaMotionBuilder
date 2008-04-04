/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _DAE_ANIMATION_TYPES_H_
#define _DAE_ANIMATION_TYPES_H_

class FCDAnimated;
class FCDConversionFunctor;

// Use this enum when export/importing an animation to tell the expected type
enum SampleType
{
	// Just one float
	kSingle = 0x01,
	// Vector value: X, Y, Z
	kVector = 0x02,
	// Color value: R, G, B
	kColour = 0x03,
	// Matrix
	kMatrix = 0x04,
	// Boolean
	kBoolean = 0x05,

	// Angle type, there will be rad <-> degree conversions
	kAngle = 0x10,
	kQualifiedAngle = 0x30,
	// Length factor will be applied
	kLength = 0x40,

	kFlagMask = 0xF0,
	kValueMask = 0x07
};

enum InterpolationType
{
	// Maya's "STEP" tangent type
	kNoInterpolation,
	// Maya's "LINEAR" tangent type
	kLinearInterpolation,
	// All the rest. On import, gets converted to "FIXED" tangent type.
	kBezierInterpolation
};
typedef fm::vector<InterpolationType> InterpolationList;

enum CAnimationResult {
	kISANIM_None = 0,
	kISANIM_Curve,
	kISANIM_Sample,
	kISANIM_Character,
};

// Holds the information for one animated entity.
class DaeAnimatedElement
{
public:
	MPlug plug;
	SampleType sampleType;

	// Import-only
	FCDAnimated* animatedValue;
	FCDConversionFunctor* conversion;
	bool isExportSampling;

public:
	DaeAnimatedElement();
	~DaeAnimatedElement();
};

#endif // _DAE_ANIMATION_TYPES_H_

