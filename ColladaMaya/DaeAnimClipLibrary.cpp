/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include <maya/MDGModifier.h>
#include <maya/MFnCharacter.h>
#include <maya/MFnClip.h>
#include "CExportOptions.h"
#include "DaeAnimationCurve.h"
#include "DaeAnimClipLibrary.h"
#include "TranslatorHelpers/CAnimationHelper.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDAnimation.h"
#include "FCDocument/FCDAnimationCurve.h"
#include "FCDocument/FCDAnimationClip.h"
//#include "FUtils/FUDaeParser.h"
//using namespace FUDaeParser;

DaeAnimClipLibrary::DaeAnimClipLibrary(DaeDoc* _doc)
:	doc(_doc)
{
}

DaeAnimClipLibrary::~DaeAnimClipLibrary()
{
	CLEAR_POINTER_VECTOR(characters);
	CLEAR_POINTER_VECTOR(clips);
	doc = NULL;
}

int DaeAnimationClip::FindPlug(const MPlug& p)
{
	uint count = plugs.length();
	for (uint i = 0; i < count; ++i)
	{
		if (p == plugs[i])
		{
			if (!p.isElement() || p.logicalIndex() == plugs[i].logicalIndex())
			{
				return i;
			}
		}
	}
	return -1;
}

// Retrieve/Create the clips associated with a character node
void DaeAnimClipLibrary::GetCharacterClips(const MObject& characterNode, DaeAnimationClipList& characterClips)
{
	MStatus status;

	// Retrieve the known clips associated with this character node
	for (DaeAnimationClipList::iterator itC = clips.begin(); itC != clips.end(); ++itC)
	{
		if ((*itC)->characterNode == characterNode) characterClips.push_back(*itC);
	}

	if (characterClips.empty())
	{
		// Create the clips associated with this character
		MFnCharacter characterFn(characterNode);
		int clipCount = characterFn.getSourceClipCount();
		if (clipCount == 0) return;

		MPlugArray plugs;
		for (int i = 0; i < clipCount; ++i)
		{
			// Export any source clip, including poses, which are simply 1-key curves.
			MObject clipNode = characterFn.getSourceClip(i);
			MFnClip clipFn(clipNode, &status);
			if (status != MStatus::kSuccess) continue;

			// Create the corresponding COLLADA animation clip
			fstring clipName = MConvert::ToFChar(doc->MayaNameToColladaName(clipFn.name()));
			DaeAnimationClip* clip = new DaeAnimationClip();
			clip->colladaClip = CDOC->GetAnimationClipLibrary()->AddEntity();
			clip->colladaClip->SetName(clipName);
			clip->colladaClip->SetDaeId(TO_STRING(clipName));
			clip->colladaClip->SetStart((float) clipFn.getSourceStart().as(MTime::kSeconds));
			clip->colladaClip->SetEnd(clip->colladaClip->GetStart() + (float) clipFn.getSourceDuration().as(MTime::kSeconds));
			clip->clipNode = clipNode;
			clip->characterNode = characterNode;
			clipFn.getMemberAnimCurves(clip->animCurves, clip->plugs);

			clips.push_back(clip);
			characterClips.push_back(clip);
		}
	}
}

// Export a plug's animations, if present
void DaeAnimClipLibrary::CreateCurve(const MPlug& plug, FCDConversionFunctor* conversion, FCDAnimation* headAnimation, FCDAnimationCurveList& curves)
{
	MStatus status;

	// Verify the character node for this plug
	MObject characterNode = CAnimationHelper::GetAnimatingNode(plug);
	MFnCharacter characterFn(characterNode, &status);
	if (status != MStatus::kSuccess) return;

	// Find/Create the clips associated with this character node
	DaeAnimationClipList clips;
	GetCharacterClips(characterNode, clips);
	if (clips.empty()) return;

	FCDAnimationCurve* mainCurve = NULL;
	for (DaeAnimationClipList::iterator it = clips.begin(); it != clips.end(); ++it)
	{
		DaeAnimationClip* clip = (*it);
		int index = clip->FindPlug(plug);
		if (index == -1) continue;

		FCDAnimation* animation = headAnimation->AddChild();
		animation->SetDaeId(headAnimation->GetDaeId() + "-" + clip->colladaClip->GetDaeId());
		FCDAnimationChannel* channel = animation->AddChannel();

		// Create the curve segment for this clip-plug pair
		MObject animCurveNode = clip->animCurves[index];
		FCDAnimationCurve* curve = DaeAnimationCurve::CreateFromMayaNode(doc, animCurveNode, channel);
		if (curve == NULL)
		{
			SAFE_DELETE(animation);
			continue;
		}

		// Prepare the new curve and assign it to the animation clip.
		if (mainCurve == NULL) mainCurve = curve;
		clip->colladaClip->AddClipCurve(curve);
		curves.push_back(curve);
	}
}

// Import a given curve, attached to the correct clip/character/...
void DaeAnimClipLibrary::ImportClipCurve(FCDAnimationCurve* curve, MPlug& plug, MObject& animCurveNode)
{
	// Only 1 clip-curve pair is supported right now
	if (curve == NULL || curve->GetClipCount() != 1)
	{
		MGlobal::displayError(FC("Multiple animation clips on one COLLADA animation curve is not yet supported by ColladaMaya."));
		return;
	}
	FCDAnimationClip* colladaClip = curve->GetClip(0);

	MDGModifier dgMod;
	MStatus status;

	// Create the corresponding clip/character nodes, if neccessary
	DaeAnimationCharacter* character = GetCharacterNode(colladaClip, plug);
	MObject sourceClipNode = GetSourceClipNode(colladaClip, character, dgMod);
	MFnCharacter characterFn(character->characterNode, &status);
	status = dgMod.doIt();
	if (status != MStatus::kSuccess) MGlobal::displayError(status.errorString());

	status = characterFn.addCurveToClip(animCurveNode, sourceClipNode, plug, dgMod);
	if (status != MStatus::kSuccess) MGlobal::displayError(status.errorString());
	status = dgMod.doIt();
	if (status != MStatus::kSuccess) MGlobal::displayError(status.errorString());
}

// Retrieve/Create the character node, given a clip-plug pair
DaeAnimationCharacter* DaeAnimClipLibrary::GetCharacterNode(FCDAnimationClip* clip, const MPlug& plug)
{
	MStatus status;

	// Find the character to which this clip belongs, if already known
	for (DaeAnimationCharacterList::iterator it = characters.begin(); it != characters.end(); ++it)
	{
		DaeAnimationCharacter* character = *it;
		FCDAnimationClipList::iterator itC = character->clips.find(clip);
		if (itC == character->clips.end()) continue;

		// Verify that this plug already belongs to the character: otherwise, add it
		uint plugCount = character->plugs.length(), j;
		for (j = 0; j < plugCount; ++j)
		{
			if (character->plugs[j] == plug) break;
		}

		if (j == plugCount)
		{
			// Add this plug to the character
			MFnCharacter characterFn(character->characterNode);
			characterFn.addMember(plug);
			character->plugs.append(plug);
		}

		return character;
	}

	// Look for the given plug amongst the known character plugs
	for (DaeAnimationCharacterList::iterator itC = characters.begin(); itC != characters.end(); ++itC)
	{
		DaeAnimationCharacter* character = (*itC);
		uint plugCount = character->plugs.length();
		for (uint j = 0; j < plugCount; ++j)
		{
			if (character->plugs[j] == plug)
			{
				// This clip belongs with this character
				character->clips.push_back(clip);
				return character;
			}
		}
	}

	// Create a new character for this plug
	DaeAnimationCharacter* character = new DaeAnimationCharacter();
	characters.push_back(character);

	MSelectionList list;
	list.add(plug);
	MFnCharacter characterFn;
	MObject characterObject = characterFn.create(list, MFnSet::kNone, &status);
	if (status != MStatus::kSuccess) MGlobal::displayError(status.errorString());
	character->characterNode = characterObject;
	character->clips.push_back(clip);
	character->plugs.append(plug);

	// For instanced clips: start at the animation start time of the document
	character->startTime = CAnimationHelper::AnimationStartTime();

	return character;
}

MObject DaeAnimClipLibrary::GetSourceClipNode(FCDAnimationClip* clip, DaeAnimationCharacter* character, MDGModifier& dgMod)
{
	MStatus status;

	// Look for this clip within our object map
	ClipObjectMap::iterator it = importedClips.find(clip);
	if (it != importedClips.end()) return (*it).second;

	// Create this new source clip
	MFnClip clipFn;
	float offset = clip->GetStart();
	float duration = clip->GetEnd() - clip->GetStart();
	MObject sourceClipObject = clipFn.createSourceClip(MTime(offset, MTime::kSeconds), MTime(duration, MTime::kSeconds), dgMod, &status);
	if (status != MStatus::kSuccess) return MObject::kNullObj;

	// Instantiate this clip once in the character, at the next available position.
	MObject instanceClipObject = clipFn.createInstancedClip(sourceClipObject, character->startTime, dgMod);
	character->startTime += MTime(duration, MTime::kSeconds);

	// Attach the clip to the character
	MFnCharacter characterFn(character->characterNode, &status);
	characterFn.attachSourceToCharacter(sourceClipObject, dgMod);
	characterFn.attachInstanceToCharacter(instanceClipObject, dgMod);

	// Add this new clip to our object map
	importedClips[clip] = sourceClipObject;
	return sourceClipObject;
}
