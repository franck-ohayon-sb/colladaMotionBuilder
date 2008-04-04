/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_BASE_LIBRARY_INCLUDED__
#define __DAE_BASE_LIBRARY_INCLUDED__

class DaeDoc;

class DaeBaseLibrary
{
public:
	DaeBaseLibrary(DaeDoc* pDoc);
	virtual ~DaeBaseLibrary();

protected:
	DaeDoc*	doc;
};

#endif // __DAE_BASE_LIBRARY_INCLUDED__
