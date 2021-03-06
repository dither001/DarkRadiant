#pragma once

#include "idatastream.h"

namespace stream
{

/**
 * A class implementing the InputStream interface around a simple byte pointer. 
 * No bounds or validity checking is performed on the given pointer.
 */
class PointerInputStream : 
	public InputStream
{
private:
	const byte* _curPos;

public:
	PointerInputStream(const byte* pointer) : 
		_curPos(pointer)
	{}

	std::size_t read(byte* buffer, std::size_t length) override
	{
		const byte* end = _curPos + length;

		while (_curPos != end)
		{
			*buffer++ = *_curPos++;
		}

		return length;
	}

	void seek(std::size_t offset)
	{
		_curPos += offset;
	}

	const byte* get()
	{
		return _curPos;
	}
};

}
