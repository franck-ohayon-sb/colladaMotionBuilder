/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/
#include "stdafx.h"
#include "MaxParticleDefinitions.h"
#include "max.h"
/*
CustAttrib* GetCustomAttribute(Animatable* object, const Class_ID *clid)
{
	if (object == NULL || clid == NULL) return NULL;

	// Process the custom attributes created for this entity
	ICustAttribContainer* customAttributeContainer = object->GetCustAttribContainer();
	if (customAttributeContainer != NULL)
	{
		int attributeCount = customAttributeContainer->GetNumCustAttribs();
		for (int i = 0; i < attributeCount; ++i)
		{
			CustAttrib* attribute = customAttributeContainer->GetCustAttrib(i);
			if (attribute->ClassID() == *clid) return attribute;
		}
	}
	return NULL;
}*/