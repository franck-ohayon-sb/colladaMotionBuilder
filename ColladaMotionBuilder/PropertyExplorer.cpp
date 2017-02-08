/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "PropertyExplorer.h"

//
// PropertyExplorer
// 

PropertyExplorer::PropertyExplorer(const fchar* _filename, FBComponent* toExplore)
{
	fm::string filename = _filename;
	filename += ".properties.txt";
	FILE* out = fopen(filename.c_str(), "wb");

	typedef fm::map<FBComponent*, int> AllComponents;
	typedef fm::vector<FBComponent*> ComponentQueue;
	ComponentQueue componentQueue;
	AllComponents allComponents;
	componentQueue.push_back(toExplore);
	allComponents.insert(toExplore, 0);
	while (!componentQueue.empty())
	{
		FBComponent* component = componentQueue.back();
		componentQueue.pop_back();

		// Process this component.
		char* className = (char*)component->ClassName();
		char* componentName = (char*)component->Name.AsString();
		fwrite("[", 1, 1, out);
		fwrite(className, strlen(className), 1, out);
		fwrite("] ", 2, 1, out);
		fwrite(componentName, strlen(componentName), 1, out);
		fwrite("\n", 1, 1, out);
		if (IsEquivalent("FBKeyingGroup", className)) continue;

		// Process the properties of this component.
		int propertyCount = component->PropertyList.GetCount();
		for (int p = 0; p < propertyCount; ++p)
		{
			FBProperty* property = component->PropertyList[p];
			FBPropertyType ptype = property->GetPropertyType();
			char* pclassName = (char*)property->ClassName();
			char* propertyName = (char*)property->GetName();

			if (ptype == kFBPT_int || ptype == kFBPT_bool || ptype == kFBPT_float || ptype == kFBPT_double || ptype == kFBPT_charptr || ptype == kFBPT_enum || ptype == kFBPT_Time || ptype == kFBPT_stringlist
				|| ptype == kFBPT_event || ptype == kFBPT_Vector4D || ptype == kFBPT_Vector3D || ptype == kFBPT_ColorRGB || ptype == kFBPT_ColorRGBA || ptype == kFBPT_TimeSpan || ptype == kFBPT_Vector2D) continue;

			// Skip many un-interesting (and crashing) properties
			if (IsEquivalent(propertyName, "Parents")) continue;
			else if (IsEquivalent(propertyName, "PrimitiveVertexCount")) continue;
			else if (IsEquivalent(propertyName, "VertexIndex")) continue;
			else if (IsEquivalent(propertyName, "NormalIndex")) continue;
			else if (IsEquivalent(propertyName, "UVIndex")) continue;
			else if (IsEquivalent(propertyName, "MaterialId")) continue;
			else if (IsEquivalent(propertyName, "TextureId")) continue;
			else if (IsEquivalent(propertyName, "Keys")) continue;

			// Process this property.
			fwrite("  ", 2, 1, out);
			fwrite("[", 1, 1, out);
			fwrite(pclassName, strlen(pclassName), 1, out);
			fwrite("] ", 2, 1, out);
			fwrite(propertyName, strlen(propertyName), 1, out);
			fwrite("\n", 1, 1, out);

			if (property->IsList())
			{
				FBPropertyListComponent* plist = (FBPropertyListComponent*) property;
				int childCount = plist->GetCount();
				for (int i = 0; i < childCount; ++i)
				{
					FBComponent* subComponent = plist->operator[](i);
					if (subComponent == NULL) continue;
					bool alreadyContained = allComponents.find(subComponent) != allComponents.end();
					if (!alreadyContained)
					{
						componentQueue.push_back(subComponent);
						allComponents.insert(subComponent, 0);
					}
					fwrite("      ", 6, 1, out);
					char* subClassName = (char*)subComponent->ClassName();
					char* subComponentName = (char*)subComponent->Name.AsString();
					fwrite("[", 1, 1, out);
					fwrite(subClassName, strlen(subClassName), 1, out);
					fwrite("] -link- ", 9, 1, out);
					fwrite(subComponentName, strlen(subComponentName), 1, out);
					fwrite("\n", 1, 1, out);
				} 
			}
			else if (ptype == kFBPT_object)
			{
				FBPropertyComponent* cp = (FBPropertyComponent*) property;
				FBComponent* subComponent = (FBComponent*) (*cp);
				if (subComponent == NULL) continue;
				bool alreadyContained = allComponents.find(subComponent) != allComponents.end();
				if (!alreadyContained)
				{
					componentQueue.push_back(subComponent);
					allComponents.insert(subComponent, 0);
				}
				fwrite("      ", 6, 1, out);
				char* subClassName = (char*)subComponent->ClassName();
				char* subComponentName = (char*)subComponent->Name.AsString();
				fwrite("[", 1, 1, out);
				fwrite(subClassName, strlen(subClassName), 1, out);
				fwrite("] -link- ", 9, 1, out);
				fwrite(subComponentName, strlen(subComponentName), 1, out);
				fwrite("\n", 1, 1, out);
			}
		}
	}

	fclose(out);
}
