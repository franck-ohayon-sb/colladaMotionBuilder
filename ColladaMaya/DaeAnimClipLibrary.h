/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _DAE_ANIMATION_CLIP_EXPORTER_INCLUDED_H_
#define _DAE_ANIMATION_CLIP_EXPORTER_INCLUDED_H_

#ifndef _DAE_ANIMATION_TYPES_H_
#include "DaeAnimationTypes.h"
#endif // _DAE_ANIMATION_TYPES_H_

class MDGModifier;

class DaeAnimationCurve;
class FCDAnimation;
class FCDAnimationCurve;
class FCDAnimationClip;

typedef fm::pvector<FCDAnimationClip> FCDAnimationClipList;
typedef fm::pvector<FCDAnimation> FCDAnimationList;
typedef fm::pvector<FCDAnimationCurve> FCDAnimationCurveList;
typedef fm::map<FCDAnimationClip*, MObject> ClipObjectMap;

class DaeAnimationClip
{
public:
	MObject characterNode, clipNode;

	// Export-only
	FCDAnimationClip* colladaClip;
	MObjectArray animCurves;
	MPlugArray plugs;
	int FindPlug(const MPlug& p);
};
typedef fm::pvector<DaeAnimationClip> DaeAnimationClipList;

class DaeAnimationCharacter
{
public:
	// Import-only
	FCDAnimationClipList clips;
	MObject characterNode;
	MPlugArray plugs;
	MTime startTime;
};
typedef fm::pvector<DaeAnimationCharacter> DaeAnimationCharacterList;

class DaeAnimClipLibrary
{
public:
	DaeAnimClipLibrary(DaeDoc* doc);
	~DaeAnimClipLibrary();

	// Import a given curve, attached to the correct clip/character/...
	void ImportClipCurve(FCDAnimationCurve* curve, MPlug& plug, MObject& animCurveNode);

	// Create an animation clip's sequential curve, for a given plug.
	void CreateCurve(const MPlug& plug, FCDConversionFunctor* conversion, FCDAnimation* headAnimation, FCDAnimationCurveList& curves);

	// Retrieve/Create the clips associated with a character node
	void GetCharacterClips(const MObject& characterNode, DaeAnimationClipList& clips);

	// Retrieve/Create the import nodes
	DaeAnimationCharacter* GetCharacterNode(FCDAnimationClip* clip, const MPlug& plug);
	MObject GetSourceClipNode(FCDAnimationClip* clip, DaeAnimationCharacter* character, MDGModifier& dgMod);

private:
	DaeAnimationCharacterList characters;
	DaeAnimationClipList clips;
	ClipObjectMap importedClips;
	DaeDoc* doc;
};

#endif // _DAE_ANIMATION_CLIP_EXPORTER_INCLUDED_H_

