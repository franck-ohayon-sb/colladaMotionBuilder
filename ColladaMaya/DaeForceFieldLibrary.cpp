/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/*
* COLLADA ForceField Library
*/

#include "StdAfx.h"
#include <maya/MSelectionList.h>
#include "CReferenceManager.h"
#include "DaeForceFieldLibrary.h"
#include "DaeDocNode.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAnimated.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDForceField.h"
#include "FCDocument/FCDForceDeflector.h"
#include "FCDocument/FCDForceDrag.h"
#include "FCDocument/FCDForceGravity.h"
#include "FCDocument/FCDForcePBomb.h"
#include "FCDocument/FCDForceWind.h"
#include "FCDocument/FCDPlaceHolder.h"
#include "FCDocument/FCDEntityReference.h"
#include "TranslatorHelpers/CDagHelper.h"

DaeForceFieldLibrary::DaeForceFieldLibrary(DaeDoc* doc)
:	DaeBaseLibrary(doc)
{
}


DaeForceFieldLibrary::~DaeForceFieldLibrary()
{
}

