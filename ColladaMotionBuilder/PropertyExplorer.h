/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _MB_PROPERTY_EXPLORER_H_
#define _MB_PROPERTY_EXPLORER_H_

class PropertyExplorer
{
public:
	PropertyExplorer(const fchar* filename, FBComponent* toExplore);
	~PropertyExplorer() {}
};

#endif // _MB_PROPERTY_EXPLORER_H_