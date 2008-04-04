/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"

#include "CSetHelper.h"

#include <maya/MStringArray.h>
#include <maya/MDagPath.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MFnSet.h>

std::vector<MObjectHandle> CSetHelper::setObjects;
CSetHelper::SetModes CSetHelper::setMode;

bool CSetHelper::setSets(const MStringArray& sets, SetModes desiredMode)
{
    setObjects.clear();

    MItDependencyNodes setIter(MFn::kSet);
    while(!setIter.isDone())
    {
        MFnSet currentSet(setIter.item());        
        for (unsigned int i = 0; i < sets.length(); ++i)
        {
            if (strstr(currentSet.name().asChar(), sets[i].asChar()) != NULL)
            {
                setObjects.push_back(setIter.item());
                break;
            }
        }
        setIter.next();
    }

    setMode = desiredMode;
    return true;
}

// Unlike Maya's default behavior, we want to consider set membership to be inheritable
bool CSetHelper::isMemberOfSet(const MDagPath& dagPath, MFnSet& Set)
{
    if (Set.isMember(dagPath))
    {
        return true;        
    }
    else
    {
        MFnDagNode dagNode(dagPath);
        MSelectionList setMembers;
        Set.getMembers(setMembers, true);
        for (unsigned int i = 0; i < setMembers.length(); ++i)
        {
            MObject memberObject;
            if (setMembers.getDependNode(i, memberObject))
            {                               
                if (dagNode.isChildOf(memberObject))
                {
                    return true;
                }
            }
        }
    }
    
    return false;
}

bool CSetHelper::isExcluded(const MDagPath& dagPath)
{    
    MFnDagNode dagNode(dagPath);
    if (dagNode.name() == "world")
        return false;
    
    bool bContainedInSet = false;
    for (unsigned int i = 0; i < CSetHelper::setObjects.size(); ++i)
    {
#		if MAYA_API_VERSION < 600
		MObject o = CSetHelper::setObjects[i];
#		else
		MObject o = CSetHelper::setObjects[i].object();
#		endif
        MFnSet currentSet(o);
        if (isMemberOfSet(dagPath, currentSet))
        {
            bContainedInSet = true;
            break;
        }
    }

    if (setMode == kExcluding)
    {
        return bContainedInSet;
    }
    else if (setMode == kIncludeOnly)
    {
        return !bContainedInSet;
    }
    else
    {
        return false;
    }
}

