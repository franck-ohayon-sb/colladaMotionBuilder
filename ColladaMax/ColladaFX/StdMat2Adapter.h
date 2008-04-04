/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COLLADAFX_STDMAT2_ADAPTER_H_
#define _COLLADAFX_STDMAT2_ADAPTER_H_

/**
A class that provides default implementations for the abstract methods declared
in the StdMat2 hierarchy: Mtl, StdMat and finally StdMat2. This is not really a
"complete" adapter since you'll have to implement abstract methods
from Animatable, ReferenceMaker and MtlBase, but at least it'll make the code
clearer.
*/
class StdMat2Adapter : public StdMat2
{
public:
	StdMat2Adapter(){}
	virtual ~StdMat2Adapter(){}

	// from Mtl
	// ========

	virtual Color GetAmbient(int /*mtlNum = 0*/, BOOL /*backFace = FALSE*/){ return Color(0,0,0); }
	virtual Color GetDiffuse(int /*mtlNum = 0*/, BOOL /*backFace = FALSE*/){ return Color(0,0,0); }
	virtual Color GetSpecular(int /*mtlNum = 0*/, BOOL /*backFace = FALSE*/){ return Color(0,0,0); }
	virtual float GetShininess(int /*mtlNum = 0*/, BOOL /*backFace = FALSE*/){ return 0.0f; }
	virtual float GetShinStr(int /*mtlNum = 0*/, BOOL /*backFace = FALSE*/){ return 0.0f; }
	virtual float GetXParency(int /*mtlNum = 0*/, BOOL /*backFace = FALSE*/){ return 0.0f; }

	virtual BOOL  GetSelfIllumColorOn(int /*mtlNum=0*/, BOOL /*backFace=FALSE*/){ return FALSE; }
	virtual float GetSelfIllum(TimeValue /*t*/){ return 0.0f; }
	virtual Color GetSelfIllumColor(int /*mtlNum*/, BOOL /*backFace*/){ return Color(0,0,0); }

	virtual void SetAmbient(Color /*c*/, TimeValue /*t*/){}
	virtual void SetDiffuse(Color /*c*/, TimeValue /*t*/){}
	virtual void SetSpecular(Color /*c*/, TimeValue /*t*/){}
	virtual void SetShininess(float /*v*/, TimeValue /*t*/){}

	// main Mtl method - you definitely should implement this at the very least.
	virtual void Shade(ShadeContext& /*sc*/){}

	// from StdMat
	// ===========
	virtual void SetSoften(BOOL /*onoff*/){}
	virtual void SetFaceMap(BOOL /*onoff*/){}
	virtual void SetTwoSided(BOOL /*onoff*/){}
	virtual void SetWire(BOOL /*onoff*/){}
	virtual void SetWireUnits(BOOL /*onOff*/){}
	virtual void SetFalloffOut(BOOL /*onOff*/){}
	virtual void SetTransparencyType(int /*type*/){}
	virtual void SetFilter(Color /*c*/, TimeValue /*t*/){}
	virtual void SetShinStr(float /*v*/, TimeValue /*t*/){}	
	virtual void SetSelfIllum(float /*v*/, TimeValue /*t*/){}

	virtual void SetOpacity(float /*v*/, TimeValue /*t*/){}
	virtual void SetOpacFalloff(float /*v*/, TimeValue /*t*/){}
	virtual void SetWireSize(float /*s*/, TimeValue /*t*/){}
	virtual void SetIOR(float /*v*/, TimeValue /*t*/){}
	virtual void LockAmbDiffTex(BOOL /*onOff*/){}

	// Sampling
	virtual void SetSamplingOn(BOOL /*on*/){}
	virtual BOOL GetSamplingOn(){ return FALSE; }

	// Obsolete Calls, not used in StdMat2....see shaders.h
	virtual void SetShading(int /*s*/){}
	virtual int GetShading(){ return -1; }

	// texmaps, these only work for translated ID's of map channels,
	// see stdMat2 for access to std ID channels translator
	virtual void EnableMap(int /*id*/, BOOL /*onoff*/){}
	virtual BOOL MapEnabled(int /*id*/){ return FALSE; }
	virtual void SetTexmapAmt(int /*id*/, float /*amt*/, TimeValue /*t*/){}
	virtual float GetTexmapAmt(int /*id*/, TimeValue /*t*/){ return 0.0f; }

	virtual BOOL GetSoften(){ return FALSE; }
	virtual BOOL GetFaceMap(){ return FALSE; }
	virtual BOOL GetTwoSided(){ return FALSE; }
	virtual BOOL GetWire(){ return FALSE; }
	virtual BOOL GetWireUnits(){ return FALSE; }
	virtual BOOL GetFalloffOut(){ return FALSE; }
	virtual int GetTransparencyType(){ return 0; }

	virtual Color GetAmbient(TimeValue /*t*/){ return Color(0,0,0); }		
	virtual Color GetDiffuse(TimeValue /*t*/){ return Color(0,0,0); }
	virtual Color GetSpecular(TimeValue /*t*/){ return Color(0,0,0); }
	virtual Color GetFilter(TimeValue /*t*/){ return Color(0,0,0); }
	virtual float GetShininess(TimeValue /*t*/){ return 0.0f; }
	virtual float GetShinStr(TimeValue /*t*/){ return 0.0f; }		
	virtual float GetOpacity(TimeValue /*t*/){ return 0.0f; }
	virtual float GetOpacFalloff(TimeValue /*t*/){ return 0.0f; }
	virtual float GetWireSize(TimeValue /*t*/){ return 0.0f; }
	virtual float GetIOR(TimeValue /*t*/){ return 0.0f; }
	virtual BOOL GetAmbDiffTexLock(){ return FALSE; }

	// from StdMat2
	// ============
	// Shader/Material UI synchronization
	virtual BOOL  KeyAtTime(int /*id*/,TimeValue /*t*/){ return FALSE; }
	virtual int   GetMapState(int /*indx*/){ return 0; }
	virtual TSTR  GetMapName(int /*indx*/){ return _T(""); }
	virtual void  SyncADTexLock(BOOL /*lockOn*/){}

	// Shaders
	virtual BOOL SwitchShader(Class_ID /*id*/){ return FALSE; }
	virtual Shader* GetShader(){ return NULL; }
	virtual BOOL IsFaceted(){ return FALSE; }
	virtual void SetFaceted(BOOL /*on*/){}

	// texture channels from stdmat id's
	virtual long StdIDToChannel(long /*id*/){ return 0; }

	// Samplers
	virtual BOOL SwitchSampler(Class_ID /*id*/){ return FALSE; }
	virtual Sampler * GetPixelSampler(int /*mtlNum*/, BOOL /*backFace*/){ return NULL; }
	
	// these params extend the UI approximation set in stdMat
	virtual Color GetSelfIllumColor(TimeValue /*t*/){ return Color(0,0,0); }
	virtual void SetSelfIllumColorOn(BOOL /*on*/){}
	virtual void SetSelfIllumColor(Color /*c*/, TimeValue /*t*/){}
	
	// these are used to simulate traditional 3ds shading by the default handlers
	virtual float GetReflectionDim(float /*diffIllumIntensity*/){ return 1.0f; }		
	virtual Color TranspColor(float /*opac*/, Color /*filt*/, Color /*diff*/){ return Color(0,0,0); }
	virtual float GetEffOpacity(ShadeContext& /*sc*/, float /*opac*/){ return 0.0f; }
};

#endif // _COLLADAFX_STDMAT2_ADAPTER_H_
