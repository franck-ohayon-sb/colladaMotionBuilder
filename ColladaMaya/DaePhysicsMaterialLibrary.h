/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_PHYSICS_MATERIAL_LIBRARY_INCLUDED__
#define __DAE_PHYSICS_MATERIAL_LIBRARY_INCLUDED__

class DaePhysicsMaterialLibrary : public DaeBaseLibrary
{
public:
	DaePhysicsMaterialLibrary(DaeDoc* pDoc):
	  DaeBaseLibrary(pDoc) {}

	virtual const char* libraryType() { return DAE_LIBRARY_PMATERIAL_ELEMENT; }
	virtual const char* childType() { return DAE_PHYSICS_MATERIAL_ELEMENT; }
	virtual const char* librarySuffix() { return "-PhysicsMaterial"; }
};

#endif // __DAE_PHYSICS_MATERIAL_LIBRARY_INCLUDED__
