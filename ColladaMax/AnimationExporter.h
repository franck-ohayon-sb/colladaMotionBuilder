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

#ifndef _ANIMATION_EXPORTER_H_
#define _ANIMATION_EXPORTER_H_

#ifndef _BASE_EXPORTER_H_
#include "BaseExporter.h"
#endif // _BASE_EXPORTER_H_
#ifndef _FCD_PARAMETER_ANIMATABLE_H_
#include "FCDocument/FCDParameterAnimatable.h"
#endif // _FCD_PARAMETER_ANIMATABLE_H_

class Control;
class FCDAnimated;
class FCDAnimatedCustom;
class FCDAnimation;
class FCDAnimationClip;
class FCDAnimationCurve;
class FCDAnimationChannel;
class FCDConversionFunctor;
class FCDENode;

class AnimationExporter : public BaseExporter
{
private:
	struct Clip
	{
		FCDAnimationClip* colladaClip;
		FCDAnimation* colladaContainer;
		Control* controller;

		Clip();
	};
	typedef fm::map<uint32, Clip> ClipMap;
	typedef fm::pvector<Clip> ClipList;
	
public:
	AnimationExporter(DocumentExporter* document);
	virtual ~AnimationExporter();

	void FinalizeExport();

	void ExportAnimatedFloat(const fm::string& id, Control* controller, FCDAnimated* colladaAnimated, FCDConversionFunctor* conversion=NULL, bool forceSampling=false);
	void ExportAnimatedPoint3(const fm::string& id, Control* controller, FCDAnimated* colladaAnimated, FCDConversionFunctor* conversion=NULL, bool forceSampling=false);
	void ExportAnimatedPoint4(const fm::string& id, Control* controller, FCDAnimated* colladaAnimated, FCDConversionFunctor* conversion=NULL, bool forceSampling=false);

	FCDAnimated* ExportAnimatedAngle(const fm::string& id, Control* controller, ColladaMaxAnimatable::Type type, FCDParameterAnimatableAngleAxis& colladaValue, FCDConversionFunctor* conversion=NULL, bool forceSampling=false);
	FCDAnimated* ExportAnimatedFloat(const fm::string& id, Control* controller, FCDParameterAnimatableFloat& colladaValue, int arrayIndex=-1, FCDConversionFunctor* conversion=NULL, bool forceSampling=false);
	FCDAnimated* ExportAnimatedPoint3(const fm::string& id, Control* controller, FCDParameterAnimatableVector3& colladaValue, int arrayIndex=-1, FCDConversionFunctor* conversion=NULL, bool forceSampling=false);
	FCDAnimated* ExportAnimatedPoint4(const fm::string& id, Control* controller, FCDParameterAnimatableVector4& colladaValue, int arrayIndex=-1, FCDConversionFunctor* conversion=NULL, bool forceSampling=false);
	FCDAnimated* ExportAnimatedAngAxis(const fm::string& id, Control* controller, ColladaMaxAnimatable::Type type, FCDParameterAnimatableAngleAxis& colladaAngleAxis, FCDConversionFunctor* conversion=NULL, bool forceSampling=false);
	FCDAnimated* ExportAnimatedTransform(const fm::string& id, INode* controller, FCDParameterAnimatableMatrix44& colladaValue);
	FCDAnimated* ExportWSMPivot(const fm::string& id, INode* node, FCDParameterAnimatableMatrix44& colladaValue);

	void ExportProperty(const fm::string& id, IParamBlock2* parameters, TSTR name, int tabIndex, FCDParameterAnimatableFloat& colladaValue, FCDConversionFunctor* conversion);
	void ExportProperty(const fm::string& id, IParamBlock2* parameters, TSTR name, int tabIndex, FCDParameterAnimatableVector3& colladaValue, FCDConversionFunctor* conversion);
	void ExportProperty(const fm::string& id, IParamBlock2* parameters, TSTR name, int tabIndex, FCDParameterAnimatableVector4& colladaValue, FCDConversionFunctor* conversion);
	inline void ExportProperty(const fm::string& id, IParamBlock2* parameters, TSTR name, int tabIndex, FCDParameterAnimatableColor4& colladaValue, FCDConversionFunctor* conversion)
	{ ExportProperty(id, parameters, name, tabIndex, (FCDParameterAnimatableVector4&) colladaValue, conversion); }

	void ExportProperty(const fm::string& id, IParamBlock* parameters, int parameterId, FCDParameterAnimatableFloat& colladaValue, FCDConversionFunctor* conversion = NULL);
	void ExportProperty(const fm::string& id, IParamBlock* parameters, int parameterId, FCDParameterAnimatableVector3& colladaValue, FCDConversionFunctor* conversion = NULL);
	void ExportProperty(const fm::string& id, IParamBlock* parameters, int parameterId, FCDParameterAnimatableVector4& colladaValue, FCDConversionFunctor* conversion = NULL);
	void ExportProperty(const fm::string& id, IParamBlock* parameters, int parameterId, FCDAnimated* colladaAnimated, FCDConversionFunctor* conversion = NULL);
	void ExportProperty(const fm::string& id, IParamBlock2* parameters, int parameterId, int tabIndex, FCDParameterAnimatableFloat& colladaValue, FCDConversionFunctor* conversion = NULL);
	void ExportProperty(const fm::string& id, IParamBlock2* parameters, int parameterId, int tabIndex, FCDParameterAnimatableVector3& colladaValue, FCDConversionFunctor* conversion = NULL);
	void ExportProperty(const fm::string& id, IParamBlock2* parameters, int parameterId, int tabIndex, FCDParameterAnimatableVector4& colladaValue, FCDConversionFunctor* conversion = NULL);
	inline void ExportProperty(const fm::string& id, IParamBlock2* parameters, int parameterId, int tabIndex, FCDParameterAnimatableColor4& colladaValue, FCDConversionFunctor* conversion = NULL)
	{ ExportProperty(id, parameters, parameterId, tabIndex, (FCDParameterAnimatableVector4&) colladaValue, conversion); }

	void SetRelativeAnimationFlag(bool isRelative) { isRelativeAnimation = isRelative; }
	void SetControlType(ColladaMaxAnimatable::Type _controlType) { controlType = _controlType; }

private:
	bool ExportAnimation(Clip* controller, ColladaMaxAnimatable::Type type, FCDAnimated* animated, FCDAnimationChannel* channel, FCDConversionFunctor* conversion=NULL, bool forceSampling=false);
	void ExportCurve(Control* controller, FCDAnimationCurve*& curve, FCDConversionFunctor* conversion=NULL, bool forceSampling=false);

	FCDAnimationCurve* CreateCurve(FCDAnimationChannel* colladaChannel, Control* controller = NULL);
	FCDAnimation* CreateAnimation(Clip* clip, const fchar* id);
	ClipList GetAnimationClips(Control* controller);

private:
	// Animation layer/clip information
	ClipMap clips;

	// Relative animation support - only for per-vertex positions
	bool isRelativeAnimation;
	static const size_t maxInitialValues = 16;
	float initialValue[maxInitialValues];

	// INTERNAL ONLY. Used in ExportCurve when exporting animated angle, to know which angle (X, Y or Z)
	// to retrieve from the Quaternion. Also used when exporting partial positions..
	ColladaMaxAnimatable::Type controlType;
};

#endif // _ANIMATION_EXPORTER_H_
