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

// Scene Node Transform Importer Class

#ifndef _TRANSFORM_IMPORTER_H_
#define _TRANSFORM_IMPORTER_H_

class DocumentImporter;
class ImpNode;
class FCDSceneNode;
class FCDTransform;

typedef FUObjectContainer<FCDTransform> FCDTransformContainer;

class TransformImporter
{
private:
	DocumentImporter* document;

	// Switched from ImpNode to INode to ease XRef import
	// see: http://sparks.discreet.com/knowledgebase/private/support/sparks_legacy_wb/publ0wtw.htm
	// for details on how the ImpNode is not different from the INode at import time.
	INode* iNode;

	bool forceSampling;
	bool isAnimated;
	FCDSceneNode* colladaSceneNode;

	// Transform buckets: matches Max's transform stack
	// Any deviation in the COLLADA scene node from this will force baking of the transform
	enum Bucket
	{ TRANSLATION = 0, ROTATE_Z, ROTATE_Y, ROTATE_X, ROTATE_AXIS, SCALE_AXIS_ROTATE, SCALE, SCALE_AXIS_ROTATE_R, BUCKET_COUNT };

	const FCDTransform* buckets[BUCKET_COUNT];

public:
	TransformImporter(DocumentImporter* document, INode *_iNode);
	~TransformImporter();

	DocumentImporter* GetDocument() { return document; }

	void ImportSceneNode(FCDSceneNode* colladaSceneNode);
	void ImportPivot(FCDSceneNode* colladaSceneNode);
	static void ImportPivot(INode* node, Matrix3& tm);

	void Finalize();

private:
	// Attempt to bucket the COLLADA transforms in the slots of the 3dsMax transform
	bool BucketTransforms(const FCDTransform** transforms, size_t transformCount);
};

#endif // _TRANSFORM_IMPORTER_H_