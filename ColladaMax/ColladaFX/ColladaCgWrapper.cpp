
#include "StdAfx.h"
#include "ColladaCgWrapper.h"

namespace scg
{
	CGeffect CreateEffectFromFile(CGcontext ctx, const char* fileName, const char** args)
	{
		__try
		{
			return cgCreateEffectFromFile(ctx,fileName,args);
		}
		__except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
		{
			return NULL;
		}
	}

	CGprogram CreateProgramFromFile(CGcontext ctx, CGenum program_type, const char *program_file, CGprofile profile, const char *entry, const char **args)
	{
		__try
		{
			return cgCreateProgramFromFile(ctx,program_type,program_file,profile,entry,args);
		}
		__except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
		{
			return NULL;
		}
	}
}
