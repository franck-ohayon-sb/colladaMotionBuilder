/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "EntityExporter.h"
#include "FCDocument/FCDEntity.h"
#include "FCDocument/FCDExtra.h"
#include "FUtils/FUDaeSyntax.h"

EntityExporter::EntityExporter(ColladaExporter* _base)
:	base(_base)
{
}

EntityExporter::~EntityExporter()
{
}

void EntityExporter::ExportEntity(FCDEntity* colladaEntity, FBComponent* entity)
{
	// Only write in the COLLADA id and name for now.
	fm::string name = (const char*) entity->Name;
	if (!name.empty())
	{
		colladaEntity->SetName(TO_FSTRING(name));
		colladaEntity->SetDaeId(name);
	}

	// Export the user-defined properties only once.
	FCDExtra* extra = colladaEntity->GetExtra();
	if (extra->GetUserHandle() != (void*) 1)
	{
		extra->SetUserHandle((void*) 1);
		ExportUserProperties(colladaEntity, entity);
	}
}

bool EntityExporter::GetPropertyValue(FBComponent* component, const char* propertyName, FMVector3& value)
{
	FBProperty* property = component->PropertyList.Find((char*) propertyName);
	if (property == NULL) return false;

	uint32 type = property->GetPropertyType();
	if (type == kFBPT_Vector3D || type == kFBPT_ColorRGB)
	{
		FBVector3d v = *(FBPropertyVector3d*) property;
		value.x = v[0]; value.y = v[1]; value.z = v[2];
		return true;
	}
	else if (type == kFBPT_Vector4D || type == kFBPT_ColorRGBA)
	{
		FBVector4d v = *(FBPropertyVector4d*) property;
		value.x = v[0]; value.y = v[1]; value.z = v[2];
		return true;
	}
	else if (type == kFBPT_Vector2D)
	{
		FBVector2d v = *(FBPropertyVector2d*) property;
		value.x = v[0]; value.y = v[1]; value.z = 0.0f;
	}
	else
	{
		FUFail("add support?");
	}
	return false;
}

bool EntityExporter::GetPropertyValue(FBComponent* component, const char* propertyName, float& value)
{
	FBProperty* property = component->PropertyList.Find((char*) propertyName);
	if (property == NULL) return false;

	if (property->GetPropertyType() == kFBPT_float)
	{
		value = (float) *(FBPropertyFloat*) property;
		return true;
	}
	else if (property->GetPropertyType() == kFBPT_double)
	{
		value = (float) (double) *(FBPropertyDouble*) property;
		return true;
	}
	else
	{
		FUFail("add support?");
	}
	return false;
}

bool EntityExporter::GetPropertyValue(FBComponent* component, const char* propertyName, int& value)
{
	FBProperty* property = component->PropertyList.Find((char*) propertyName);
	if (property == NULL) return false;

	if (property->GetPropertyType() == kFBPT_int || property->GetPropertyType() == kFBPT_enum)
	{
		value = (int) *(FBPropertyInt*) property;
		return true;
	}
	else
	{
		FUFail("add support?");
	}
	return false;
}

bool EntityExporter::GetPropertyValue(FBComponent* component, const char* propertyName, bool& value)
{
	FBProperty* property = component->PropertyList.Find((char*) propertyName);
	if (property == NULL) return false;

	if (property->GetPropertyType() == kFBPT_bool)
	{
		value = (bool) *(FBPropertyBool*) property;
		return true;
	}
	else if (property->GetPropertyType() == kFBPT_int)
	{
		value = ((int) *(FBPropertyInt*) property) != 0;
		return true;
	}
	else
	{
		FUFail("add support?");
	}
	return false;
}

void EntityExporter::ExportUserProperties(FCDEntity* colladaEntity, FBComponent* entity, bool debug)
{
	FCDENode* colladaUserNode = NULL;

	// Process the user-defined attributes.
	int propertyCount = entity->PropertyList.GetCount();
	for (int i = 0; i < propertyCount; ++i)
	{
		FBProperty* property = entity->PropertyList[i];
		if (!property->IsUserProperty() && !debug) continue;
		if (property->IsList() || property->IsReferenceProperty()) continue;

		if (colladaUserNode == NULL)
		{
			// Create the FCollada user-attributes extra tree.
			FCDETechnique* technique = colladaEntity->GetExtra()->GetDefaultType()->AddTechnique(DAE_FCOLLADA_PROFILE);
			colladaUserNode = technique->AddChildNode(DAEFC_DYNAMIC_ATTRIBUTES_ELEMENT);
		}

		// Create a node for this property.
		fm::string propertyName = property->GetName();
		FCDENode* colladaExtra = colladaUserNode->AddChildNode(propertyName);

		// Process this property by type.
		switch (property->GetPropertyType())
		{
		case kFBPT_int: {
			colladaExtra->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_INT_ATTRIBUTE_TYPE);
			colladaExtra->SetContent(TO_FSTRING(property->AsInt()));
			break; }

		case kFBPT_bool: {
			bool vb;
			colladaExtra->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_BOOLEAN_ATTRIBUTE_TYPE);
			property->GetData(&vb, sizeof(bool));
			colladaExtra->SetContent(TO_FSTRING(vb));
			break; }

		case kFBPT_float: {
			float vf;
			colladaExtra->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_FLOAT_ATTRIBUTE_TYPE);
			property->GetData(&vf, sizeof(float));
			colladaExtra->SetContent(TO_FSTRING(vf));
			break; }

		case kFBPT_double: {
			double vd;
			colladaExtra->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_FLOAT_ATTRIBUTE_TYPE);
			property->GetData(&vd, sizeof(double));
			colladaExtra->SetContent(TO_FSTRING(vd));
			break; }

		case kFBPT_charptr: {
			colladaExtra->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_STRING_ATTRIBUTE_TYPE);
			char* cp = property->AsString();
			colladaExtra->SetContent(TO_FSTRING(cp));
			break; }

		case kFBPT_enum: {
			uint32 ve;
			colladaExtra->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_INT_ATTRIBUTE_TYPE);
			property->GetData(&ve, sizeof(uint32));
			colladaExtra->SetContent(TO_FSTRING(ve));
			break; }

		case kFBPT_String: {
			FBString vsz;
			colladaExtra->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_STRING_ATTRIBUTE_TYPE);
			property->GetData(&vsz, sizeof(FBString));
			colladaExtra->SetContent(TO_FSTRING((const char*) vsz));
			break; }

		case kFBPT_Vector4D: {
			FBVector4d v;
			colladaExtra->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_FLOAT4_ATTRIBUTE_TYPE);
			property->GetData(&v, sizeof(FBVector4d));
			colladaExtra->SetContent(TO_FSTRING(ToFMVector4(v)));
			break; }

		case kFBPT_Vector3D: {
			FBVector3d v;
			colladaExtra->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_FLOAT3_ATTRIBUTE_TYPE);
			property->GetData(&v, sizeof(FBVector3d));
			colladaExtra->SetContent(TO_FSTRING(ToFMVector3(v)));
			break; }

		case kFBPT_ColorRGB: {
			FBVector3d v;
			colladaExtra->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_COLOR3_ATTRIBUTE_TYPE);
			property->GetData(&v, sizeof(FBVector3d));
			colladaExtra->SetContent(TO_FSTRING(ToFMVector3(v)));
			break; }

		case kFBPT_ColorRGBA: {
			FBVector4d v;
			colladaExtra->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_FLOAT4_ATTRIBUTE_TYPE);
			property->GetData(&v, sizeof(FBVector4d));
			colladaExtra->SetContent(TO_FSTRING(ToFMVector4(v)));
			break; }

		case kFBPT_Vector2D: {
			FBVector2d v;
			colladaExtra->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_FLOAT2_ATTRIBUTE_TYPE);
			property->GetData(&v, sizeof(FBVector2d));
			colladaExtra->SetContent(TO_FSTRING(ToFMVector2(v)));
			break; }

		case kFBPT_unknown:
		case kFBPT_Time:
		case kFBPT_object:
		case kFBPT_event:
		case kFBPT_stringlist:
		case kFBPT_Action:
		case kFBPT_Reference:
		case kFBPT_TimeSpan:
		case kFBPT_kReference:
			break;
		default: FUFail(break);
		}
	}
}
