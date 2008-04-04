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

#include "StdAfx.h"
#include "TargetImporter.h"
#include "DocumentImporter.h"
#include "NodeImporter.h"
#include "FCDocument/FCDSceneNode.h"

TargetImporter::TargetImporter(DocumentImporter* _document)
{
	document = _document;
	targetDistance = 100.0f;
}

TargetImporter::~TargetImporter()
{
	document = NULL;
}

void TargetImporter::ImportTarget(NodeImporter* targetedNode, FCDSceneNode* colladaTargetNode)
{
	// Find the target node importer
	EntityImporter* entityImporter = document->FindInstance(colladaTargetNode->GetDaeId());
	if (entityImporter == NULL || entityImporter->GetType() != EntityImporter::NODE) return;
	NodeImporter* targetNodeImporter = (NodeImporter*) entityImporter;

	// Calculate the target distance
	Matrix3 targetMx = targetNodeImporter->GetWorldTransform(0);
	Matrix3 targetedMx = targetedNode->GetWorldTransform(0);
	Point3 targetTranslation = targetMx.GetTrans();
	Point3 targetedTranslation = targetedMx.GetTrans();
	targetDistance = (targetTranslation - targetedTranslation).Length();

	// Attach the target
	ImpNode* targetNode = targetNodeImporter->GetImportNode();
	document->GetImportInterface()->BindToTarget(targetedNode->GetImportNode(), targetNode);

	// Max resets the camera's transform, for some odd reason, so reset the transform
	targetedNode->GetImportNode()->SetTransform(0, targetedMx);
}
