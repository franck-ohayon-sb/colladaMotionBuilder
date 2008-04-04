/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COLLADA_MAX_TYPES_H_
#define _COLLADA_MAX_TYPES_H_

// eventually, this file should be the bottleneck for every SuperClassID and ClassID calls.

namespace ColladaMaxTypes
{
	enum MaxType
	{
		UNKNOWN,
		BONE,
		CAMERA,
		HELPER,
		LIGHT,
		MATERIAL,
		MESH,
		SPLINE,
		NURBS_CURVE,
		NURBS_SURFACE,
		PARTICLE_SYSTEM,
		XREF_OBJECT,
		XREF_MATERIAL,
		FORCE_FIELD
	};

	MaxType Get(INode* node, Animatable* p, bool detectXRef = false);

	// This should be defined somewhere in the max api?
#define ROTATION_LIST_CLASS_ID		0x4B4B1003
};

namespace ColladaMaxAnimatable
{
	enum Type
	{
		FLOAT,
		FLOAT3,
		FLOAT4,

		ROTATION_X,
		ROTATION_Y,
		ROTATION_Z,

		SCALE_ROT_AXIS, // The pivot rotation for scale/skew
		SCALE_ROT_AXIS_R, // The reverse pivot rotation for scale/skew

		// Collapsed types
		SCALE = FLOAT3,
		TRANSLATION = FLOAT3
	};
};

// This is the name of the paramblock created to hold the root-level dynamic attributes
#define COLLADA_EXTRA_PARAMBLOCK_NAME		FC("Extra Attributes")

#define IPARAMBLK2_CLASS_ID		Class_ID(130, 0)

#define MAX_NODE_CLASS_ID		Class_ID(0x002, 0)

#endif // _COLLADA_MAX_TYPES_H_
