/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_EMITTER_LIBRARY_INCLUDED__
#define __DAE_EMITTER_LIBRARY_INCLUDED__

#ifndef __DAE_BASE_LIBRARY_INCLUDED__
#include "DaeBaseLibrary.h"
#endif // __DAE_BASE_LIBRARY_INCLUDED__

class DaeDoc;
class FCDEmitter;
class FCDEmitterInstance;
class FCDParticlePCloud;

class DaeEmitterLibrary : public DaeBaseLibrary
{
public:
	DaeEmitterLibrary(DaeDoc* doc);
	~DaeEmitterLibrary();

};

#endif // __DAE_EMITTER_LIBRARY_INCLUDED__
