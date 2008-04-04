/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	Copyright (C) 2004-2005 Alias Systems Corp.
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __DAE_INPUT_INCLUDED__
#define __DAE_INPUT_INCLUDED__

enum DaeInputType
{
	kNULL = 0,
	kVERTEX,
	kPOSITION,
	kNORMAL,
	kTEXCOORD,
	kCOLOUR,

	// Export-only
	kGEOTANGENT,
	kGEOBINORMAL,
	kTEXTANGENT,
	kTEXBINORMAL,

	// Unsupported, but valid
	kUV, 

	// Maya-specific
	kEXTRA, // Blind Data
};

//
// A generic Input attribute
//
class DaeInput
{
public:
	DaeInputType type; 
	MString semantic; 
	int idx;

	DaeInput() : type(kNULL), semantic(""), idx(-1) {}
	DaeInput(DaeInputType type, MString semantic, int idx=-1)
		: type(type), semantic(semantic), idx(idx) {}
};


//
// A generic call back handler to allow sub-elements to specify 
// the inputs they require
//
class DaeInputVisitor
{
public:
	DaeInputVisitor() {}
	virtual ~DaeInputVisitor() {}
	
	virtual void		visitInput(DaeInput p) = 0;
};


//
// A helper version of the above which simply accumulates parameters
//
class DaeInputAccumulator : public DaeInputVisitor
{
public:
	DaeInputAccumulator() {}
	virtual ~DaeInputAccumulator() {}
	
	virtual void	visitInput(DaeInput p) { parameters.push_back(p); };
	uint			length() const { return (uint) parameters.size(); };
	inline DaeInput& operator[] (unsigned int i) { return parameters[i]; };

private:
	vector<DaeInput> parameters;
};

#endif 
