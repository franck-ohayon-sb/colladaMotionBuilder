/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __ROTATE_HELPER_INCLUDED__
#define __ROTATE_HELPER_INCLUDED__

class DaeDoc;
class FCDSceneNode;
class FCDTRotation;
class FCDTransform;
typedef FUObjectContainer<FCDTransform> FCDTransformContainer;

class CRotateHelper
{
public:
	CRotateHelper(DaeDoc* doc, FCDTransformContainer* container, MEulerRotation rotation);
	void Output();
	inline void SetEliminateEmptyRotations(bool val) { eliminateEmptyRotations = true; }
	inline void SetTransformId(const char* _transformId) { transformId = _transformId; }
	inline void SetParentNode(FCDSceneNode* _parentNode) { parentNode = _parentNode; }

	FCDTRotation* colladaRotations[3]; // 3: 0 is X, 1 is Y, 2 is Z.

private:
	DaeDoc* doc;
	const char*	transformId;
	MEulerRotation rot;
	FCDTransformContainer* container;
	FCDSceneNode* parentNode;
	bool eliminateEmptyRotations;

	FCDTRotation* CreateRotation(const char* coordinate, double angle, const FMVector3& axis);
	void OutX();
	void OutY();
	void OutZ();
};

#endif /* __ROTATE_HELPER_INCLUDED__ */

