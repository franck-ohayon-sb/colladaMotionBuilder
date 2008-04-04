/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "AnimationImporter.h"
#include "ColladaAttrib.h"
#include "ColladaDocumentContainer.h"
#include "DocumentImporter.h"
#include "EntityImporter.h"
#include "SimpleAttrib.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDEntity.h"
#include "FCDocument/FCDEntityInstance.h"
#include "FCDocument/FCDExtra.h"

EntityImporter::EntityImporter(DocumentImporter* _document, NodeImporter* parent)
:	document(_document), finalized(false)
{
	parents.push_back(parent);
}

EntityImporter::~EntityImporter()
{
	document = NULL;
	parents.clear();
}

void EntityImporter::Finalize(FCDEntityInstance* colladaInstance)
{
	if (colladaInstance != NULL)
	{
		FCDEntity* entity = colladaInstance->GetEntity();
		if (entity != NULL) Finalize(entity, GetObject());
	}
	finalized = true;
}

void EntityImporter::Finalize(FCDEntity* colladaEntity, Animatable* object)
{
	if (colladaEntity == NULL || object == NULL) return;

	// If this is a node, it cannot be instanced, so Finalize every time.
	// else, only finalize once.
	if (finalized && GetType() != NODE) return;

	// Import the COLLADA-linking custom attribute.
	AttachEntity(colladaEntity, object);

	// Import the custom attributes from the FCollada extra tree.
	FCDExtra* extra = colladaEntity->GetExtra();
	FCDENode* rootNode = extra->GetDefaultType()->FindRootNode(DAEFC_DYNAMIC_ATTRIBUTES_ELEMENT);
	if (rootNode != NULL)
	{
		// Retrieve the custom attribute container.
		ICustAttribContainer* container = object->GetCustAttribContainer();
		if (container == NULL)
		{
			object->AllocCustAttribContainer();
			container = object->GetCustAttribContainer();
		}

		// This param block will be used to hold the root-level valued parameters that Maya is fond of.
		FStringList rootNames;
		SimpleAttrib* rootAttributes = (SimpleAttrib*) document->MaxCreate(CUST_ATTRIB_CLASS_ID, SIMPLE_ATTRIB_CLASS_ID);
		ParamBlockDesc2* rootParameters = rootAttributes->GetColladaBlockDesc(COLLADA_EXTRA_PARAMBLOCK_NAME); 
		FCDENodeList rootNodes;

		// Process the root-level extra nodes.
		uint32 rootControlId = 256;
		size_t nodeCount = rootNode->GetChildNodeCount();
		for (size_t r = 0; r < nodeCount; ++r)
		{
			FCDENode* blockNode = rootNode->GetChildNode(r);
			size_t blockChildNodeCount = blockNode->GetChildNodeCount();
			if (blockChildNodeCount == 0) // Root-level valued parameter node
			{
				ImportEntityAttribute(rootParameters, blockNode, rootControlId, rootNames);
				rootNodes.push_back(blockNode);
			}
			else
			{
				// Create a parameter block with all the child valued parameters
				uint32 controlId = 256;
				FStringList names;

				// Generate a custom attribute for these parameters
				SimpleAttrib* dynamicAttributes = (SimpleAttrib*) document->MaxCreate(CUST_ATTRIB_CLASS_ID, SIMPLE_ATTRIB_CLASS_ID);
				ParamBlockDesc2* parameters = dynamicAttributes->GetColladaBlockDesc((TCHAR*) (new fstring(blockNode->GetName()))->c_str());
				for (size_t i = 0; i < blockChildNodeCount; ++i)
				{
					FCDENode* parameterNode = blockNode->GetChildNode(i);
					ImportEntityAttribute(parameters, parameterNode, controlId, names);
				}
				dynamicAttributes->SetName(blockNode->GetName());
				dynamicAttributes->CreateParamBlock();
				container->AppendCustAttrib(dynamicAttributes);

				// The names will now be re-assigned to their permanent list
				FStringList* names2 = new FStringList();
				names2->resize(names.size());
				for (size_t i = 0; i < names.size(); ++i) names2->at(i) = names[i];
				dynamicAttributes->SetParamNames(names2);

				// As a second pass, read in the values and animations for these parameters
				short parameterIndex = 0;
				IParamBlock2* paramBlock = dynamicAttributes->GetParamBlock(0);
				for (size_t i = 0; i < blockChildNodeCount; ++i)
				{
					FCDENode* parameterNode = blockNode->GetChildNode(i);
					ImportEntityAttributeValue(paramBlock, parameterNode, parameterIndex, *names2);
				}
			}
		}

		// If root-level valued attributes were found, create the dynamic attributes.
		FUAssert(rootParameters->Count() == (USHORT) rootNodes.size(), return);
		if (rootParameters->Count() > 0)
		{
			rootAttributes->SetName(COLLADA_EXTRA_PARAMBLOCK_NAME);
			rootAttributes->CreateParamBlock();
			container->AppendCustAttrib(rootAttributes);

			// The names will now be re-assigned to their permanent list
			FStringList* rootNames2 = new FStringList();
			rootNames2->resize(rootNames.size());
			for (size_t i = 0; i < rootNames.size(); ++i) rootNames2->at(i) = rootNames[i];
			rootAttributes->SetParamNames(rootNames2);

			// As a second pass, read in the values and animations for these parameters
			short parameterIndex = 0;
			IParamBlock2* paramBlock = rootAttributes->GetParamBlock(0);
			for (FCDENodeList::iterator itN = rootNodes.begin(); itN != rootNodes.end(); ++itN)
			{
				ImportEntityAttributeValue(paramBlock, *itN, parameterIndex, *rootNames2);
			}
		}
		else
		{
			rootAttributes->DeleteMe();
		}
	}
}

void EntityImporter::ImportEntityAttribute(ParamBlockDesc2* parameterBlock, FCDENode* parameterNode, uint32& controlId, FStringList& pnames)
{
	ParamID pid = parameterBlock->Count();

	size_t nodeChildCount = parameterNode->GetChildNodeCount();
	if (nodeChildCount > 0)
	{
		// 3dsMax does not support a full tree of attributes, so collapse these attributes in this parameter block.
		for (size_t i = 0; i < nodeChildCount; ++i)
		{
			FCDENode* childNode = parameterNode->GetChildNode(i);
			ImportEntityAttribute(parameterBlock, childNode, controlId, pnames);
		}
	}
	else
	{
		// Retrieve the type/name/value of the parameter from the extra tree node.
		const FCDEAttribute* _type = parameterNode->FindAttribute(DAE_TYPE_ATTRIBUTE);
		pnames.push_back(parameterNode->GetName());
		TCHAR* name = (TCHAR*) pnames.back().c_str();
		fstring type = (_type != NULL) ? _type->GetValue() : FC("");

		// The names for the parameters are temporarily assigned.
		// To avoid leakage, once the parameter count is known, we will re-assign all these.
		if (IsEquivalent(type, DAEFC_INT_ATTRIBUTE_TYPE))
		{
			uint32 editId = controlId++;
			uint32 spinnerId = controlId++;
			parameterBlock->AddParam(pid, name, TYPE_INT, P_ANIMATABLE, 0, p_ui, TYPE_SPINNER, EDITTYPE_INT, editId, spinnerId, SPIN_AUTOSCALE, end);
		}
		else if (IsEquivalent(type, DAEFC_BOOLEAN_ATTRIBUTE_TYPE))
		{
			parameterBlock->AddParam(pid, name, TYPE_BOOL, P_ANIMATABLE, 0, p_ui, TYPE_CHECKBUTTON, controlId++, end);
		}
		else if (IsEquivalent(type, DAEFC_FLOAT3_ATTRIBUTE_TYPE))
		{
			parameterBlock->AddParam(pid, name, TYPE_POINT3, P_ANIMATABLE, 0, p_ui, TYPE_COLORSWATCH, controlId++, end);
		}
		else if (IsEquivalent(type, DAEFC_FLOAT4_ATTRIBUTE_TYPE) || IsEquivalent(type, DAEFC_COLOR3_ATTRIBUTE_TYPE))
		{
			parameterBlock->AddParam(pid, name, TYPE_RGBA, P_ANIMATABLE, 0, p_ui, TYPE_COLORSWATCH, controlId++, end);
		}
		else if (IsEquivalent(type, DAEFC_MATRIX_ATTRIBUTE_TYPE))
		{
			parameterBlock->AddParam(pid, name, TYPE_MATRIX3, 0, 0, end); // not animatable, no ui available
		}
		else if (IsEquivalent(type, DAEFC_STRING_ATTRIBUTE_TYPE))
		{
			parameterBlock->AddParam(pid, name, TYPE_STRING, 0, 0, p_ui, TYPE_EDITBOX, controlId++, end); // not animatable
		}
		else // assume a float
		{
			uint32 editId = controlId++;
			uint32 spinnerId = controlId++;
			parameterBlock->AddParam(pid, name, TYPE_FLOAT, P_ANIMATABLE, 0, p_ui, TYPE_SPINNER, EDITTYPE_FLOAT, editId, spinnerId, SPIN_AUTOSCALE, end);
		}
	}
}

void EntityImporter::ImportEntityAttributeValue(IParamBlock2* parameterBlock, FCDENode* parameterNode, short& parameterIndex, FStringList& pnames)
{
	size_t nodeChildCount = parameterNode->GetChildNodeCount();
	if (nodeChildCount > 0)
	{
		// 3dsMax does not support a full tree of attributes, so collapse these attributes got collapsed in this parameter block.
		for (size_t i = 0; i < nodeChildCount; ++i)
		{
			FCDENode* childNode = parameterNode->GetChildNode(i);
			ImportEntityAttributeValue(parameterBlock, childNode, parameterIndex, pnames);
		}
	}
	else
	{
		// Retrieve the value of the parameter
		const fchar* content = parameterNode->GetContent();

		// Enforce the new name for this parameter
		ParamID pid = parameterIndex++;
		ParamDef& definition = parameterBlock->GetParamDef(pid);
		definition.int_name = (TCHAR*) pnames[pid].c_str();

		// Process the parameters by the converted type.
		if (parameterBlock->NumParams() <= pid) return;
		switch ((int) definition.type)
		{
		case TYPE_FLOAT: {
			float value = FUStringConversion::ToFloat(content);
			parameterBlock->SetValue(pid, TIME_INITIAL_POSE, value, 0);
			ANIM->ImportAnimatedFloat(parameterBlock, pid, parameterNode->GetAnimated());
			break; }

		case TYPE_POINT3: {
			Point3 value = ToPoint3(FUStringConversion::ToVector3(content));
			parameterBlock->SetValue(pid, TIME_INITIAL_POSE, value, 0);
			ANIM->ImportAnimatedPoint3(parameterBlock, pid, parameterNode->GetAnimated());
			break; }

		case TYPE_RGBA: {
			AColor value = ToAColor(FUStringConversion::ToVector4(content));
			parameterBlock->SetValue(pid, TIME_INITIAL_POSE, value, 0);
			ANIM->ImportAnimatedFRGBA(parameterBlock, pid, parameterNode->GetAnimated());
			break; }

		case TYPE_INT: {
			int value = FUStringConversion::ToInt32(content);
			parameterBlock->SetValue(pid, TIME_INITIAL_POSE, value, 0);
			ANIM->ImportAnimatedFloat(parameterBlock, pid, parameterNode->GetAnimated());
			break; }

		case TYPE_BOOL: {
			bool value = FUStringConversion::ToBoolean(content);
			parameterBlock->SetValue(pid, TIME_INITIAL_POSE, value, 0);
			ANIM->ImportAnimatedFloat(parameterBlock, pid, parameterNode->GetAnimated());
			break; }

		case TYPE_MATRIX3: {
			FMMatrix44 matrix; FUStringConversion::ToMatrix(content, matrix);
			Matrix3 value = ToMatrix3(matrix);
			parameterBlock->SetValue(pid, TIME_INITIAL_POSE, value, 0);
			break; }

		case TYPE_STRING: {
			TCHAR* value = const_cast<TCHAR*>(content);
			parameterBlock->SetValue(pid, TIME_INITIAL_POSE, value, 0);
			break; }
		}
	}
}

void EntityImporter::AttachEntity(FCDEntity* entity, Animatable* object)
{
	FUAssert(object != NULL && entity != NULL, return);

	// Add a Custom Attribute to it
	ICustAttribContainer* caList = object->GetCustAttribContainer();
	if (caList == NULL)
	{
		object->AllocCustAttribContainer();
		caList = object->GetCustAttribContainer();
	}

	if (caList != NULL)
	{
		// First, ENSURE that we do not already have a ColladaAttrib,
		// this happens if this node has been instanced.
		int attributeCount = caList->GetNumCustAttribs();
		for (int i = 0; i < attributeCount; ++i)
		{
			CustAttrib* attribute = caList->GetCustAttrib(i);
			if (attribute->ClassID() == COLLADA_ATTRIB_CLASS_ID) return;
		}
		ColladaAttrib* ca = (ColladaAttrib*) GetDocument()->MaxCreate(CUST_ATTRIB_CLASS_ID, COLLADA_ATTRIB_CLASS_ID);
		caList->AppendCustAttrib(ca);
		ca->SetDaeId(entity->GetDaeId());
		ca->SetDocumentContainer(GetDocument()->GetDocumentContainer());
	}
}
