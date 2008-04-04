/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_GEOMETRY_CLASSES__
#define __DAE_GEOMETRY_CLASSES__

class FCDGeometry;

#include "DaeEntity.h"

class DaeGeometry : public DaeEntity
{
public:
	FCDGeometry*			colladaGeometry;
	FCDGeometryInstance*	colladaInstance;

	MPointArray				points;
	MIntArray				faceVertexCounts;
	MIntArray				faceConnects;
	Int32List				textureSetIndices;

	// used for compatibility issues between different representations
	MObjectArray			children;

public:
	DaeGeometry(DaeDoc* doc, FCDGeometry* entity, const MObject& node);
	virtual ~DaeGeometry();

	// override
	virtual void Instantiate(DaeEntity *sceneNode);
};

#endif
