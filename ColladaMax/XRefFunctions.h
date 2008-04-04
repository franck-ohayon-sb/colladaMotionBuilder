/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _XREF_FUNCTIONS
#define _XREF_FUNCTIONS

/*
 * Small namespace to contain commonly used static XRef-related functions
 */
namespace XRefFunctions
{
	// helpers
	extern bool IsXRefObject(Animatable* obj);
	extern bool IsXRefMaterial(Animatable* anim);
	extern fstring GetURL(Animatable* xref);
	extern bool IsXRefCOLLADA(Animatable* xref);
	extern fm::string GetSourceName(Animatable* xref);

	// resolve XRefs
	extern Object* GetXRefObjectSource(Object* xref);
	extern Mtl* GetXRefMaterialSource(Mtl* xref);
	extern fm::string GetXRefObjectName(Object* xref);
}

#endif