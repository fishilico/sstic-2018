#ifndef BLOCKCIPHER_WASM2C_H_GENERATED_
#define BLOCKCIPHER_WASM2C_H_GENERATED_
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "wasm-rt.h"

#ifndef WASM_RT_MODULE_PREFIX
#define WASM_RT_MODULE_PREFIX
#endif

#define WASM_RT_PASTE_(x, y) x ## y
#define WASM_RT_PASTE(x, y) WASM_RT_PASTE_(x, y)
#define WASM_RT_ADD_PREFIX(x) WASM_RT_PASTE(WASM_RT_MODULE_PREFIX, x)

#define WASM_RT_DEFINE_EXTERNAL(decl, target) decl = &target;

/* TODO(binji): only use stdint.h types in header */
typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;
typedef float f32;
typedef double f64;

extern void WASM_RT_ADD_PREFIX(init)(void);

/* import: 'env' 'memory' */
extern wasm_rt_memory_t (*Z_envZ_memory);
/* import: 'env' 'DYNAMICTOP_PTR' */
extern u32 (*Z_envZ_DYNAMICTOP_PTRZ_i);
/* import: 'env' 'STACKTOP' */
extern u32 (*Z_envZ_STACKTOPZ_i);
/* import: 'env' 'STACK_MAX' */
extern u32 (*Z_envZ_STACK_MAXZ_i);
/* import: 'env' 'enlargeMemory' */
extern u32 (*Z_envZ_enlargeMemoryZ_iv)(void);
/* import: 'env' 'getTotalMemory' */
extern u32 (*Z_envZ_getTotalMemoryZ_iv)(void);
/* import: 'env' 'abortOnCannotGrowMemory' */
extern u32 (*Z_envZ_abortOnCannotGrowMemoryZ_iv)(void);
/* import: 'env' '___setErrNo' */
extern void (*Z_envZ____setErrNoZ_vi)(u32);
/* import: 'env' '_emscripten_asm_const_ii' */
extern u32 (*Z_envZ__emscripten_asm_const_iiZ_iii)(u32, u32);
/* import: 'env' '_emscripten_memcpy_big' */
extern u32 (*Z_envZ__emscripten_memcpy_bigZ_iiii)(u32, u32, u32);

/* export: '___errno_location' */
extern u32 (*WASM_RT_ADD_PREFIX(Z____errno_locationZ_iv))(void);
/* export: '_decryptBlock' */
extern void (*WASM_RT_ADD_PREFIX(Z__decryptBlockZ_vii))(u32, u32);
/* export: '_free' */
extern void (*WASM_RT_ADD_PREFIX(Z__freeZ_vi))(u32);
/* export: '_getFlag' */
extern u32 (*WASM_RT_ADD_PREFIX(Z__getFlagZ_iii))(u32, u32);
/* export: '_malloc' */
extern u32 (*WASM_RT_ADD_PREFIX(Z__mallocZ_ii))(u32);
/* export: '_memcpy' */
extern u32 (*WASM_RT_ADD_PREFIX(Z__memcpyZ_iiii))(u32, u32, u32);
/* export: '_memset' */
extern u32 (*WASM_RT_ADD_PREFIX(Z__memsetZ_iiii))(u32, u32, u32);
/* export: '_sbrk' */
extern u32 (*WASM_RT_ADD_PREFIX(Z__sbrkZ_ii))(u32);
/* export: '_setDecryptKey' */
extern void (*WASM_RT_ADD_PREFIX(Z__setDecryptKeyZ_vii))(u32, u32);
/* export: 'establishStackSpace' */
extern void (*WASM_RT_ADD_PREFIX(Z_establishStackSpaceZ_vii))(u32, u32);
/* export: 'getTempRet0' */
extern u32 (*WASM_RT_ADD_PREFIX(Z_getTempRet0Z_iv))(void);
/* export: 'runPostSets' */
extern void (*WASM_RT_ADD_PREFIX(Z_runPostSetsZ_vv))(void);
/* export: 'setTempRet0' */
extern void (*WASM_RT_ADD_PREFIX(Z_setTempRet0Z_vi))(u32);
/* export: 'setThrew' */
extern void (*WASM_RT_ADD_PREFIX(Z_setThrewZ_vii))(u32, u32);
/* export: 'stackAlloc' */
extern u32 (*WASM_RT_ADD_PREFIX(Z_stackAllocZ_ii))(u32);
/* export: 'stackRestore' */
extern void (*WASM_RT_ADD_PREFIX(Z_stackRestoreZ_vi))(u32);
/* export: 'stackSave' */
extern u32 (*WASM_RT_ADD_PREFIX(Z_stackSaveZ_iv))(void);
#ifdef __cplusplus
}
#endif

#endif  /* BLOCKCIPHER_WASM2C_H_GENERATED_ */
