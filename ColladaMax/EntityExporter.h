/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _ENTITY_EXPORTER_H_
#define _ENTITY_EXPORTER_H_

#ifndef _BASE_EXPORTER_H_
#include "BaseExporter.h"
#endif // _BASE_EXPORTER_H_

class Animtable;
class DocumentExporter;
class FCDEntity;

class EntityExporter : public BaseExporter
{
private:

public:
	EntityExporter(DocumentExporter* doc);
	virtual ~EntityExporter();

	void ExportEntity(Animatable* maxEntity, FCDEntity* colladaEntity, const TCHAR* suggestedId);
};

#endif // _ENTITY_EXPORTER_H_