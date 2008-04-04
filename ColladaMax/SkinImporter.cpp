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

// Skin Controller Importer Class

#include "StdAfx.h"
#include "ISkin.h"
#include "modstack.h"
#include "DocumentImporter.h"
#include "EntityImporterFactory.h"
#include "GeometryImporter.h"
#include "NodeImporter.h"
#include "SkinImporter.h"
#include "TransformImporter.h"
#include "MorpherImporter.h"
#include "ColladaModifiers.h"
#include "FCDocument/FCDSkinController.h"
#include "FCDocument/FCDControllerInstance.h"
#include "FCDocument/FCDSceneNode.h"

#include "iInstanceMgr.h"

SkinImporter::SkinImporter(DocumentImporter* document, NodeImporter* parent)
:	EntityImporter(document, parent)
{
	target = NULL;
	colladaSkin = NULL;
	derivedObject = NULL;
	skinModifier = NULL;
}

SkinImporter::~SkinImporter()
{
	SAFE_DELETE(target);

	colladaSkin = NULL;
	derivedObject = NULL;
	skinModifier = NULL;
}

// Retrieve the object to reference for this entity
Object* SkinImporter::GetObject()
{
	return derivedObject;
}

void SkinImporter::AddParent(NodeImporter* parent)
{
	EntityImporter::AddParent(parent);
	if (target != NULL) target->AddParent(parent);
}

// Main entry point for the class.
// Create a Max mesh-derived object, with the skin modifier from a COLLADA skin controller.
Object* SkinImporter::ImportSkinController(FCDSkinController* _colladaSkin)
{
	colladaSkin = _colladaSkin;

	// Import the COLLADA target for this skin
	FCDEntity* colladaTarget = colladaSkin->GetTarget();
	if (colladaTarget == NULL) return NULL;
	target = EntityImporterFactory::Create(colladaTarget, GetDocument(), GetParent());
	if (target == NULL) return NULL;

	// Create the skin modifier
	skinModifier = (Modifier*) GetDocument()->MaxCreate(OSM_CLASS_ID, SKIN_CLASSID);
	if (skinModifier == NULL) return NULL;

	// Create the derived object for the target and the modifier
	Object* object = target->GetObject();
	if (object->ClassID() == derivObjClassID || object->ClassID() == WSMDerivObjClassID)
	{
		// Object is a derived object, just attach ourselves to it
		derivedObject = (IDerivedObject*) object;
	}
	else
	{
		// Create the derived object for the target and the modifier
		derivedObject = CreateDerivedObject(object);
	}
	derivedObject->AddModifier(skinModifier);
	return derivedObject;
}

typedef fm::pvector<INode> INodeList;

bool SkinImporter::SetTargetCurrentImport(INode* currImport)
{
	if (target == NULL) return false;
	if (target->GetType() == EntityImporter::GEOMETRY)
	{
		GeometryImporter *geom = (GeometryImporter*)target;
		geom->SetCurrentImport(currImport);
		return true;
	}
	else if (target->GetType() == EntityImporter::MORPHER)
	{
		MorpherImporter *morph = (MorpherImporter*)target;
		morph->SetTargetCurrentImport(currImport);
		return true;
	}

	return false;
}

// Local function, tests similarity on bones for FCD instance and modifier
bool SkinImporter::BonesAreCorrect(FCDControllerInstance* inst, Modifier* mod)
{
	if (!inst || !mod) return false;

	ISkin* iskin = (ISkin*) mod->GetInterface(I_SKIN);
	
	if (!iskin) return false;

	size_t numBones = inst->GetJointCount();
	if ((size_t)iskin->GetNumBones() != numBones) return false;

	bool ret = true;
	for (int i = 0; i < (int) numBones; ++i)
	{
		FCDSceneNode* joint = inst->GetJoint(i);
		if (joint == NULL) continue;
		NodeImporter* importer = (NodeImporter*)GetDocument()->FindInstance(joint->GetDaeId());
		if (importer == NULL || importer->GetType() != EntityImporter::NODE) break;
		INode* aBone = importer->GetImportNode()->GetINode();

		if (iskin->GetBoneFlat(i) != aBone) 
		{
			ret = false;
			break;
		}
	}

	mod->ReleaseInterface(I_SKIN2, (void*)iskin);
	return ret;
}

// Find the derived object which parents the obj, or null if obj is base.
IDerivedObject* GetObjHierParent(INode* node, IDerivedObject* obj)
{
	Object *currentObject = node->GetObjectRef();
	IDerivedObject *pobj = NULL;
	while (currentObject && currentObject != obj)
	{
		SClass_ID sc = currentObject->SuperClassID();
		if (sc == GEN_DERIVOB_CLASS_ID || sc == DERIVOB_CLASS_ID || sc == WSM_DERIVOB_CLASS_ID)
		{
			pobj = (IDerivedObject*)currentObject;
			currentObject = (Object*) pobj->GetObjRef();
		}
	}
	return pobj;
}

// Make the given obj unique on the node.
IDerivedObject* MakeObjUnique(INode* node, IDerivedObject* obj)
{
	if (obj == NULL) return NULL;

	IDerivedObject *pobj = GetObjHierParent(node, obj);

	RefTargetHandle newObj = obj->Clone();
	if (pobj)
	{
		int refIdx = pobj->FindRef((RefTargetHandle)obj);
		pobj->ReplaceReference(refIdx, newObj, TRUE);
	}
	else
	{
		// It is root object.
		node->SetObjectRef((Object*)newObj);
	}
	return (IDerivedObject*)newObj;
}

//  Maybe uniqizize an instance, if this entityInstance lists different
//  bones than the ones used so far.
bool SkinImporter::MaybeRebuild(FCDEntityInstance* instance)
{
	FCDControllerInstance* inst = (FCDControllerInstance*)instance;

	FCDSceneNode* fcdNode = instance->GetParent();
	NodeImporter* importer = (NodeImporter*) GetDocument()->FindInstance(fcdNode->GetDaeId());
	if (importer == NULL && fcdNode->GetParent(0) != NULL) importer = (NodeImporter*) GetDocument()->FindInstance(fcdNode->GetParent(0)->GetDaeId());
	if (importer == NULL || importer->GetType() != EntityImporter::NODE) return false;
	INode* skinnedNode = importer->GetImportNode()->GetINode();

	static INode* damn = skinnedNode;

	if (skinnedNode)
	{
		ColladaSkin origSkin; origSkin.Resolve(skinnedNode->GetObjectRef());
		if (BonesAreCorrect(inst, origSkin.GetModifier()))
			return false;

		// Assume parents are the equivalents of nodes in scene?
		int pCount = GetParentCount();


		for (int i = 0; i < pCount; i++)
		{
			INode *possNode = GetParent(i)->GetImportNode()->GetINode();
			// No need to test our own bone again
			if (possNode == skinnedNode)
				continue;

			ColladaSkin skin; skin.Resolve(possNode->GetObjectRef());

			if (BonesAreCorrect(inst, skin.GetModifier()))
			{
				IDerivedObject *newObj = skin.GetDerivedObject();
				IDerivedObject *oldObj = origSkin.GetDerivedObject();
				IDerivedObject *pobj = GetObjHierParent(skinnedNode, oldObj);
				if (pobj)
				{
					// This object has the correct instance, lets steal it.
					int refIdx = pobj->FindRef((RefTargetHandle)oldObj);
					pobj->ReplaceReference(refIdx, newObj);
				}
				else
					skinnedNode->SetObjectRef(newObj);

				// The following prob doesnt work, but try if things go wrong (with mixing morph and skin)
				//origSkin.GetDerivedObject()->SetModifier(origSkin.GetModifierIndex(), skin.GetModifier());
				return false;
			}
		}


		// If we reach here, we need to reset our modifier
		Modifier *newSkin = (Modifier*) GetDocument()->MaxCreate(OSM_CLASS_ID, SKIN_CLASSID);
		if (newSkin == NULL) return false;

		IDerivedObject *newObj = MakeObjUnique(skinnedNode, origSkin.GetDerivedObject());
		// New obj is clone of old, should be same index for skin we want to replace.
		newObj->SetModifier(origSkin.GetModifierIndex(), newSkin);

		skinModifier = newSkin;
		return true;
	}
	return false;
}

// Finalize the import of the skin modifier
void SkinImporter::Finalize(FCDEntityInstance* instance)
{
	// Finalize the target controller/geometry
	if (target != NULL) target->Finalize(instance);
	if (IsFinalized())
	{
		// Maybe we will want to build a new instance,
		// if our instance carries unique info.
		if (!MaybeRebuild(instance))
			return;
	}

	// Force onto the parent nodes' transformation the bind-shape matrix.
	fm::vector<Matrix3> nodeTMs;
	Matrix3 bindShapeTM = ToMatrix3(colladaSkin->GetBindShapeTransform());
	int parentCount = GetParentCount();

	for (int p = 0; p < parentCount; ++p)
	{
		// TODO: Need to add some sort of sorting to be able to retrieve the parent with the identity transform.
		INode* parentNode = GetParent(p)->GetImportNode()->GetINode();
		Matrix3 overwrittenTM;
		if (GetParent(p)->HasPivot())
		{
			// This controller has been instantiated, perhaps wrongly, as a pivot node.
			// So we want to overwrite the pivot TM.
			overwrittenTM = parentNode->GetObjectTM(TIME_INITIAL_POSE);
			Matrix3 nodeTM = parentNode->GetNodeTM(TIME_INITIAL_POSE);
			bindShapeTM *= nodeTM;
			nodeTM.Invert();
			overwrittenTM *= nodeTM;
			TransformImporter::ImportPivot(parentNode, bindShapeTM);
		}
		else
		{
			overwrittenTM = parentNode->GetNodeTM(TIME_INITIAL_POSE);
			parentNode->SetNodeTM(TIME_INITIAL_POSE, bindShapeTM);
		}
		nodeTMs.push_back(overwrittenTM);
	}

	// Get the skin's import interface
	ISkinImportData* importSkin = (ISkinImportData*) skinModifier->GetInterface(I_SKINIMPORTDATA);
	if (importSkin == NULL) return;

	FCDSceneNode* fcdNode = instance->GetParent();
	NodeImporter* importer = (NodeImporter*) GetDocument()->FindInstance(fcdNode->GetDaeId());
	if (importer == NULL && fcdNode->GetParent(0) != NULL) importer = (NodeImporter*) GetDocument()->FindInstance(fcdNode->GetParent(0)->GetDaeId());
	if (importer == NULL || importer->GetType() != EntityImporter::NODE)
		return;
	INode* skinNode = importer->GetImportNode()->GetINode();

	// BUG. This call may cause a crash if the EPoly.dlo - MNMath.dll when using an Editable Poly mesh with a Skin Modifier
	skinNode->EvalWorldState(0);

	// Set the bind shape's initial TM and apply it to the current transform
	// in order to recreate the correct environment
	importSkin->SetSkinTm(skinNode, bindShapeTM, bindShapeTM);

	// Assign to the skin the bones and their initial TM matrices.
	//FCDJointList& joints = colladaSkin->GetJoints();
	FCDControllerInstance *ctrlInstance = (FCDControllerInstance*)instance;
	size_t boneCount = ctrlInstance->GetJointCount();

	INodeList bones; bones.resize(boneCount);
	for (size_t i = 0; i < boneCount; ++i)
	{
		FCDSceneNode* joint = ctrlInstance->GetJoint(i);
		if (joint == NULL) continue;
		
		NodeImporter* importer = (NodeImporter*) GetDocument()->FindInstance(joint->GetDaeId());
		if (importer == NULL || importer->GetType() != EntityImporter::NODE) return;
		bones[i] = importer->GetImportNode()->GetINode();
		if (importSkin->AddBoneEx(bones[i], TRUE))
		{
			// Record the bone's INode and set the correct initial transform matrix
			Matrix3 bindPose = ToMatrix3(colladaSkin->GetJoint(i)->GetBindPoseInverse());
			bindPose.Invert();
			importSkin->SetBoneTm(bones[i], bindPose, bindPose);
		}
	}

	// As noted in the Sparks knowledgebase: "Problem importing weights into Physique modifier"
	// For some reason the node must be evaluated after all the bones are added,
	// in order to be able to add the custom weights using the AddWeights function.
	skinNode->EvalWorldState(0);

	// Pre-allocate these guys to avoid many memory allocation/re-allocation.
	Tab<INode*> vertexBones;
	Tab<float> vertexWeights; 

	// Assign to the skin the vertex joint-weight pairs
	const FCDSkinControllerVertex* influences = colladaSkin->GetVertexInfluences();
	size_t vertexCount = colladaSkin->GetInfluenceCount();
	for (size_t i = 0; i < vertexCount; ++i)
	{
		const FCDSkinControllerVertex& influence = influences[i];
		size_t pairCount = influence.GetPairCount();
		if (pairCount == 0) continue;

		// Massage the joint/weight pairs in the format that 3dsMax accepts
		vertexBones.SetCount((int) pairCount);
		vertexWeights.SetCount((int) pairCount);
		for (size_t j = 0; j < pairCount; ++j)
		{
			const FCDJointWeightPair* pair = influence.GetPair(j);
			if (pair->jointIndex >= 0)
			{
				FUAssert(pair->jointIndex < (int) boneCount, continue);
				vertexBones[j] = bones[pair->jointIndex];
				vertexWeights[j] = pair->weight;
			}
			else
			{
				vertexBones[j] = NULL;
				vertexWeights[j] = pair->weight;
			}
		}

		// Enforce the weights for this vertex in the skin modifier
		importSkin->AddWeights(skinNode, (int) i, vertexBones, vertexWeights);
	}

	// [glaforte 16-06-2006]
	// From Josée Carrier of the SPARKS SDK support:
	// There is a known issue in 3dsMax 8 with the UI. Force a refresh by modifying a param block element.
	//-
	// skin_advance = 2: from enum in maxsdk\samples\modifiers\bonesdef\BonesDef_Constants.h
	int skin_advance = 2;
	IParamBlock2* pblock_advance = skinModifier->GetParamBlockByID(skin_advance);
	// skin_advance_bonelimit=7: from enum in maxsdk\samples\modifiers\bonesdef\BonesDef_Constants.h
	int skin_advance_bonelimit = 7;
	// Get and set the parameter value to force a refresh
	int tmpBoneLimit = 20;
	pblock_advance->GetValue(skin_advance_bonelimit, 0, tmpBoneLimit, FOREVER);
	pblock_advance->SetValue(skin_advance_bonelimit, 0, tmpBoneLimit);
	//-

	// Restore the original node transform or object pivot transform
	for (int p = 0; p < parentCount; ++p)
	{
		INode* parentNode = GetParent(p)->GetImportNode()->GetINode();
		Matrix3& overwrittenTM = nodeTMs[p];
		if (GetParent(p)->HasPivot())
		{
			// This controller has been instantiated, perhaps wrongly, as a pivot node.
			// So we want to overwrite the pivot TM.
			TransformImporter::ImportPivot(parentNode, overwrittenTM);
		}
		else
		{
			parentNode->SetNodeTM(TIME_INITIAL_POSE, overwrittenTM);
		}
	}
	EntityImporter::Finalize(instance);
}
