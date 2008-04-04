/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _MAX_PARTICLE_DEFINTIONS_H_
#define _MAX_PARTICLE_DEFINTIONS_H_

#ifndef __JAGTYPES__
#include "maxtypes.h"
#endif //__JAGTYPES__

/*
 * These velocity scales are taken from extensive samples of Max's particle systems.
 */
#define COMPLEX_PARTICLE_VEL_SCALE	GetFrameRate() //(0.01f*1200.0f)
#define SIMPLE_PARTICLE_VEL_SCALE	(0.01f*1200.0f)

namespace FSParticleSimple
{
	// Guessed parameters
	enum params {
		render_count,
		viewport_count,
		drop_size,
		drop_speed,
		variation,
		display,
		start_time,
		life_time,
		width,
		length,
		hide_emitter,	
		birth_rate,	
		constant,	
		render_type,
		num_params
	};
}

/*
 * The complex emitters share a great many parameters.
 * however, the parameters are broken up and scattered 
 * through out the parameter block.  The following listings
 * are the largest chunks of contiguous common parameter definitions
 */
namespace FSParticleComplex
{
	// all complex particles share some references
	enum refs { pblock_ref, ref_flake_inst, num_refs };

	enum life_params 
	{
		// Basic birth/life params
		birth_method,
		birth_rate,
		total_number,
		emit_start,
		emit_stop,
		display_until,
		life,
		life_var,
		num_life_params
	};

	enum basic_particle_params
	{
		// Basic particle params
		size = 0,
		size_var,
		grow_time,
		fade_time,
		rnd_seed,
		num_basic_params
	};

	enum class_params
	{
		// Particle class params
		particle_class = 0,
		particle_type,
		meta_tension,
		metatension_var,
		meta_course,
		auto_coarseness,
		num_class_params,
	};
	
	enum mat_params
	{
		// material params
		use_subtree = 0,
		animation_offset,
		animation_offset_time,
		viewport_shows,
		display_portion,
		mapping_type,
		mapping_time,
		mapping_dist,
		num_mat_params
	};

	enum rot_params
	{
		// Rotation params
		spin_time = 0,
		spin_time_var,
		spin_phase,
		spin_phase_var,
		spin_axis_choose,
		spin_axis_x,
		spin_axis_y,
		spin_axis_z,
		spin_axis_var,
		emit_influence,
		emit_multiplier,
		emit_mult_var,
		num_spin_params
	};

	enum spawn_params
	{
		// Spawn type params
		spawn_type = 0,
		number_of_gens,
		number_of_spawns,
		direction_chaos,
		speed_chaos,
		speed_chaos_sign,
		inherit_old_particle_velocity,
		scale_chaos,
		scale_chaos_sign,
		lifespan_entry_field,
		constant_spawn_speed,
		constant_spawn_scale,
		num_spawn_params
	};

#define NUM_PARAMS  FSParticleComplex::num_life_params + \
					FSParticleComplex::num_basic_params + \
					FSParticleComplex::num_class_params + \
					FSParticleComplex::num_mat_params + \
					FSParticleComplex::num_spin_params + \
					FSParticleComplex::num_spawn_params
}

/*
 * Defines related to the Spray simple particle system
 */
namespace FSParticleSpray
{
	const Class_ID classID(2614500000, 0);

	// No extra parameters
	enum params {
		num_params = FSParticleSimple::num_params
	};
}

/*
 * defines for the Snow simple particle system
 */
namespace FSParticleSnow
{
	const Class_ID classID((unsigned long)-1680467295, 0);

	// Guessed parameters
	enum params {
		tumble = FSParticleSimple::num_params,
		tumble_rate,
		num_params
	};
}

/*
 * defines for the Blizzard Particle System
 */
namespace FSParticleBlizzard
{
	const Class_ID classID(0x5835054d, 0x564b40ed);
						  
	enum param_ids
	{
		tumble,
		tumble_rate,
		emit_map,
		emitter_length,
		speed,
		speed_var,
		life_offset,
		//insert common life params
		sub_frame_move	= life_offset + FSParticleComplex::num_life_params,
		sub_frame_time,
		basic_offset,
		//insert common basic params
		emitter_width	= basic_offset + FSParticleComplex::num_basic_params,
		emitter_hidden,
		class_offset,
		//insert common class params
		mtl_offset		= class_offset + FSParticleComplex::num_class_params,
		//insert common mtl params
		spin_offset		= mtl_offset +  FSParticleComplex::num_mat_params,
		//insert common spin params
		spawn_offset	= spin_offset + FSParticleComplex::num_spin_params,
		//insert common spawn params
		custom_mtl			= spawn_offset + FSParticleComplex::num_spawn_params,
		meta_course_viewport,
		subframe_rot,
		spawn_percent,
		spawn_mult_var,
		draft,
		dies_after_X,
		dies_after_X_var,
		// Interparticle collisions
		ipc_enable,
		ipc_steps,
		ipc_bounce,
		ipc_bounce_var,
		num_params
	};
}
/*
 * Defines for the super spray particle system
 */
namespace FSParticleSuperSpray
{
	const Class_ID classID(0x74f811e3, 0x21fb7b57);

	enum param_ids
	{
		off_axis,
		axis_spread,
		off_plane,
		plane_spread,
		speed,
		speed_var,
		life_offset,
		//insert common life params
		sub_frame_move	= life_offset + FSParticleComplex::num_life_params,
		sub_frame_time,
		basic_offset,
		//insert common basic params
		emitter_icon_size	= basic_offset + FSParticleComplex::num_basic_params,
		emitter_hidden,
		class_offset,
		//insert common class params
		mtl_offset		= class_offset + FSParticleComplex::num_class_params,
		//insert common mtl params
		spin_offset		= mtl_offset +  FSParticleComplex::num_mat_params,
		//insert common spin params
		spawn_offset	= spin_offset + FSParticleComplex::num_spin_params,
		//insert common spawn params
		bubble_amp		= spawn_offset + FSParticleComplex::num_spawn_params,
		bubble_amp_var,
		bubble_period,
		bubble_period_var,
		bubble_phase,
		bubble_phase_var,

		stretch,
		custom_mtl,
		viewport_courseness,
		subframe_rot,
		spawn_percent,
		spawn_mult_var,
		surface_tracking,
		dies_after_X,
		dies_after_X_var,

		ipc_enable,
		ipc_steps,
		ipc_bounce,
		ipc_bounce_var,
	};
}

/*
 * defines for the PArray particle system
 */
namespace FSParticlePArray
{
	const Class_ID classID(238822837, 278730329);

	enum { emit_node_ref = FSParticleComplex::num_refs, num_refs };

	enum {
		distribution,
		emitter_count,
		speed,
		speed_var,
		angle_divergence,
		life_offset,
		// insert basic life params
		sub_frame_move	= FSParticleComplex::num_life_params + life_offset,
		sub_frame_time,
		basic_offset,
		// insert basic params
		emitter_size	= FSParticleComplex::num_basic_params + basic_offset,
		emitter_hidden,
		class_offset,
		// insert class params
		frag_thickness	= FSParticleComplex::num_class_params + class_offset,
		frag_method,
		frag_count,
		smooth_angle,
		mtl_offset,
		// insert common material parameters
		custom_mtl	= FSParticleComplex::num_mat_params + mtl_offset,
		side_material,
		back_material,
		front_material,
		spin_offset,
		// insert rotational parameters
		bubble_amp		= FSParticleComplex::num_spin_params + spin_offset,
		bubble_amp_var,
		bubble_period,
		bubble_period_var,
		bubble_phase,
		bubble_phase_var,
		stretch,
		spawn_offset,
		// insert spawn type parameters
		meta_course_viewport = FSParticleComplex::num_spawn_params + spawn_offset,
		subframe_rotation_checkbox,
		spawn_percent,
		spawn_mult_var,
		not_draft,
		use_selected,
		die_after_X,
		die_after_X_var,
		ipc_enable,
		ipc_steps,
		ipc_bounce,
		ipc_bounce_var,
		num_params
	};
}

/*
 * defines for the PCloud particle system
 */
namespace FSParticlePCloud 
{
	const Class_ID classID(470760751, 808520441);

	enum { emit_node_ref = FSParticleComplex::num_refs, num_refs };

	enum {
		distribution_method,
		speed,
		speed_var,
		emit_dir,
		emit_x,
		emit_y,
		emit_z,
		variation,
		life_offset,
		// Insert basic birth params
		custom_mtl		= FSParticleComplex::num_life_params + life_offset,
		basic_offset,
		// insert basic params
		emitter_width	= FSParticleComplex::num_basic_params + basic_offset,
		emitter_height,
		emitter_depth,
		emitter_hidden,
		class_offset,
		//insert common class params
		mtl_offset		= class_offset + FSParticleComplex::num_class_params,
		//insert common mtl params
		spin_offset		= mtl_offset +  FSParticleComplex::num_mat_params,
		//insert common spin params
		bubble_amp		= spin_offset + FSParticleComplex::num_spin_params,
		bubble_amp_var,
		bubble_period,
		bubble_period_var,
		bubble_phase,
		bubble_phase_var,
		stretch,
		spawn_offset,
		//insert common spawn params
		meta_courseness_viewport = FSParticleComplex::num_spawn_params + spawn_offset,
		spawn_percent,
		spawn_mult_var,
		not_draft,
		die_after,
		die_after_var,
		ipc_enable,
		ipc_steps,
		ipc_bounce,
		ipc_bounce_var,
		num_params
	};
};

#endif