/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _CXATTRIBUTE_H
#define _CXATTRIBUTE_H

class CFXShaderNode;

// These are the parameter annotations exported by ColladaMaya
// They do NOT reflect the annotations held by the CgFX source, if any.
#define CFX_ANNOTATION_UINAME	"UIName"
#define CFX_ANNOTATION_UIMIN	"UIMin"
#define CFX_ANNOTATION_UIMAX	"UIMax"
#define CFX_ANNOTATION_UITYPE	"UIType"
// possible UI types
#define CFX_UITYPE_COLOR		"Color"
#define CFX_UITYPE_NAVIGATION	"Navigation"
#define CFX_UITYPE_VECTOR		"Vector"

class CFXParameter
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

public:
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

public:
	enum cxBindTarget
	{
		kNONE,
		kVERTEX,
		kFRAGMENT,
		kBOTH
	};

public:
	CFXParameter(CFXShaderNode* parent);
	~CFXParameter();
	bool create(CGparameter parameter);

	void addAttribute();
	void removeAttribute();

	inline const fchar* getName() const { return MConvert::ToFChar(name); }
	inline const MString& getAttributeName() const { return attributeName; }

	inline Type getType() const { return type; }
	inline Semantic getSemantic() const { return semantic; }
	inline const char* getTypeString() const { return cgGetTypeString(cgGetParameterNamedType(m_parameter)); }
	inline const char* getSemanticString() const { return cgGetParameterSemantic(m_parameter); }
	const char* getSemanticStringForFXC();
	
	MPlug getPlug();
	inline const MObject& getAttribute() { return attribute; }
	
	void updateMeshData(const float * vertexArray, const float ** colorArrays, int id[10], int colorCount, const float ** normalArrays, int normalId, int normalCount, const float ** uvArrays, int uvId, int uvCount, int isUV[10], MString setName[10]);
	void setColorOrUV(const float ** colorArrays, int colorId, int colorCount, const float ** uvArrays, int uvCount, int isUV, MString name, const float ** normalArrays, int normalId, int normalCount);
	void updateAttrValue(const MMatrix& shaderSpace, const MMatrix& worldSpace);
	
	inline bool isTexture() { return (type == kSampler2D || type == kSamplerCUBE); }
	inline void setTextureOffset(int offset) { textureOffset = offset; }
	inline int getTextureOffset() const { return textureOffset; }

	inline int getIntValue() const { return (int)m_r; }
	inline float getFloatValue() const { return m_r; }
	inline float getFloatValueX() const { return m_r; }
	inline float getFloatValueY() const { return m_g; }
	inline float getFloatValueZ() const { return m_b; }
	
	const CGparameter& getCGParameter() const { return m_parameter; }
	
	// access UI type: Color, Vector, Navigation
	inline void setUIType(const fchar* type) { uiType = type;}
	inline const MString& getUIType() const { return uiType; }
	
	// set UI type by annotation
	void readUIWidget();
	
	inline void setSource(const char* str_source) { m_source = str_source; }
	inline const MString& getSource() const { return m_source; }

	void setDefaultValue(float v);
	void setDefaultValue(float r, float g, float b);
	void setBounds(float min, float max);

	// Synchronize the value with the plug's.
//	void reset(const MObject& shaderNode)
	void Synchronize();
	
	// access program target
	inline void setBindTarget(cxBindTarget target) { m_target = target; }
	inline cxBindTarget getBindTarget() const { return m_target; }
	
	// access program entry
	inline void setProgramEntry(MString entry) { m_programEntry = entry; }
	inline const MString& getProgramEntry() const { return m_programEntry; }

	// override the CG parameter after a reload/program change.
	inline void overrideParameter(CGparameter _parameter) { m_parameter = _parameter; }

	// dirty flag
	inline bool IsDirty() { return dirty; }
	inline void SetDirty() { dirty = true; }
	inline void ResetDirty() { dirty = false; }

	// UI related
	inline const MString& getUIName() const { return UIName; }
	inline void setUIName(const MString& _uiname) { UIName = _uiname; }

protected:
	static GLuint loadTexture(const fchar* filename, int offset);
	static GLuint loadCompressedTextureCUBE(const fchar *filename, int offset);
	GLuint CreateDefault2DTexture();
	GLuint CreateDefaultCUBETexture();

	void loadDefaultTexture();
	
private:
	CFXShaderNode* parent;
	MString name;

	Semantic semantic;
	// storing MObject refers to the shader parent seems crashing Maya all the time
	//MObject parentNode;
	MObject attribute;
	CGparameter m_parameter;
	
	int textureOffset;
	MString m_source;
	MString uiType;
	
	MString attributeName;
	Type type;
	float m_r, m_g, m_b;
	MMatrix matrixValue;
	GLuint textureid;
	bool dirty;

	// Annotation related
	MString UIName;

	// vertex or fragment program to bind
	cxBindTarget m_target;
	
	// program parameter entry name
	MString m_programEntry;
};
#endif

