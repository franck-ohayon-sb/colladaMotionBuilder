/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COLLADA_CONTROLLER_H_
#define _COLLADA_CONTROLLER_H_

#ifndef __CONTROL__
#include "control.h"
#endif // __CONTROL__

extern Class_ID COLLADA_CONTROLLER_CLASS_ID;

class ColladaController : public Control
{
private:

public:
	ColladaController();
	~ColladaController();
	virtual void DeleteThis();

	virtual SClass_ID SuperClassID();
	virtual Class_ID ClassID();

	virtual void Copy(Control *from) { from; }
	virtual void CommitValue(TimeValue t) { t; }
	virtual void RestoreValue(TimeValue t) { t; }

	virtual INode* GetTarget() { return NULL; } 
	virtual RefResult SetTarget(INode *targ) { targ; return REF_SUCCEED; }

	// Implemented by transform controllers that have position controller
	// that can be edited in the trajectory branch
	virtual Control *GetPositionController() {return NULL;}
	virtual Control *GetRotationController() {return NULL;}
	virtual Control *GetScaleController() {return NULL;}
	virtual BOOL SetPositionController(Control *c) { c; return FALSE;}
	virtual BOOL SetRotationController(Control *c) { c; return FALSE;}
	virtual BOOL SetScaleController(Control *c) { c; return FALSE;}

	// If a controller has an 'X', 'Y', 'Z', or 'W' controller, it can implement
	// these methods so that its sub controllers can respect track view filters
	virtual Control *GetXController() {return NULL;}
	virtual Control *GetYController() {return NULL;}
	virtual Control *GetZController() {return NULL;}
	virtual Control *GetWController() {return NULL;}

	// Implemented by look at controllers that have a float valued roll
	// controller so that the roll can be edited via the transform type-in
	virtual Control *GetRollController() {return NULL;}
	virtual BOOL SetRollController(Control *c) { c; return FALSE;}

	virtual BOOL IsLeaf() {return TRUE;}
	virtual int IsKeyable() {return 1;}

	// If a controller does not want to allow another controller
	// to be assigned on top of it, it can return FALSE to this method.
	virtual BOOL IsReplaceable() {return TRUE;}		

	// This is called on TM, pos, rot, and scale controllers when their
	// input matrix is about to change. If they return FALSE, the node will
	// call SetValue() to make the necessary adjustments.
	virtual BOOL ChangeParents(TimeValue t,const Matrix3& oldP,const Matrix3& newP,const Matrix3& tm) { t; oldP; newP; tm; return TRUE; }

	// val points to an instance of a data type that corresponds with the controller
	// type. float for float controllers, etc.
	// Note that for SetValue on Rotation controllers, if the SetValue is
	// relative, val points to an AngAxis while if it is absolute it points
	// to a Quat.
	virtual void GetValue(TimeValue t, void *val, Interval &valid, GetSetMethod method=CTRL_ABSOLUTE);
	virtual	void SetValue(TimeValue t, void *val, int commit=1, GetSetMethod method=CTRL_ABSOLUTE);

	virtual bool GetLocalTMComponents(TimeValue t, TMComponentsArg& cmpts, Matrix3Indirect& parentMatrix);

	// Default implementations of load and save handle loading and saving of out of range type.
	// Call these from derived class load and save.
	// NOTE: Must call these before any of the derived class chunks are loaded or saved.
	virtual IOResult Save(ISave *isave);
	virtual IOResult Load(ILoad *iload);

	virtual RefResult NotifyRefChanged(Interval,RefTargetHandle,PartID &,RefMessage) { return REF_SUCCEED; }
};

ClassDesc* GetColladaControllerClassDesc();

#endif // _COLLADA_CONTROLLER_H_
