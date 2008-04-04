/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __COLLADA_SET_HELPER___
#define __COLLADA_SET_HELPER___

#if MAYA_API_VERSION < 600
typedef MObject MObjectHandle;
#else
#	include <maya/MObjectHandle.h>
#endif

class MStringArray;
class MDagPath;
class MFnSet;

class CSetHelper
{
public:
    enum SetModes
    {
        kExcluding,
        kIncludeOnly
    };    

    static bool setSets(const MStringArray& sets, SetModes desiredMode);
    static bool isExcluded(const MDagPath& dagPath);

private:
    static bool isMemberOfSet(const MDagPath& dagPath, MFnSet& Set);

    static std::vector<MObjectHandle> setObjects;
    static SetModes setMode;
};

#endif

