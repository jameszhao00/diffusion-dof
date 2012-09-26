#pragma once
#include "package.h"
#include "gfx.h"

void TW_CALL CopyCDStringToClient(char **destPtr, const char *src)
{
	size_t srcLen = (src!=NULL) ? strlen(src) : 0;
	size_t destLen = (*destPtr!=NULL) ? strlen(*destPtr) : 0;

	// alloc or realloc dest memory block if needed
	if( *destPtr==NULL )
		*destPtr = (char *)malloc(srcLen+1);
	else if( srcLen>destLen )
		*destPtr = (char *)realloc(*destPtr, srcLen+1);

	// copy src
	if( srcLen>0 )
		strncpy(*destPtr, src, srcLen);
	(*destPtr)[srcLen] = '\0'; // null-terminated string
}
