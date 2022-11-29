// civc header file.
//
// version: 0.0.1
#ifndef __CIV_H
#define __CIV_H

// Types and functions
#include <stdbool.h> // bool
#include <stdlib.h>  // size_t, NULL, malloc, exit, etc.
#include <stdint.h>  // usize16_t, etc
#include <string.h>  // (mem|str)(cmp|cpy|move|set), strlen
#include <setjmp.h>  // setjmp

// Testing and debuggin
#include <assert.h>  // assert
#include <stdio.h>   // printf
#include <errno.h>   // errno global

#include "constants.h"


#if UINTPTR_MAX == 0xFFFFFFFF
#define RSIZE   4
#else
#define RSIZE   8
#endif

#define ALIGN1      1
#define ALIGN_SLOT  RSIZE

#define not  !
#define and  &&
#define msk  &
#define or   ||
#define jn   |
#define R0   return 0;
#define RV   return;

#define eprintf(F, ...)   fprintf(stderr, F __VA_OPT__(,) __VA_ARGS__)

// TO asTO(FROM);
#define DEFINE_AS(FROM, TO) \
  TO* FROM ## _ ## as ## TO(FROM* f) { return (TO*) f; }

// #################################
// # Core Types and common methods

// Core Types
typedef uint8_t              U1;
typedef uint16_t             U2;
typedef uint32_t             U4;
typedef uint64_t             U8;
typedef size_t               Slot;

typedef int8_t               I1;
typedef int16_t              I2;
typedef int32_t              I4;
typedef int64_t              I8;
#if RSIZE == 4
typedef I4                   ISlot;
#else
typedef I8                   ISlot;
#endif

extern const U1* emptyNt; // empty null-terminated string

// ####
// # Core Structs
typedef struct { U1* dat; U2 len;                   } Slc;
typedef struct { U1* dat; U2 len; U2 cap;           } Buf;
typedef struct { U1* dat; U2 len; U2 cap; U2 plc;   } PlcBuf;
typedef struct { U1 count; U1 dat[];                } CStr;
typedef struct { U1* dat; U2 head; U2 tail; U2 cap; } Ring;

typedef struct _Sll {
  struct _Sll* next;
  void* dat;
} Sll;

typedef struct _Dll {
  struct _Dll* next;
  struct _Dll* prev;
  void* dat;
} Dll;

typedef struct { Dll* start; } DllRoot;

// ####
// # Core methods

// Get the required addition/subtraction to ptr to achieve alignment
Slot align(Slot ptr, U2 alignment);

// ##
// # Big Endian (unaligned) Fetch/Store
// Big endian store the largest values first. They are frequently used for
// networking code or where bytes are packed together without being aligned.
//
// The naming is confusing, since "big endian" makes you think the "big" value
// would be at the "end". This is not the case. The name was originally derived
// from Gulliver's travels where the "Liliputians", aka the Little
// Putians(Endians), break the little part of the egg on their "head". For an
// array, the start is also called the head. The Liliputians went to war with
// their neighbors (who broke the big side of the egg on their head) due to the
// difference of this custom.  Similarily, hardware architects at the dawn of
// computing were going to war over big/little endianness.
U4   ftBE(U1* p, Slot size);
void srBE(U1* p, Slot size, U4 value);

// ##
// # min/max
U4   U4_min(U4 a, U4 b);
Slot Slot_min(Slot a, Slot b);
U4   U4_max(U4 a, U4 b);
Slot Slot_max(Slot a, Slot b);

// #################################
// # Slc: data slice of up to 64KiB indexes (0x10,000)
// Slice is just a data pointer and a len.
Slc  Slc_frNt(U1* s); // from null-terminated str
Slc  Slc_frCStr(CStr* c);
I4   Slc_cmp(Slc a, Slc b);

#define _ntLit(STR)        .dat = STR, .len = sizeof(STR) - 1
#define Slc_ntLit(STR)     ((Slc){ _ntLit(STR) })
#define Buf_ntLit(STR)     ((PlcBuf) { _ntLit(STR), .cap = sizeof(STR) - 1 })
#define PlcBuf_ntLit(STR)  ((PlcBuf) { _ntLit(STR), .cap = sizeof(STR) - 1 })
#define Slc_lit(...)  (Slc){           \
    .dat = (U1[]){__VA_ARGS__},        \
    .len = sizeof((U1[]){__VA_ARGS__}) \
  }

// Use this with printf using the '%.*s' format, like so:
//
//   printf("Printing my slice: %.*s\n", Dat_printf(slc));
//
// This is safe to use with CStr, Buf and PlcBuf.
#define Dat_printf(DAT)   (DAT).len, (DAT).dat

// #################################
// # Buf + PlcBuf: buffers of up to 64KiB indexes (0x10,000)
// Buffers are a data pointer, a length (used data) and a capacity
// PlcBuf has an additional field "plc" to keep place while processing a buffer.
Slc* Buf_asSlc(Buf*);
Slc* PlcBuf_asSlc(PlcBuf*);
Buf* PlcBuf_asBuf(PlcBuf*);

// Attempt to extend Buf. Return true if there is not enough space.
bool Buf_extend(Buf* b, Slc s);
bool Buf_extendNt(Buf* b, U1* s);

// #################################
// # Ring: a lock-free ring buffer.
// Data is written to the tail and read from the head.

// The ring buffer is empty when head == tail and full when head + 1 == tail.
U2   Ring_len(Ring* r);

// Get the current character and advance the head.
// Returns NULL if empty.
// Typically this is used like:  while(( c = Ring_next(r) )) { ... }
U1*  Ring_next(Ring* r);

// These return true if the buffer is not large enough.
bool   Ring_push(Ring* r, U1 c);
bool   Ring_extend(Ring* r, Slc s);

// These are used for Ring_printf
Slc Ring_first(Ring* r);
Slc Ring_second(Ring* r);

// Remove dat[:plc], shifting data[plc:len] to the left.
//
// This is extremely useful when reading files: a few bytes (i.e. a word, a
// line) can be processed at a time, then File_read called again.
void PlcBuf_shift(PlcBuf*);

// CStr
bool CStr_varAssert(U4 line, U1* STR, U1* LEN);

// Declare a CStr global. It's your job to assert that the LEN is valid.
#define CStr_ntLitUnchecked(NAME, LEN, STR) \
  char _CStr_ ## NAME[1 + sizeof(STR)] = LEN STR; \
  CStr* NAME = (CStr*) _CStr_ ## NAME;

// Declare a CStr variable with auto-assert.
// The assert runs every time the function is called (not performant),
// but this is fine for one-off functions or tests.
#define CStr_ntVar(NAME, LEN, STR)      \
  static CStr_ntLitUnchecked(NAME, LEN, STR); \
  assert(CStr_varAssert(__LINE__, STR, LEN));

// #################################
// # Sll: Singly Linked List
void Sll_add(Sll** root, Sll* node);
Sll* Sll_pop(Sll** root);

// #################################
// # Dll: Doubly Linked List

// Add to next: to -> a ==> to -> b -> a
void Dll_add(Dll* to, Dll* node);

// Pop from next: from -> b -> a ==> from -> a (return b)
//
// Note: from->prev is never used, so the following is safe:
//
//   Dll* root = ...;
//   Dll* b = Dll_pop((Dll*)&root);
Dll* Dll_pop(Dll* from);

// Remove from chain: a -> node -> b ==> a -> b
Dll* Dll_remove(Dll* node);

// Add to root, preserving prev/next nodes.
//
//     root              root
//       v      ==>        v
// a <-> b           a <-> node <-> b
void DllRoot_add(DllRoot* root, Dll* node);

Dll* DllRoot_pop(DllRoot* root);

// #################################
// # Binary Search Tree
typedef struct _Bst {
  struct _Bst* l; struct _Bst* r;
  CStr* key;
} Bst;

I4   Bst_find(Bst** node, Slc slc);
Bst* Bst_add(Bst** root, Bst* add);

// #################################
// # Error Handling and Testing

#define TEST(NAME) \
  void test_ ## NAME () {              \
    jmp_buf localErrJmp;               \
    Civ_init();                        \
    civ.fb->errJmp = &localErrJmp;     \
    eprintf("## Testing " #NAME "...\n"); \
    if(setjmp(localErrJmp)) { civ.civErrPrinter(); exit(1); }
#define END_TEST  }

#define SET_ERR(E)  if(true) { \
  civ.fb->err = E; \
  eprintf("Longjmping to: %X\n", civ.fb->errJmp); \
  longjmp(*civ.fb->errJmp, 1); }
#define ASSERT(C, E)   if(!(C)) { SET_ERR(Slc_ntLit(E)); }
#define ASSERT_NO_ERR()    assert(!civ.fb->err)
#define TASSERT_EQ(EXPECT, CODE) if(1) { \
  typeof(EXPECT) __result = CODE; \
  if((EXPECT) != __result) eprintf("!!! Assertion failed: 0x%X == 0x%X\n", EXPECT, __result); \
  assert((EXPECT) == __result); }


// #################################
// # Methods and Roles

// Role Execute: Xr(myRole, meth, a, b) -> myRole.m.meth(myRole.d, a, b)
#define Xr(R, M, ...)    (R).m->M((R).d __VA_OPT__(,) __VA_ARGS__)

// Declare role method:
//   Role_METHOD(myFunc, U1, U2) -> void (*)(void*, U1, U2) myFunc
#define Role_METHOD(M, ...)      ((void (*)(void* __VA_OPT__(,) __VA_ARGS__)) M)
#define Role_METHODR(M, R, ...)  ((R    (*)(void* __VA_OPT__(,) __VA_ARGS__)) M)

// #################################
// # BA: Block Allocator
#define BLOCK_PO2  12
#define BLOCK_SIZE  (1<<BLOCK_PO2)
#define BLOCK_AVAIL (BLOCK_SIZE - (sizeof(U2) * 2))
#define BLOCK_END  0xFF

typedef struct { U2 bot; U2 top; } BlockInfo;

typedef struct {
  U1 dat[BLOCK_AVAIL];
  BlockInfo info;
} Block;

typedef struct _BANode {
  struct _BANode* next; struct _BANode* prev; // Dll
  Block* block;
} BANode;

typedef struct { BANode* free; Slot len; } BA;

Dll*     BANode_asDll(BANode* node);
DllRoot* BA_asDllRoot(BA* ba);

BANode* BA_alloc(BA* ba);
void BA_free(BA* ba, BANode* node);
void BA_freeAll(BA* ba, BANode* nodes);

// Free an array of nodes and blocks. Typically used to initialize BA.
void BA_freeArray(BA* ba, Slot len, BANode nodes[], Block blocks[]);

// #################################
// # Arena Role
typedef struct {
  void  (*drop)            (void* d);
  void* (*alloc)           (void* d, Slot sz, U2 alignment);
  void  (*free)            (void* d, void* dat, Slot sz, U2 alignment);
  Slot  (*maxAlloc)        (void* d);
} MArena;

typedef struct { const MArena* m; void* d; } Arena;

// Methods that depend on arena
Buf    Buf_new(Arena arena, U2 cap); // Note: check that buf.dat != NULL
PlcBuf PlcBuf_new(Arena arena, U2 cap);

// #################################
// # Resource Role
typedef struct {
  // Perform resource cleanup and drop any non-POD data (i.e. buffers, etc)
  // This must NOT free the `d` pointer itself.
  //
  // Return true if drop is done. Returning false means it will be called again
  // (although other resources may be called first).
  bool (*drop)            (void* d, Arena* a);
} MResource;

typedef struct { const MResource* m; void* d; } Resource;
Resource* Arena_asResource(Arena*);

// #################################
// # BBA: Block Bump Arena
// This is a bump arena. Allocations "bump" the len (unaligned) or cap (aligned)
// indexes. Frees reverse the bump and MUST be done in reverse order (or skipped
// if you drop the whole arena). Free must use the exact same arguments as
// alloc.
//
// This allocator is great for data that just grows and is rarely freed or the
// free-order is very controlled. This is probably the fastest possible
// arbitrary-sized allocator.
//
// Fngi uses this allocator for "growing" the code heap. Therefore,
// unaligned allocations always grow from top to bottom.

typedef struct { BA* ba; BANode* dat; } BBA;

DllRoot* BBA_asDllRoot(BBA* bba);
Arena    BBA_asArena(BBA* b);
#define  BBA_block(BBA) ((BBA)->dat->block)
#define  BBA_info(BBA)  (BBA_block(BBA)->info)

void   BBA_drop(BBA* bba);  // drop whole Arena
Slot   BBA_spare(BBA* bba); // get spare bytes

void*  BBA_alloc(BBA* bba, Slot sz, U2 alignment);
void   BBA_free (BBA* bba, void* data, Slot sz, U2 alignment);

Slot   BBA_maxAlloc(void* anything); // actually constant

extern const MArena mBBA;

// #################################
// # Civ Global Environment

typedef struct _Fiber {
  struct _Fiber* next;
  struct _Fiber* prev;
  jmp_buf*   errJmp;
  Arena*     arena;     // Global default arena
  Slc err;
} Fiber;

typedef struct {
  BA         ba;    // root block allocator
  Fiber*     fb;    // currently executing fiber

  // Misc (normally not set/read)
  Fiber rootFiber;
  void (*civErrPrinter)();
} Civ;

extern Civ civ;

void Civ_init();

// #################################
// # File
// Unlike many roles, the File role requires the data structure to follow the
// below. This is because interacting with files are inherently interacting with
// buffers.
//
// System-specific data can expand on this, such as storing the file name/etc
// as a child-class of File.
//
// The file uses a PlcBuf. Typically the user will ingest some number of bytes,
// moving buf.plc and then calling PlcBuf_shift to clear data and read more
// bytes from the file.

#define File_seek_SET  1 // seek from beginning
#define File_seek_CUR  2 // seek from current position
#define File_seek_END  3 // seek from end

typedef struct {
  Slot      pos;   // current position in file. If seek: desired position.
  Slot      fid;   // file id or reference
  PlcBuf   buf;   // buffer for reading or writing data
  U2       code;  // status or error (File_*)
} File;

typedef struct {
  // Resource methods
  bool (*drop) (File* d);

  // Close a file
  void (*close) (File* d);

  // Open a file. Platform must define File_(RDWR|RDONLY|WRONLY|TRUNC)
  void (*open)  (File* d, Slc path, Slot options);

  // Stop async operations (may be noop)
  void (*stop)  (File* d);

  // Seek in the file whence=File_seek_(SET|CUR|END)
  void (*seek)  (File* d, ISlot offset, U1 whence);

  // Read from a file into d buffer.
  void (*read)  (File* d);

  // Write to a file from d buffer.
  void (*write)(File* d);
} MFile;

typedef struct { const MFile* m; File* d; } RFile;  // Role
Resource* RFile_asResource(RFile*);

// If set it is a real "file index/id"
#define File_INDEX      ((Slot)1 << ((sizeof(Slot) * 8) - 1))

#define File_CLOSED   0x00

#define File_SEEKING  0x10
#define File_READING  0x11
#define File_WRITING  0x12
#define File_STOPPING 0x13

#define File_DONE     0xD0
#define File_STOPPED  0xD1
#define File_EOF      0xD2

#define File_ERROR    0xE0
#define File_EIO      0xE2
#endif // __CIV_H
