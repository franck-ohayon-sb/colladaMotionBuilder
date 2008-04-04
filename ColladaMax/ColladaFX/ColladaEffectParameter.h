/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _COLLADAFX_COLLADA_EFFECT_PARAMETER_H_
#define _COLLADAFX_COLLADA_EFFECT_PARAMETER_H_

class ColladaEffectPass;
class ColladaEffectShader;
class DlgTemplateBuilder;

class DocumentExporter;
class FCDEffectProfileFX;
class FCDMaterial;
class FCDEffectParameterSurface;
class FCDObjectWithId;

class ColladaEffectParameter
{
public:
	enum Type
	{
		kBool,
		kHalf,
		kFloat,
		kHalf2,
		kFloat2,
		kHalf3,
		kFloat3,
		kHalf4,
		kFloat4,
		kHalf4x4,
		kFloat4x4,
		kSurface,
		kSampler2D,
		kSamplerCUBE,
		kStruct,
		kUnknown
	};
	static Type convertCGtype(CGtype type);

	enum Semantic
	{
		kEMPTY,
		kPOSITION,
		kCOLOR0,
		kCOLOR1,
		kNORMAL,
		kTEXCOORD0,
		kTEXCOORD1,
		kTEXCOORD2,
		kTEXCOORD3,
		kTEXCOORD4,
		kTEXCOORD5,
		kTEXCOORD6,
		kTEXCOORD7,
		kVIEWPROJ,
		kVIEWPROJI,
		kVIEWPROJIT,
		kVIEW,
		kVIEWI,
		kVIEWIT,
		kPROJ,
		kPROJI,
		kPROJIT,
		kOBJECT,
		kOBJECTI,
		kOBJECTIT,
		kWORLD,
		kWORLDI,
		kWORLDIT,
		kWORLDVIEW,
		kWORLDVIEWI,
		kWORLDVIEWIT,
		kWORLDVIEWPROJ,
		kWORLDVIEWPROJI,
		kWORLDVIEWPROJIT,
		kTIME,
		kUNKNOWN
	};
	static Semantic convertCGsemantic(const char* name);

	enum BindTargetType
	{
		kNONE = 0x00,
		kVERTEX = 0x01,
		kFRAGMENT = 0x02,
	};

protected:
	ColladaEffectShader* mParent;
	
	Type mType;
	Semantic mSemantic;

	fstring mName;
	fstring mEntryName;
	fstring mUIName;

	uint16 mParamID;
	IParamBlock2 *mParamBlock;
	bool mIsDirty;

	uint32 mBindTarget;

	// Cg specific
	CGparameter mParameter;
	fm::vector<CGparameter> mExpandedParameters;

	FCDObjectWithId* mParentWithId;

public:
	ColladaEffectParameter(ColladaEffectShader* parentP, CGparameter paramP, Type type, Semantic semantic);
	virtual ~ColladaEffectParameter();

	void setParent(ColladaEffectShader* parentP){ mParent = parentP; }
	bool initialize(CGparameter paramP);

	Type getType() const { return mType; }
	Semantic getSemantic() const { return mSemantic; }
	const char* getSemanticString();
	CGparameter getCGParameter(){ return mParameter; }

	// COLLADA export/import specific
	void setColladaParent(FCDObjectWithId* parent){ mParentWithId = parent; }
	fstring getReferenceString();

	void clearBindings() { mBindTarget = kNONE; }
	void addBinding(BindTargetType to) { mBindTarget |= to; }
	bool isBound(BindTargetType to){ return ((mBindTarget & to) > 0); }

	// expanded (D3D) parameters.
	size_t getExpandedParameterCount() const { return mExpandedParameters.size(); }
	CGparameter getExpandedParameter(size_t idx) { return mExpandedParameters[idx]; }
	void addExpandedParameter(CGparameter param){ mExpandedParameters.push_back(param); }
	void clearExpandedParameters(){ mExpandedParameters.clear(); }

	const fstring& getName() const { return mName; }
	const fstring& getProgramEntry() const { return mEntryName; }
	const fstring& getUIName() const { return mUIName; }

	void setParamID(uint16 pid){ mParamID = pid; }
	void setParamBlock(IParamBlock2 *pb) { mParamBlock = pb; }
	IParamBlock2* getParamBlock() { return mParamBlock; }
	ParamID getParamID() { return mParamID; }
	void setProgramEntry(const fstring& entry){ mEntryName = entry; }

	// material instance binding query methods (binding parameter semantics to animation channels)
	virtual bool hasNode() { return false; }
	virtual INode* getNode() { return NULL; }

	/** Set this flag to force synchronization before the next update.*/
	void setDirty() { mIsDirty = true; }

	/** Calls the AddParam method on the parameter block description.*/
	virtual void addToDescription(ParamBlockDesc2 *desc) =0;

	DLGITEMTEMPLATE& buildTemplateLabel(DlgTemplateBuilder& tb, WORD& dialogCount, WORD lineCount);

	/** Appends its UI to the dialog template builder.*/
	virtual void buildTemplateItem(DlgTemplateBuilder& tb, WORD& dialogCount, WORD& lineCount) = 0;

	/** Update Cg with internal data. The parent pass has a pointer to a D3D9 window to retrieve transforms.*/
	virtual void synchronize(){}
	virtual void update() = 0;

	// called on D3D device change.
	virtual void releaseTexture(){}
	virtual void rebuildTexture(IDirect3DDevice9*){}

	virtual void doExport(DocumentExporter* document, FCDEffectProfileFX* profile, FCDMaterial* instance) = 0;
};

/** A singleton class to trigger redraws on time change.
	Used by parameters having the TIME semantic, or simply animated parameters.
*/
class ColladaEffectTimeObserver : public TimeChangeCallback
{
private:
	static ColladaEffectTimeObserver sInstance;
	TimeValue mTime;
	int mObserverCount;

private:
	ColladaEffectTimeObserver()
		:mTime(0)
		,mObserverCount(0)
	{
		GetCOREInterface()->RegisterTimeChangeCallback(this);
	}

public:
	~ColladaEffectTimeObserver()
	{
		GetCOREInterface()->UnRegisterTimeChangeCallback(this);
	}

	void AddObserver()
	{
		++ mObserverCount;
	}

	void RemoveObserver()
	{
		-- mObserverCount;
	}

	// TimeChangeCallback interface implementation
	virtual void TimeChanged(TimeValue UNUSED(t))
	{
		if (mObserverCount > 0)
		{
			// not enough, it won't refresh when moving the time slider
			//GetCOREInterface()->RedrawViews(t);
			GetCOREInterface()->ForceCompleteRedraw();
		}
	}

	static ColladaEffectTimeObserver& GetInstance(){ return sInstance; }
};

///////////////////////////////////////////////////////////////////////////////
// CONCRETE PARAMETERS
///////////////////////////////////////////////////////////////////////////////

/** Use this factory to allocate the ColladaEffectParameters.*/
class ColladaEffectParameterFactory
{
public:
	static ColladaEffectParameter* create(ColladaEffectShader* parentShader, CGparameter param);
};

/** A single boolean parameter.*/
class ColladaEffectParameterBool : public ColladaEffectParameter
{
private:
	BOOL mBool;

public:
	ColladaEffectParameterBool(ColladaEffectShader* parentP, CGparameter param, Type type, Semantic semantic);
	virtual ~ColladaEffectParameterBool(){}

	virtual void addToDescription(ParamBlockDesc2 *desc);
	virtual void buildTemplateItem(DlgTemplateBuilder& tb, WORD& dialogCount, WORD& lineCount);
	virtual void update();
	virtual void doExport(DocumentExporter* document, FCDEffectProfileFX* profile, FCDMaterial* instance);

private:
	void synchronize();
};

/** A single floating point value parameter.*/
class ColladaEffectParameterFloat : public ColladaEffectParameter
{
private:
	float mFloat;
	float mUIMin, mUIMax;

public:
	ColladaEffectParameterFloat(ColladaEffectShader* parentP, CGparameter param, Type type, Semantic semantic);
	virtual ~ColladaEffectParameterFloat();

	virtual void addToDescription(ParamBlockDesc2 *desc);
	virtual void buildTemplateItem(DlgTemplateBuilder& tb, WORD& dialogCount, WORD& lineCount);
	virtual void update();
	virtual void doExport(DocumentExporter* document, FCDEffectProfileFX* profile, FCDMaterial* instance);

private:
	void synchronize();
};

/** A matrix parameter.*/
class ColladaEffectParameterMatrix : public ColladaEffectParameter
{
private:
	Matrix3 mMatrix;

public:
	ColladaEffectParameterMatrix(ColladaEffectShader* parentP, CGparameter param, Type type, Semantic semantic);
	virtual ~ColladaEffectParameterMatrix(){}

	virtual void addToDescription(ParamBlockDesc2 *desc);
	virtual void buildTemplateItem(DlgTemplateBuilder& tb, WORD& dialogCount, WORD& lineCount);
	virtual void update();
	virtual void doExport(DocumentExporter* document, FCDEffectProfileFX* profile, FCDMaterial* instance);

private:
	void synchronize();
};

/** An RGBA color.*/
class ColladaEffectParameterColor : public ColladaEffectParameter
{
private:
	Color mColor;

public:
	ColladaEffectParameterColor(ColladaEffectShader* parentP, CGparameter param, Type type, Semantic semantic);
	virtual ~ColladaEffectParameterColor();

	virtual void addToDescription(ParamBlockDesc2 *desc);
	virtual void buildTemplateItem(DlgTemplateBuilder& tb, WORD& dialogCount, WORD& lineCount);
	virtual void update();
	virtual void doExport(DocumentExporter* document, FCDEffectProfileFX* profile, FCDMaterial* instance);

private:
	void synchronize();
};

/** A scene node parameter (lights, or anything that has a position.*/
class ColladaEffectParameterNode : public ColladaEffectParameter
{
private:
	INode* mNode;
	SClass_ID mSuperClassID;
	fstring mObjectID; /**< The Object annotation value.*/
	fm::string mNodeId; /**< INTERNAL. Used at import time.*/

public:
	ColladaEffectParameterNode(ColladaEffectShader* parentP, CGparameter param, Type type, Semantic semantic);
	virtual ~ColladaEffectParameterNode(){}

	virtual void addToDescription(ParamBlockDesc2 *desc);
	virtual void buildTemplateItem(DlgTemplateBuilder& tb, WORD& dialogCount, WORD& lineCount);
	SClass_ID getNodeType() const { return mSuperClassID; }

	SClass_ID getSuperClassID() const { return mSuperClassID; }
	void setSuperClassID(SClass_ID sid) { mSuperClassID = sid; }

	void setObjectID(const char* annValue){ mObjectID = annValue; }
	const fstring& getObjectID() const { return mObjectID; }

	// overrides from parent class
	virtual bool hasNode() { return true; }
	virtual INode* getNode() { return mNode; }

	virtual void update();
	virtual void doExport(DocumentExporter* document, FCDEffectProfileFX* profile, FCDMaterial* instance);

	// INTERNAL
	void setNodeId(const fm::string& id){ mNodeId = id; }
	fm::string& getNodeId() { return mNodeId; }

private:
	void synchronize();
};

class ColladaEffectParameterSampler : public ColladaEffectParameter
{
public:
	enum SamplerType
	{
		kSAMPLER_2D,
		kSAMPLER_3D, // not supported
		kSAMPLER_CUBE
	};

private:
	PBBitmap* mBitmap;
	IDirect3DBaseTexture9* mTexture;
	int mStage;
	fstring mDefaultResourceName;
	SamplerType mSamplerType;

	//typedef fm::tree<D3DSAMPLERSTATETYPE, DWORD> SamplerStateMap;
	//SamplerStateMap mStates;

public:
	ColladaEffectParameterSampler(ColladaEffectShader* parentP, CGparameter param, Type type, Semantic semantic);
	virtual ~ColladaEffectParameterSampler(){}

	virtual void addToDescription(ParamBlockDesc2 *desc);
	virtual void buildTemplateItem(DlgTemplateBuilder& tb, WORD& dialogCount, WORD& lineCount);
	virtual void update();
	virtual void releaseTexture();
	virtual void rebuildTexture(IDirect3DDevice9* pNewDevice);
	virtual void doExport(DocumentExporter* document, FCDEffectProfileFX* profile, FCDMaterial* instance);
	FCDEffectParameterSurface* exportSurface(DocumentExporter* document, FCDEffectProfileFX* profile, FCDMaterial* instance);

private:
	void synchronize();
};

#endif // _COLLADAFX_COLLADA_EFFECT_PARAMETER_H_
