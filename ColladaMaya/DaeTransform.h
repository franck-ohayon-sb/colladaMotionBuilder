/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_TRANSFORM_CLASS__
#define __DAE_TRANSFORM_CLASS__

#include <maya/MTransformationMatrix.h>

class DaeDoc;
class DaeEntity;
class FCDTransform;
class FCDTMatrix;
class FCDTTranslation;
class FCDTRotation;
class FCDTScale;
class FCDTSkew;
class FCDTLookAt;
typedef fm::pvector<FCDTransform> FCDTransformList;

class DaeTransformPresence
{
public:
	enum EPresence
	{
		kUnused = 0,
		kPresent = 1,
		kNecessary = 2
	};

	EPresence trans;
	EPresence rot[3];
	EPresence rotAxis[3];
	EPresence jointOrient[3];
	EPresence scale;

	FCDTTranslation* transNode;
	FCDTRotation* rotNode[3];
	FCDTRotation* rotAxisNode[3];
	FCDTRotation* jointOrientNode[3];
	FCDTScale* scaleNode;

	DaeTransformPresence() { memset(this, 0, sizeof(DaeTransformPresence)); }
};

class DaeTransform
{
private:
	DaeDoc* doc;
	DaeEntity* element;
	FCDSceneNode* colladaNode;

	MObject transform;
	MTransformationMatrix transformation;
	DaeTransformPresence presence;

	// Export-specific: make sure to export the rotation order correctly by always enforcing
	// the elements of the first rotation.
	bool isFirstRotation;

	// Import-specific: used when attempting to bucket the COLLADA transforms
	int rotationOrder[9];
	int rotationOrderCount;
	bool noRotationOrder;
	bool isJoint;

	enum
	{
		TRANSLATION = 0, ROTATE_PIVOT_TRANSLATION, ROTATE_PIVOT,
		JOINT_ORIENT_1, JOINT_ORIENT_2, JOINT_ORIENT_3, ROTATE_1, ROTATE_2, ROTATE_3, ROTATE_A,
		ROTATE_AXIS_1, ROTATE_AXIS_2, ROTATE_AXIS_3, ROTATE_PIVOT_R, SCALE_PIVOT_TRANSLATION,
		SCALE_PIVOT, SKEW_XY, SKEW_XZ, SKEW_YZ, SCALE, SCALE_PIVOT_R, BUCKET_COUNT
	};

	const FCDTransform* buckets[BUCKET_COUNT];
	int bucketDepth;


public:
	DaeTransform(DaeDoc* doc, FCDSceneNode* _colladaNode);
	~DaeTransform();

	const MObject& GetTransform() { return transform; }
	void SetTransform(const MObject& transform);
	void SetTransformation(const MTransformationMatrix& transformation);
	MObject CreateTransform(const MObject& parent, const MString& name);

	void ExportTransform();
	void ExportVisibility();
	void ImportTransform(FCDSceneNode* colladaNode);
	void ClearTransform();

	MTransformationMatrix GetTransformation(){ return transformation; }

private:
	void ExportLookatTransform();
	void ExportDecomposedTransform();

	void ExportTranslation(const char* name, const MVector& translation, bool animation = false);
	void ExportTranslation(const char* name, const MPoint& translation, bool animation = false);
	void ExportRotation(const char* name, const MEulerRotation& rot, DaeTransformPresence::EPresence* p, FCDTRotation** rotationTransforms);
	void ExportScale();
	void ExportMatrix(const MMatrix& matrix);

	bool BucketTransforms(FCDSceneNode* node);
	bool BucketTranslation(FCDTTranslation* translation);
	bool BucketRotation(FCDTRotation* rotation);
	bool BucketScale(FCDTScale* scale);
	bool BucketSkew(FCDTSkew* skew);
	void ImportTranslation(FCDTTranslation* transform, const char* plugName);
	void ImportRotation(FCDTRotation* transform, const char* plugName);
	void ImportScale(FCDTScale* transform);
	void ImportSkew(FCDTSkew* transform, const char* plugName);

	void CalculateRotationOrder();
};

#endif
