/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/*
	Based off the a sample by Neil Hazzard and Nikolai Sander.
	Copyright (C) Autodesk 2000, All Rights Reserved.
*/

#include "StdAfx.h"
#include "BaseExporter.h"
#include "iskin.h"
#include "ColladaModifiers.h"

//
// ColladaModifier
//

Modifier* ColladaModifier::GetModifier()
{
	return object->GetModifier(modifierIndex);
}

// from SDK samples: modifiers\SkinWrap
void ColladaModifier::Resolve(Object *obj)
{
	if (obj == NULL)
	{
		return;
	}

	SClass_ID		sc;
	IDerivedObject* dobj;
	Object *currentObject = obj;

	int modIdx = 0;
	sc = obj->SuperClassID();
	if (sc == GEN_DERIVOB_CLASS_ID || sc == DERIVOB_CLASS_ID || sc == WSM_DERIVOB_CLASS_ID)
	{
		dobj = (IDerivedObject*)obj;
		while (sc == GEN_DERIVOB_CLASS_ID || sc == DERIVOB_CLASS_ID || sc == WSM_DERIVOB_CLASS_ID)
		{
			for (int j = 0; j < dobj->NumModifiers(); j++, modIdx++)
			{
				Modifier* mod = dobj->GetModifier(j);
				if (this->Matches(mod))
				{
					object = dobj;
					modifierIndex = j;
					modifierStackIndex = modIdx;
					resolved = true;
					return;
				}
			}
			dobj = (IDerivedObject*) dobj->GetObjRef();
			currentObject = (Object*) dobj;
			sc = dobj->SuperClassID();
		}
	}

	int bct = currentObject->NumPipeBranches(FALSE);
	if (bct > 0)
	{
		for (int bi = 0; bi < bct; bi++)
		{
			Object* bobj = currentObject->GetPipeBranch(bi,FALSE);
			Resolve(bobj);
			return;
		}
	}
}

int ColladaSkin::Matches(Modifier* modifier)
{
	Class_ID cid = modifier->ClassID();
	if (cid == SKIN_CLASSID)
	{
		ISkin* skin = (ISkin*) modifier->GetInterface(I_SKIN);
		if (skin == NULL) return false;
		return skin->GetNumBonesFlat() > 0;
	}
	else return cid == PHYSIQUE_CLASSID;
}

///////////////////////////////////////////////////////////////////////////////
// ColladaMorph
///////////////////////////////////////////////////////////////////////////////

int ColladaMorph::GetNumActiveTargets()
{
	MorphR3* morph = GetInterface();
	if (morph == NULL)
		return 0;

	int retval = 0;
	size_t chanCount = morph->chanBank.size();
	for (size_t i = 0; i < chanCount; i++)
	{
		if (morph->chanBank[i].mActive)
			retval ++;
	}

	return retval;
}


///////////////////////////////////////////////////////////////////////////////
// ColladaSkin
///////////////////////////////////////////////////////////////////////////////

ColladaSkinInterface *ColladaSkin::GetInterface(INode *node)
{
	if (resolved)
	{
		Modifier *m = GetModifier();
		if (m->ClassID() == SKIN_CLASSID)
			return (ColladaSkinInterface*)new ISkinModInterface(m, node);
		if (m->ClassID() == PHYSIQUE_CLASSID)
			return (ColladaSkinInterface*)new IPhysiqueModInterface(m, node);
	}
	return NULL;
}


Object* ColladaModifier::GetInitialPose()
{
	Object* initialPose = NULL;
	if (resolved)
	{
		// Remember that 3dsMax has the modifier stack reversed
		// So that evaluating the zero'th modifier implies evaluating the whole modifier stack.
		int modifierCount = object->NumModifiers();
		if (modifierIndex < modifierCount - 1)
		{
			ObjectState state = object->Eval(TIME_INITIAL_POSE, modifierIndex + 1);
			initialPose = state.obj;
		}
		else
		{
			initialPose = object->GetObjRef();
		}
	}
	return initialPose;
}


///////////////////////////////////////////////////////////////////////////////
// ISkinModInterface
///////////////////////////////////////////////////////////////////////////////

// ONLY CONSTRUCTOR: Assumes we have been called with correct
// modifier
ISkinModInterface::ISkinModInterface(Modifier *m, INode *node)
{
	assert(m != NULL);
	this->m = m;
	this->node = node;
	this->modInterface = (ISkin*)m->GetInterface(I_SKIN);
	assert(modInterface != NULL);
	this->context = modInterface->GetContextInterface(node);
	assert(context != NULL);
}

// Release our interface on death
ISkinModInterface::~ISkinModInterface()
{
	// Check to make sure we are released
	// If we are not, someone has forgotten
	// about us!
	assert(modInterface == NULL);
}

void ISkinModInterface::ReleaseMe()
{
	m->ReleaseInterface(I_SKIN, (void*)modInterface);
	modInterface = NULL;
	delete this;
}

bool ISkinModInterface::GetSkinInitTM(Matrix3 &initTM, bool bObjOffset)
{
	return modInterface->GetSkinInitTM(node, initTM, bObjOffset) == SKIN_OK;
}

int ISkinModInterface::GetNumBones()
{
	return modInterface->GetNumBones();
}

INode *ISkinModInterface::GetBone(int i)
{
	return modInterface->GetBone(i);
}

bool ISkinModInterface::GetBoneInitTM(INode *node, Matrix3 &boneInitTM)
{
	return modInterface->GetBoneInitTM(node, boneInitTM) == SKIN_OK;
}

int ISkinModInterface::GetNumVertices()
{
	return context->GetNumPoints();
}
	
int ISkinModInterface::GetNumAssignedBones(int i)
{
	return context->GetNumAssignedBones(i);
}

float ISkinModInterface::GetBoneWeight(int vertexIdx, int boneIdx)
{
	return context->GetBoneWeight(vertexIdx, boneIdx);
}

int ISkinModInterface::GetAssignedBone(int vertexIdx, int boneIdx)
{
	return context->GetAssignedBone(vertexIdx, boneIdx);
}



///////////////////////////////////////////////////////////////////////////////
// IPhysiqueModInterface
///////////////////////////////////////////////////////////////////////////////

// ONLY CONSTRUCTOR: Assumes we have been called with correct
// modifier
IPhysiqueModInterface::IPhysiqueModInterface(Modifier *m, INode *node)
{
	numBones = 0;
	assert(m != NULL);
	this->m = m;
	this->node = node;
	this->modInterface = (IPhysiqueExport*)m->GetInterface(I_PHYINTERFACE);
	assert(modInterface != NULL);

	this->context = modInterface->GetContextInterface(node);
	assert(context != NULL);

	// Now, Physique doesnt provide a way for us to list
	// all our bones nicely, so try and find all our info here.

	//convert to rigid for time independent vertex assignment
	context->ConvertToRigid(TRUE);

	//allow blending to export multi-link assignments
	context->AllowBlending(true);

	// Attempt to find all Physique bone nodes by using its MATRIX_RETURNED
	// inherent hierarchical nature - ie find the root
	// bone, by definition all children are physique nodes too

	if (context->GetNumberVertices() > 0)
	{
		Matrix3 ignored;
		INode *nextNode, *rootNode = NULL;
		IPhyVertexExport *vtxExport = context->GetVertexInterface(0);
		if (vtxExport)
        {
	        //need to check if vertex has blending
	        if (vtxExport->GetVertexType() & BLENDED_TYPE)
		    {
				IPhyBlendedRigidVertex *vtxBlend = (IPhyBlendedRigidVertex *)vtxExport;
				rootNode = vtxBlend->GetNode(0);
			}
			else
			{
				IPhyRigidVertex *vtxNoBlend = (IPhyRigidVertex *)vtxExport;
            	rootNode = vtxNoBlend->GetNode();
			}
			context->ReleaseVertexInterface(vtxExport);
		}

		if (rootNode != NULL)
		{
			nextNode = rootNode->GetParentNode();
			while (nextNode != NULL && !nextNode->IsRootNode())
			{
				if (modInterface->GetInitNodeTM(nextNode, ignored) == NODE_NOT_FOUND)
					break;

				rootNode = nextNode;
				nextNode = rootNode->GetParentNode();
			}

			// now we have a root node, turn around and add all children to our nodeList.
			//bonesMap[rootNode] = 1;
			FillBoneMap(rootNode);
		}
	}

	size_t blsize = bonesList.size();
	size_t bmsize = bonesMap.size();
	size_t nbsize = (size_t)numBones;
	FUAssert(blsize == bmsize && nbsize == bmsize,);

	/*
	for (int i = 0; i < context->GetNumberVertices(); i++)
	{
		IPhyVertexExport *vtxExport = context->GetVertexInterface(i);
		if (vtxExport)
        {
	        //need to check if vertex has blending
	        if (vtxExport->GetVertexType() & BLENDED_TYPE)
		    {
				IPhyBlendedRigidVertex *vtxBlend = (IPhyBlendedRigidVertex *)vtxExport;

				for (int n = 0; n < vtxBlend->GetNumberNodes(); n++)
				{
					INode *bone = vtxBlend->GetNode(n);
					// bone not found in our list
					if (GetBonesListIdx(bone) < 0)
					{
						bonesList.push_back(bone);
						bonesMap[bone] = numBones++;
					}
				}
			}
			else
			{
				IPhyRigidVertex *vtxNoBlend = (IPhyRigidVertex *)vtxExport;
            	INode *bone = vtxNoBlend->GetNode();
				// bone not found in our list
				if (GetBonesListIdx(bone) < 0)
				{
					bonesList.push_back(bone);
					bonesMap[bone] = numBones++;
				}
            }
			context->ReleaseVertexInterface(vtxExport);
		}
	}
	*/
}

// Release our interfaces on death
IPhysiqueModInterface::~IPhysiqueModInterface()
{
	// Are we all released?
	assert(context == NULL);
	assert(modInterface == NULL);
}

void IPhysiqueModInterface::ReleaseMe()
{
	modInterface->ReleaseContextInterface(context);
	context = NULL;
	m->ReleaseInterface(I_PHYINTERFACE, modInterface);
	modInterface = NULL;
	// when we are all released, delete ourselves
	delete this;
}

bool IPhysiqueModInterface::GetSkinInitTM(Matrix3 &initTM, bool bObjOffset)
{
	// We should really allow some errors.
	bObjOffset = bObjOffset;
	return modInterface->GetInitNodeTM(node, initTM) == MATRIX_RETURNED;
}

int IPhysiqueModInterface::GetNumBones()
{
	//return bonesList.Count();
	return numBones;
}

INode *IPhysiqueModInterface::GetBone(int i)
{
	return bonesList[i];
}

bool IPhysiqueModInterface::GetBoneInitTM(INode *node, Matrix3 &boneInitTM)
{
	return modInterface->GetInitNodeTM(node, boneInitTM) == MATRIX_RETURNED;
}

int IPhysiqueModInterface::GetNumVertices()
{
	return context->GetNumberVertices();
}
	
int IPhysiqueModInterface::GetNumAssignedBones(int i)
{
	int retval = 0;

	IPhyVertexExport *vtxExport = context->GetVertexInterface(i);
	if (vtxExport)
	{
		if (vtxExport->GetVertexType() & BLENDED_TYPE)
		{
			IPhyBlendedRigidVertex *vtxBlend = (IPhyBlendedRigidVertex *)vtxExport;
			retval = vtxBlend->GetNumberNodes();
		}
		else
		{
			retval = 1;
		}
		context->ReleaseVertexInterface(vtxExport);
	}
	return retval;
}

float IPhysiqueModInterface::GetBoneWeight(int vertexIdx, int boneIdx)
{
	float retval = 0.0f;

	IPhyVertexExport *vtxExport = context->GetVertexInterface(vertexIdx);
	if (vtxExport)
	{
		if (vtxExport->GetVertexType() & BLENDED_TYPE)
		{
			IPhyBlendedRigidVertex *vtxBlend = (IPhyBlendedRigidVertex *)vtxExport;
			retval = vtxBlend->GetWeight(boneIdx);
		}
		else
		{
			if (boneIdx == 0) retval = 1.0f;
		}
		context->ReleaseVertexInterface(vtxExport);
	}
	return retval;
}

int IPhysiqueModInterface::GetAssignedBone(int vertexIdx, int boneIdx)
{
	int retval = -1;
	IPhyVertexExport *vtxExport = context->GetVertexInterface(vertexIdx);
	if (vtxExport)
	{
		if (vtxExport->GetVertexType() & BLENDED_TYPE)
		{
			IPhyBlendedRigidVertex *vtxBlend = (IPhyBlendedRigidVertex *)vtxExport;
			retval = GetBonesListIdx(vtxBlend->GetNode(boneIdx));
		}
		else
		{
			IPhyRigidVertex *vtxBlend = (IPhyRigidVertex *)vtxExport;
			if (boneIdx == 0) retval = GetBonesListIdx(vtxBlend->GetNode());
		}
		context->ReleaseVertexInterface(vtxExport);
	}
	assert(retval >= 0);
	return retval;
}

/** private methods **/

int IPhysiqueModInterface::GetBonesListIdx(INode *node)
{
	fm::map<INode*, int>::iterator itr = bonesMap.find(node);
	if (itr != bonesMap.end())
		return (*itr).second;
	return -1;
/*	for (int j = 0; j < bonesList.Count(); j++)
	{
		if (bonesList[j] == node)
			return j;
	}
	return -1; */
}

void IPhysiqueModInterface::FillBoneMap(INode* parentNode)
{
	if (parentNode == NULL)
		return;

	//If the bone is a biped bone, scale needs to be
	//restored before exporting skin data 
	ScaleBiped(parentNode, 0);

	bonesMap[parentNode] = (int)numBones++;
	bonesList.push_back(parentNode);

	int numChildren = parentNode->NumberOfChildren();
	for (int i = 0; i < numChildren; i++)
	{
		FillBoneMap(parentNode->GetChildNode(i));
	}
}

// Taken from PhyExportSample
//
// This function can be used to set the non-uniform scale of a biped.
// The node argument should be a biped node.
// If the scale argument is non-zero the non-uniform scale will be removed from the biped.
// Remove the non-uniform scale before exporting biped nodes and animation data
// If the scale argument is zero the non-uniform scaling will be reapplied to the biped.
// Add the non-uniform scaling back on the biped before exporting skin data
//***********************************************************************************
void IPhysiqueModInterface::ScaleBiped(INode* node, int scale)
{
	if (node->IsRootNode()) return;

	// Use the class ID to check to see if we have a biped node
	Control* c = node->GetTMController();
    if ((c->ClassID() == BIPSLAVE_CONTROL_CLASS_ID) ||
         (c->ClassID() == BIPBODY_CONTROL_CLASS_ID) ||
         (c->ClassID() == FOOTPRINT_CLASS_ID))
    {

        // Get the Biped Export Interface from the controller 
        IBipedExport *BipIface = (IBipedExport *) c->GetInterface(I_BIPINTERFACE);

        // Either remove the non-uniform scale from the biped, 
		// or add it back in depending on the boolean scale value
        BipIface->RemoveNonUniformScale(scale);
		Control* iMaster = (Control *) c->GetInterface(I_MASTER);
		iMaster->NotifyDependents(FOREVER, PART_TM, REFMSG_CHANGE);
		
		// Release the interfaces
		c->ReleaseInterface(I_MASTER,iMaster);
		c->ReleaseInterface(I_BIPINTERFACE,BipIface);
	}
}
