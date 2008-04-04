/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "ColladaController.h"

//
// ColladaControllerClassDesc
//

Class_ID COLLADA_CONTROLLER_CLASS_ID(0xA7471BC1, 0xB811DF9F);

class ColladaControllerClassDesc : public ClassDesc
{
public:
	int 			IsPublic() { return 1; }
	void *			Create(BOOL loading) { loading; return new ColladaController(); }
	const TCHAR *	ClassName() { return "ColladaController" ; }
	SClass_ID		SuperClassID() { return CTRL_INTEGER_CLASS_ID; }
	Class_ID		ClassID() { return COLLADA_CONTROLLER_CLASS_ID; }
	const TCHAR* 	Category() { return _T("");  }
};

static ColladaControllerClassDesc controllerClassDesc;
ClassDesc* GetColladaControllerClassDesc() {return &controllerClassDesc;}

//
// ColladaController
//

ColladaController::ColladaController()
{
	// Default infinity types: constant.
	SetORT(ORT_CONSTANT,ORT_BEFORE);
	SetORT(ORT_CONSTANT,ORT_AFTER);
}

ColladaController::~ColladaController()
{
}

SClass_ID ColladaController::SuperClassID()
{
	return CTRL_INTEGER_CLASS_ID;
}

Class_ID ColladaController::ClassID()
{
	return COLLADA_CONTROLLER_CLASS_ID;
}

void ColladaController::DeleteThis()
{
	delete this;
}

void ColladaController::GetValue(TimeValue t, void *val, Interval &valid, GetSetMethod method)
{
	valid.SetInfinite();
	Matrix3* v = (Matrix3*) val;
	*v = Matrix3(1);

	t; method;
}

void ColladaController::SetValue(TimeValue t, void *val, int commit, GetSetMethod method)
{
	t; val; commit; method;
}

bool ColladaController::GetLocalTMComponents(TimeValue t, TMComponentsArg& cmpts, Matrix3Indirect& parentMatrix)
{
	t; cmpts; parentMatrix;
	return true;
}

IOResult ColladaController::Save(ISave *isave)
{
	return Control::Save(isave);
}

IOResult ColladaController::Load(ILoad *iload)
{
	return Control::Load(iload);
}
