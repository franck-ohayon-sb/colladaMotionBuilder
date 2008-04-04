/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "DaeDocNode.h"
#include "DaeEntityManager.h"
#include "TranslatorHelpers/CDagHelper.h"
#include <maya/MFnAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnCompoundAttribute.h>
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDEntity.h"
#include "FCDocument/FCDExtra.h"
#include "FUtils/FUDaeSyntax.h"

#if MAYA_API_VERSION < 850
#define getData2Int getData
#define getData2Short getData
#define getData2Float getData
#define getData2Double getData
#define getData3Int getData
#define getData3Short getData
#define getData3Float getData
#define getData3Double getData
#define getData4Double getData
#define setData2Int setData
#define setData2Float setData
#define setData3Int setData
#define setData3Float setData
#define setData4Double setData
#endif // Maya < 8.5 has a different interface for MFnNumericData


DaeEntityManager::DaeEntityManager(DaeDoc* doc)
:	DaeBaseLibrary(doc)
{
	entities.reserve(128);
}

DaeEntityManager::~DaeEntityManager()
{
	Clear();
}

void DaeEntityManager::Clear()
{
	CLEAR_POINTER_VECTOR(entities);
	entities.reserve(128);
}

void DaeEntityManager::ReleaseNodes()
{
	for (DaeEntityList::iterator it = entities.begin(); it != entities.end(); ++it)
	{
		(*it)->ReleaseNode();
	}
}

void DaeEntityManager::RegisterEntity(DaeEntity* entity)
{
	entities.push_back(entity);
}

void DaeEntityManager::ReleaseEntity(DaeEntity* entity)
{
	uint index = GetEntityIndex(entity);
	if (index < entities.size()) entities[index] = NULL;
	SAFE_DELETE(entity);
}

uint DaeEntityManager::GetEntityIndex(DaeEntity* entity)
{
	return (uint) (entities.find(entity) - entities.begin());
}

void DaeEntityManager::ExportEntity(const MObject& obj, FCDEntity* entity)
{
	MStatus status;
	MFnDependencyNode objFn(obj, &status);
	if (!status) return;

	// Export all dynamic attributes
	FCDENode* dynamicsNode = NULL;
	uint attributeCount = objFn.attributeCount();
	for (uint i = 0; i < attributeCount; ++i)
	{
		MFnAttribute attrFn(objFn.attribute(i));

		// Skip some known dynamic attributes
		if (attrFn.parent() != MObject::kNullObj) continue;
		if (!attrFn.isDynamic() || !attrFn.isStorable() || !attrFn.isReadable() || !attrFn.isWritable()) continue;
		if (attrFn.name() == "collada") continue; // Used by the DaeDocNode to link entities with the FCollada objects

		// Well-known dynamic attributes that we don't want to export.
		if (obj.hasFn(MFn::kTransform))
		{
			if (attrFn.name() == "lockInfluenceWeights") continue; // used in all joints.
		}

		// Retrieve this attribute's value
		MPlug plug = objFn.findPlug(attrFn.object(), &status);

		// Add this dynamic attribute as a parameter
		if (dynamicsNode == NULL)
		{
			FCDETechnique* technique = entity->GetExtra()->GetDefaultType()->AddTechnique(DAEMAYA_MAYA_PROFILE);
			dynamicsNode = technique->AddChildNode(DAEFC_DYNAMIC_ATTRIBUTES_ELEMENT);
		}
		ExportEntityAttribute(plug, dynamicsNode);
	}

	// Export the layer information
	MPlug drawOverridePlug = objFn.findPlug("drawOverride");
	if (CDagHelper::HasConnection(drawOverridePlug, false, true))
	{
		MObject layerNode = CDagHelper::GetNodeConnectedTo(drawOverridePlug);
		MFnDependencyNode layerNodeFn(layerNode);
		if (!layerNode.isNull() && layerNode.hasFn(MFn::kDisplayLayer) && layerNodeFn.name() != "defaultLayer")
		{
			FCDLayer* colladaLayer = CDOC->FindLayer(layerNodeFn.name().asChar());
			if (colladaLayer == NULL)
			{
				colladaLayer = CDOC->AddLayer();
				colladaLayer->name = layerNodeFn.name().asChar();
			}
			colladaLayer->objects.push_back(entity->GetDaeId());
		}
	}
}

void DaeEntityManager::ExportEntityAttribute(MPlug& plug, FCDENode* parentNode)
{
	// Retrieve the plug's attribute and create the extra tree node for it
	MObject attribute(plug.attribute());
	MFnAttribute attrFn(attribute);
	const char* attrName = attrFn.name().asChar();
	FCDENode* parameterNode = parentNode->AddChildNode(attrName);
	parameterNode->AddAttribute(DAEMAYA_SHORTNAME_PARAMETER, attrFn.shortName().asChar());

	// Process the attribute according to its type.
	const char* typeName = NULL;
	MFnTypedAttribute typedAttrFn(attrFn.object());
	if (attribute.hasFn(MFn::kTypedAttribute) && typedAttrFn.attrType() != MFnData::kNumeric)
	{
		switch (typedAttrFn.attrType())
		{
		case MFnData::kString: { // Not animatable
			parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_STRING_ATTRIBUTE_TYPE);
			MString value; plug.getValue(value);
			parameterNode->SetContent(TO_FSTRING(value.asChar()));
			break; }

		case MFnData::kMatrix: { // Not animatable
			parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_MATRIX_ATTRIBUTE_TYPE);
			MMatrix value; CDagHelper::GetPlugValue(plug, value);
			parameterNode->SetContent(TO_FSTRING(MConvert::ToFMatrix(value)));
			break; }

		default: break; // none of the other types will be importable.
		}
	}
	else if (attribute.hasFn(MFn::kNumericAttribute) || typedAttrFn.attrType() == MFnData::kNumeric)
	{
		MObject o; plug.getValue(o);
		MFnNumericData data(o);
		MFnNumericData::Type type;
		if (!o.isNull())
		{
			type = data.numericType();
		}
		else
		{
			MFnNumericAttribute nattrFn(attrFn.object());
			type = nattrFn.unitType();
		}

		switch (type)
		{
		case MFnNumericData::kBoolean: {
			parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_BOOLEAN_ATTRIBUTE_TYPE);
			bool value; CDagHelper::GetPlugValue(plug, value);
			parameterNode->SetContent(TO_FSTRING(value));
			ANIM->AddPlugAnimation(plug, parameterNode->GetAnimated(), kBoolean, NULL);
			break; }

		case MFnNumericData::kByte:
		case MFnNumericData::kChar: {
			parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_INT_ATTRIBUTE_TYPE);
			char c; plug.getValue(c);
			parameterNode->SetContent(TO_FSTRING((int32) c));
			ANIM->AddPlugAnimation(plug, parameterNode->GetAnimated(), kSingle, NULL);
			break; }
		
		case MFnNumericData::kShort:
		case MFnNumericData::kInt: {
			parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_INT_ATTRIBUTE_TYPE);
			int i; plug.getValue(i);
			parameterNode->SetContent(TO_FSTRING((int32) i));
			ANIM->AddPlugAnimation(plug, parameterNode->GetAnimated(), kSingle, NULL);
			break; }
								   
		case MFnNumericData::kFloat:
		case MFnNumericData::kDouble: {
			parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_FLOAT_ATTRIBUTE_TYPE);
			float f; plug.getValue(f);
			parameterNode->SetContent(TO_FSTRING(f));
			ANIM->AddPlugAnimation(plug, parameterNode->GetAnimated(), kSingle, NULL);
			break; }
			
		case MFnNumericData::k2Short:{
			parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_INT2_ATTRIBUTE_TYPE);
			short i1, i2; data.getData2Short(i1, i2);
			FUStringBuilder builder; builder.set(i1); builder.append((fchar) ' '); builder.append(i2);
			parameterNode->SetContent(builder.ToCharPtr());
			ANIM->AddPlugAnimation(plug, parameterNode->GetAnimated(), kVector, NULL); // Not sure how kVector will react here.
			break; }

		case MFnNumericData::k2Int: {
			parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_INT2_ATTRIBUTE_TYPE);
			int i1, i2; data.getData2Int(i1, i2);
			FUStringBuilder builder; builder.set(i1); builder.append((fchar) ' '); builder.append(i2);
			parameterNode->SetContent(builder.ToCharPtr());
			ANIM->AddPlugAnimation(plug, parameterNode->GetAnimated(), kVector, NULL); // Not sure how kVector will react here.
			break; }

		case MFnNumericData::k2Float: {
			parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_FLOAT2_ATTRIBUTE_TYPE);
			float f1, f2; data.getData2Float(f1, f2);
			FUStringBuilder builder; builder.set(f1); builder.append((fchar) ' '); builder.append(f2);
			parameterNode->SetContent(builder.ToCharPtr());
			ANIM->AddPlugAnimation(plug, parameterNode->GetAnimated(), kVector, NULL); // Not sure how kVector will react here.
			break; }

		case MFnNumericData::k2Double: {
			parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_FLOAT2_ATTRIBUTE_TYPE);
			double f1, f2; data.getData2Double(f1, f2);
			FUStringBuilder builder; builder.set(f1); builder.append((fchar) ' '); builder.append(f2);
			parameterNode->SetContent(builder.ToCharPtr());
			ANIM->AddPlugAnimation(plug, parameterNode->GetAnimated(), kVector, NULL); // Not sure how kVector will react here.
			break; }

		case MFnNumericData::k3Short: {
			parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_INT3_ATTRIBUTE_TYPE);
			short i1, i2, i3; data.getData3Short(i1, i2, i3);
			FUStringBuilder builder; builder.set(i1); builder.append((fchar) ' '); builder.append(i2); builder.append((fchar) ' '); builder.append(i3);
			parameterNode->SetContent(builder.ToCharPtr());
			ANIM->AddPlugAnimation(plug, parameterNode->GetAnimated(), kVector, NULL);
			break; }

		case MFnNumericData::k3Int: {
			parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_INT3_ATTRIBUTE_TYPE);
			int i1, i2, i3; data.getData3Int(i1, i2, i3);
			FUStringBuilder builder; builder.set(i1); builder.append((fchar) ' '); builder.append(i2); builder.append((fchar) ' '); builder.append(i3);
			parameterNode->SetContent(builder.ToCharPtr());
			ANIM->AddPlugAnimation(plug, parameterNode->GetAnimated(), kVector, NULL);
			break; }

		case MFnNumericData::k3Double: {
			bool isColor = attrFn.isUsedAsColor();
			parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, isColor ? DAEFC_COLOR3_ATTRIBUTE_TYPE : DAEFC_FLOAT3_ATTRIBUTE_TYPE);
			double f1, f2, f3; data.getData3Double(f1, f2, f3);
			FUStringBuilder builder; builder.set(f1); builder.append((fchar) ' '); builder.append(f2); builder.append((fchar) ' '); builder.append(f3);
			parameterNode->SetContent(builder.ToCharPtr());
			ANIM->AddPlugAnimation(plug, parameterNode->GetAnimated(), kVector, NULL);
			break; }

		case MFnNumericData::k3Float: {
			bool isColor = attrFn.isUsedAsColor();
			parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, isColor ? DAEFC_COLOR3_ATTRIBUTE_TYPE : DAEFC_FLOAT3_ATTRIBUTE_TYPE);
			float f1, f2, f3; data.getData3Float(f1, f2, f3);
			FUStringBuilder builder; builder.set(f1); builder.append((fchar) ' '); builder.append(f2); builder.append((fchar) ' '); builder.append(f3);
			parameterNode->SetContent(builder.ToCharPtr());
			ANIM->AddPlugAnimation(plug, parameterNode->GetAnimated(), kVector, NULL);
			break; }

#if MAYA_API_VERSION >= 800
		case MFnNumericData::k4Double: {
			parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_FLOAT4_ATTRIBUTE_TYPE);
			double f1, f2, f3, f4; data.getData4Double(f1, f2, f3, f4);
			FUStringBuilder builder; builder.set(f1); builder.append((fchar) ' '); builder.append(f2); builder.append((fchar) ' '); builder.append(f3);  builder.append((fchar) ' '); builder.append(f4);
			parameterNode->SetContent(builder.ToCharPtr());
			ANIM->AddPlugAnimation(plug, parameterNode->GetAnimated(), kColour, NULL);
			break; }
#endif
		}
	}
	else if (attribute.hasFn(MFn::kCompoundAttribute))
	{
		MFnCompoundAttribute compoundFn(attribute);
		uint32 childCount = compoundFn.numChildren();
		for (uint32 i = 0; i < childCount; ++i)
		{
			MObject childAttribute = compoundFn.child(i);
			MPlug childPlug = plug.child(childAttribute);
			ExportEntityAttribute(childPlug, parameterNode);
		}

		// Add an sid attribute to compound attributes to help the support of animations.
		parameterNode->AddAttribute(DAE_SID_ATTRIBUTE, attrFn.name().asChar());
	}
	else if (attribute.hasFn(MFn::kEnumAttribute))
	{
		MFnEnumAttribute enumFn(attrFn.object());
		int index; CDagHelper::GetPlugValue(plug, index);

		MStatus status;
		MString value = enumFn.fieldName((short) index, &status);
		FUAssert(status, return);

		parameterNode->AddAttribute(DAE_TYPE_ATTRIBUTE, DAEFC_STRING_ATTRIBUTE_TYPE);

		parameterNode->SetContent(TO_FSTRING(value.asChar()));
		ANIM->AddPlugAnimation(plug, parameterNode->GetAnimated(), kSingle, NULL);

		// TODO: Export all possibilities with index values. This will take lots more space, but it will
		// provide enough information to re-create the enum properly.
	}
}

void DaeEntityManager::ImportEntity(MObject& obj, DaeEntity* entity)
{
	MStatus status;
	MFnDependencyNode objFn(obj, &status);
	if (!status || entity == NULL || entity->entity == NULL) return;

	// Retrieves the user-defined notes [Backward/ColladaMax Compatibility]
	if (entity->entity->HasNote())
	{
		CDagHelper::CreateAttribute(obj, "notes", "nts", MFnData::kString, TO_STRING(entity->entity->GetNote()).c_str());
	}

	// Look in the extra information for dynamic attributes
	FCDExtra* extra = entity->entity->GetExtra();
	if (extra != NULL)
	{
		FCDENode* dynamicAttributesNode = extra->GetDefaultType()->FindRootNode(DAEFC_DYNAMIC_ATTRIBUTES_ELEMENT);
		if (dynamicAttributesNode != NULL)
		{
			for (size_t i = 0; i < dynamicAttributesNode->GetChildNodeCount(); ++i)
			{
				// The import of user-defined attributes is done in two passes,
				// as we support any tree of user-defined attributes.
				// Since Maya requires us to add the root attributes only, we
				// cannot set the values until the second pass.
				FCDENode* rootNode = dynamicAttributesNode->GetChildNode(i);
				MObject rootAttribute = ImportEntityAttribute(MObject::kNullObj, rootNode);
				MPlug rootPlug = CDagHelper::AddAttribute(obj, rootAttribute);
				ImportEntityAttributeValues(rootPlug, rootNode);
			}
			SAFE_RELEASE(dynamicAttributesNode);
		}
	}

	// Connect this entity with the DaeDoc node.
	NODE->LinkEntity(doc, entity, obj);
}

MObject DaeEntityManager::ImportEntityAttribute(MObject& parentAttribute, FCDENode* parameterNode)
{
	// Retrieve the long and short name for the attribute
	const char* parameterName = parameterNode->GetName();
	const FCDEAttribute* shortNameAttribute = parameterNode->FindAttribute(DAEMAYA_SHORTNAME_PARAMETER);
	fm::string shortName = (shortNameAttribute != NULL) ? TO_STRING(shortNameAttribute->GetValue()).c_str() : parameterName;

	// Generate the attribute object
#define SET_ATTRIBUTE_FLAGS(attrFn, isKeyable) \
			attrFn.setStorable(true); attrFn.setKeyable(isKeyable); attrFn.setCached(true); attrFn.setReadable(true); attrFn.setWritable(true);

	MObject attributeObject(MObject::kNullObj);
	if (parameterNode->GetChildNodeCount() == 0)
	{
		// This is a tree leaf: create a typed or numeric attribute.

		// Retrieve and process the type of the attribute
		// If no type is provided, create a float attribute: that's a safe bet.
		MFnTypedAttribute typedFn;
		MFnNumericAttribute numericFn;
		const FCDEAttribute* typeAttribute = parameterNode->FindAttribute(DAE_TYPE_ATTRIBUTE);
		fm::string typeName = (typeAttribute != NULL) ? TO_STRING(typeAttribute->GetValue()) : emptyString;
		if (typeAttribute == NULL || typeName == DAEFC_FLOAT_ATTRIBUTE_TYPE)
		{
			attributeObject = numericFn.create(parameterName, shortName.c_str(), MFnNumericData::kFloat);
			SET_ATTRIBUTE_FLAGS(numericFn, true);
		}
		else if (typeName == DAEFC_INT_ATTRIBUTE_TYPE)
		{
			attributeObject = numericFn.create(parameterName, shortName.c_str(), MFnNumericData::kInt);
			SET_ATTRIBUTE_FLAGS(numericFn, true);
		}
		else if (typeName == DAEFC_INT2_ATTRIBUTE_TYPE)
		{
			attributeObject = numericFn.create(parameterName, shortName.c_str(), MFnNumericData::k2Int);
			SET_ATTRIBUTE_FLAGS(numericFn, true);
		}
		else if (typeName == DAEFC_INT3_ATTRIBUTE_TYPE)
		{
			attributeObject = numericFn.create(parameterName, shortName.c_str(), MFnNumericData::k3Int);
			SET_ATTRIBUTE_FLAGS(numericFn, true);
		}
		else if (typeName == DAEFC_BOOLEAN_ATTRIBUTE_TYPE)
		{
			attributeObject = numericFn.create(parameterName, shortName.c_str(), MFnNumericData::kBoolean);
			SET_ATTRIBUTE_FLAGS(numericFn, true);
		}
		else if (typeName == DAEFC_FLOAT2_ATTRIBUTE_TYPE)
		{
			attributeObject = numericFn.create(parameterName, shortName.c_str(), MFnNumericData::k2Float);
			SET_ATTRIBUTE_FLAGS(numericFn, true);
		}
#if MAYA_API_VERSION >= 850
		else if (typeName == DAEFC_FLOAT3_ATTRIBUTE_TYPE)
#else
		else if (typeName == DAEFC_FLOAT3_ATTRIBUTE_TYPE
			|| typeName == DAEFC_FLOAT4_ATTRIBUTE_TYPE)
#endif
		{
			attributeObject = numericFn.createPoint(parameterName, shortName.c_str());
			SET_ATTRIBUTE_FLAGS(numericFn, true);
		}
		else if (typeName == DAEFC_COLOR3_ATTRIBUTE_TYPE)
		{
			attributeObject = numericFn.createColor(parameterName, shortName.c_str());
			SET_ATTRIBUTE_FLAGS(numericFn, true);
		}
#if MAYA_API_VERSION >= 850
		else if (typeName == DAEFC_FLOAT4_ATTRIBUTE_TYPE)
		{
			attributeObject = numericFn.create(parameterName, shortName.c_str(), MFnNumericData::k4Double);
			SET_ATTRIBUTE_FLAGS(numericFn, true);
		}
#endif
		else if (typeName == DAEFC_MATRIX_ATTRIBUTE_TYPE)
		{
			attributeObject = typedFn.create(parameterName, shortName.c_str(), MFnData::kMatrix);
			SET_ATTRIBUTE_FLAGS(typedFn, false);
		}
		else if (typeName == DAEFC_STRING_ATTRIBUTE_TYPE)
		{
			attributeObject = typedFn.create(parameterName, shortName.c_str(), MFnData::kString);
			SET_ATTRIBUTE_FLAGS(typedFn, false);
		}
		else // Unknown type: create a float attribute.
		{
			attributeObject = numericFn.create(parameterName, shortName.c_str(), MFnNumericData::kFloat);
			SET_ATTRIBUTE_FLAGS(numericFn, true);
		}
	}
	else
	{
		// This tree node contains child attributes: create a compound attribute.
		MFnCompoundAttribute compoundFn;
		SET_ATTRIBUTE_FLAGS(compoundFn, false);
		attributeObject = compoundFn.create(parameterName, shortName.c_str());
		size_t childCount = parameterNode->GetChildNodeCount();
		for (size_t i = 0; i < childCount; ++i)
		{
			FCDENode* childNode = parameterNode->GetChildNode(i);
			ImportEntityAttribute(attributeObject, childNode);
		}
	}
#undef SET_ATTRIBUTE_FLAGS

	// Attach the attribute to its parent in the plug tree.
	if (attributeObject != MObject::kNullObj && parentAttribute != MObject::kNullObj)
	{
		MFnCompoundAttribute compoundFn(parentAttribute);
		compoundFn.addChild(attributeObject);
	}

	return attributeObject;
}

void DaeEntityManager::ImportEntityAttributeValues(MPlug& parameterPlug, FCDENode* parameterNode)
{
	// Retrieve the long name for the attribute and its value.
	const fchar* parameterValue = parameterNode->GetContent();

	// Retrieve the attribute object and its type.
	MObject attributeObject = parameterPlug.attribute();
	if (attributeObject.hasFn(MFn::kNumericAttribute))
	{
		MObject o; parameterPlug.getValue(o);
		MFnNumericData dataFn(o);
		MFnNumericData::Type type;
		if (!o.isNull())
		{
			type = dataFn.numericType();
		}
		else
		{
			MFnNumericAttribute nattrFn(attributeObject);
			type = nattrFn.unitType();
		}
		switch (type)
		{
		case MFnNumericData::kFloat: {
			float value = FUStringConversion::ToFloat(parameterValue);
			parameterPlug.setValue(value);
			ANIM->AddPlugAnimation(parameterPlug, parameterNode->GetAnimated(), kSingle, NULL);
			break; }
		case MFnNumericData::k2Float: {
			float value1 = FUStringConversion::ToFloat(&parameterValue);
			float value2 = FUStringConversion::ToFloat(&parameterValue);
			dataFn.setData2Float(value1, value2);
			parameterPlug.setValue(o);
			ANIM->AddPlugAnimation(parameterPlug, parameterNode->GetAnimated(), kVector, NULL);
			break; }
		case MFnNumericData::k3Float:
		case MFnNumericData::k3Double: {
			MColor value = MConvert::ToMColor(FUStringConversion::ToVector3(parameterValue));
			CDagHelper::SetPlugValue(parameterPlug, value);
			ANIM->AddPlugAnimation(parameterPlug, parameterNode->GetAnimated(), kColour, NULL);
			break; }
#if MAYA_API_VERSION >= 850
		case MFnNumericData::k4Double: {
			float value1 = FUStringConversion::ToFloat(&parameterValue);
			float value2 = FUStringConversion::ToFloat(&parameterValue);
			float value3 = FUStringConversion::ToFloat(&parameterValue);
			float value4 = FUStringConversion::ToFloat(&parameterValue);
			dataFn.setData(value1, value2, value3, value4);
			parameterPlug.setValue(o);
			ANIM->AddPlugAnimation(parameterPlug, parameterNode->GetAnimated(), kVector, NULL);
			break; }
#endif

		case MFnNumericData::kInt: {
			int32 value = FUStringConversion::ToInt32(parameterValue);
			parameterPlug.setValue((int) value);
			ANIM->AddPlugAnimation(parameterPlug, parameterNode->GetAnimated(), kSingle, NULL);
			break; }
		case MFnNumericData::k2Int: {
			int32 value1 = FUStringConversion::ToInt32(&parameterValue);
			int32 value2 = FUStringConversion::ToInt32(&parameterValue);
			dataFn.setData2Int((int) value1, (int) value2);
			parameterPlug.setValue(o);
			ANIM->AddPlugAnimation(parameterPlug, parameterNode->GetAnimated(), kVector, NULL);
			break; }
		case MFnNumericData::k3Int: {
			int32 value1 = FUStringConversion::ToInt32(&parameterValue);
			int32 value2 = FUStringConversion::ToInt32(&parameterValue);
			int32 value3 = FUStringConversion::ToInt32(&parameterValue);
			dataFn.setData3Int((int) value1, (int) value2, (int) value3);
			parameterPlug.setValue(o);
			ANIM->AddPlugAnimation(parameterPlug, parameterNode->GetAnimated(), kVector, NULL);
			break; }

		case MFnNumericData::kBoolean: {
			bool value = FUStringConversion::ToBoolean(parameterValue);
			parameterPlug.setValue(value);
			ANIM->AddPlugAnimation(parameterPlug, parameterNode->GetAnimated(), kBoolean, NULL);
			break; }
		}
	}
	else if (attributeObject.hasFn(MFn::kTypedAttribute))
	{
		MFnTypedAttribute typedFn(attributeObject);
		switch (typedFn.attrType())
		{
		case MFnData::kMatrix: {
			FMMatrix44 matrix;
			FUStringConversion::ToMatrix(parameterValue, matrix);
			MMatrix value = MConvert::ToMMatrix(matrix);
			CDagHelper::SetPlugValue(parameterPlug, value);
			break; }

		case MFnData::kString: {
			MString parameterValueString(parameterValue);
			parameterPlug.setValue(parameterValueString);
			break; }
		}
	}
	else if (attributeObject.hasFn(MFn::kCompoundAttribute))
	{
		// Import all the child values of this compound attribute.
		size_t childCount = parameterNode->GetChildNodeCount();
		for (size_t i = 0; i < childCount; ++i)
		{
			FCDENode* childNode = parameterNode->GetChildNode(i);

			// Simply name-match the plugs created above and the child node.
			const char* parameterName = childNode->GetName();
			MPlug childPlug = CDagHelper::GetChildPlug(parameterPlug, parameterName);
			ImportEntityAttributeValues(childPlug, childNode);
		}
	}
}
