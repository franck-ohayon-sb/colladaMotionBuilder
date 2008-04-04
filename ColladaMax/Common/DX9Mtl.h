/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _DX9_MATERIAL_H_
#define _DX9_MATERIAL_H_

// This information was found partially in the SDK headers and by hand.
namespace DX9Material
{
	// Reference #s.
	const uint32 FXPARAMS_PBLOCK_REF = 0;
	const uint32 FXFILE_PBLOCK_REF = 1;
	const uint32 CONTROL_PBLOCK_REF = 2;
	const uint32 FXTECHNIQUE_PBLOCK_REF = 3;

	// FXPARAMS_PBLOCK_REF
	// This one is filled in automatically by 3dsMax, according to the HLSL parameters

	// FXFILENAME_PBLOCK_REF
	enum
	{
		fxfile_filename // TYPE_FILENAME
	};

	// CONTROL_PBLOCK_REF

	// FXTECHNIQUE_PBLOCK_REF
	enum
	{
		fxtechnique_id // TYPE_INT
	};

	// Also: sub-material 0 is always considered to be the software fall-back.
};

#endif // _DX9_MATERIAL_H_