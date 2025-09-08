/*=============================================================================
	UnGcc.h: Unreal definitions for GCC/Clang running under Win32 or a POSIX OS.
=============================================================================*/

/*----------------------------------------------------------------------------
	Platform compiler definitions.
----------------------------------------------------------------------------*/

#ifdef PLATFORM_WIN32
#define __WIN32__	1
#endif

#ifndef PLATFORM_BIG_ENDIAN
#define __INTEL__	1
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef PLATFORM_WIN32
#include <minwindef.h>
#endif

/*----------------------------------------------------------------------------
	Platform specifics types and defines.
----------------------------------------------------------------------------*/

// Undo any Windows defines.
#undef BYTE
#undef CHAR
#undef WORD
#undef DWORD
#undef INT
#undef FLOAT
#undef MAXBYTE
#undef MAXWORD
#undef MAXDWORD
#undef MAXINT
#undef VOID
#undef CDECL

// Make sure HANDLE is defined.
#ifndef _WINDOWS_
	#define HANDLE void*
	#define HINSTANCE void*
#endif

// Sizes.
enum {DEFAULT_ALIGNMENT = 8 }; // Default boundary to align memory allocations on.
enum {CACHE_LINE_SIZE   = 32}; // Cache line size.

// Optimization macros.
#define DISABLE_OPTIMIZATION _Pragma("GCC push_options") \
	_Pragma("GCC optimize(\"O0\")")
#define ENABLE_OPTIMIZATION  _Pragma("GCC pop_options")

// Function type macros.
#define VARARGS  /* Functions with variable arguments */
#ifdef PLATFORM_WIN32
#define DLL_IMPORT __declspec(dllimport)  /* Import function from DLL */
#define DLL_EXPORT __declspec(dllexport)  /* Export function to DLL */
#define CDECL
#define STDCALL
#else
#define DLL_IMPORT
#define DLL_EXPORT
#define CDECL
#define STDCALL
#define __cdecl
#define __stdcall
#endif

#ifdef UNREAL_STATIC
#undef DLL_IMPORT
#define DLL_IMPORT
#endif

// Variable arguments.
#define GET_VARARGS(msg,fmt)	\
{	\
	va_list ArgPtr;	\
	va_start( ArgPtr, fmt );	\
	vsprintf( msg, fmt, ArgPtr );	\
	va_end( ArgPtr );	\
}
#define GET_VARARGSR(msg,fmt,result)	\
{	\
	va_list ArgPtr;	\
	va_start( ArgPtr, fmt );	\
	result = vsprintf( msg, fmt, ArgPtr );	\
	va_end( ArgPtr );	\
}

// Compiler name.
#ifdef _DEBUG
	#define COMPILER "Compiled with GCC (Debug)"
#else
	#define COMPILER "Compiled with GCC"
#endif

// Bitfield alignment.
#define GCC_PACK(n) __attribute__((packed, aligned(n)))
#define GCC_ALIGN(n) __attribute__((aligned(n)))

// Hidden attribute, used for GPackage.
#ifdef PLATFORM_WIN32
#define GCC_HIDDEN
#elif !defined(UNREAL_STATIC)
#define GCC_HIDDEN __attribute__((visibility("hidden")))
#else
#define GCC_HIDDEN
#endif

#define GCC_USED __attribute__((used))

// Unsigned base types.
typedef uint16_t _WORD;  // 16-bit signed.
typedef uint64_t QWORD;  // 64-bit unsigned.

// Signed base types.
typedef char     CHAR;   // 8-bit  signed.
typedef int16_t  SWORD;  // 16-bit signed.
typedef int64_t  SQWORD; // 64-bit signed.

// Other base types.
typedef int32_t  UBOOL;  // Boolean 0 (false) or 1 (true).
typedef double   DOUBLE; // 64-bit IEEE double.

#ifndef PLATFORM_WIN32 // On Windows these are defined in minwindef.h.
// Unsigned base types.
typedef uint8_t  BYTE;   // 8-bit  unsigned.
typedef uint32_t DWORD;  // 32-bit unsigned.
// Signed base types.
typedef int32_t  INT;    // 32-bit signed.
typedef int64_t __int64; // 64-bit signed.
// Other base types.
typedef float    FLOAT;  // 32-bit IEEE floating point.
#endif

// If C++ exception handling is disabled, force guarding to be off.
#ifdef PLATFORM_NO_EXCEPTIONS
	#undef  DO_GUARD
	#undef  DO_SLOW_GUARD
	#define DO_GUARD 0
	#define DO_SLOW_GUARD 0
#endif

// Make sure characters are signed.
static_assert((char)-1 < 0, "char must be signed.");

// No VC++ asm.
#undef ASM
#define ASM 0

// FILE forward declaration.
#define USEEK_CUR SEEK_CUR
#define USEEK_END SEEK_END
#define USEEK_SET SEEK_SET

// Pathnames.
#ifdef PLATFORM_WIN32
#define PATH(s) s
#else
#define PATH(s) appUnixPath( s )
char* appUnixPath( const char* Path );
#endif

// NULL.
#ifndef NULL
#define NULL 0
#endif

// Platform-specific strings.
#ifdef PLATFORM_WIN32
#define LINE_TERMINATOR "\r\n"
#define PATH_SEPARATOR "\\"
#define DLLEXT ".dll"
#else
#define LINE_TERMINATOR "\n"
#define PATH_SEPARATOR "/"
#define DLLEXT ".so"
#endif

// Package implementation.
#ifdef UNREAL_STATIC
	#define IMPLEMENT_PACKAGE_PLATFORM(pkgname) \
		extern "C" {BYTE GCC_USED DLL_EXPORT GLoaded##pkgname;} \
		STATIC_EXPORT( GLoaded##pkgname, GLoaded##pkgname )
#elif defined(PLATFORM_WIN32)
	#define IMPLEMENT_PACKAGE_PLATFORM(pkgname) \
		extern "C" {HINSTANCE hInstance;} \
		INT __declspec(dllexport) __attribute__((stdcall)) DllMain( HINSTANCE hInInstance, DWORD Reason, void* Reserved ) \
		{ hInstance = hInInstance; return 1; }
#else
	#define IMPLEMENT_PACKAGE_PLATFORM(pkgname) \
		extern "C" {HINSTANCE hInstance;} \
		BYTE GLoaded##pkgname;
#endif

// Windows aliases for POSIX functions or types.
#ifndef PLATFORM_WIN32
#define _utime utime
#define _stat stat
#define _isnan(x) isnan(x)
#define stricmp(x, y) strcasecmp((x), (y))
#define strnicmp(x, y, n) strncasecmp((x), (y), (n))
#define _strnicmp(x, y, n) strncasecmp((x), (y), (n))
#endif

/*----------------------------------------------------------------------------
	Functions.
----------------------------------------------------------------------------*/

//
// CPU cycles, related to GSecondsPerCycle.
//
CORE_API DWORD appCycles();

//
// Seconds, arbitrarily based.
//
extern CORE_API DOUBLE GSecondsPerCycle;
CORE_API DOUBLE appSeconds();

//
// Sleep for seconds.
//
CORE_API void appSleep( FLOAT Sec );

//
// appGetGUID
//
void appGetGUID( void* GUID );

/*----------------------------------------------------------------------------
	Globals.
----------------------------------------------------------------------------*/

// System identification.
extern "C"
{
	extern HINSTANCE      hInstance;
	extern CORE_API UBOOL GIsMMX;
	extern CORE_API UBOOL GIsPentiumPro;
	extern CORE_API UBOOL GIsKatmai;
	extern CORE_API UBOOL GIsK6;
	extern CORE_API UBOOL GIsK63D;
}

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
