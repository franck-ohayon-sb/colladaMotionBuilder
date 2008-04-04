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

#ifndef _CAMERA_EXPORTER_H_
#define _CAMERA_EXPORTER_H_

#include "BaseExporter.h"

class FCDEntity;
class FCDENode;
class INode;
class CameraObject;

typedef fm::pvector<INode> NodeList;

class CameraExporter : public BaseExporter
{
public:
	CameraExporter(DocumentExporter* document);
	virtual ~CameraExporter();

	FCDEntity* ExportCamera(INode* node, CameraObject* object);

	void CalculateForcedNodes(INode* node, CameraObject* object, NodeList& forcedNodes);
	void LinkCameras();

private:
	void ExportExtraParameter(FCDENode *parentNode, const char *colladaID, IParamBlock2 *params, int pid, int maxType, const fm::string &animID);

};

#endif // _CAMERA_EXPORTER_H_