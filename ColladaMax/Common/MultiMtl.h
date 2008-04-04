/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _MULTI_MTL_H_
#define _MULTI_MTL_H_

// The information from this file comes from the MaxSDK sample: /material/multi.cpp.
namespace FSMultiMaterial
{
	const Class_ID classID(MULTI_CLASS_ID,0);
	// Reference #s.
	const uint32 PBLOCK_REF = 0;
	const uint32 MTL_REF = 1;

	// multi_params param IDs
	enum
	{ 
		multi_mtls,
		multi_ons,
		multi_names,
		multi_ids,
	};
};

// From the SDK sample /material/composit.cpp
namespace FSCompositeMap
{
	const Class_ID classID(COMPOSITE_CLASS_ID,0);

	// Parameter and ParamBlock IDs
	enum { comptex_params, };  // pblock ID
	// param IDs
	enum 
	{ 
		comptex_tex, comptex_ons

	};
}

namespace FSMixMap
{
	// Mix controller param IDs
	enum 
	{ 
		mix_mix, mix_curvea, mix_curveb, mix_usecurve,
		mix_color1, mix_color2,
		mix_map1, mix_map2, mix_mask,		
		mix_map1_on, mix_map2_on,  mix_mask_on, // main grad params 
		mix_output
	};
}

namespace FSCompositeMtl
{
	const Class_ID classID(0x61dc0cd7, 0x13640af6);

	enum { compmat_params, };  // pblock ID

	enum 
	{ 
		compmat_mtls,
		compmat_type, 
		compmat_map_on, 
		compmat_amount
	};
}

#endif // _MULTI_MTL_H_