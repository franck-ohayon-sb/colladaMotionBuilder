/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COLLADA_ID_CREATOR_H_
#define _COLLADA_ID_CREATOR_H_

namespace ColladaID
{
	/** Create a COLLADA ID string from a Max Node.
		@param n The Node.
		@param useXRefName In the event of the node referencing an XRef object,
			specifies wether we use the XRef object's name or not for the ID.
		@return The ID string.*/
	fstring Create(INode *n, bool useXRefName = false);

	/** Create a COLLADA ID string from a Max Material.
		@param maxMtl The Material.
		@param resolveXRef In the event of the Material being an XRef, specifies
			wether we resolve it first to get its name or not.
		@return The ID string.*/
	fstring Create(Mtl *maxMtl, bool resolveXRef = true);
};

#endif // _COLLADA_ID_CREATOR_H_
