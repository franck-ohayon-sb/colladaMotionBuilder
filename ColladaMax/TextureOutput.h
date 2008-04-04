/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/
/*
Based on the 3dsMax COLLADA Tools:
Copyright (C) 2005-2006 Autodesk Media Entertainment
MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _TEXTURE_OUTPUT_H_
#define _TEXTURE_OUTPUT_H_

// All values in here were devined by trial and error
namespace FSTextureOutput
{
	// Paramblock Ref Id
	enum RefIds
	{
		param_block_ref_id,
		unknown1,
		unknown2
	};

	enum ParamBlockIds
	{
		rgb_level,
		rgb_offset,
		output_amount,
		bump_amount,
	};
}

#endif // _TEXTURE_OUTPUT_H_