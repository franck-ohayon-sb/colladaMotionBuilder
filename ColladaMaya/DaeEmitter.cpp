/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "DaeEmitter.h"
#include "TranslatorHelpers/CDagHelper.h"
#include "FCDocument/FCDEmitter.h"

//
// DaeEmitter
//

DaeEmitter::DaeEmitter(DaeDoc* doc, FCDEmitter* entity, const MObject& node)
:	DaeEntity(doc, entity, node)
,	colladaEmitter(entity), colladaInstance(NULL)
{
}

DaeEmitter::~DaeEmitter()
{
	colladaEmitter = NULL;
	colladaInstance = NULL;
}
