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

// Rendering Light Importer Class

#ifndef _LIGHT_IMPORTER_H_
#define _LIGHT_IMPORTER_H_

#include "EntityImporter.h"

class FCDLight;
class DocumentImporter;
class NodeImporter;
class LightObject;

class LightImporter : public EntityImporter
{
private:
	FCDLight* colladaLight;
	LightObject* lightObject;

public:
	LightImporter(DocumentImporter* document, NodeImporter* parent);
	~LightImporter();

	// EntityImporter overrides
	virtual Type GetType() { return LIGHT; }
	virtual Object* GetObject() { return lightObject; }

	LightObject* ImportLight(FCDLight* colladaCamera);

private:
	/** Import shadow-casting parameters for lights */
	void ImportShadowParams(GenLight* light, FCDENode* extra);
	void ImportExclusionList(FCDENode *shadowParams, GenLight* light);
	void ImportShadowMap(FCDENode *shadowParams, GenLight* light);
	void ImportLightMap(FCDENode *shadowParams, GenLight* light);
};

#endif // _LIGHT_IMPORTER_H_

