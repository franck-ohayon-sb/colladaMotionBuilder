/*
	Copyright (C) 2005-2007 Feeling Software Inc.

	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "FCDocument/FCDExtra.h"
#include "FUtils/FUStringConversion.h"

/*
	This sample plug-in is very simple and is meant more as a start-up
	project for your own plug-ins rather than an example of how
	to write a valid plug-in.
*/

BOOL APIENTRY DllMain(HMODULE UNUSED(hModule), DWORD UNUSED(ul_reason_for_call), LPVOID UNUSED(lpReserved))
{
    return TRUE;
}

// A simple object
class SampleCounter : public FUObject
{
private:
	DeclareObjectType(FUObject);

public:
	uint32 counter;

	SampleCounter() : counter(0) {}
	virtual ~SampleCounter() {}
};

// A simple plugin class.
class SampleFColladaPlugin : public FCPExtraTechnique
{
private:
	DeclareObjectType(FCPExtraTechnique);

public:
	SampleFColladaPlugin() {}
	virtual ~SampleFColladaPlugin() {}

	// FColladaPlugin interface
	virtual const char* GetProfileName() { return "FCOLLADA_COUNTER"; }
	virtual FUObject* ReadFromArchive(FCDETechnique* techniqueNode, FUObject* parent);
	virtual void WriteToArchive(FCDETechnique* parentName, const FUObject* handle) const;

	// FUPlugin interface
	virtual const char* GetPluginName() const { return "FCollada Sample Counter Plug-in"; }
	virtual uint32 GetPluginVersion() const { return 0x10001; } // 1.01
};

ImplementObjectType(SampleFColladaPlugin);
ImplementObjectType(SampleCounter);

// The three necessary function to expose.
extern "C"
{
	uint32 GetPluginCount() { return 1; }
	const FUObjectType* GetPluginType(uint32 UNUSED(index)) { return &SampleFColladaPlugin::GetClassType(); }
	FUPlugin* CreatePlugin(uint32 UNUSED(index)) { return new SampleFColladaPlugin(); }
}

// The counter implementation
FUObject* SampleFColladaPlugin::ReadFromArchive(FCDETechnique* techniqueNode, FUObject* UNUSED(parent))
{
	SampleCounter* counter = new SampleCounter();

	// Look for a "counter" element.
	FCDENode* counterNode = techniqueNode->FindParameter("counter");
	if (counterNode != NULL)
	{
		counter->counter = FUStringConversion::ToUInt32(counterNode->GetContent());
	}
	return counter;
}

void SampleFColladaPlugin::WriteToArchive(FCDETechnique* techniqueNode, const FUObject* handle) const
{
	if (handle->HasType(SampleCounter::GetClassType()))
	{
		SampleCounter* counter = (SampleCounter*) handle;
		counter->counter++; // this is where the magic happens.
		techniqueNode->AddParameter("counter", counter->counter);
	}
}
