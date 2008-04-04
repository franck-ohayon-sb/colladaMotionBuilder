/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COLLADAFX_SAFE_CG_WRAPPER_H_
#define _COLLADAFX_SAFE_CG_WRAPPER_H_

/** Some Cg methods crash depending on the Cg version.
	This wrapper namespace prevent the application from crashing by
	surrounding the faulty Cg code with Win32-specific SEH exception
	handling.*/
namespace scg
{
	CGeffect CreateEffectFromFile(CGcontext ctx, const char* fileName, const char** args);
	CGprogram CreateProgramFromFile(CGcontext ctx, CGenum program_type, const char *program_file, CGprofile profile, const char *entry, const char **args);
}

#endif // _COLLADAFX_SAFE_CG_WRAPPER_H_
