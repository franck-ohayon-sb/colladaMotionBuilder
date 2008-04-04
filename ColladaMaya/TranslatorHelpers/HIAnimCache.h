/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/*
* Thanks to JHoerner and KThacker for their contributions
*/

#ifndef __HIANIMCACHE_H__
#define __HIANIMCACHE_H__

#include <libxml/parser.h>
#include <maya/MObject.h>
#include <maya/MDagPath.h>
#include <maya/MPlug.h>
#include <maya/MTime.h>
#include <maya/MFloatArray.h>
#include "CAnimationHelper.h"
#include <vector>

class CAnimCache
{
public:
	CAnimCache(DaeDoc* doc);

	virtual ~CAnimCache()
	{
		nodeSearch = NULL;
		nodes.clear();
	}

	void CachePlug(const MPlug& plug, bool isMatrix);
	bool MarkPlugWanted(const MPlug& plug);
	bool FindCacheNode(const MObject& node);
	bool FindCachePlug(const MPlug& plug, FloatList*& inputs, FloatList*& outputs);
	bool FindCachePlug(const MPlug& plug, bool& isAnimated);

	void SampleExpression(const MObject& object);
	void SampleConstraint(const MDagPath& dagPath);
	void SampleIKHandle(const MDagPath& dagPath);

	// Sample all the cached plugs
	void SamplePlugs();

private:
	void CacheTransformNode(const MObject& node);

private:
	struct CacheNode
	{
		struct Part
		{
			MPlug plug;
			FloatList values;
			bool isMatrix, isWanted, isAnimated;

			Part() : isMatrix(false), isWanted(false), isAnimated(false) {}
			Part(const MPlug& plug) : plug(plug), isMatrix(false), isWanted(false), isAnimated(false) {}
		};

		MObject node;
		fm::vector<Part> parts;
		CacheNode(const MObject& node) : node(node) {}

		CacheNode& operator=(const CacheNode &a)
		{
			node = a.node;
			parts = a.parts;
			return *this;
		}
	};

	typedef fm::map<MObject, CacheNode*> CacheNodeMap;
	typedef fm::vector<CacheNode::Part> CachePartList;

	// list of nodes that require sampling
	CacheNodeMap nodes;
	CacheNode* nodeSearch;

	DaeDoc* doc;
};

#endif  // __HIANIMCACHE_H__
