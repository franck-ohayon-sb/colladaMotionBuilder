/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "XRefIncludes.h"
#include "XRefFunctions.h"
#include "FUtils/FUUri.h"
#include "FUtils/FUFileManager.h"

bool XRefFunctions::IsXRefObject(Animatable* obj)
{
#if USE_MAX_8
	return IXRefObject8::Is_IXRefObject8(* obj);
#elif USE_MAX_7
	return (obj->SuperClassID() == SYSTEM_CLASS_ID && obj->ClassID().PartA() == XREFOBJ_CLASS_ID);
#endif
}

bool XRefFunctions::IsXRefMaterial(Animatable* anim)
{
#if USE_MAX_8
	return IXRefMaterial::Is_IXRefMaterial(*anim);
#elif USE_MAX_7
	// not supported
	(void)anim;
	return false;
#endif
}

fstring XRefFunctions::GetURL(Animatable* xref)
{
	if (!IsXRefObject(xref) && !IsXRefMaterial(xref)) return FS("");

	fstring maxUrl;
#if USE_MAX_8
	{
		IXRefItem* x = IXRefItem::GetInterface(*xref);
		maxUrl = (x != NULL) ? x->GetSrcFileName() : emptyFString;
	}
#elif USE_MAX_7
	{
		IXRefObject* x = (IXRefObject*) xref;
		maxUrl = TO_FSTRING(x->GetFileName(FALSE).data());
	}
#endif 

	size_t xrefTokenIndex = maxUrl.find(FC(".xreffile"));
	if (xrefTokenIndex != fstring::npos)
	{
		// Remove the ".xreffile.max" suffix. The old extension should still be there.
		maxUrl.erase(xrefTokenIndex);
	}

	// Replace the extension by .dae
	return maxUrl;
}

bool XRefFunctions::IsXRefCOLLADA(Animatable* xref)
{
	// Not performant, but the simplest method is to look at the extension of the filename.
	FUUri uri(XRefFunctions::GetURL(xref));
	fstring extension = FUFileManager::GetFileExtension(uri.GetPath());
	return IsEquivalentI(extension, FC("dae")) || IsEquivalentI(extension, FC("xml"));
}

fm::string XRefFunctions::GetSourceName(Animatable* xref)
{
#if USE_MAX_8
	IXRefItem* x = IXRefItem::GetInterface(* xref);
	if (x == NULL)
		return "";

	return fm::string(x->GetSrcItemName());

#elif USE_MAX_7
	if (IsXRefObject(xref))
	{
		IXRefObject* xobj = (IXRefObject*)xref;
		return fm::string(xobj->GetObjName(FALSE).data());
	}
	else
	{
		return "(not xref)";
	}
#endif
}

Object* XRefFunctions::GetXRefObjectSource(Object* xref)
{
	if (!IsXRefObject(xref)) return xref; // this is the source
#if USE_MAX_8

	IXRefObject8* xobj = IXRefObject8::GetInterface(* xref);
	if (xobj == NULL) return NULL;

	// resolve nested
	Object* src = xobj->GetSourceObject(true, NULL);

#elif USE_MAX_7
	XRefObject_REDEFINITION* xobj = (XRefObject_REDEFINITION*)xref;
	Object* src = xobj->obj;

#endif
	return src;
}

fm::string XRefFunctions::GetXRefObjectName(Object* xref)
{
	if (!IsXRefObject(xref)) return emptyString; // this is the source
#if USE_MAX_8

	IXRefObject8* xobj = IXRefObject8::GetInterface(*xref);
	if (xobj == NULL) return emptyString;
	IXRefItem* item = IXRefItem::GetInterface(*xref);
	if (item == NULL) return emptyString;
	return item->GetSrcItemName();
#elif USE_MAX_7

	XRefObject_REDEFINITION* xobj = (XRefObject_REDEFINITION*)xref;
	return xobj->xrefObj.data();
#endif
}

Mtl* XRefFunctions::GetXRefMaterialSource(Mtl* xref)
{
#if USE_MAX_8
	
	if (!IsXRefMaterial(xref))
	{
		// this is the source
		return xref;
	}

	IXRefMaterial* xmtl = IXRefMaterial::GetInterface(* xref);
	if (xmtl == NULL)
		return NULL;

	// resolve nested
	Mtl* src = xmtl->GetSourceMaterial(true);
	return src;

#elif USE_MAX_7
	// not supported in Max 7
	return xref;
#endif
}

