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

// Target Importer Class, used for lights and cameras

#ifndef _TARGET_IMPORTER_H_
#define _TARGET_IMPORTER_H_

class DocumentImporter;
class NodeImporter;
class FCDSceneNode;

class TargetImporter
{
private:
	DocumentImporter* document;
	float targetDistance;

public:
	TargetImporter(DocumentImporter* document);
	~TargetImporter();

	float GetTargetDistance() { return targetDistance; }

	void ImportTarget(NodeImporter* targetedNode, FCDSceneNode* colladaTargetNode);
};

#endif // _TARGET_IMPORTER_H_