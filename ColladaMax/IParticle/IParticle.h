/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/*
	Based off the 3dsMax COLLADA Tools
	Copyright (C) 2005-2007 Feeling Software Inc.
	Copyright (C) 2005-2006 Autodesk Media & Entertainment.
*/

/*
	Particles Internal access classes
*/

#ifndef _IPARTICLE_INTERFACE_H_
#define _IPARTICLE_INTERFACE_H_

#define DLLImport __declspec(dllimport)

#pragma warning(disable : 4100)

#include <Max.h>
#include <simpobj.h>
#include "iparamm.h"
#include "RandGenerator.h"


// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//        WARNING - These class are taken from the MaxSDK/Samples/particles/Suprprts folder
//				This section is of no interest to us, all we want is the padding to 
//					so our definitions match max
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
class CommonParticle;
class PCloudParticle;
class PArrayParticle;
class boxlst;

typedef struct{
 TimeValue tl;
 int gennum;
} TimeAndGen;

typedef struct{
  Tab<TimeAndGen> tl;
} TimeLst;

typedef struct
{ Point3 vel;
  int K;
  TimeValue oneframe;
  BOOL inaxis;
}InDirInfo;

typedef struct
{ float dirchaos,spchaos,scchaos;
  int spsign,scsign,invel,spconst,scconst,axisentered;
  Point3 Axis;
  float axisvar;
} SpawnVars;

typedef struct{
float M,Vsz;
Point3 Ve,vel,pos;
Point3 wbb[8];
}CacheDataType;

class FSCommonParticleDraw : public CustomParticleDisplay 
{
public:
	float tumble,scale;
	BOOL firstpart;
	CommonParticle *obj;
	int disptype,ptype,bb,anifr,anioff;
	boxlst *bboxpt;
	TimeValue t;
	InDirInfo indir;

	FSCommonParticleDraw() {obj=NULL;}
	BOOL DrawParticle(GraphicsWindow *gw,ParticleSys &parts,int i);
};

class PCloudParticleDraw : public CustomParticleDisplay {
public:
	BOOL firstpart;
	PCloudParticle *obj;
	int disptype,ptype,bb,anifr,anioff;
	boxlst *bboxpt;
	TimeValue t;
	InDirInfo indir;

	PCloudParticleDraw() {obj=NULL;}
	BOOL DrawParticle(GraphicsWindow *gw,ParticleSys &parts,int i);
};

class PArrayParticleDraw : public CustomParticleDisplay {
public:
//		float size,VSz,
	BOOL firstpart;
	PArrayParticle *obj;
	int disptype,ptype,bb,anifr,anioff;
	boxlst *bboxpt;
	TimeValue t;
	InDirInfo indir;

	PArrayParticleDraw() {obj=NULL;}
	BOOL DrawParticle(GraphicsWindow *gw,ParticleSys &parts,int i);
};

class RandGeneratorParticles : public RandGenerator {
public:
// override for common rand functions for particle systems
	static const float IntMax;
	static const float IntMax1;
	static const float HalfIntMax;

	int RNDSign() { return RandSign(); }
	float RND01() { return Rand01(); }
	float RND11() { return Rand11(); }
	float RND55() { return Rand55(); }
	int RND0x(int maxnum) { return Rand0X(maxnum); }

	Point3 CalcSpread(float divangle,Point3 oldnorm);
	void VectorVar(Point3 *vel,float R,float MaxAngle);
	Point3 DoSpawnVars(SpawnVars spvars,Point3 pv,Point3 holdv,float *radius,Point3 *sW);
};

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//        WARNING - The following definitions give us access to various max classes
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

// Is the base class for Blizzard and SuperSpray
class FSCommonParticle : public SimpleParticle, IParamArray
{
public:
	FSCommonParticleDraw theSuprSprayDraw;

	int stepSize,size;

	ULONG dflags;
	Mesh *cmesh,*dispmesh;
	INode *custnode,*cnode;
	TSTR custname;
	DWORD flags;
	BOOL cancelled,wasmulti;

	Mtl *origmtl;

	Point3 boxcenter;
	int CustMtls;
	TimeLst times;
	
	Tab<int> nmtls;
	Tab<INode*> nlist;
	Tab<int> llist;
	int deftime;
	int maincount,rseed;
	/*
	... <more class (mostly functions) we dont care about>
	*/
};

// PCloud 'interface'
class FSPCloudParticle : public SimpleParticle, IParamArray, RandGeneratorParticles 
{
	public:
	PCloudParticleDraw thePCloudDraw;
	FSPCloudParticle();
	~FSPCloudParticle();

	int stepSize,size,maincount,fragflags;

	Mesh *cmesh,*dispmesh;
	Box3 *cmbb;
	TimeValue dispt;
	INode *custnode,*distnode,*cnode,*mbase;
	TSTR custname,distname,normname;
	DWORD flags;
	ULONG dflags;
	BOOL cancelled,wasmulti;

	Mtl *origmtl;

	Point3 boxcenter;
	int CustMtls;
	Tab<int> nmtls;
	TimeLst times;
	void GetTimes(TimeLst &times,TimeValue t,int anifr,int ltype);
	void TreeDoGroup(INode *node,Matrix3 tspace,Mesh *cmesh,Box3 *cmbb,int *numV,int *numF,int *tvnum,int *ismapped,TimeValue t,int subtree,int custmtl);
	void CheckTree(INode *node,Matrix3 tspace,Mesh *cmesh,Box3 *cmbb,int *numV,int *numF,int *tvnum,int *ismapped,TimeValue t,int subtree,int custmtl);
	void GetMesh(TimeValue t,int subtree,int custmtl);
	void GetNextBB(INode *node,int subtree,int *count,int *tabmax,Point3 boxcenter,TimeValue t,int tcount,INode *onode);
	void DoGroupBB(INode *node,int subtree,int *count,int *tabmax,Point3 boxcenter,TimeValue t,int tcount,INode *onode);
	void GetallBB(INode *custnode,int subtree,TimeValue t);

	void SetUpList();
	void AddToList(INode *newnode,int i,BOOL add);
	void DeleteFromList(int nnum,BOOL all);
	Tab<INode*> nlist;
	Tab<int> llist;
	/*
	... <more class (mostly functions) we dont care about>
	*/
};

/*
class FSPArrayParticle : public SimpleParticle, IParamArray {
public:
	PArrayParticleDraw thePArrayDraw;
	FSPArrayParticle();
	~FSPArrayParticle();
	int stepSize,size,oldtvnum,lastrnd,emitrnd;
	BOOL cancelled,wasmulti,storernd;
	BOOL fromgeom;
	INode *distnode,*custnode,*cnode;
	CacheDataType *storefrag;
	TSTR distname,custname;
	ULONG dflags;
	int fragflags;
	BOOL doTVs;
	Mtl *origmtl;

	// to fix #182223 & 270224, add these members
	Matrix3 lastTM;
	TimeValue lastTMTime;

	BOOL GenerateNotGeom(TimeValue t,TimeValue lastt,int c,int counter,INode *distnode,int type,Matrix3 tm,Matrix3 nxttm);
	void GetInfoFromObject(float thick,int *c,INode *distnode,TimeValue t,TimeValue lastt);
	void GetLocalBoundBox(TimeValue t, INode *inode,ViewExp* vpt,  Box3& box); 
	void GetWorldBoundBox(TimeValue t, INode *inode, ViewExp* vpt, Box3& box);
	void RendGeom(Mesh *pm,Matrix3 itm,int maxmtl,int maptype,BOOL eitmtl,float mval,PArrayParticleDraw thePArrayDraw,TVFace defface,BOOL notrend);

	Tab<int> nmtls;
	void DoSpawn(int j,int spmult,SpawnVars spvars,TimeValue lvar,BOOL emits,int &oldcnt);
	void SetUpList();
	void AddToList(INode *newnode,int i,BOOL add);
	void DeleteFromList(int nnum,BOOL all);
	Tab<INode*> nlist;
	Tab<int> llist;
	/*
	... <more class (mostly functions) we dont care about>
	*
};

*/

class FSPArrayParticle : public SimpleParticle, IParamArray 
{
public:
	PArrayParticleDraw thePArrayDraw;

	int stepSize,size,oldtvnum,lastrnd,emitrnd;
	BOOL cancelled,wasmulti,storernd;
	BOOL fromgeom;
	INode *distnode,*custnode,*cnode;
	CacheDataType *storefrag;
	TSTR distname,custname;
	ULONG dflags;
	int fragflags;
	BOOL doTVs;
	Mtl *origmtl;

	Matrix3 lastTM;
	TimeValue lastTMTime;

	Tab<int> nmtls;
	Tab<INode*> nlist;
	Tab<int> llist;
	int deftime;
	int maincount;

};

#endif // _MORPHER_INTERFACE_H_
