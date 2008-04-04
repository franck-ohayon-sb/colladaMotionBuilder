/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_EMITTER_CLASSES__
#define __DAE_EMITTER_CLASSES__

class FCDEmitter;

#include "DaeEntity.h"

class DaeEmitter : public DaeEntity
{
public:
	FCDEmitter*			colladaEmitter;
	FCDEmitterInstance*	colladaInstance;

public:
	DaeEmitter(DaeDoc* doc, FCDEmitter* entity, const MObject& node);
	virtual ~DaeEmitter();
};

#endif
