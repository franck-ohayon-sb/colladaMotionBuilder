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

#ifndef _LIGHT_EXPORTER_H_
#define _LIGHT_EXPORTER_H_

#ifndef _BASE_EXPORTER_H_
#include "BaseExporter.h"
#endif // _BASE_EXPORTER_H_

class FCDEntity;
class INode;
class LightObject;

typedef fm::pvector<INode> NodeList;

class LightExporter : public BaseExporter
{
public:
	LightExporter(DocumentExporter* document);
	virtual ~LightExporter();

	FCDEntity* ExportLight(INode* node, LightObject* object);
	FCDEntity* ExportAmbientLight(const Point3& ambient);
	void LinkLights();

	void CalculateForcedNodes(INode* node, LightObject* object, NodeList& forcedNodes);

private:
	/** Export the shadow-casting parameters of this light */
	void ExportShadowParams(GenLight* light, FCDENode* extra);
	/** Export a projection map if present */
	void ExportLightMap(GenLight* light, FCDENode* extra);
};

#endif // _LIGHT_EXPORTER_H_