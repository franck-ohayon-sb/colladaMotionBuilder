/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/
/*
	Based on the 3dsMax COLLADA Tools:
	Copyright (C) 2005-2006 Autodesk Media Entertainment
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

// Entity Importer Factory Class - Creates entity importers
// Useful because both NodeImporter and the controller importers create sub-entities

#ifndef _ENTITY_IMPORTER_FACTOR_H_
#define _ENTITY_IMPORTER_FACTOR_H_

class DocumentImporter;
class EntityImporter;
class NodeImporter;
class FCDEntity;

class EntityImporterFactory
{
private:
	EntityImporterFactory() {}
	~EntityImporterFactory() {}

public:
	static EntityImporter* Create(FCDEntity* colladaEntity, DocumentImporter* document, NodeImporter* parentNode);
};

#endif // _ENTITY_IMPORTER_FACTOR_H_