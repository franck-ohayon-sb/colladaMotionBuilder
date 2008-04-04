/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_COLLADAFXBEHAVIOR_INCLUDED__
#define __DAE_COLLADAFXBEHAVIOR_INCLUDED__

#ifndef _MPxDragAndDropBehavior
#include <maya/MPxDragAndDropBehavior.h>
#endif // _MPxDragAndDropBehavior

class CFXBehavior : public MPxDragAndDropBehavior
{

	public:

	CFXBehavior();
	virtual ~CFXBehavior();

	static void *creator();

	virtual bool shouldBeUsedFor(	MObject &sourceNode, 
									MObject &destinationNode,
									MPlug &sourcePlug, 
									MPlug &destinationPlug);

	virtual MStatus connectNodeToAttr(MObject &sourceNode,
									  MPlug &destinationPlug, bool force);

	protected:


	private:
};

#endif
