// Copyright © 2025 CCP ehf.

#include "StringConversions.h"
#import <AppKit/AppKit.h>

std::wstring UTF8ToWide( const char* utf8String )
{
	NSString* str = [[NSString alloc] initWithBytes:utf8String length:strlen( utf8String ) encoding:NSUTF8StringEncoding];
	NSData* data = [str dataUsingEncoding: CFStringConvertEncodingToNSStringEncoding( kCFStringEncodingUTF32LE )];
	return std::wstring( static_cast<const wchar_t*>( [data bytes] ), [data length] / sizeof(wchar_t) );
}

std::string WideToUTF8( const wchar_t* wideString )
{
	NSString* str = [[NSString alloc] initWithBytes:wideString length:wcslen( wideString ) * sizeof(wchar_t) encoding:NSUTF32LittleEndianStringEncoding];
	NSData* data = [str dataUsingEncoding: CFStringConvertEncodingToNSStringEncoding( kCFStringEncodingUTF8 )];
	return std::string( static_cast<const char*>( [data bytes] ), [data length] );
}