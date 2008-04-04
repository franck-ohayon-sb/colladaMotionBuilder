/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _ENTITY_EXPORTER_H_
#define _ENTITY_EXPORTER_H_

class ColladaExporter;
class FCDEntity;

class EntityExporter
{
private:
	ColladaExporter* base;

public:
	EntityExporter(ColladaExporter* base);
	virtual ~EntityExporter();

	ColladaExporter* GetBaseExporter() { return base; }

	// Common entity information export.
	void ExportEntity(FCDEntity* colladaEntity, FBComponent* entity);

	// Property look-ups.
	bool GetPropertyValue(FBComponent* component, const char* propertyName, FMVector3& value);
	bool GetPropertyValue(FBComponent* component, const char* propertyName, float& value);
	bool GetPropertyValue(FBComponent* component, const char* propertyName, int& value);
	bool GetPropertyValue(FBComponent* component, const char* propertyName, bool& value);

	// Writes out the user-properties. If debug is true, ALL properties are exported.
	void ExportUserProperties(FCDEntity* colladaEntity, FBComponent* entity, bool debug=false);
};

#endif // _ENTITY_EXPORTER_H_