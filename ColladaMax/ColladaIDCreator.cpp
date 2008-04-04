/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "ColladaIDCreator.h"
#include "ColladaAttrib.h"
#include "XRefFunctions.h"
#include "FCDocument/FCDObject.h"
#include "FCDocument/FCDObjectWithId.h"

fm::string ColladaID::Create(INode *maxNode, bool useXRefName)
{
	if (maxNode == NULL)
		return "(null)";

	Object *maxObj = maxNode->GetObjectRef();
	if (maxObj == NULL)
		return "(null)";
	
	fstring id = "";
	if (useXRefName && XRefFunctions::IsXRefObject(maxObj))
	{
		// take the XRef name
		id += XRefFunctions::GetSourceName(maxObj);
	}
	else
	{
		id += TO_STRING(maxNode->GetName());
	}
	char *suffix = "";

	switch (ColladaMaxTypes::Get(maxNode, maxObj))
	{
	case ColladaMaxTypes::MESH:
		suffix = "-mesh"; break;
	case ColladaMaxTypes::SPLINE:
	case ColladaMaxTypes::NURBS_CURVE:
		suffix = "-spline"; break;
	case ColladaMaxTypes::NURBS_SURFACE:
		suffix = "-nurbs"; break;
	case ColladaMaxTypes::LIGHT:
		suffix = "-light"; break;
	case ColladaMaxTypes::CAMERA:
		suffix = "-camera"; break;
	case ColladaMaxTypes::HELPER:
		suffix = "-helper"; break;
	case ColladaMaxTypes::BONE:
		suffix = "-bone"; break;
	default:
		suffix = "-unknown"; break;
	}

	id += suffix;
	return FCDObjectWithId::CleanId(id);
}

fm::string ColladaID::Create(Mtl* maxMtl, bool resolveXRef)
{
	if (maxMtl == NULL) return "(null)";

	// resolve XRefs, if any
	if (resolveXRef)
	{
		maxMtl = XRefFunctions::GetXRefMaterialSource(maxMtl);
	}

	// Check for a COLLADA attribute.
	ICustAttribContainer* container = maxMtl->GetCustAttribContainer();
	if (container != NULL)
	{
		for (size_t i = 0; i < (size_t) container->GetNumCustAttribs(); ++i)
		{
			CustAttrib* attrib = container->GetCustAttrib((int) i);
			if (attrib->ClassID() == COLLADA_ATTRIB_CLASS_ID)
			{
				return ((ColladaAttrib*) attrib)->GetDaeId();
			}
		}
	}

	fm::string id = maxMtl->GetName().data();
	char *suffix = "";

	switch (ColladaMaxTypes::Get(NULL, maxMtl))
	{
	case ColladaMaxTypes::MATERIAL:
		// no suffix for materials
		suffix = "";
		break;
	default:
		suffix = "-unknown";
		break;
	}

	id += suffix;
	return FCDObjectWithId::CleanId(id);
}
