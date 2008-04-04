/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_PHYSICS_SCENE_LIBRARY_INCLUDED__
#define __DAE_PHYSICS_SCENE_LIBRARY_INCLUDED__

class FCDPhysicsScene;

class DaePhysicsSceneLibrary : public DaeBaseLibrary
{
public:
	DaePhysicsSceneLibrary(DaeDoc* pDoc);

	void Import();

	virtual const char* libraryType() { return DAE_LIBRARY_PSCENE_ELEMENT; }
	virtual const char* childType() { return DAE_PHYSICS_SCENE_ELEMENT; }
	virtual const char* librarySuffix() { return "-PhysicsScene"; }

private:
	void ImportScene(FCDPhysicsScene* scene);
};

#endif // __DAE_PHYSICS_SCENE_LIBRARY_INCLUDED__
