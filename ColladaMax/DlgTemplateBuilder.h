/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _DLGTEMPLATE_BUILDER_H_
#define _DLGTEMPLATE_BUILDER_H_

/** Aligns a buffer on the specified size boundary.*/
#define ALIGN(ptr, size) ptr = (uint8*) ((((size_t) ptr) + size - 1) & (~(size - 1)))

class DlgTemplateBuilder
{
private:
	uint8* data; /**< Initial data pointer.*/
	uint8* buffer; /**< Current data pointer.*/

public:
	DlgTemplateBuilder(size_t bufferSizeP)
	{
		data = new uint8[bufferSizeP];
		memset(data, 0, sizeof(uint8) * bufferSizeP);
		buffer = data;
	}

	~DlgTemplateBuilder()
	{
		SAFE_DELETE_ARRAY(data);
	}

	void Align()
	{
		ALIGN(buffer, sizeof(DWORD));
	}

	void AppendWString(const char* str)
	{
		if (str == NULL) return;

		// Copy the string over, upsizing the characters.
		while (*str != 0)
		{
			*(wchar_t*) buffer = (wchar_t) *(str++);
			buffer += sizeof(wchar_t);
		}

		// NULL-terminate
		*(wchar_t*) buffer = (wchar_t) 0;
		buffer += sizeof(wchar_t);
	}

	template <class ValueType>
	ValueType& Append()
	{
		ValueType* p = (ValueType*) buffer;
		buffer += sizeof(ValueType);
		return *p;
	}

	template <class ValueType>
	void Append(const ValueType& v)
	{
		Append<ValueType>() = v;
	}

	DLGTEMPLATE* buildTemplate()
	{
		size_t bufferSize = buffer - data;
		DLGTEMPLATE* ret = (DLGTEMPLATE*) new uint8[bufferSize];
		memcpy(ret, data, bufferSize);
		return ret;
	}

};

#endif // _DLGTEMPLATE_BUILDER_H_
