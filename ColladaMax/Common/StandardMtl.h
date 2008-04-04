/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _STANDARD_MTL_H_
#define _STANDARD_MTL_H_

// This information is taken from the 'StdMat2' sample.
namespace FSStandardMaterial
{
	// reference numbers
	const uint32 OLD_PBLOCK_REF = 0;
	const uint32 TEXMAPS_REF = 1;
	const uint32 SHADER_REF = 2;
	const uint32 SHADER_PB_REF = 3;
	const uint32 EXTENDED_PB_REF = 4;
	const uint32 SAMPLING_PB_REF = 5;
	const uint32 MAPS_PB_REF = 6;
	const uint32 DYNAMICS_PB_REF = 7;		// [glaforte] typo corrected, so you won't find this #define in the MaxSDK!
	const uint32 SAMPLER_REF = 8;

	// shdr_params param IDs
	enum 
	{ 
		shdr_ambient, shdr_diffuse, shdr_specular,
		shdr_ad_texlock, shdr_ad_lock, shdr_ds_lock, 
		shdr_use_self_illum_color, shdr_self_illum_amnt, shdr_self_illum_color, 
		shdr_spec_lvl, shdr_glossiness, shdr_soften
	};

	// std2_shader param IDs
	enum 
	{ 
		std2_shader_type, std2_wire, std2_two_sided, std2_face_map, std2_faceted,
		std2_shader_by_name,  // virtual param for accessing shader type by name
	};

	// std2_extended param IDs
	enum 
	{ 
		std2_opacity_type, std2_opacity, std2_filter_color, std2_ep_filter_map,
		std2_falloff_type, std2_falloff_amnt, 
		std2_ior,
		std2_wire_size, std2_wire_units,
		std2_apply_refl_dimming, std2_dim_lvl, std2_refl_lvl,
		std2_translucent_blur, std2_ep_translucent_blur_map,
		std2_translucent_color, std2_ep_translucent_color_map,
	};

	// std2_sampling param IDs
	enum 
	{ 
		std2_ssampler, std2_ssampler_qual, std2_ssampler_enable, 
		std2_ssampler_adapt_on, std2_ssampler_adapt_threshold, std2_ssampler_advanced,
		std2_ssampler_subsample_tex_on, std2_ssampler_by_name, 
		std2_ssampler_param0, std2_ssampler_param1, std2_ssampler_useglobal
	};

	// std_maps param IDs
	enum 
	{
		std2_map_enables, std2_maps, std2_map_amnts, std2_mp_ad_texlock, 
	};

	// std2_dynamics param IDs
	enum 
	{
		std2_bounce, std2_static_friction, std2_sliding_friction,
	};

	enum ShaderClassID
	{
		STD2_PHONG_CLASS_ID					= 0x00000037,
		STD2_BLINN_SHADER_CLASS_ID			= 0x00000038,
		STD2_METAL_SHADER_CLASS_ID			= 0x00000039,
		STD2_ANISOTROPIC_SHADER_CLASS_ID	= 0x2857f460,
		STD2_MULTI_LAYER_SHADER_CLASS_ID	= 0x2857f470,
		STD2_OREN_NAYER_BLINN_CLASS_ID		= 0x2857f421
	};
}

namespace FSStdUVGen
{
	enum { pblock }; 

	// paramblock subanims
	enum 
	{
		u_offset,
		v_offset,
		u_tiling,
		v_tiling,
		u_angle,
		v_angle,
		w_angle,
		blur,
		noise_amt,
		noise_size,
		noise_level,
		phase,
		blur_offset,
		num_subs
	};
}

#endif // _STANDARD_MTL_H_