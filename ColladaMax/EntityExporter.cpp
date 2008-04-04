/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "AnimationExporter.h"
#include "ColladaAttrib.h"
#include "DocumentExporter.h"
#include "EntityExporter.h"
#include "ColladaOptions.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDEntity.h"
#include "FCDocument/FCDExtra.h"
#include "ColladaFX/ColladaEffectPass.h"
#include "FUtils/FUDaeEnum.h"

//
// EntityExporter
//

EntityExporter::EntityExporter(DocumentExporter* _doc)
:	BaseExporter(_doc)
{
}

EntityExporter::~EntityExporter()
{
}

void EntityExporter::ExportEntity(Animatable* maxEntity, FCDEntity* colladaEntity, const TCHAR* suggestedId)
{
	FUAssert(maxEntity != NULL && colladaEntity != NULL, return);
	FCDENode* extraNode = NULL;

	// Process the custom attributes created for this entity
	ColladaAttrib* colladaAttribute = NULL;
	ICustAttribContainer* customAttributeContainer = maxEntity->GetCustAttribContainer();
	if (customAttributeContainer != NULL)
	{
		int attributeCount = customAttributeContainer->GetNumCustAttribs();
		for (int i = 0; i < attributeCount; ++i)
		{
			CustAttrib* attribute = customAttributeContainer->GetCustAttrib(i);
			if (attribute->NumParamBlocks() < 1) continue;

			if (attribute->ClassID() == COLLADA_ATTRIB_CLASS_ID)
			{
				// This is a COLLADA custom attribute, mark it for later.
				colladaAttribute = (ColladaAttrib*) attribute;
				continue;
			}

			if (attribute->ClassID() == COLLADA_EFFECT_PARAMETER_COLLECTION_ID)
			{
				// This is a COLLADA effect custom attribute already getting exported
				continue;
			}

			// Well known default custom attributes: we're not interested in them.
			TCHAR* name = attribute->GetName();
			if (name == NULL || *name == 0) continue;
			if (IsEquivalent(name, FC("DirectX Manager"))) continue; // Usually attached to StdMat2 objects
			if (IsEquivalent(name, FC("mental ray: material custom attribute"))) continue; // Usually attached to StdMat2 objects
			if (IsEquivalent(name, FC("mental ray: Indirect Illumination custom attribute"))) continue; // Attached to spot lights.
			if (IsEquivalent(name, FC("mental ray: light shader custom attribute"))) continue; // Attached to lights.
			if (IsEquivalent(name, FC("mental ray : attribut personnalisé d'illumination indirecte"))) continue; // Attached to StdMat2 objects in French 3dsMax.

			if (extraNode == NULL)
			{
				// Create the extra element for the dynamic attributes
				FCDExtra* colladaExtra = colladaEntity->GetExtra();
				FCDETechnique* colladaTechnique = colladaExtra->GetDefaultType()->AddTechnique(DAE_FCOLLADA_PROFILE);
				extraNode = colladaTechnique->AddChildNode(DAEFC_DYNAMIC_ATTRIBUTES_ELEMENT);
			}

			FCDENode* parameterRootNode;
			if (IsEquivalent(name, COLLADA_EXTRA_PARAMBLOCK_NAME))
			{
				// This is the import's default param block for root-level valued attributes.
				parameterRootNode = extraNode;
			}
			else
			{
				// Create an extra tree node for this param block.
				fstring nodeName = FCDEntity::CleanName(name);
				parameterRootNode = extraNode->AddChildNode(nodeName);
				parameterRootNode->AddAttribute(DAE_SID_ATTRIBUTE, nodeName);
			}

			IParamBlock2* paramBlock = attribute->GetParamBlock(0);
			int parameterCount = paramBlock->NumParams();
			for (int _p = 0; _p < parameterCount; ++_p)
			{
				ParamID p = paramBlock->IndextoID(_p);
				ParamDef& d = paramBlock->GetParamDef(p);
				switch ((int) d.type)
				{
				case TYPE_INT_TAB:
					if (paramBlock->Count(p) == 0) continue;
				case TYPE_INT: {
					int value = paramBlock->GetInt(p, TIME_EXPORT_START, 0);
					FCDENode* parameterNode = parameterRootNode->AddParameter(d.int_name, value);
					parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_INT_ATTRIBUTE_TYPE);
					ANIM->ExportAnimatedFloat(d.int_name, paramBlock->GetController(p, 0), parameterNode->GetAnimated(), NULL);
					break; }

				case TYPE_FLOAT_TAB:
					if (paramBlock->Count(p) == 0) continue;
				case TYPE_FLOAT: {
					float value = paramBlock->GetFloat(p, TIME_EXPORT_START, 0);
					FCDENode* parameterNode = parameterRootNode->AddParameter(d.int_name, value);
					parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_FLOAT_ATTRIBUTE_TYPE);
					ANIM->ExportAnimatedFloat(d.int_name, paramBlock->GetController(p, 0), parameterNode->GetAnimated(), NULL);
					break; }

				case TYPE_STRING_TAB:
					if (paramBlock->Count(p) == 0) continue;
				case TYPE_STRING: { // Not-animatable
					TCHAR* value = paramBlock->GetStr(p, TIME_EXPORT_START, 0);
					FCDENode* parameterNode = parameterRootNode->AddParameter(d.int_name, value);
					parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_STRING_ATTRIBUTE_TYPE);
					break; }

				case TYPE_BOOL_TAB:
					if (paramBlock->Count(p) == 0) continue;
				case TYPE_BOOL: {
					int value = paramBlock->GetInt(p, TIME_EXPORT_START, 0);
					FCDENode* parameterNode = parameterRootNode->AddParameter(d.int_name, value != 0);
					parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_BOOLEAN_ATTRIBUTE_TYPE);
					ANIM->ExportAnimatedFloat(d.int_name, paramBlock->GetController(p, 0), parameterNode->GetAnimated(), NULL);
					break; }

				case TYPE_POINT3_TAB:
					if (paramBlock->Count(p) == 0) continue;
				case TYPE_POINT3: {
					Point3 value = paramBlock->GetPoint3(p, TIME_EXPORT_START, 0);
					FCDENode* parameterNode = parameterRootNode->AddParameter(d.int_name, TO_FSTRING(ToFMVector3(value)));
					parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_FLOAT3_ATTRIBUTE_TYPE);
					parameterNode->GetAnimated()->Resize(3, FUDaeAccessor::XYZW, true);
					ANIM->ExportAnimatedPoint3(d.int_name, paramBlock->GetController(p, 0), parameterNode->GetAnimated(), NULL);
					break; }

				case TYPE_FRGBA_TAB:
				case TYPE_RGBA_TAB:
					if (paramBlock->Count(p) == 0) continue;
				case TYPE_FRGBA:
				case TYPE_RGBA: {
					AColor value = paramBlock->GetAColor(p, TIME_EXPORT_START, 0);
					FCDENode* parameterNode = parameterRootNode->AddParameter(d.int_name, TO_FSTRING(ToFMVector4(value)));
					parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_COLOR3_ATTRIBUTE_TYPE);
					parameterNode->GetAnimated()->Resize(4, FUDaeAccessor::RGBA, true);
					ANIM->ExportAnimatedPoint4(d.int_name, paramBlock->GetController(p, 0), parameterNode->GetAnimated(), NULL);
					break; }

				case TYPE_POINT4_TAB:
					if (paramBlock->Count(p) == 0) continue;
				case TYPE_POINT4: {
					Point4 value = paramBlock->GetPoint4(p, TIME_EXPORT_START, 0);
					FCDENode* parameterNode = parameterRootNode->AddParameter(d.int_name, TO_FSTRING(ToFMVector4(value)));
					parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_FLOAT4_ATTRIBUTE_TYPE);
					parameterNode->GetAnimated()->Resize(4, FUDaeAccessor::RGBA, true);
					ANIM->ExportAnimatedPoint4(d.int_name, paramBlock->GetController(p, 0), parameterNode->GetAnimated(), NULL);
					break; }
				}
			}
		}
	}

	// Read in and re-use the COLLADA id from the COLLADA custom attribute.
	if (colladaAttribute != NULL)
	{
		colladaEntity->SetDaeId(colladaAttribute->GetDaeId());
	}
	else if (suggestedId != NULL)
	{
		colladaEntity->SetDaeId(suggestedId);
	}
}
