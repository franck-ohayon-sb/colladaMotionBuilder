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

// Animation Importer Class
// Creates controllers and curves for given COLLADA animations

#ifndef _ANIMATION_IMPORTER_H_
#define _ANIMATION_IMPORTER_H_

#ifndef _FCD_PARAMETER_ANIMATABLE_H_
#include "FCDocument/FCDParameterAnimatable.h"
#endif // _FCD_PARAMETER_ANIMATABLE_H_

class DocumentImporter;
class FCDAnimated;
class FCDAnimationCurve;
class FCDAnimationMultiCurve;
class FCDConversionFunctor;
class FCDTRotation;
class FCDTTranslation;
class FCDTScale;
class FCDSceneNode;
class IKeyControl;
class MasterPointControl;

class AnimationImporter
{
private:
	DocumentImporter* document;

public:
	AnimationImporter(DocumentImporter* document);
	~AnimationImporter();

	// COLLADA has time in seconds, while Max uses frame time.
	// The following returns the conversion factor.
	inline static float GetTimeFactor() { return GetTicksPerFrame() * GetFrameRate(); }

	// Imports into Max a given COLLADA animation curve for a simple float property
	inline void ImportAnimatedFloat(Animatable* entity, const fm::string& propertyName, const FCDParameterAnimatableFloat& colladaValue, FCDConversionFunctor* conversion=NULL, bool collapse=false) { return ImportAnimatedFloat(entity, propertyName.c_str(), colladaValue, conversion, collapse); }
	void ImportAnimatedFloat(Animatable* entity, const char* propertyName, const FCDParameterAnimatableFloat& colladaValue, FCDConversionFunctor* conversion=NULL, bool collapse=false);
	void ImportAnimatedFloat(Animatable* entity, const char* propertyName, const FCDAnimated* colladaAnimated, FCDConversionFunctor* conversion=NULL, bool collapse=false);
	void ImportAnimatedFloat(Animatable* entity, const char* propertyName, const FCDAnimationCurve* colladaCurve, FCDConversionFunctor* conversion=NULL);
	void ImportAnimatedFloat(IParamBlock2* entity, uint32 pid, const FCDParameterAnimatableFloat& colladaValue, FCDConversionFunctor* conversion=NULL, bool collapse=false);
	void ImportAnimatedFloat(IParamBlock2* entity, uint32 pid, FCDAnimated* colladaAnimated, FCDConversionFunctor* conversion=NULL, bool collapse=false);
	inline void ImportAnimatedFloat(IParamBlock2* entity, uint32 pid, const FCDAnimated* colladaAnimated, FCDConversionFunctor* conversion=NULL, bool collapse=false) { return ImportAnimatedFloat(entity, pid, const_cast<FCDAnimated*>(colladaAnimated), conversion, collapse); }
	void ImportAnimatedFloat(Animatable* entity, uint32 subAnimId, FCDAnimated* colladaAnimated, FCDConversionFunctor* conversion=NULL, bool collapse=false);
	void ImportAnimatedFloat(Animatable* entity, uint32 subAnimId, FCDAnimationCurve* colladaCurve, FCDConversionFunctor* conversion=NULL);

	// Imports into Max the 3d COLLADA animation curve for a color/point3 value
	void ImportAnimatedPoint4(Animatable* entity, const char* propertyName, const FCDParameterAnimatableVector4& colladaValue, FCDConversionFunctor* conversion=NULL);
	void ImportAnimatedPoint4(Animatable* entity, uint32 pid, const FCDParameterAnimatableVector4& colladaValue, FCDConversionFunctor* conversion=NULL);
	void ImportAnimatedFRGBA(Animatable* entity, uint32 pid, const FCDParameterAnimatableColor4& colladaValue, FCDConversionFunctor* conversion=NULL);
	void ImportAnimatedFRGBA(IParamBlock2* entity, uint32 pid, const FCDAnimated* colladaAnimated, FCDConversionFunctor* conversion = NULL);
	void ImportAnimatedFRGBA(Animatable* entity, uint32 pid, const FCDAnimated* colladaAnimated, FCDConversionFunctor* conversion = NULL);
	void ImportAnimatedPoint3(Animatable* entity, const char* propertyName, const FCDParameterAnimatableVector3& colladaValue, FCDConversionFunctor* conversion=NULL);
	void ImportAnimatedPoint3(Animatable* entity, uint32 pid, const FCDParameterAnimatableVector3& colladaValue, FCDConversionFunctor* conversion=NULL);
	void ImportAnimatedPoint3(MasterPointControl* masterController, int subControllerIndex, const FCDAnimationMultiCurve* curve, FCDConversionFunctor* conversion=NULL);

	// Imports into Max the 3d COLLADA animation curves for a translation
	void ImportAnimatedTranslation(Control* tmController, const FCDTTranslation* translation);

	// Imports Euler/Angle-axis rotations into Max from the COLLADA animation curves
	// Some of the euler angle rotations may be NULL, when they are not animated.
	void ImportAnimatedAxisRotation(Animatable* entity, const fm::string& propertyName, const FCDTRotation* angleAxisRotation);
	void ImportAnimatedEulerRotation(Animatable* entity, const fm::string& propertyName, const FCDTRotation* xRotation, const FCDTRotation* yRotation, const FCDTRotation* zRotation);

	// Imports a scale factor with scale-axis rotation animation into Max.
	// Note that the scale-axis rotation may be NULL to imply no rotation.
	void ImportAnimatedScale(Animatable* entity, const fm::string& propertyName, const FCDTScale* scale, const FCDTRotation* scaleAxisRotation);

	// Imports a Max scene node transform, using sampling of the COLLADA scene node's transforms
	void ImportAnimatedSceneNode(Animatable* tmController, FCDSceneNode* colladaSceneNode);

public:
	// Search for a property, given by name, on a given Animatable entity
	static IParamBlock2* FindProperty(Animatable* entity, const fm::string& propertyName, ParamID& pid);

	// Search for a sub-animatable object, given by name, on a given Animatable entity
	static Animatable* FindAnimatable(Animatable* entity, const char* animName, int& subAnimIndex);

	// Create a keyframe controller for a given animatable's property
	template <class FCDAnimationXCurve>
	IKeyControl* CreateKeyController(Animatable* entity, const char* propertyName, SClass_ID sclassID, const Class_ID& classID, FCDAnimationXCurve* curve);
	template <class FCDAnimationXCurve>
	IKeyControl* CreateKeyController(Animatable* entity, uint32 subAnimNum, SClass_ID sclassID, const Class_ID& classID, FCDAnimationXCurve* curve);
	template <class FCDAnimationXCurve>
	inline IKeyControl* CreateKeyController(Animatable* entity, const fm::string& propertyName, SClass_ID sclassID, const Class_ID& classID, FCDAnimationXCurve* curve) { return CreateKeyController(entity, propertyName.c_str(), sclassID, classID, curve); }

	// Returns whether a curve should be processed as a bezier curve
	template <class FCDAnimationXCurve>
	static bool IsBezierCurve(const FCDAnimationXCurve* colladaCurve);

	// Fills in a specific controller type with the key data from a given COLLADA animation curve
	void FillFloatController(Control* controller, const FCDParameterAnimatableFloat& colladaAnimated, bool isBezier);

	template <class FCDAnimationXCurve, int Dimensions>
	void FillController(IKeyControl* controller, const FCDAnimationXCurve* colladaCurve, bool isBezier);
};

// Some common conversion functions
#undef DegToRad
static inline float DegToRad(float v) { return v * DEG_TO_RAD; }

#endif // _ANIMATION_IMPORTER_H_