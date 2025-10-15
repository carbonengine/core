// Copyright © 2013 CCP ehf.

#pragma once
#ifndef ICcpStream_H
#define ICcpStream_H


struct ICcpStream
{
	enum SeekOrigin
	{
		SO_BEGIN		= 0,
		SO_CURRENT		= 1,
		SO_END			= 2,
	};

	// Reads from the stream into a buffer.
	// Returns the number of bytes read.
	virtual ptrdiff_t Read( void* dest, ptrdiff_t count ) = 0;
	
	// Writes from a buffer into the stream.
	// Returns the number of bytes written.
	virtual ptrdiff_t Write( const void* source, size_t count ) = 0;

	virtual ptrdiff_t Seek( ptrdiff_t distance, SeekOrigin method ) = 0;

	virtual ptrdiff_t GetPosition() = 0;

	virtual ptrdiff_t GetSize() = 0;
};

#endif