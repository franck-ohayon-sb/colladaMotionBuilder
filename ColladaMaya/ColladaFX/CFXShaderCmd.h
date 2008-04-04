/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _MPxCommand
#include <maya/MPxCommand.h>
#endif // _MPxCommand

class CFXShaderCommand : public MPxCommand 
{ 
public:
	CFXShaderCommand();
	virtual	~CFXShaderCommand();

	MStatus doIt(const MArgList& args); 
	MStatus parseArgs(const MArgList& args);

	static void* creator(); 
	static MSyntax newSyntax();
}; 
