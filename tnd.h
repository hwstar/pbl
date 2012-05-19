/*
* tnd.h
*/

#ifndef TNDH

// For CCS compiler

#if defined(__PCM__)
typedef int1	u1;
typedef u1	bool;
typedef int8	u8;
typedef	int16	u16;
typedef int32	u32;

typedef signed int i8;
typedef signed long i16;
typedef signed long long i32;
typedef unsigned char uchar;

#else

typedef bit	u1;
typedef u1	bool;
typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned short long u24;
typedef unsigned long u32;

typedef char i8;
typedef int i16;
typedef short long i24;
typedef long i32;
typedef unsigned char uchar;

#endif

#define TRUE 1
#define FALSE 0
#define ON 1
#define OFF 0
#define PASS 0
#define FAIL 1

#define TNDH

#endif
