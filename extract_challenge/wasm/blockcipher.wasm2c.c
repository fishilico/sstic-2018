#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "blockcipher.wasm2c.h"
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define LIKELY(x) __builtin_expect(!!(x), 1)

#define TRAP(x) (wasm_rt_trap(WASM_RT_TRAP_##x), 0)

#define FUNC_PROLOGUE                                            \
  if (++wasm_rt_call_stack_depth > WASM_RT_MAX_CALL_STACK_DEPTH) \
    TRAP(EXHAUSTION)

#define FUNC_EPILOGUE --wasm_rt_call_stack_depth

#define UNREACHABLE TRAP(UNREACHABLE)

#define CALL_INDIRECT(table, t, ft, x, ...)          \
  (LIKELY((x) < table.size && table.data[x].func &&  \
          table.data[x].func_type == func_types[ft]) \
       ? ((t)table.data[x].func)(__VA_ARGS__)        \
       : TRAP(CALL_INDIRECT))

#define MEMCHECK(mem, a, t)  \
  if (UNLIKELY((a) + sizeof(t) > mem->size)) TRAP(OOB)

#define DEFINE_LOAD(name, t1, t2, t3)              \
  static inline t3 name(wasm_rt_memory_t* mem, u64 addr) {   \
    MEMCHECK(mem, addr, t1);                       \
    t1 result;                                     \
    memcpy(&result, &mem->data[addr], sizeof(t1)); \
    return (t3)(t2)result;                         \
  }

#define DEFINE_STORE(name, t1, t2)                           \
  static inline void name(wasm_rt_memory_t* mem, u64 addr, t2 value) { \
    MEMCHECK(mem, addr, t1);                                 \
    t1 wrapped = (t1)value;                                  \
    memcpy(&mem->data[addr], &wrapped, sizeof(t1));          \
  }

DEFINE_LOAD(i32_load, u32, u32, u32);
DEFINE_LOAD(i64_load, u64, u64, u64);
DEFINE_LOAD(f32_load, f32, f32, f32);
DEFINE_LOAD(f64_load, f64, f64, f64);
DEFINE_LOAD(i32_load8_s, s8, s32, u32);
DEFINE_LOAD(i64_load8_s, s8, s64, u64);
DEFINE_LOAD(i32_load8_u, u8, u32, u32);
DEFINE_LOAD(i64_load8_u, u8, u64, u64);
DEFINE_LOAD(i32_load16_s, s16, s32, u32);
DEFINE_LOAD(i64_load16_s, s16, s64, u64);
DEFINE_LOAD(i32_load16_u, u16, u32, u32);
DEFINE_LOAD(i64_load16_u, u16, u64, u64);
DEFINE_LOAD(i64_load32_s, s32, s64, u64);
DEFINE_LOAD(i64_load32_u, u32, u64, u64);
DEFINE_STORE(i32_store, u32, u32);
DEFINE_STORE(i64_store, u64, u64);
DEFINE_STORE(f32_store, f32, f32);
DEFINE_STORE(f64_store, f64, f64);
DEFINE_STORE(i32_store8, u8, u32);
DEFINE_STORE(i32_store16, u16, u32);
DEFINE_STORE(i64_store8, u8, u64);
DEFINE_STORE(i64_store16, u16, u64);
DEFINE_STORE(i64_store32, u32, u64);

#define I32_CLZ(x) ((x) ? __builtin_clz(x) : 32)
#define I64_CLZ(x) ((x) ? __builtin_clzll(x) : 64)
#define I32_CTZ(x) ((x) ? __builtin_ctz(x) : 32)
#define I64_CTZ(x) ((x) ? __builtin_ctzll(x) : 64)
#define I32_POPCNT(x) (__builtin_popcount(x))
#define I64_POPCNT(x) (__builtin_popcountll(x))

#define DIV_S(ut, min, x, y)                                 \
   ((UNLIKELY((y) == 0)) ?                TRAP(DIV_BY_ZERO)  \
  : (UNLIKELY((x) == min && (y) == -1)) ? TRAP(INT_OVERFLOW) \
  : (ut)((x) / (y)))

#define REM_S(ut, min, x, y)                                \
   ((UNLIKELY((y) == 0)) ?                TRAP(DIV_BY_ZERO) \
  : (UNLIKELY((x) == min && (y) == -1)) ? 0                 \
  : (ut)((x) % (y)))

#define I32_DIV_S(x, y) DIV_S(u32, INT32_MIN, (s32)x, (s32)y)
#define I64_DIV_S(x, y) DIV_S(u64, INT64_MIN, (s64)x, (s64)y)
#define I32_REM_S(x, y) REM_S(u32, INT32_MIN, (s32)x, (s32)y)
#define I64_REM_S(x, y) REM_S(u64, INT64_MIN, (s64)x, (s64)y)

#define DIVREM_U(op, x, y) \
  ((UNLIKELY((y) == 0)) ? TRAP(DIV_BY_ZERO) : ((x) op (y)))

#define DIV_U(x, y) DIVREM_U(/, x, y)
#define REM_U(x, y) DIVREM_U(%, x, y)

#define ROTL(x, y, mask) \
  (((x) << ((y) & (mask))) | ((x) >> (((mask) - (y) + 1) & (mask))))
#define ROTR(x, y, mask) \
  (((x) >> ((y) & (mask))) | ((x) << (((mask) - (y) + 1) & (mask))))

#define I32_ROTL(x, y) ROTL(x, y, 31)
#define I64_ROTL(x, y) ROTL(x, y, 63)
#define I32_ROTR(x, y) ROTR(x, y, 31)
#define I64_ROTR(x, y) ROTR(x, y, 63)

#define FMIN(x, y)                                          \
   ((UNLIKELY((x) != (x))) ? NAN                            \
  : (UNLIKELY((y) != (y))) ? NAN                            \
  : (UNLIKELY((x) == 0 && (y) == 0)) ? (signbit(x) ? x : y) \
  : (x < y) ? x : y)

#define FMAX(x, y)                                          \
   ((UNLIKELY((x) != (x))) ? NAN                            \
  : (UNLIKELY((y) != (y))) ? NAN                            \
  : (UNLIKELY((x) == 0 && (y) == 0)) ? (signbit(x) ? y : x) \
  : (x > y) ? x : y)

#define TRUNC_S(ut, st, ft, min, max, maxop, x)                             \
   ((UNLIKELY((x) != (x))) ? TRAP(INVALID_CONVERSION)                       \
  : (UNLIKELY((x) < (ft)(min) || (x) maxop (ft)(max))) ? TRAP(INT_OVERFLOW) \
  : (ut)(st)(x))

#define I32_TRUNC_S_F32(x) TRUNC_S(u32, s32, f32, INT32_MIN, INT32_MAX, >=, x)
#define I64_TRUNC_S_F32(x) TRUNC_S(u64, s64, f32, INT64_MIN, INT64_MAX, >=, x)
#define I32_TRUNC_S_F64(x) TRUNC_S(u32, s32, f64, INT32_MIN, INT32_MAX, >,  x)
#define I64_TRUNC_S_F64(x) TRUNC_S(u64, s64, f64, INT64_MIN, INT64_MAX, >=, x)

#define TRUNC_U(ut, ft, max, maxop, x)                                    \
   ((UNLIKELY((x) != (x))) ? TRAP(INVALID_CONVERSION)                     \
  : (UNLIKELY((x) <= (ft)-1 || (x) maxop (ft)(max))) ? TRAP(INT_OVERFLOW) \
  : (ut)(x))

#define I32_TRUNC_U_F32(x) TRUNC_U(u32, f32, UINT32_MAX, >=, x)
#define I64_TRUNC_U_F32(x) TRUNC_U(u64, f32, UINT64_MAX, >=, x)
#define I32_TRUNC_U_F64(x) TRUNC_U(u32, f64, UINT32_MAX, >,  x)
#define I64_TRUNC_U_F64(x) TRUNC_U(u64, f64, UINT64_MAX, >=, x)

#define DEFINE_REINTERPRET(name, t1, t2)  \
  static inline t2 name(t1 x) {           \
    t2 result;                            \
    memcpy(&result, &x, sizeof(result));  \
    return result;                        \
  }

DEFINE_REINTERPRET(f32_reinterpret_i32, u32, f32)
DEFINE_REINTERPRET(i32_reinterpret_f32, f32, u32)
DEFINE_REINTERPRET(f64_reinterpret_i64, u64, f64)
DEFINE_REINTERPRET(i64_reinterpret_f64, f64, u64)


static u32 func_types[8];

static void init_func_types(void) {
  func_types[0] = wasm_rt_register_func_type(0, 1, WASM_RT_I32);
  func_types[1] = wasm_rt_register_func_type(1, 0, WASM_RT_I32);
  func_types[2] = wasm_rt_register_func_type(2, 1, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32);
  func_types[3] = wasm_rt_register_func_type(3, 1, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32);
  func_types[4] = wasm_rt_register_func_type(1, 1, WASM_RT_I32, WASM_RT_I32);
  func_types[5] = wasm_rt_register_func_type(2, 0, WASM_RT_I32, WASM_RT_I32);
  func_types[6] = wasm_rt_register_func_type(4, 1, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32);
  func_types[7] = wasm_rt_register_func_type(0, 0);
}

static u32 stackAlloc(u32);
static u32 stackSave(void);
static void stackRestore(u32);
static void establishStackSpace(u32, u32);
static void setThrew(u32, u32);
static void setTempRet0(u32);
static u32 getTempRet0(void);
static void f13(u32, u32);
static void _setDecryptKey(u32, u32);
static void _decryptBlock(u32, u32);
static u32 f16(u32, u32, u32, u32);
static u32 f17(u32);
static u32 _getFlag(u32, u32);
static u32 _malloc(u32);
static void _free(u32);
static u32 f21(u32, u32);
static u32 ___errno_location(void);
static void runPostSets(void);
static u32 _memcpy(u32, u32, u32);
static u32 _memset(u32, u32, u32);
static u32 _sbrk(u32);

static u32 g3;
static u32 g4;
static u32 g5;
static u32 g6;
static u32 g7;
static u32 g8;

static void init_globals(void) {
  g3 = (*Z_envZ_DYNAMICTOP_PTRZ_i);
  g4 = (*Z_envZ_STACKTOPZ_i);
  g5 = (*Z_envZ_STACK_MAXZ_i);
  g6 = 0u;
  g7 = 0u;
  g8 = 0u;
}

static u32 stackAlloc(u32 p0) {
  u32 l0 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1;
  i0 = g4;
  l0 = i0;
  i0 = g4;
  i1 = p0;
  i0 += i1;
  g4 = i0;
  i0 = g4;
  i1 = 15u;
  i0 += i1;
  i1 = 4294967280u;
  i0 &= i1;
  g4 = i0;
  i0 = l0;
  FUNC_EPILOGUE;
  return i0;
}

static u32 stackSave(void) {
  FUNC_PROLOGUE;
  u32 i0;
  i0 = g4;
  FUNC_EPILOGUE;
  return i0;
}

static void stackRestore(u32 p0) {
  FUNC_PROLOGUE;
  u32 i0;
  i0 = p0;
  g4 = i0;
  FUNC_EPILOGUE;
}

static void establishStackSpace(u32 p0, u32 p1) {
  FUNC_PROLOGUE;
  u32 i0;
  i0 = p0;
  g4 = i0;
  i0 = p1;
  g5 = i0;
  FUNC_EPILOGUE;
}

static void setThrew(u32 p0, u32 p1) {
  FUNC_PROLOGUE;
  u32 i0;
  i0 = g6;
  i0 = !(i0);
  if (i0) {
    i0 = p0;
    g6 = i0;
    i0 = p1;
    g7 = i0;
  }
  FUNC_EPILOGUE;
}

static void setTempRet0(u32 p0) {
  FUNC_PROLOGUE;
  u32 i0;
  i0 = p0;
  g8 = i0;
  FUNC_EPILOGUE;
}

static u32 getTempRet0(void) {
  FUNC_PROLOGUE;
  u32 i0;
  i0 = g8;
  FUNC_EPILOGUE;
  return i0;
}

static void f13(u32 p0, u32 p1) {
  u32 l0 = 0, l1 = 0, l2 = 0, l3 = 0, l4 = 0, l5 = 0, l6 = 0, l7 = 0, 
      l8 = 0, l9 = 0, l10 = 0, l11 = 0, l12 = 0, l13 = 0, l14 = 0, l15 = 0, 
      l16 = 0, l17 = 0, l18 = 0, l19 = 0, l20 = 0, l21 = 0, l22 = 0, l23 = 0, 
      l24 = 0, l25 = 0, l26 = 0, l27 = 0;
  u64 l28 = 0, l29 = 0, l30 = 0, l31 = 0, l32 = 0, l33 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  u64 j0, j1, j2, j3;
  i0 = g4;
  l6 = i0;
  i0 = g4;
  i1 = 64u;
  i0 += i1;
  g4 = i0;
  i0 = l6;
  i1 = 32u;
  i0 += i1;
  l2 = i0;
  i1 = p1;
  j1 = i64_load(Z_envZ_memory, (u64)(i1));
  i64_store(Z_envZ_memory, (u64)(i0), j1);
  i0 = l2;
  i1 = p1;
  j1 = i64_load(Z_envZ_memory, (u64)(i1 + 8));
  i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
  i0 = l6;
  i1 = 16u;
  i0 += i1;
  l1 = i0;
  i1 = p1;
  i2 = 16u;
  i1 += i2;
  p1 = i1;
  j1 = i64_load(Z_envZ_memory, (u64)(i1));
  i64_store(Z_envZ_memory, (u64)(i0), j1);
  i0 = l1;
  i1 = p1;
  j1 = i64_load(Z_envZ_memory, (u64)(i1 + 8));
  i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
  i0 = p0;
  i1 = l2;
  j1 = i64_load(Z_envZ_memory, (u64)(i1));
  l30 = j1;
  i64_store(Z_envZ_memory, (u64)(i0), j1);
  i0 = p0;
  i1 = l2;
  j1 = i64_load(Z_envZ_memory, (u64)(i1 + 8));
  l31 = j1;
  i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
  i0 = p0;
  i1 = l1;
  j1 = i64_load(Z_envZ_memory, (u64)(i1));
  l32 = j1;
  i64_store(Z_envZ_memory, (u64)(i0 + 16), j1);
  i0 = p0;
  i1 = l1;
  j1 = i64_load(Z_envZ_memory, (u64)(i1 + 8));
  l33 = j1;
  i64_store(Z_envZ_memory, (u64)(i0 + 24), j1);
  i0 = l6;
  i1 = 48u;
  i0 += i1;
  l3 = i0;
  i1 = 8u;
  i0 += i1;
  l18 = i0;
  i0 = l3;
  i1 = 15u;
  i0 += i1;
  l12 = i0;
  i0 = l6;
  l0 = i0;
  i1 = 8u;
  i0 += i1;
  l9 = i0;
  i0 = l0;
  i1 = 1u;
  i0 += i1;
  l19 = i0;
  i0 = l0;
  i1 = 2u;
  i0 += i1;
  l20 = i0;
  i0 = l0;
  i1 = 3u;
  i0 += i1;
  l21 = i0;
  i0 = l0;
  i1 = 4u;
  i0 += i1;
  l22 = i0;
  i0 = l0;
  i1 = 5u;
  i0 += i1;
  l23 = i0;
  i0 = l0;
  i1 = 6u;
  i0 += i1;
  l24 = i0;
  i0 = l0;
  i1 = 7u;
  i0 += i1;
  l25 = i0;
  i0 = l0;
  i1 = 8u;
  i0 += i1;
  l26 = i0;
  i0 = l0;
  i1 = 9u;
  i0 += i1;
  l27 = i0;
  i0 = l0;
  i1 = 10u;
  i0 += i1;
  l13 = i0;
  i0 = l0;
  i1 = 11u;
  i0 += i1;
  l14 = i0;
  i0 = l0;
  i1 = 12u;
  i0 += i1;
  l15 = i0;
  i0 = l0;
  i1 = 13u;
  i0 += i1;
  l16 = i0;
  i0 = l0;
  i1 = 14u;
  i0 += i1;
  l17 = i0;
  i0 = l0;
  i1 = 15u;
  i0 += i1;
  l10 = i0;
  i0 = 1u;
  l7 = i0;
  L0: 
    i0 = l3;
    j1 = 0ull;
    i64_store(Z_envZ_memory, (u64)(i0), j1);
    i0 = l3;
    j1 = 0ull;
    i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
    i0 = l12;
    i1 = l7;
    i2 = 255u;
    i1 &= i2;
    l2 = i1;
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0u;
    l8 = i0;
    L1: 
      i0 = 14u;
      l1 = i0;
      L2: 
        i0 = l3;
        i1 = l1;
        i2 = 1u;
        i1 += i2;
        i0 += i1;
        i1 = l3;
        i2 = l1;
        i1 += i2;
        i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
        p1 = i1;
        i32_store8(Z_envZ_memory, (u64)(i0), i1);
        i0 = 0u;
        l4 = i0;
        i0 = l1;
        i1 = 1305u;
        i0 += i1;
        i0 = i32_load8_s(Z_envZ_memory, (u64)(i0));
        l5 = i0;
        L3: 
          i0 = l5;
          i1 = 1u;
          i0 &= i1;
          if (i0) {
            i0 = p1;
          } else {
            i0 = 0u;
          }
          i1 = l4;
          i0 ^= i1;
          l4 = i0;
          i0 = p1;
          i1 = 255u;
          i0 &= i1;
          p1 = i0;
          i1 = 1u;
          i0 <<= (i1 & 31);
          l11 = i0;
          i0 = p1;
          i1 = 128u;
          i0 &= i1;
          if (i0) {
            i0 = 195u;
          } else {
            i0 = 0u;
          }
          i1 = l11;
          i0 ^= i1;
          i1 = 255u;
          i0 &= i1;
          p1 = i0;
          i0 = l5;
          i1 = 255u;
          i0 &= i1;
          i1 = 1u;
          i0 >>= (i1 & 31);
          l5 = i0;
          if (i0) {goto L3;}
        i0 = l4;
        i1 = l2;
        i0 ^= i1;
        l2 = i0;
        i0 = l1;
        i1 = 4294967295u;
        i0 += i1;
        p1 = i0;
        i0 = l1;
        i1 = 0u;
        i0 = (u32)((s32)i0 > (s32)i1);
        if (i0) {
          i0 = p1;
          l1 = i0;
          goto L2;
        }
      i0 = l3;
      i1 = l2;
      i32_store8(Z_envZ_memory, (u64)(i0), i1);
      i0 = l8;
      i1 = 1u;
      i0 += i1;
      l8 = i0;
      i1 = 16u;
      i0 = i0 != i1;
      if (i0) {
        i0 = l12;
        i0 = i32_load8_s(Z_envZ_memory, (u64)(i0));
        l2 = i0;
        goto L1;
      }
    i0 = l3;
    j0 = i64_load(Z_envZ_memory, (u64)(i0));
    j1 = l30;
    j0 ^= j1;
    l28 = j0;
    i0 = l9;
    i1 = l18;
    j1 = i64_load(Z_envZ_memory, (u64)(i1));
    j2 = l31;
    j1 ^= j2;
    l29 = j1;
    i64_store(Z_envZ_memory, (u64)(i0), j1);
    i0 = l0;
    i1 = 0u;
    j2 = l28;
    i2 = (u32)(j2);
    i3 = 255u;
    i2 &= i3;
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l19;
    i1 = 0u;
    j2 = l28;
    j3 = 8ull;
    j2 >>= (j3 & 63);
    i2 = (u32)(j2);
    i3 = 255u;
    i2 &= i3;
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l20;
    i1 = 0u;
    j2 = l28;
    j3 = 16ull;
    j2 >>= (j3 & 63);
    i2 = (u32)(j2);
    i3 = 255u;
    i2 &= i3;
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l21;
    i1 = 0u;
    j2 = l28;
    j3 = 24ull;
    j2 >>= (j3 & 63);
    i2 = (u32)(j2);
    i3 = 255u;
    i2 &= i3;
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l22;
    i1 = 0u;
    j2 = l28;
    j3 = 32ull;
    j2 >>= (j3 & 63);
    i2 = (u32)(j2);
    i3 = 255u;
    i2 &= i3;
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l23;
    i1 = 0u;
    j2 = l28;
    j3 = 40ull;
    j2 >>= (j3 & 63);
    i2 = (u32)(j2);
    i3 = 255u;
    i2 &= i3;
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l24;
    i1 = 0u;
    j2 = l28;
    j3 = 48ull;
    j2 >>= (j3 & 63);
    i2 = (u32)(j2);
    i3 = 255u;
    i2 &= i3;
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l25;
    i1 = 0u;
    j2 = l28;
    j3 = 56ull;
    j2 >>= (j3 & 63);
    i2 = (u32)(j2);
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l26;
    i1 = 0u;
    j2 = l29;
    i2 = (u32)(j2);
    i3 = 255u;
    i2 &= i3;
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l27;
    i1 = 0u;
    j2 = l29;
    j3 = 8ull;
    j2 >>= (j3 & 63);
    i2 = (u32)(j2);
    i3 = 255u;
    i2 &= i3;
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l13;
    i1 = 0u;
    i2 = l13;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l14;
    i1 = 0u;
    i2 = l14;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l15;
    i1 = 0u;
    i2 = l15;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l16;
    i1 = 0u;
    i2 = l16;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l17;
    i1 = 0u;
    i2 = l17;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l10;
    i1 = 0u;
    i2 = l10;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i2 = 255u;
    i1 &= i2;
    l2 = i1;
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0u;
    l8 = i0;
    L8: 
      i0 = 14u;
      l1 = i0;
      L9: 
        i0 = l0;
        i1 = l1;
        i2 = 1u;
        i1 += i2;
        i0 += i1;
        i1 = l0;
        i2 = l1;
        i1 += i2;
        i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
        p1 = i1;
        i32_store8(Z_envZ_memory, (u64)(i0), i1);
        i0 = 0u;
        l4 = i0;
        i0 = l1;
        i1 = 1305u;
        i0 += i1;
        i0 = i32_load8_s(Z_envZ_memory, (u64)(i0));
        l5 = i0;
        L10: 
          i0 = l5;
          i1 = 1u;
          i0 &= i1;
          if (i0) {
            i0 = p1;
          } else {
            i0 = 0u;
          }
          i1 = l4;
          i0 ^= i1;
          l4 = i0;
          i0 = p1;
          i1 = 255u;
          i0 &= i1;
          p1 = i0;
          i1 = 1u;
          i0 <<= (i1 & 31);
          l11 = i0;
          i0 = p1;
          i1 = 128u;
          i0 &= i1;
          if (i0) {
            i0 = 195u;
          } else {
            i0 = 0u;
          }
          i1 = l11;
          i0 ^= i1;
          i1 = 255u;
          i0 &= i1;
          p1 = i0;
          i0 = l5;
          i1 = 255u;
          i0 &= i1;
          i1 = 1u;
          i0 >>= (i1 & 31);
          l5 = i0;
          if (i0) {goto L10;}
        i0 = l4;
        i1 = l2;
        i0 ^= i1;
        l2 = i0;
        i0 = l1;
        i1 = 4294967295u;
        i0 += i1;
        p1 = i0;
        i0 = l1;
        i1 = 0u;
        i0 = (u32)((s32)i0 > (s32)i1);
        if (i0) {
          i0 = p1;
          l1 = i0;
          goto L9;
        }
      i0 = l0;
      i1 = l2;
      i32_store8(Z_envZ_memory, (u64)(i0), i1);
      i0 = l8;
      i1 = 1u;
      i0 += i1;
      l8 = i0;
      i1 = 16u;
      i0 = i0 != i1;
      if (i0) {
        i0 = l10;
        i0 = i32_load8_s(Z_envZ_memory, (u64)(i0));
        l2 = i0;
        goto L8;
      }
    i0 = l0;
    i1 = l0;
    j1 = i64_load(Z_envZ_memory, (u64)(i1));
    j2 = l32;
    j1 ^= j2;
    l28 = j1;
    i64_store(Z_envZ_memory, (u64)(i0), j1);
    i0 = l9;
    i1 = l9;
    j1 = i64_load(Z_envZ_memory, (u64)(i1));
    j2 = l33;
    j1 ^= j2;
    l29 = j1;
    i64_store(Z_envZ_memory, (u64)(i0), j1);
    i0 = l7;
    i1 = 7u;
    i0 &= i1;
    i0 = !(i0);
    if (i0) {
      i0 = p0;
      i1 = l7;
      i2 = 2u;
      i1 = (u32)((s32)i1 >> (i2 & 31));
      p1 = i1;
      i2 = 4u;
      i1 <<= (i2 & 31);
      i0 += i1;
      j1 = l28;
      i64_store(Z_envZ_memory, (u64)(i0), j1);
      i0 = p0;
      i1 = p1;
      i2 = 4u;
      i1 <<= (i2 & 31);
      i0 += i1;
      j1 = l29;
      i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
      i0 = p0;
      i1 = p1;
      i2 = 1u;
      i1 += i2;
      p1 = i1;
      i2 = 4u;
      i1 <<= (i2 & 31);
      i0 += i1;
      j1 = l30;
      i64_store(Z_envZ_memory, (u64)(i0), j1);
      i0 = p0;
      i1 = p1;
      i2 = 4u;
      i1 <<= (i2 & 31);
      i0 += i1;
      j1 = l31;
      i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
    }
    i0 = l7;
    i1 = 1u;
    i0 += i1;
    l7 = i0;
    i1 = 33u;
    i0 = i0 != i1;
    if (i0) {
      j0 = l31;
      l33 = j0;
      j0 = l30;
      l32 = j0;
      j0 = l28;
      l30 = j0;
      j0 = l29;
      l31 = j0;
      goto L0;
    }
  i0 = l6;
  g4 = i0;
  FUNC_EPILOGUE;
}

static void _setDecryptKey(u32 p0, u32 p1) {
  FUNC_PROLOGUE;
  u32 i0, i1;
  i0 = p0;
  i1 = p1;
  f13(i0, i1);
  FUNC_EPILOGUE;
}

static void _decryptBlock(u32 p0, u32 p1) {
  u32 l0 = 0, l1 = 0, l2 = 0, l3 = 0, l4 = 0, l5 = 0, l6 = 0, l7 = 0, 
      l8 = 0, l9 = 0, l10 = 0, l11 = 0, l12 = 0, l13 = 0, l14 = 0, l15 = 0, 
      l16 = 0, l17 = 0, l18 = 0, l19 = 0, l20 = 0, l21 = 0, l22 = 0, l23 = 0, 
      l24 = 0, l25 = 0, l26 = 0;
  u64 l27 = 0, l28 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3, i4;
  u64 j1, j2;
  i0 = g4;
  l23 = i0;
  i0 = g4;
  i1 = 16u;
  i0 += i1;
  g4 = i0;
  i0 = l23;
  l0 = i0;
  i1 = p0;
  j1 = i64_load(Z_envZ_memory, (u64)(i1 + 144));
  i2 = p1;
  j2 = i64_load(Z_envZ_memory, (u64)(i2));
  j1 ^= j2;
  i64_store(Z_envZ_memory, (u64)(i0), j1);
  i0 = l0;
  i1 = 8u;
  i0 += i1;
  l4 = i0;
  i1 = p0;
  j1 = i64_load(Z_envZ_memory, (u64)(i1 + 152));
  i2 = p1;
  i3 = 8u;
  i2 += i3;
  l26 = i2;
  j2 = i64_load(Z_envZ_memory, (u64)(i2));
  j1 ^= j2;
  i64_store(Z_envZ_memory, (u64)(i0), j1);
  i0 = l0;
  i1 = 15u;
  i0 += i1;
  l3 = i0;
  i0 = l0;
  i1 = 1u;
  i0 += i1;
  l5 = i0;
  i0 = l0;
  i1 = 2u;
  i0 += i1;
  l6 = i0;
  i0 = l0;
  i1 = 3u;
  i0 += i1;
  l7 = i0;
  i0 = l0;
  i1 = 4u;
  i0 += i1;
  l8 = i0;
  i0 = l0;
  i1 = 5u;
  i0 += i1;
  l9 = i0;
  i0 = l0;
  i1 = 6u;
  i0 += i1;
  l10 = i0;
  i0 = l0;
  i1 = 7u;
  i0 += i1;
  l11 = i0;
  i0 = l0;
  i1 = 8u;
  i0 += i1;
  l12 = i0;
  i0 = l0;
  i1 = 9u;
  i0 += i1;
  l13 = i0;
  i0 = l0;
  i1 = 10u;
  i0 += i1;
  l14 = i0;
  i0 = l0;
  i1 = 11u;
  i0 += i1;
  l15 = i0;
  i0 = l0;
  i1 = 12u;
  i0 += i1;
  l16 = i0;
  i0 = l0;
  i1 = 13u;
  i0 += i1;
  l17 = i0;
  i0 = l0;
  i1 = 14u;
  i0 += i1;
  l18 = i0;
  i0 = 8u;
  l2 = i0;
  L0: 
    i0 = 0u;
    l24 = i0;
    L1: 
      i0 = 0u;
      l1 = i0;
      i0 = l0;
      i0 = i32_load8_s(Z_envZ_memory, (u64)(i0));
      l19 = i0;
      L2: 
        i0 = l0;
        i1 = l1;
        i0 += i1;
        i1 = l0;
        i2 = l1;
        i3 = 1u;
        i2 += i3;
        l25 = i2;
        i1 += i2;
        i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
        l20 = i1;
        i32_store8(Z_envZ_memory, (u64)(i0), i1);
        i0 = 0u;
        l21 = i0;
        i0 = l1;
        i1 = 1305u;
        i0 += i1;
        i0 = i32_load8_s(Z_envZ_memory, (u64)(i0));
        l22 = i0;
        i0 = l20;
        l1 = i0;
        L3: 
          i0 = l22;
          i1 = 1u;
          i0 &= i1;
          if (i0) {
            i0 = l1;
          } else {
            i0 = 0u;
          }
          i1 = l21;
          i0 ^= i1;
          l21 = i0;
          i0 = l1;
          i1 = 255u;
          i0 &= i1;
          l1 = i0;
          i1 = 1u;
          i0 <<= (i1 & 31);
          l20 = i0;
          i0 = l1;
          i1 = 128u;
          i0 &= i1;
          if (i0) {
            i0 = 195u;
          } else {
            i0 = 0u;
          }
          i1 = l20;
          i0 ^= i1;
          i1 = 255u;
          i0 &= i1;
          l1 = i0;
          i0 = l22;
          i1 = 255u;
          i0 &= i1;
          i1 = 1u;
          i0 >>= (i1 & 31);
          l22 = i0;
          if (i0) {goto L3;}
        i0 = l21;
        i1 = l19;
        i0 ^= i1;
        l19 = i0;
        i0 = l25;
        i1 = 15u;
        i0 = i0 != i1;
        if (i0) {
          i0 = l25;
          l1 = i0;
          goto L2;
        }
      i0 = l3;
      i1 = l19;
      i32_store8(Z_envZ_memory, (u64)(i0), i1);
      i0 = l24;
      i1 = 1u;
      i0 += i1;
      l24 = i0;
      i1 = 16u;
      i0 = i0 != i1;
      if (i0) {goto L1;}
    i0 = l0;
    i1 = 0u;
    i2 = l0;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l5;
    i1 = 0u;
    i2 = l5;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l6;
    i1 = 0u;
    i2 = l6;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l7;
    i1 = 0u;
    i2 = l7;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l8;
    i1 = 0u;
    i2 = l8;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l9;
    i1 = 0u;
    i2 = l9;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l10;
    i1 = 0u;
    i2 = l10;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l11;
    i1 = 0u;
    i2 = l11;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l12;
    i1 = 0u;
    i2 = l12;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l13;
    i1 = 0u;
    i2 = l13;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l14;
    i1 = 0u;
    i2 = l14;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l15;
    i1 = 0u;
    i2 = l15;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l16;
    i1 = 0u;
    i2 = l16;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l17;
    i1 = 0u;
    i2 = l17;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l18;
    i1 = 0u;
    i2 = l18;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l3;
    i1 = 0u;
    i2 = l3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i3 = 1024u;
    i2 += i3;
    i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i32_store8(Z_envZ_memory, (u64)(i0), i1);
    i0 = l0;
    i1 = l0;
    j1 = i64_load(Z_envZ_memory, (u64)(i1));
    i2 = p0;
    i3 = l2;
    i4 = 4u;
    i3 <<= (i4 & 31);
    i2 += i3;
    j2 = i64_load(Z_envZ_memory, (u64)(i2));
    j1 ^= j2;
    l27 = j1;
    i64_store(Z_envZ_memory, (u64)(i0), j1);
    i0 = l4;
    i1 = l4;
    j1 = i64_load(Z_envZ_memory, (u64)(i1));
    i2 = p0;
    i3 = l2;
    i4 = 4u;
    i3 <<= (i4 & 31);
    i2 += i3;
    j2 = i64_load(Z_envZ_memory, (u64)(i2 + 8));
    j1 ^= j2;
    l28 = j1;
    i64_store(Z_envZ_memory, (u64)(i0), j1);
    i0 = l2;
    i1 = 4294967295u;
    i0 += i1;
    l1 = i0;
    i0 = l2;
    i1 = 0u;
    i0 = (u32)((s32)i0 > (s32)i1);
    if (i0) {
      i0 = l1;
      l2 = i0;
      goto L0;
    }
  i0 = l0;
  j1 = l27;
  i1 = (u32)(j1);
  i2 = 255u;
  i1 &= i2;
  i2 = 1024u;
  i1 += i2;
  i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
  i32_store8(Z_envZ_memory, (u64)(i0), i1);
  i0 = l5;
  j1 = l27;
  j2 = 8ull;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i2 = 255u;
  i1 &= i2;
  i2 = 1024u;
  i1 += i2;
  i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
  i32_store8(Z_envZ_memory, (u64)(i0), i1);
  i0 = l6;
  j1 = l27;
  j2 = 16ull;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i2 = 255u;
  i1 &= i2;
  i2 = 1024u;
  i1 += i2;
  i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
  i32_store8(Z_envZ_memory, (u64)(i0), i1);
  i0 = l7;
  j1 = l27;
  j2 = 24ull;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i2 = 255u;
  i1 &= i2;
  i2 = 1024u;
  i1 += i2;
  i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
  i32_store8(Z_envZ_memory, (u64)(i0), i1);
  i0 = l8;
  j1 = l27;
  j2 = 32ull;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i2 = 255u;
  i1 &= i2;
  i2 = 1024u;
  i1 += i2;
  i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
  i32_store8(Z_envZ_memory, (u64)(i0), i1);
  i0 = l9;
  j1 = l27;
  j2 = 40ull;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i2 = 255u;
  i1 &= i2;
  i2 = 1024u;
  i1 += i2;
  i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
  i32_store8(Z_envZ_memory, (u64)(i0), i1);
  i0 = l10;
  j1 = l27;
  j2 = 48ull;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i2 = 255u;
  i1 &= i2;
  i2 = 1024u;
  i1 += i2;
  i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
  i32_store8(Z_envZ_memory, (u64)(i0), i1);
  i0 = l11;
  j1 = l27;
  j2 = 56ull;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i2 = 1024u;
  i1 += i2;
  i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
  i32_store8(Z_envZ_memory, (u64)(i0), i1);
  i0 = l12;
  j1 = l28;
  i1 = (u32)(j1);
  i2 = 255u;
  i1 &= i2;
  i2 = 1024u;
  i1 += i2;
  i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
  i32_store8(Z_envZ_memory, (u64)(i0), i1);
  i0 = l13;
  j1 = l28;
  j2 = 8ull;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i2 = 255u;
  i1 &= i2;
  i2 = 1024u;
  i1 += i2;
  i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
  i32_store8(Z_envZ_memory, (u64)(i0), i1);
  i0 = l14;
  j1 = l28;
  j2 = 16ull;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i2 = 255u;
  i1 &= i2;
  i2 = 1024u;
  i1 += i2;
  i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
  i32_store8(Z_envZ_memory, (u64)(i0), i1);
  i0 = l15;
  j1 = l28;
  j2 = 24ull;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i2 = 255u;
  i1 &= i2;
  i2 = 1024u;
  i1 += i2;
  i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
  i32_store8(Z_envZ_memory, (u64)(i0), i1);
  i0 = l16;
  j1 = l28;
  j2 = 32ull;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i2 = 255u;
  i1 &= i2;
  i2 = 1024u;
  i1 += i2;
  i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
  i32_store8(Z_envZ_memory, (u64)(i0), i1);
  i0 = l17;
  j1 = l28;
  j2 = 40ull;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i2 = 255u;
  i1 &= i2;
  i2 = 1024u;
  i1 += i2;
  i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
  i32_store8(Z_envZ_memory, (u64)(i0), i1);
  i0 = l18;
  j1 = l28;
  j2 = 48ull;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i2 = 255u;
  i1 &= i2;
  i2 = 1024u;
  i1 += i2;
  i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
  i32_store8(Z_envZ_memory, (u64)(i0), i1);
  i0 = l3;
  j1 = l28;
  j2 = 56ull;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i2 = 1024u;
  i1 += i2;
  i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
  i32_store8(Z_envZ_memory, (u64)(i0), i1);
  i0 = p1;
  i1 = l0;
  j1 = i64_load(Z_envZ_memory, (u64)(i1));
  i64_store(Z_envZ_memory, (u64)(i0), j1);
  i0 = l26;
  i1 = l4;
  j1 = i64_load(Z_envZ_memory, (u64)(i1));
  i64_store(Z_envZ_memory, (u64)(i0), j1);
  i0 = l23;
  g4 = i0;
  FUNC_EPILOGUE;
}

static u32 f16(u32 p0, u32 p1, u32 p2, u32 p3) {
  u32 l0 = 0, l1 = 0, l2 = 0, l3 = 0, l4 = 0, l5 = 0;
  u64 l6 = 0, l7 = 0, l8 = 0, l9 = 0, l10 = 0, l11 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  u64 j0, j1, j2, j3;
  i0 = p3;
  i1 = 15u;
  i0 &= i1;
  if (i0) {
    i0 = 4294967295u;
    goto Bfunc;
  }
  i0 = p3;
  i1 = 16u;
  i0 = I32_DIV_S(i0, i1);
  l3 = i0;
  i0 = p3;
  i1 = 15u;
  i0 = (u32)((s32)i0 <= (s32)i1);
  if (i0) {
    i0 = 0u;
    goto Bfunc;
  }
  i0 = 0u;
  p3 = i0;
  L2: 
    i0 = p0;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l4 = i0;
    i0 = 32u;
    l0 = i0;
    i0 = p1;
    i1 = p3;
    i2 = 4u;
    i1 <<= (i2 & 31);
    l5 = i1;
    i0 += i1;
    l1 = i0;
    i1 = 8u;
    i0 += i1;
    l2 = i0;
    i0 = i32_load8_u(Z_envZ_memory, (u64)(i0 + 6));
    j0 = (u64)(i0);
    j1 = 48ull;
    j0 <<= (j1 & 63);
    i1 = l2;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 7));
    j1 = (u64)(i1);
    j2 = 56ull;
    j1 <<= (j2 & 63);
    j0 |= j1;
    i1 = l2;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 5));
    j1 = (u64)(i1);
    j2 = 40ull;
    j1 <<= (j2 & 63);
    j0 |= j1;
    i1 = l2;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 4));
    j1 = (u64)(i1);
    j2 = 32ull;
    j1 <<= (j2 & 63);
    j0 |= j1;
    i1 = l2;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 3));
    j1 = (u64)(i1);
    j2 = 24ull;
    j1 <<= (j2 & 63);
    j0 |= j1;
    i1 = l2;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 2));
    j1 = (u64)(i1);
    j2 = 16ull;
    j1 <<= (j2 & 63);
    j0 |= j1;
    i1 = l2;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 1));
    j1 = (u64)(i1);
    j2 = 8ull;
    j1 <<= (j2 & 63);
    j0 |= j1;
    i1 = l2;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1));
    j1 = (u64)(i1);
    j0 |= j1;
    l8 = j0;
    i0 = l1;
    i0 = i32_load8_u(Z_envZ_memory, (u64)(i0 + 6));
    j0 = (u64)(i0);
    j1 = 48ull;
    j0 <<= (j1 & 63);
    i1 = l1;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 7));
    j1 = (u64)(i1);
    j2 = 56ull;
    j1 <<= (j2 & 63);
    j0 |= j1;
    i1 = l1;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 5));
    j1 = (u64)(i1);
    j2 = 40ull;
    j1 <<= (j2 & 63);
    j0 |= j1;
    i1 = l1;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 4));
    j1 = (u64)(i1);
    j2 = 32ull;
    j1 <<= (j2 & 63);
    j0 |= j1;
    i1 = l1;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 3));
    j1 = (u64)(i1);
    j2 = 24ull;
    j1 <<= (j2 & 63);
    j0 |= j1;
    i1 = l1;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 2));
    j1 = (u64)(i1);
    j2 = 16ull;
    j1 <<= (j2 & 63);
    j0 |= j1;
    i1 = l1;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 1));
    j1 = (u64)(i1);
    j2 = 8ull;
    j1 <<= (j2 & 63);
    j0 |= j1;
    i1 = l1;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1));
    j1 = (u64)(i1);
    j0 |= j1;
    l9 = j0;
    L3: 
      i0 = l4;
      i1 = l0;
      i2 = 4294967295u;
      i1 += i2;
      l1 = i1;
      i2 = 3u;
      i1 <<= (i2 & 31);
      i0 += i1;
      j0 = i64_load(Z_envZ_memory, (u64)(i0));
      j1 = l8;
      j0 ^= j1;
      j1 = l8;
      j2 = l9;
      j1 ^= j2;
      l7 = j1;
      j2 = 3ull;
      j1 >>= (j2 & 63);
      l11 = j1;
      j2 = l7;
      j3 = 61ull;
      j2 <<= (j3 & 63);
      j1 |= j2;
      l9 = j1;
      j0 -= j1;
      l6 = j0;
      j1 = 56ull;
      j0 >>= (j1 & 63);
      l10 = j0;
      j0 = l6;
      j1 = 8ull;
      j0 <<= (j1 & 63);
      j1 = l10;
      j0 |= j1;
      l8 = j0;
      i0 = l0;
      i1 = 1u;
      i0 = (u32)((s32)i0 > (s32)i1);
      if (i0) {
        i0 = l1;
        l0 = i0;
        goto L3;
      }
    i0 = p2;
    i1 = l5;
    i0 += i1;
    l0 = i0;
    j1 = l11;
    i64_store8(Z_envZ_memory, (u64)(i0), j1);
    i0 = l0;
    j1 = l7;
    j2 = 11ull;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 1), j1);
    i0 = l0;
    j1 = l7;
    j2 = 19ull;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 2), j1);
    i0 = l0;
    j1 = l7;
    j2 = 27ull;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 3), j1);
    i0 = l0;
    j1 = l7;
    j2 = 35ull;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 4), j1);
    i0 = l0;
    j1 = l7;
    j2 = 43ull;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 5), j1);
    i0 = l0;
    j1 = l7;
    j2 = 51ull;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 6), j1);
    i0 = l0;
    j1 = l9;
    j2 = 56ull;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 7), j1);
    i0 = l0;
    i1 = 8u;
    i0 += i1;
    l0 = i0;
    j1 = l10;
    i64_store8(Z_envZ_memory, (u64)(i0), j1);
    i0 = l0;
    j1 = l6;
    i64_store8(Z_envZ_memory, (u64)(i0 + 1), j1);
    i0 = l0;
    j1 = l6;
    j2 = 8ull;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 2), j1);
    i0 = l0;
    j1 = l6;
    j2 = 16ull;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 3), j1);
    i0 = l0;
    j1 = l6;
    j2 = 24ull;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 4), j1);
    i0 = l0;
    j1 = l6;
    j2 = 32ull;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 5), j1);
    i0 = l0;
    j1 = l6;
    j2 = 40ull;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 6), j1);
    i0 = l0;
    j1 = l6;
    j2 = 48ull;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 7), j1);
    i0 = p3;
    i1 = 1u;
    i0 += i1;
    p3 = i0;
    i1 = l3;
    i0 = i0 != i1;
    if (i0) {goto L2;}
    i0 = 0u;
    p0 = i0;
  i0 = p0;
  Bfunc:;
  FUNC_EPILOGUE;
  return i0;
}

static u32 f17(u32 p0) {
  u32 l0 = 0, l1 = 0, l2 = 0;
  u64 l3 = 0, l4 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  u64 j0, j1, j2, j3, j4;
  i0 = p0;
  i0 = !(i0);
  if (i0) {
    i0 = 0u;
    goto Bfunc;
  }
  i0 = 1u;
  i1 = 4u;
  i0 = f21(i0, i1);
  l2 = i0;
  i0 = !(i0);
  if (i0) {
    i0 = 0u;
    goto Bfunc;
  }
  i0 = l2;
  i1 = 256u;
  i1 = _malloc(i1);
  l0 = i1;
  i32_store(Z_envZ_memory, (u64)(i0), i1);
  i0 = l0;
  i0 = !(i0);
  if (i0) {
    i0 = 0u;
    goto Bfunc;
  }
  i0 = p0;
  i0 = i32_load8_u(Z_envZ_memory, (u64)(i0 + 14));
  j0 = (u64)(i0);
  j1 = 48ull;
  j0 <<= (j1 & 63);
  i1 = p0;
  i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 15));
  j1 = (u64)(i1);
  j2 = 56ull;
  j1 <<= (j2 & 63);
  j0 |= j1;
  i1 = p0;
  i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 13));
  j1 = (u64)(i1);
  j2 = 40ull;
  j1 <<= (j2 & 63);
  j0 |= j1;
  i1 = p0;
  i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 12));
  j1 = (u64)(i1);
  j2 = 32ull;
  j1 <<= (j2 & 63);
  j0 |= j1;
  i1 = p0;
  i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 11));
  j1 = (u64)(i1);
  j2 = 24ull;
  j1 <<= (j2 & 63);
  j0 |= j1;
  i1 = p0;
  i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 10));
  j1 = (u64)(i1);
  j2 = 16ull;
  j1 <<= (j2 & 63);
  j0 |= j1;
  i1 = p0;
  i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 9));
  j1 = (u64)(i1);
  j2 = 8ull;
  j1 <<= (j2 & 63);
  j0 |= j1;
  i1 = p0;
  i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 8));
  j1 = (u64)(i1);
  j0 |= j1;
  l4 = j0;
  i0 = l0;
  i1 = p0;
  i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 6));
  j1 = (u64)(i1);
  j2 = 48ull;
  j1 <<= (j2 & 63);
  i2 = p0;
  i2 = i32_load8_u(Z_envZ_memory, (u64)(i2 + 7));
  j2 = (u64)(i2);
  j3 = 56ull;
  j2 <<= (j3 & 63);
  j1 |= j2;
  i2 = p0;
  i2 = i32_load8_u(Z_envZ_memory, (u64)(i2 + 5));
  j2 = (u64)(i2);
  j3 = 40ull;
  j2 <<= (j3 & 63);
  j1 |= j2;
  i2 = p0;
  i2 = i32_load8_u(Z_envZ_memory, (u64)(i2 + 4));
  j2 = (u64)(i2);
  j3 = 32ull;
  j2 <<= (j3 & 63);
  j1 |= j2;
  i2 = p0;
  i2 = i32_load8_u(Z_envZ_memory, (u64)(i2 + 3));
  j2 = (u64)(i2);
  j3 = 24ull;
  j2 <<= (j3 & 63);
  j1 |= j2;
  i2 = p0;
  i2 = i32_load8_u(Z_envZ_memory, (u64)(i2 + 2));
  j2 = (u64)(i2);
  j3 = 16ull;
  j2 <<= (j3 & 63);
  j1 |= j2;
  i2 = p0;
  i2 = i32_load8_u(Z_envZ_memory, (u64)(i2 + 1));
  j2 = (u64)(i2);
  j3 = 8ull;
  j2 <<= (j3 & 63);
  j1 |= j2;
  i2 = p0;
  i2 = i32_load8_u(Z_envZ_memory, (u64)(i2));
  j2 = (u64)(i2);
  j1 |= j2;
  l3 = j1;
  i64_store(Z_envZ_memory, (u64)(i0), j1);
  L3: 
    i0 = l0;
    i1 = l1;
    i2 = 1u;
    i1 += i2;
    p0 = i1;
    i2 = 3u;
    i1 <<= (i2 & 31);
    i0 += i1;
    j1 = l4;
    j2 = 8ull;
    j1 >>= (j2 & 63);
    j2 = l4;
    j3 = 56ull;
    j2 <<= (j3 & 63);
    j1 |= j2;
    j2 = l3;
    j1 += j2;
    i2 = l1;
    j2 = (u64)(s64)(s32)(i2);
    j1 ^= j2;
    l4 = j1;
    j2 = l3;
    j3 = 3ull;
    j2 <<= (j3 & 63);
    j3 = l3;
    j4 = 61ull;
    j3 >>= (j4 & 63);
    j2 |= j3;
    j1 ^= j2;
    l3 = j1;
    i64_store(Z_envZ_memory, (u64)(i0), j1);
    i0 = p0;
    i1 = 31u;
    i0 = i0 != i1;
    if (i0) {
      i0 = p0;
      l1 = i0;
      goto L3;
    }
  i0 = l2;
  Bfunc:;
  FUNC_EPILOGUE;
  return i0;
}

static u32 _getFlag(u32 p0, u32 p1) {
  u32 l0 = 0, l1 = 0, l2 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  u64 j1;
  i0 = g4;
  l1 = i0;
  i0 = g4;
  i1 = 112u;
  i0 += i1;
  g4 = i0;
  i0 = l1;
  i1 = 64u;
  i0 += i1;
  l2 = i0;
  i1 = 1321u;
  j1 = i64_load(Z_envZ_memory, (u64)(i1));
  i64_store(Z_envZ_memory, (u64)(i0), j1);
  i0 = l2;
  i1 = 1329u;
  j1 = i64_load(Z_envZ_memory, (u64)(i1));
  i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
  i0 = l2;
  i1 = 1337u;
  j1 = i64_load(Z_envZ_memory, (u64)(i1));
  i64_store(Z_envZ_memory, (u64)(i0 + 16), j1);
  i0 = l2;
  i1 = 1345u;
  j1 = i64_load(Z_envZ_memory, (u64)(i1));
  i64_store(Z_envZ_memory, (u64)(i0 + 24), j1);
  i0 = l2;
  i1 = 1353u;
  j1 = i64_load(Z_envZ_memory, (u64)(i1));
  i64_store(Z_envZ_memory, (u64)(i0 + 32), j1);
  i0 = l2;
  i1 = 1361u;
  j1 = i64_load(Z_envZ_memory, (u64)(i1));
  i64_store(Z_envZ_memory, (u64)(i0 + 40), j1);
  i0 = l1;
  l0 = i0;
  i1 = 1369u;
  j1 = i64_load(Z_envZ_memory, (u64)(i1));
  i64_store(Z_envZ_memory, (u64)(i0), j1);
  i0 = l0;
  i1 = 1377u;
  j1 = i64_load(Z_envZ_memory, (u64)(i1));
  i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
  i0 = p0;
  i1 = 89594904u;
  i0 = i0 != i1;
  if (i0) {
    i0 = l1;
    g4 = i0;
    i0 = 0u;
    goto Bfunc;
  }
  i0 = l0;
  i0 = f17(i0);
  p0 = i0;
  i0 = !(i0);
  if (i0) {
    i0 = l1;
    g4 = i0;
    i0 = 0u;
    goto Bfunc;
  }
  i0 = p0;
  i1 = l2;
  i2 = l1;
  i3 = 16u;
  i2 += i3;
  l0 = i2;
  i3 = 48u;
  i0 = f16(i0, i1, i2, i3);
  i0 = p1;
  i1 = l0;
  j1 = i64_load(Z_envZ_memory, (u64)(i1));
  i64_store(Z_envZ_memory, (u64)(i0), j1);
  i0 = p1;
  i1 = l0;
  j1 = i64_load(Z_envZ_memory, (u64)(i1 + 8));
  i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
  i0 = p1;
  i1 = l0;
  j1 = i64_load(Z_envZ_memory, (u64)(i1 + 16));
  i64_store(Z_envZ_memory, (u64)(i0 + 16), j1);
  i0 = p1;
  i1 = l0;
  j1 = i64_load(Z_envZ_memory, (u64)(i1 + 24));
  i64_store(Z_envZ_memory, (u64)(i0 + 24), j1);
  i0 = p1;
  i1 = l0;
  j1 = i64_load(Z_envZ_memory, (u64)(i1 + 32));
  i64_store(Z_envZ_memory, (u64)(i0 + 32), j1);
  i0 = p1;
  i1 = l0;
  i1 = i32_load(Z_envZ_memory, (u64)(i1 + 40));
  i32_store(Z_envZ_memory, (u64)(i0 + 40), i1);
  i0 = p0;
  _free(i0);
  i0 = l1;
  g4 = i0;
  i0 = 1u;
  Bfunc:;
  FUNC_EPILOGUE;
  return i0;
}

static u32 _malloc(u32 p0) {
  u32 l0 = 0, l1 = 0, l2 = 0, l3 = 0, l4 = 0, l5 = 0, l6 = 0, l7 = 0, 
      l8 = 0, l9 = 0, l10 = 0, l11 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3, i4;
  u64 j1;
  i0 = g4;
  l9 = i0;
  i0 = g4;
  i1 = 16u;
  i0 += i1;
  g4 = i0;
  i0 = l9;
  l7 = i0;
  i0 = p0;
  i1 = 245u;
  i0 = i0 < i1;
  if (i0) {
    i0 = p0;
    i1 = 11u;
    i0 += i1;
    i1 = 4294967288u;
    i0 &= i1;
    l2 = i0;
    i0 = 1388u;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l5 = i0;
    i1 = p0;
    i2 = 11u;
    i1 = i1 < i2;
    if (i1) {
      i1 = 16u;
      l2 = i1;
    } else {
      i1 = l2;
    }
    i2 = 3u;
    i1 >>= (i2 & 31);
    p0 = i1;
    i0 >>= (i1 & 31);
    l0 = i0;
    i1 = 3u;
    i0 &= i1;
    if (i0) {
      i0 = l0;
      i1 = 1u;
      i0 &= i1;
      i1 = 1u;
      i0 ^= i1;
      i1 = p0;
      i0 += i1;
      p0 = i0;
      i1 = 3u;
      i0 <<= (i1 & 31);
      i1 = 1428u;
      i0 += i1;
      l0 = i0;
      i1 = 8u;
      i0 += i1;
      l4 = i0;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l2 = i0;
      i1 = 8u;
      i0 += i1;
      l3 = i0;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l1 = i0;
      i1 = l0;
      i0 = i0 == i1;
      if (i0) {
        i0 = 1388u;
        i1 = l5;
        i2 = 1u;
        i3 = p0;
        i2 <<= (i3 & 31);
        i3 = 4294967295u;
        i2 ^= i3;
        i1 &= i2;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
      } else {
        i0 = l1;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
        i0 = l4;
        i1 = l1;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
      }
      i0 = l2;
      i1 = p0;
      i2 = 3u;
      i1 <<= (i2 & 31);
      p0 = i1;
      i2 = 3u;
      i1 |= i2;
      i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
      i0 = l2;
      i1 = p0;
      i0 += i1;
      i1 = 4u;
      i0 += i1;
      p0 = i0;
      i1 = p0;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      i2 = 1u;
      i1 |= i2;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l9;
      g4 = i0;
      i0 = l3;
      goto Bfunc;
    }
    i0 = l2;
    i1 = 1396u;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    l8 = i1;
    i0 = i0 > i1;
    if (i0) {
      i0 = l0;
      if (i0) {
        i0 = l0;
        i1 = p0;
        i0 <<= (i1 & 31);
        i1 = 2u;
        i2 = p0;
        i1 <<= (i2 & 31);
        p0 = i1;
        i2 = 0u;
        i3 = p0;
        i2 -= i3;
        i1 |= i2;
        i0 &= i1;
        p0 = i0;
        i1 = 0u;
        i2 = p0;
        i1 -= i2;
        i0 &= i1;
        i1 = 4294967295u;
        i0 += i1;
        l0 = i0;
        i1 = 12u;
        i0 >>= (i1 & 31);
        i1 = 16u;
        i0 &= i1;
        p0 = i0;
        i0 = l0;
        i1 = p0;
        i0 >>= (i1 & 31);
        l0 = i0;
        i1 = 5u;
        i0 >>= (i1 & 31);
        i1 = 8u;
        i0 &= i1;
        l1 = i0;
        i1 = p0;
        i0 |= i1;
        i1 = l0;
        i2 = l1;
        i1 >>= (i2 & 31);
        p0 = i1;
        i2 = 2u;
        i1 >>= (i2 & 31);
        i2 = 4u;
        i1 &= i2;
        l0 = i1;
        i0 |= i1;
        i1 = p0;
        i2 = l0;
        i1 >>= (i2 & 31);
        p0 = i1;
        i2 = 1u;
        i1 >>= (i2 & 31);
        i2 = 2u;
        i1 &= i2;
        l0 = i1;
        i0 |= i1;
        i1 = p0;
        i2 = l0;
        i1 >>= (i2 & 31);
        p0 = i1;
        i2 = 1u;
        i1 >>= (i2 & 31);
        i2 = 1u;
        i1 &= i2;
        l0 = i1;
        i0 |= i1;
        i1 = p0;
        i2 = l0;
        i1 >>= (i2 & 31);
        i0 += i1;
        l1 = i0;
        i1 = 3u;
        i0 <<= (i1 & 31);
        i1 = 1428u;
        i0 += i1;
        p0 = i0;
        i1 = 8u;
        i0 += i1;
        l3 = i0;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l0 = i0;
        i1 = 8u;
        i0 += i1;
        l6 = i0;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l4 = i0;
        i1 = p0;
        i0 = i0 == i1;
        if (i0) {
          i0 = 1388u;
          i1 = l5;
          i2 = 1u;
          i3 = l1;
          i2 <<= (i3 & 31);
          i3 = 4294967295u;
          i2 ^= i3;
          i1 &= i2;
          p0 = i1;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
        } else {
          i0 = l4;
          i1 = p0;
          i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
          i0 = l3;
          i1 = l4;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          i0 = l5;
          p0 = i0;
        }
        i0 = l0;
        i1 = l2;
        i2 = 3u;
        i1 |= i2;
        i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
        i0 = l0;
        i1 = l2;
        i0 += i1;
        l3 = i0;
        i1 = l1;
        i2 = 3u;
        i1 <<= (i2 & 31);
        l1 = i1;
        i2 = l2;
        i1 -= i2;
        l4 = i1;
        i2 = 1u;
        i1 |= i2;
        i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
        i0 = l0;
        i1 = l1;
        i0 += i1;
        i1 = l4;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = l8;
        if (i0) {
          i0 = 1408u;
          i0 = i32_load(Z_envZ_memory, (u64)(i0));
          l1 = i0;
          i0 = l8;
          i1 = 3u;
          i0 >>= (i1 & 31);
          l2 = i0;
          i1 = 3u;
          i0 <<= (i1 & 31);
          i1 = 1428u;
          i0 += i1;
          l0 = i0;
          i0 = p0;
          i1 = 1u;
          i2 = l2;
          i1 <<= (i2 & 31);
          l2 = i1;
          i0 &= i1;
          if (i0) {
            i0 = l0;
            i1 = 8u;
            i0 += i1;
            l2 = i0;
            i0 = i32_load(Z_envZ_memory, (u64)(i0));
          } else {
            i0 = 1388u;
            i1 = p0;
            i2 = l2;
            i1 |= i2;
            i32_store(Z_envZ_memory, (u64)(i0), i1);
            i0 = l0;
            i1 = 8u;
            i0 += i1;
            l2 = i0;
            i0 = l0;
          }
          p0 = i0;
          i0 = l2;
          i1 = l1;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          i0 = p0;
          i1 = l1;
          i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
          i0 = l1;
          i1 = p0;
          i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
          i0 = l1;
          i1 = l0;
          i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
        }
        i0 = 1396u;
        i1 = l4;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = 1408u;
        i1 = l3;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = l9;
        g4 = i0;
        i0 = l6;
        goto Bfunc;
      }
      i0 = 1392u;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l11 = i0;
      if (i0) {
        i0 = l11;
        i1 = 0u;
        i2 = l11;
        i1 -= i2;
        i0 &= i1;
        i1 = 4294967295u;
        i0 += i1;
        l0 = i0;
        i1 = 12u;
        i0 >>= (i1 & 31);
        i1 = 16u;
        i0 &= i1;
        p0 = i0;
        i0 = l0;
        i1 = p0;
        i0 >>= (i1 & 31);
        l0 = i0;
        i1 = 5u;
        i0 >>= (i1 & 31);
        i1 = 8u;
        i0 &= i1;
        l1 = i0;
        i1 = p0;
        i0 |= i1;
        i1 = l0;
        i2 = l1;
        i1 >>= (i2 & 31);
        p0 = i1;
        i2 = 2u;
        i1 >>= (i2 & 31);
        i2 = 4u;
        i1 &= i2;
        l0 = i1;
        i0 |= i1;
        i1 = p0;
        i2 = l0;
        i1 >>= (i2 & 31);
        p0 = i1;
        i2 = 1u;
        i1 >>= (i2 & 31);
        i2 = 2u;
        i1 &= i2;
        l0 = i1;
        i0 |= i1;
        i1 = p0;
        i2 = l0;
        i1 >>= (i2 & 31);
        p0 = i1;
        i2 = 1u;
        i1 >>= (i2 & 31);
        i2 = 1u;
        i1 &= i2;
        l0 = i1;
        i0 |= i1;
        i1 = p0;
        i2 = l0;
        i1 >>= (i2 & 31);
        i0 += i1;
        i1 = 2u;
        i0 <<= (i1 & 31);
        i1 = 1692u;
        i0 += i1;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l1 = i0;
        i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
        i1 = 4294967288u;
        i0 &= i1;
        i1 = l2;
        i0 -= i1;
        l0 = i0;
        i0 = l1;
        i1 = 16u;
        i0 += i1;
        i1 = l1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 16));
        i1 = !(i1);
        i2 = 2u;
        i1 <<= (i2 & 31);
        i0 += i1;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        p0 = i0;
        if (i0) {
          L12: 
            i0 = p0;
            i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
            i1 = 4294967288u;
            i0 &= i1;
            i1 = l2;
            i0 -= i1;
            l4 = i0;
            i1 = l0;
            i0 = i0 < i1;
            l3 = i0;
            if (i0) {
              i0 = l4;
              l0 = i0;
            }
            i0 = l3;
            if (i0) {
              i0 = p0;
              l1 = i0;
            }
            i0 = p0;
            i1 = 16u;
            i0 += i1;
            i1 = p0;
            i1 = i32_load(Z_envZ_memory, (u64)(i1 + 16));
            i1 = !(i1);
            i2 = 2u;
            i1 <<= (i2 & 31);
            i0 += i1;
            i0 = i32_load(Z_envZ_memory, (u64)(i0));
            p0 = i0;
            if (i0) {goto L12;}
            i0 = l0;
            l4 = i0;
        } else {
          i0 = l0;
          l4 = i0;
        }
        i0 = l1;
        i1 = l2;
        i0 += i1;
        l10 = i0;
        i1 = l1;
        i0 = i0 > i1;
        if (i0) {
          i0 = l1;
          i0 = i32_load(Z_envZ_memory, (u64)(i0 + 24));
          l7 = i0;
          i0 = l1;
          i0 = i32_load(Z_envZ_memory, (u64)(i0 + 12));
          p0 = i0;
          i1 = l1;
          i0 = i0 == i1;
          if (i0) {
            i0 = l1;
            i1 = 20u;
            i0 += i1;
            l0 = i0;
            i0 = i32_load(Z_envZ_memory, (u64)(i0));
            p0 = i0;
            i0 = !(i0);
            if (i0) {
              i0 = l1;
              i1 = 16u;
              i0 += i1;
              l0 = i0;
              i0 = i32_load(Z_envZ_memory, (u64)(i0));
              p0 = i0;
              i0 = !(i0);
              if (i0) {
                i0 = 0u;
                p0 = i0;
                goto B16;
              }
            }
            L20: 
              i0 = p0;
              i1 = 20u;
              i0 += i1;
              l3 = i0;
              i0 = i32_load(Z_envZ_memory, (u64)(i0));
              l6 = i0;
              if (i0) {
                i0 = l6;
                p0 = i0;
                i0 = l3;
                l0 = i0;
                goto L20;
              }
              i0 = p0;
              i1 = 16u;
              i0 += i1;
              l3 = i0;
              i0 = i32_load(Z_envZ_memory, (u64)(i0));
              l6 = i0;
              if (i0) {
                i0 = l6;
                p0 = i0;
                i0 = l3;
                l0 = i0;
                goto L20;
              }
            i0 = l0;
            i1 = 0u;
            i32_store(Z_envZ_memory, (u64)(i0), i1);
          } else {
            i0 = l1;
            i0 = i32_load(Z_envZ_memory, (u64)(i0 + 8));
            l0 = i0;
            i1 = p0;
            i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
            i0 = p0;
            i1 = l0;
            i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
          }
          B16:;
          i0 = l7;
          if (i0) {
            i0 = l1;
            i1 = l1;
            i1 = i32_load(Z_envZ_memory, (u64)(i1 + 28));
            l0 = i1;
            i2 = 2u;
            i1 <<= (i2 & 31);
            i2 = 1692u;
            i1 += i2;
            l3 = i1;
            i1 = i32_load(Z_envZ_memory, (u64)(i1));
            i0 = i0 == i1;
            if (i0) {
              i0 = l3;
              i1 = p0;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
              i0 = p0;
              i0 = !(i0);
              if (i0) {
                i0 = 1392u;
                i1 = l11;
                i2 = 1u;
                i3 = l0;
                i2 <<= (i3 & 31);
                i3 = 4294967295u;
                i2 ^= i3;
                i1 &= i2;
                i32_store(Z_envZ_memory, (u64)(i0), i1);
                goto B23;
              }
            } else {
              i0 = l7;
              i1 = 16u;
              i0 += i1;
              i1 = l7;
              i1 = i32_load(Z_envZ_memory, (u64)(i1 + 16));
              i2 = l1;
              i1 = i1 != i2;
              i2 = 2u;
              i1 <<= (i2 & 31);
              i0 += i1;
              i1 = p0;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
              i0 = p0;
              i0 = !(i0);
              if (i0) {goto B23;}
            }
            i0 = p0;
            i1 = l7;
            i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
            i0 = l1;
            i0 = i32_load(Z_envZ_memory, (u64)(i0 + 16));
            l0 = i0;
            if (i0) {
              i0 = p0;
              i1 = l0;
              i32_store(Z_envZ_memory, (u64)(i0 + 16), i1);
              i0 = l0;
              i1 = p0;
              i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
            }
            i0 = l1;
            i0 = i32_load(Z_envZ_memory, (u64)(i0 + 20));
            l0 = i0;
            if (i0) {
              i0 = p0;
              i1 = l0;
              i32_store(Z_envZ_memory, (u64)(i0 + 20), i1);
              i0 = l0;
              i1 = p0;
              i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
            }
          }
          B23:;
          i0 = l4;
          i1 = 16u;
          i0 = i0 < i1;
          if (i0) {
            i0 = l1;
            i1 = l4;
            i2 = l2;
            i1 += i2;
            p0 = i1;
            i2 = 3u;
            i1 |= i2;
            i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
            i0 = l1;
            i1 = p0;
            i0 += i1;
            i1 = 4u;
            i0 += i1;
            p0 = i0;
            i1 = p0;
            i1 = i32_load(Z_envZ_memory, (u64)(i1));
            i2 = 1u;
            i1 |= i2;
            i32_store(Z_envZ_memory, (u64)(i0), i1);
          } else {
            i0 = l1;
            i1 = l2;
            i2 = 3u;
            i1 |= i2;
            i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
            i0 = l10;
            i1 = l4;
            i2 = 1u;
            i1 |= i2;
            i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
            i0 = l10;
            i1 = l4;
            i0 += i1;
            i1 = l4;
            i32_store(Z_envZ_memory, (u64)(i0), i1);
            i0 = l8;
            if (i0) {
              i0 = 1408u;
              i0 = i32_load(Z_envZ_memory, (u64)(i0));
              l3 = i0;
              i0 = l8;
              i1 = 3u;
              i0 >>= (i1 & 31);
              l0 = i0;
              i1 = 3u;
              i0 <<= (i1 & 31);
              i1 = 1428u;
              i0 += i1;
              p0 = i0;
              i0 = l5;
              i1 = 1u;
              i2 = l0;
              i1 <<= (i2 & 31);
              l0 = i1;
              i0 &= i1;
              if (i0) {
                i0 = p0;
                i1 = 8u;
                i0 += i1;
                l2 = i0;
                i0 = i32_load(Z_envZ_memory, (u64)(i0));
              } else {
                i0 = 1388u;
                i1 = l5;
                i2 = l0;
                i1 |= i2;
                i32_store(Z_envZ_memory, (u64)(i0), i1);
                i0 = p0;
                i1 = 8u;
                i0 += i1;
                l2 = i0;
                i0 = p0;
              }
              l0 = i0;
              i0 = l2;
              i1 = l3;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
              i0 = l0;
              i1 = l3;
              i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
              i0 = l3;
              i1 = l0;
              i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
              i0 = l3;
              i1 = p0;
              i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
            }
            i0 = 1396u;
            i1 = l4;
            i32_store(Z_envZ_memory, (u64)(i0), i1);
            i0 = 1408u;
            i1 = l10;
            i32_store(Z_envZ_memory, (u64)(i0), i1);
          }
          i0 = l9;
          g4 = i0;
          i0 = l1;
          i1 = 8u;
          i0 += i1;
          goto Bfunc;
        } else {
          i0 = l2;
          p0 = i0;
        }
      } else {
        i0 = l2;
        p0 = i0;
      }
    } else {
      i0 = l2;
      p0 = i0;
    }
  } else {
    i0 = p0;
    i1 = 4294967231u;
    i0 = i0 > i1;
    if (i0) {
      i0 = 4294967295u;
      p0 = i0;
    } else {
      i0 = p0;
      i1 = 11u;
      i0 += i1;
      p0 = i0;
      i1 = 4294967288u;
      i0 &= i1;
      l1 = i0;
      i0 = 1392u;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l4 = i0;
      if (i0) {
        i0 = p0;
        i1 = 8u;
        i0 >>= (i1 & 31);
        p0 = i0;
        if (i0) {
          i0 = l1;
          i1 = 16777215u;
          i0 = i0 > i1;
          if (i0) {
            i0 = 31u;
          } else {
            i0 = l1;
            i1 = 14u;
            i2 = p0;
            i3 = p0;
            i4 = 1048320u;
            i3 += i4;
            i4 = 16u;
            i3 >>= (i4 & 31);
            i4 = 8u;
            i3 &= i4;
            p0 = i3;
            i2 <<= (i3 & 31);
            l0 = i2;
            i3 = 520192u;
            i2 += i3;
            i3 = 16u;
            i2 >>= (i3 & 31);
            i3 = 4u;
            i2 &= i3;
            l2 = i2;
            i3 = p0;
            i2 |= i3;
            i3 = l0;
            i4 = l2;
            i3 <<= (i4 & 31);
            p0 = i3;
            i4 = 245760u;
            i3 += i4;
            i4 = 16u;
            i3 >>= (i4 & 31);
            i4 = 2u;
            i3 &= i4;
            l0 = i3;
            i2 |= i3;
            i1 -= i2;
            i2 = p0;
            i3 = l0;
            i2 <<= (i3 & 31);
            i3 = 15u;
            i2 >>= (i3 & 31);
            i1 += i2;
            p0 = i1;
            i2 = 7u;
            i1 += i2;
            i0 >>= (i1 & 31);
            i1 = 1u;
            i0 &= i1;
            i1 = p0;
            i2 = 1u;
            i1 <<= (i2 & 31);
            i0 |= i1;
          }
        } else {
          i0 = 0u;
        }
        l8 = i0;
        i0 = 0u;
        i1 = l1;
        i0 -= i1;
        l2 = i0;
        i0 = l8;
        i1 = 2u;
        i0 <<= (i1 & 31);
        i1 = 1692u;
        i0 += i1;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        p0 = i0;
        if (i0) {
          i0 = 25u;
          i1 = l8;
          i2 = 1u;
          i1 >>= (i2 & 31);
          i0 -= i1;
          l3 = i0;
          i0 = 0u;
          l0 = i0;
          i0 = l1;
          i1 = l8;
          i2 = 31u;
          i1 = i1 == i2;
          if (i1) {
            i1 = 0u;
          } else {
            i1 = l3;
          }
          i0 <<= (i1 & 31);
          l6 = i0;
          i0 = 0u;
          l3 = i0;
          L40: 
            i0 = p0;
            i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
            i1 = 4294967288u;
            i0 &= i1;
            i1 = l1;
            i0 -= i1;
            l5 = i0;
            i1 = l2;
            i0 = i0 < i1;
            if (i0) {
              i0 = l5;
              if (i0) {
                i0 = p0;
                l0 = i0;
                i0 = l5;
                l2 = i0;
              } else {
                i0 = 0u;
                l2 = i0;
                i0 = p0;
                l0 = i0;
                goto B37;
              }
            }
            i0 = p0;
            i0 = i32_load(Z_envZ_memory, (u64)(i0 + 20));
            l5 = i0;
            i0 = !(i0);
            i1 = l5;
            i2 = p0;
            i3 = 16u;
            i2 += i3;
            i3 = l6;
            i4 = 31u;
            i3 >>= (i4 & 31);
            i4 = 2u;
            i3 <<= (i4 & 31);
            i2 += i3;
            i2 = i32_load(Z_envZ_memory, (u64)(i2));
            p0 = i2;
            i1 = i1 == i2;
            i0 |= i1;
            i0 = !(i0);
            if (i0) {
              i0 = l5;
              l3 = i0;
            }
            i0 = l6;
            i1 = p0;
            i1 = !(i1);
            l5 = i1;
            i2 = 1u;
            i1 ^= i2;
            i0 <<= (i1 & 31);
            l6 = i0;
            i0 = l5;
            i0 = !(i0);
            if (i0) {goto L40;}
            i0 = l0;
            p0 = i0;
        } else {
          i0 = 0u;
          p0 = i0;
        }
        i0 = l3;
        i1 = p0;
        i0 |= i1;
        if (i0) {
          i0 = l3;
        } else {
          i0 = l4;
          i1 = 2u;
          i2 = l8;
          i1 <<= (i2 & 31);
          p0 = i1;
          i2 = 0u;
          i3 = p0;
          i2 -= i3;
          i1 |= i2;
          i0 &= i1;
          p0 = i0;
          i0 = !(i0);
          if (i0) {
            i0 = l1;
            p0 = i0;
            goto B0;
          }
          i0 = p0;
          i1 = 0u;
          i2 = p0;
          i1 -= i2;
          i0 &= i1;
          i1 = 4294967295u;
          i0 += i1;
          l3 = i0;
          i1 = 12u;
          i0 >>= (i1 & 31);
          i1 = 16u;
          i0 &= i1;
          l0 = i0;
          i0 = 0u;
          p0 = i0;
          i0 = l3;
          i1 = l0;
          i0 >>= (i1 & 31);
          l3 = i0;
          i1 = 5u;
          i0 >>= (i1 & 31);
          i1 = 8u;
          i0 &= i1;
          l6 = i0;
          i1 = l0;
          i0 |= i1;
          i1 = l3;
          i2 = l6;
          i1 >>= (i2 & 31);
          l0 = i1;
          i2 = 2u;
          i1 >>= (i2 & 31);
          i2 = 4u;
          i1 &= i2;
          l3 = i1;
          i0 |= i1;
          i1 = l0;
          i2 = l3;
          i1 >>= (i2 & 31);
          l0 = i1;
          i2 = 1u;
          i1 >>= (i2 & 31);
          i2 = 2u;
          i1 &= i2;
          l3 = i1;
          i0 |= i1;
          i1 = l0;
          i2 = l3;
          i1 >>= (i2 & 31);
          l0 = i1;
          i2 = 1u;
          i1 >>= (i2 & 31);
          i2 = 1u;
          i1 &= i2;
          l3 = i1;
          i0 |= i1;
          i1 = l0;
          i2 = l3;
          i1 >>= (i2 & 31);
          i0 += i1;
          i1 = 2u;
          i0 <<= (i1 & 31);
          i1 = 1692u;
          i0 += i1;
          i0 = i32_load(Z_envZ_memory, (u64)(i0));
        }
        l0 = i0;
        if (i0) {goto B37;}
        i0 = p0;
        l3 = i0;
        goto B36;
        B37:;
        L46: 
          i0 = l0;
          i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
          i1 = 4294967288u;
          i0 &= i1;
          i1 = l1;
          i0 -= i1;
          l3 = i0;
          i1 = l2;
          i0 = i0 < i1;
          l6 = i0;
          if (i0) {
            i0 = l3;
            l2 = i0;
          }
          i0 = l6;
          if (i0) {
            i0 = l0;
            p0 = i0;
          }
          i0 = l0;
          i1 = 16u;
          i0 += i1;
          i1 = l0;
          i1 = i32_load(Z_envZ_memory, (u64)(i1 + 16));
          i1 = !(i1);
          i2 = 2u;
          i1 <<= (i2 & 31);
          i0 += i1;
          i0 = i32_load(Z_envZ_memory, (u64)(i0));
          l0 = i0;
          if (i0) {goto L46;}
          i0 = p0;
          l3 = i0;
        B36:;
        i0 = l3;
        if (i0) {
          i0 = l2;
          i1 = 1396u;
          i1 = i32_load(Z_envZ_memory, (u64)(i1));
          i2 = l1;
          i1 -= i2;
          i0 = i0 < i1;
          if (i0) {
            i0 = l3;
            i1 = l1;
            i0 += i1;
            l7 = i0;
            i1 = l3;
            i0 = i0 <= i1;
            if (i0) {
              i0 = l9;
              g4 = i0;
              i0 = 0u;
              goto Bfunc;
            }
            i0 = l3;
            i0 = i32_load(Z_envZ_memory, (u64)(i0 + 24));
            l8 = i0;
            i0 = l3;
            i0 = i32_load(Z_envZ_memory, (u64)(i0 + 12));
            p0 = i0;
            i1 = l3;
            i0 = i0 == i1;
            if (i0) {
              i0 = l3;
              i1 = 20u;
              i0 += i1;
              l0 = i0;
              i0 = i32_load(Z_envZ_memory, (u64)(i0));
              p0 = i0;
              i0 = !(i0);
              if (i0) {
                i0 = l3;
                i1 = 16u;
                i0 += i1;
                l0 = i0;
                i0 = i32_load(Z_envZ_memory, (u64)(i0));
                p0 = i0;
                i0 = !(i0);
                if (i0) {
                  i0 = 0u;
                  p0 = i0;
                  goto B52;
                }
              }
              L56: 
                i0 = p0;
                i1 = 20u;
                i0 += i1;
                l6 = i0;
                i0 = i32_load(Z_envZ_memory, (u64)(i0));
                l5 = i0;
                if (i0) {
                  i0 = l5;
                  p0 = i0;
                  i0 = l6;
                  l0 = i0;
                  goto L56;
                }
                i0 = p0;
                i1 = 16u;
                i0 += i1;
                l6 = i0;
                i0 = i32_load(Z_envZ_memory, (u64)(i0));
                l5 = i0;
                if (i0) {
                  i0 = l5;
                  p0 = i0;
                  i0 = l6;
                  l0 = i0;
                  goto L56;
                }
              i0 = l0;
              i1 = 0u;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
            } else {
              i0 = l3;
              i0 = i32_load(Z_envZ_memory, (u64)(i0 + 8));
              l0 = i0;
              i1 = p0;
              i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
              i0 = p0;
              i1 = l0;
              i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
            }
            B52:;
            i0 = l8;
            if (i0) {
              i0 = l3;
              i1 = l3;
              i1 = i32_load(Z_envZ_memory, (u64)(i1 + 28));
              l0 = i1;
              i2 = 2u;
              i1 <<= (i2 & 31);
              i2 = 1692u;
              i1 += i2;
              l6 = i1;
              i1 = i32_load(Z_envZ_memory, (u64)(i1));
              i0 = i0 == i1;
              if (i0) {
                i0 = l6;
                i1 = p0;
                i32_store(Z_envZ_memory, (u64)(i0), i1);
                i0 = p0;
                i0 = !(i0);
                if (i0) {
                  i0 = 1392u;
                  i1 = l4;
                  i2 = 1u;
                  i3 = l0;
                  i2 <<= (i3 & 31);
                  i3 = 4294967295u;
                  i2 ^= i3;
                  i1 &= i2;
                  p0 = i1;
                  i32_store(Z_envZ_memory, (u64)(i0), i1);
                  goto B59;
                }
              } else {
                i0 = l8;
                i1 = 16u;
                i0 += i1;
                i1 = l8;
                i1 = i32_load(Z_envZ_memory, (u64)(i1 + 16));
                i2 = l3;
                i1 = i1 != i2;
                i2 = 2u;
                i1 <<= (i2 & 31);
                i0 += i1;
                i1 = p0;
                i32_store(Z_envZ_memory, (u64)(i0), i1);
                i0 = p0;
                i0 = !(i0);
                if (i0) {
                  i0 = l4;
                  p0 = i0;
                  goto B59;
                }
              }
              i0 = p0;
              i1 = l8;
              i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
              i0 = l3;
              i0 = i32_load(Z_envZ_memory, (u64)(i0 + 16));
              l0 = i0;
              if (i0) {
                i0 = p0;
                i1 = l0;
                i32_store(Z_envZ_memory, (u64)(i0 + 16), i1);
                i0 = l0;
                i1 = p0;
                i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
              }
              i0 = l3;
              i0 = i32_load(Z_envZ_memory, (u64)(i0 + 20));
              l0 = i0;
              if (i0) {
                i0 = p0;
                i1 = l0;
                i32_store(Z_envZ_memory, (u64)(i0 + 20), i1);
                i0 = l0;
                i1 = p0;
                i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
                i0 = l4;
              } else {
                i0 = l4;
              }
            } else {
              i0 = l4;
            }
            p0 = i0;
            B59:;
            i0 = l2;
            i1 = 16u;
            i0 = i0 < i1;
            if (i0) {
              i0 = l3;
              i1 = l2;
              i2 = l1;
              i1 += i2;
              p0 = i1;
              i2 = 3u;
              i1 |= i2;
              i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
              i0 = l3;
              i1 = p0;
              i0 += i1;
              i1 = 4u;
              i0 += i1;
              p0 = i0;
              i1 = p0;
              i1 = i32_load(Z_envZ_memory, (u64)(i1));
              i2 = 1u;
              i1 |= i2;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
            } else {
              i0 = l3;
              i1 = l1;
              i2 = 3u;
              i1 |= i2;
              i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
              i0 = l7;
              i1 = l2;
              i2 = 1u;
              i1 |= i2;
              i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
              i0 = l7;
              i1 = l2;
              i0 += i1;
              i1 = l2;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
              i0 = l2;
              i1 = 3u;
              i0 >>= (i1 & 31);
              l0 = i0;
              i0 = l2;
              i1 = 256u;
              i0 = i0 < i1;
              if (i0) {
                i0 = l0;
                i1 = 3u;
                i0 <<= (i1 & 31);
                i1 = 1428u;
                i0 += i1;
                p0 = i0;
                i0 = 1388u;
                i0 = i32_load(Z_envZ_memory, (u64)(i0));
                l2 = i0;
                i1 = 1u;
                i2 = l0;
                i1 <<= (i2 & 31);
                l0 = i1;
                i0 &= i1;
                if (i0) {
                  i0 = p0;
                  i1 = 8u;
                  i0 += i1;
                  l2 = i0;
                  i0 = i32_load(Z_envZ_memory, (u64)(i0));
                } else {
                  i0 = 1388u;
                  i1 = l2;
                  i2 = l0;
                  i1 |= i2;
                  i32_store(Z_envZ_memory, (u64)(i0), i1);
                  i0 = p0;
                  i1 = 8u;
                  i0 += i1;
                  l2 = i0;
                  i0 = p0;
                }
                l0 = i0;
                i0 = l2;
                i1 = l7;
                i32_store(Z_envZ_memory, (u64)(i0), i1);
                i0 = l0;
                i1 = l7;
                i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
                i0 = l7;
                i1 = l0;
                i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
                i0 = l7;
                i1 = p0;
                i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
                goto B66;
              }
              i0 = l2;
              i1 = 8u;
              i0 >>= (i1 & 31);
              l0 = i0;
              if (i0) {
                i0 = l2;
                i1 = 16777215u;
                i0 = i0 > i1;
                if (i0) {
                  i0 = 31u;
                } else {
                  i0 = l2;
                  i1 = 14u;
                  i2 = l0;
                  i3 = l0;
                  i4 = 1048320u;
                  i3 += i4;
                  i4 = 16u;
                  i3 >>= (i4 & 31);
                  i4 = 8u;
                  i3 &= i4;
                  l0 = i3;
                  i2 <<= (i3 & 31);
                  l1 = i2;
                  i3 = 520192u;
                  i2 += i3;
                  i3 = 16u;
                  i2 >>= (i3 & 31);
                  i3 = 4u;
                  i2 &= i3;
                  l4 = i2;
                  i3 = l0;
                  i2 |= i3;
                  i3 = l1;
                  i4 = l4;
                  i3 <<= (i4 & 31);
                  l0 = i3;
                  i4 = 245760u;
                  i3 += i4;
                  i4 = 16u;
                  i3 >>= (i4 & 31);
                  i4 = 2u;
                  i3 &= i4;
                  l1 = i3;
                  i2 |= i3;
                  i1 -= i2;
                  i2 = l0;
                  i3 = l1;
                  i2 <<= (i3 & 31);
                  i3 = 15u;
                  i2 >>= (i3 & 31);
                  i1 += i2;
                  l0 = i1;
                  i2 = 7u;
                  i1 += i2;
                  i0 >>= (i1 & 31);
                  i1 = 1u;
                  i0 &= i1;
                  i1 = l0;
                  i2 = 1u;
                  i1 <<= (i2 & 31);
                  i0 |= i1;
                }
              } else {
                i0 = 0u;
              }
              l0 = i0;
              i1 = 2u;
              i0 <<= (i1 & 31);
              i1 = 1692u;
              i0 += i1;
              l1 = i0;
              i0 = l7;
              i1 = l0;
              i32_store(Z_envZ_memory, (u64)(i0 + 28), i1);
              i0 = l7;
              i1 = 16u;
              i0 += i1;
              l4 = i0;
              i1 = 0u;
              i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
              i0 = l4;
              i1 = 0u;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
              i0 = p0;
              i1 = 1u;
              i2 = l0;
              i1 <<= (i2 & 31);
              l4 = i1;
              i0 &= i1;
              i0 = !(i0);
              if (i0) {
                i0 = 1392u;
                i1 = p0;
                i2 = l4;
                i1 |= i2;
                i32_store(Z_envZ_memory, (u64)(i0), i1);
                i0 = l1;
                i1 = l7;
                i32_store(Z_envZ_memory, (u64)(i0), i1);
                i0 = l7;
                i1 = l1;
                i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
                i0 = l7;
                i1 = l7;
                i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
                i0 = l7;
                i1 = l7;
                i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
                goto B66;
              }
              i0 = l1;
              i0 = i32_load(Z_envZ_memory, (u64)(i0));
              p0 = i0;
              i0 = 25u;
              i1 = l0;
              i2 = 1u;
              i1 >>= (i2 & 31);
              i0 -= i1;
              l1 = i0;
              i0 = l2;
              i1 = l0;
              i2 = 31u;
              i1 = i1 == i2;
              if (i1) {
                i1 = 0u;
              } else {
                i1 = l1;
              }
              i0 <<= (i1 & 31);
              l0 = i0;
              L75: 
                i0 = p0;
                i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
                i1 = 4294967288u;
                i0 &= i1;
                i1 = l2;
                i0 = i0 == i1;
                if (i0) {goto B74;}
                i0 = l0;
                i1 = 1u;
                i0 <<= (i1 & 31);
                l1 = i0;
                i0 = p0;
                i1 = 16u;
                i0 += i1;
                i1 = l0;
                i2 = 31u;
                i1 >>= (i2 & 31);
                i2 = 2u;
                i1 <<= (i2 & 31);
                i0 += i1;
                l0 = i0;
                i0 = i32_load(Z_envZ_memory, (u64)(i0));
                l4 = i0;
                if (i0) {
                  i0 = l1;
                  l0 = i0;
                  i0 = l4;
                  p0 = i0;
                  goto L75;
                }
              i0 = l0;
              i1 = l7;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
              i0 = l7;
              i1 = p0;
              i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
              i0 = l7;
              i1 = l7;
              i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
              i0 = l7;
              i1 = l7;
              i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
              goto B66;
              B74:;
              i0 = p0;
              i1 = 8u;
              i0 += i1;
              l0 = i0;
              i0 = i32_load(Z_envZ_memory, (u64)(i0));
              l2 = i0;
              i1 = l7;
              i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
              i0 = l0;
              i1 = l7;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
              i0 = l7;
              i1 = l2;
              i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
              i0 = l7;
              i1 = p0;
              i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
              i0 = l7;
              i1 = 0u;
              i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
            }
            B66:;
            i0 = l9;
            g4 = i0;
            i0 = l3;
            i1 = 8u;
            i0 += i1;
            goto Bfunc;
          } else {
            i0 = l1;
            p0 = i0;
          }
        } else {
          i0 = l1;
          p0 = i0;
        }
      } else {
        i0 = l1;
        p0 = i0;
      }
    }
  }
  B0:;
  i0 = 1396u;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  l2 = i0;
  i1 = p0;
  i0 = i0 >= i1;
  if (i0) {
    i0 = 1408u;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l0 = i0;
    i0 = l2;
    i1 = p0;
    i0 -= i1;
    l1 = i0;
    i1 = 15u;
    i0 = i0 > i1;
    if (i0) {
      i0 = 1408u;
      i1 = l0;
      i2 = p0;
      i1 += i2;
      l4 = i1;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = 1396u;
      i1 = l1;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l4;
      i1 = l1;
      i2 = 1u;
      i1 |= i2;
      i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
      i0 = l0;
      i1 = l2;
      i0 += i1;
      i1 = l1;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l0;
      i1 = p0;
      i2 = 3u;
      i1 |= i2;
      i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    } else {
      i0 = 1396u;
      i1 = 0u;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = 1408u;
      i1 = 0u;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l0;
      i1 = l2;
      i2 = 3u;
      i1 |= i2;
      i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
      i0 = l0;
      i1 = l2;
      i0 += i1;
      i1 = 4u;
      i0 += i1;
      p0 = i0;
      i1 = p0;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      i2 = 1u;
      i1 |= i2;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
    }
    i0 = l9;
    g4 = i0;
    i0 = l0;
    i1 = 8u;
    i0 += i1;
    goto Bfunc;
  }
  i0 = 1400u;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  l2 = i0;
  i1 = p0;
  i0 = i0 > i1;
  if (i0) {
    i0 = 1400u;
    i1 = l2;
    i2 = p0;
    i1 -= i2;
    l2 = i1;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1412u;
    i1 = 1412u;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    l0 = i1;
    i2 = p0;
    i1 += i2;
    l1 = i1;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l1;
    i1 = l2;
    i2 = 1u;
    i1 |= i2;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    i0 = l0;
    i1 = p0;
    i2 = 3u;
    i1 |= i2;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    i0 = l9;
    g4 = i0;
    i0 = l0;
    i1 = 8u;
    i0 += i1;
    goto Bfunc;
  }
  i0 = 1860u;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  if (i0) {
    i0 = 1868u;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
  } else {
    i0 = 1868u;
    i1 = 4096u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1864u;
    i1 = 4096u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1872u;
    i1 = 4294967295u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1876u;
    i1 = 4294967295u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1880u;
    i1 = 0u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1832u;
    i1 = 0u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1860u;
    i1 = l7;
    i2 = 4294967280u;
    i1 &= i2;
    i2 = 1431655768u;
    i1 ^= i2;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 4096u;
  }
  l0 = i0;
  i1 = p0;
  i2 = 47u;
  i1 += i2;
  l3 = i1;
  i0 += i1;
  l6 = i0;
  i1 = 0u;
  i2 = l0;
  i1 -= i2;
  l5 = i1;
  i0 &= i1;
  l4 = i0;
  i1 = p0;
  i0 = i0 <= i1;
  if (i0) {
    i0 = l9;
    g4 = i0;
    i0 = 0u;
    goto Bfunc;
  }
  i0 = 1828u;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  l0 = i0;
  if (i0) {
    i0 = 1820u;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l1 = i0;
    i1 = l4;
    i0 += i1;
    l7 = i0;
    i1 = l1;
    i0 = i0 <= i1;
    i1 = l7;
    i2 = l0;
    i1 = i1 > i2;
    i0 |= i1;
    if (i0) {
      i0 = l9;
      g4 = i0;
      i0 = 0u;
      goto Bfunc;
    }
  }
  i0 = p0;
  i1 = 48u;
  i0 += i1;
  l7 = i0;
  i0 = 1832u;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  i1 = 4u;
  i0 &= i1;
  if (i0) {
    i0 = 0u;
    l2 = i0;
  } else {
    i0 = 1412u;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l0 = i0;
    i0 = !(i0);
    if (i0) {goto B89;}
    i0 = 1836u;
    l1 = i0;
    L90: 
      i0 = l1;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l8 = i0;
      i1 = l0;
      i0 = i0 <= i1;
      if (i0) {
        i0 = l8;
        i1 = l1;
        i2 = 4u;
        i1 += i2;
        l8 = i1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1));
        i0 += i1;
        i1 = l0;
        i0 = i0 > i1;
        if (i0) {goto B91;}
      }
      i0 = l1;
      i0 = i32_load(Z_envZ_memory, (u64)(i0 + 8));
      l1 = i0;
      if (i0) {goto L90;}
      goto B89;
      B91:;
    i0 = l6;
    i1 = l2;
    i0 -= i1;
    i1 = l5;
    i0 &= i1;
    l2 = i0;
    i1 = 2147483647u;
    i0 = i0 < i1;
    if (i0) {
      i0 = l2;
      i0 = _sbrk(i0);
      l0 = i0;
      i1 = l1;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      i2 = l8;
      i2 = i32_load(Z_envZ_memory, (u64)(i2));
      i1 += i2;
      i0 = i0 == i1;
      if (i0) {
        i0 = l0;
        i1 = 4294967295u;
        i0 = i0 != i1;
        if (i0) {goto B85;}
      } else {
        goto B88;
      }
    } else {
      i0 = 0u;
      l2 = i0;
    }
    goto B87;
    B89:;
    i0 = 0u;
    i0 = _sbrk(i0);
    l0 = i0;
    i1 = 4294967295u;
    i0 = i0 == i1;
    if (i0) {
      i0 = 0u;
      l2 = i0;
    } else {
      i0 = 1864u;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l1 = i0;
      i1 = 4294967295u;
      i0 += i1;
      l6 = i0;
      i1 = l0;
      l2 = i1;
      i0 += i1;
      i1 = 0u;
      i2 = l1;
      i1 -= i2;
      i0 &= i1;
      i1 = l2;
      i0 -= i1;
      l1 = i0;
      i0 = l6;
      i1 = l2;
      i0 &= i1;
      if (i0) {
        i0 = l1;
      } else {
        i0 = 0u;
      }
      i1 = l4;
      i0 += i1;
      l2 = i0;
      i1 = 1820u;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      l6 = i1;
      i0 += i1;
      l1 = i0;
      i0 = l2;
      i1 = p0;
      i0 = i0 > i1;
      i1 = l2;
      i2 = 2147483647u;
      i1 = i1 < i2;
      i0 &= i1;
      if (i0) {
        i0 = 1828u;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l5 = i0;
        if (i0) {
          i0 = l1;
          i1 = l6;
          i0 = i0 <= i1;
          i1 = l1;
          i2 = l5;
          i1 = i1 > i2;
          i0 |= i1;
          if (i0) {
            i0 = 0u;
            l2 = i0;
            goto B87;
          }
        }
        i0 = l2;
        i0 = _sbrk(i0);
        l1 = i0;
        i1 = l0;
        i0 = i0 == i1;
        if (i0) {goto B85;}
        i0 = l1;
        l0 = i0;
        goto B88;
      } else {
        i0 = 0u;
        l2 = i0;
      }
    }
    goto B87;
    B88:;
    i0 = l7;
    i1 = l2;
    i0 = i0 > i1;
    i1 = l2;
    i2 = 2147483647u;
    i1 = i1 < i2;
    i2 = l0;
    i3 = 4294967295u;
    i2 = i2 != i3;
    i1 &= i2;
    i0 &= i1;
    i0 = !(i0);
    if (i0) {
      i0 = l0;
      i1 = 4294967295u;
      i0 = i0 == i1;
      if (i0) {
        i0 = 0u;
        l2 = i0;
        goto B87;
      } else {
        goto B85;
      }
      UNREACHABLE;
    }
    i0 = l3;
    i1 = l2;
    i0 -= i1;
    i1 = 1868u;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    l1 = i1;
    i0 += i1;
    i1 = 0u;
    i2 = l1;
    i1 -= i2;
    i0 &= i1;
    l1 = i0;
    i1 = 2147483647u;
    i0 = i0 >= i1;
    if (i0) {goto B85;}
    i0 = 0u;
    i1 = l2;
    i0 -= i1;
    l3 = i0;
    i0 = l1;
    i0 = _sbrk(i0);
    i1 = 4294967295u;
    i0 = i0 == i1;
    if (i0) {
      i0 = l3;
      i0 = _sbrk(i0);
      i0 = 0u;
      l2 = i0;
    } else {
      i0 = l1;
      i1 = l2;
      i0 += i1;
      l2 = i0;
      goto B85;
    }
    B87:;
    i0 = 1832u;
    i1 = 1832u;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    i2 = 4u;
    i1 |= i2;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
  }
  i0 = l4;
  i1 = 2147483647u;
  i0 = i0 < i1;
  if (i0) {
    i0 = l4;
    i0 = _sbrk(i0);
    l0 = i0;
    i1 = 0u;
    i1 = _sbrk(i1);
    l1 = i1;
    i0 = i0 < i1;
    i1 = l0;
    i2 = 4294967295u;
    i1 = i1 != i2;
    i2 = l1;
    i3 = 4294967295u;
    i2 = i2 != i3;
    i1 &= i2;
    i0 &= i1;
    l4 = i0;
    i0 = l1;
    i1 = l0;
    i0 -= i1;
    l1 = i0;
    i1 = p0;
    i2 = 40u;
    i1 += i2;
    i0 = i0 > i1;
    l3 = i0;
    if (i0) {
      i0 = l1;
      l2 = i0;
    }
    i0 = l0;
    i1 = 4294967295u;
    i0 = i0 == i1;
    i1 = l3;
    i2 = 1u;
    i1 ^= i2;
    i0 |= i1;
    i1 = l4;
    i2 = 1u;
    i1 ^= i2;
    i0 |= i1;
    i0 = !(i0);
    if (i0) {goto B85;}
  }
  goto B84;
  B85:;
  i0 = 1820u;
  i1 = 1820u;
  i1 = i32_load(Z_envZ_memory, (u64)(i1));
  i2 = l2;
  i1 += i2;
  l1 = i1;
  i32_store(Z_envZ_memory, (u64)(i0), i1);
  i0 = l1;
  i1 = 1824u;
  i1 = i32_load(Z_envZ_memory, (u64)(i1));
  i0 = i0 > i1;
  if (i0) {
    i0 = 1824u;
    i1 = l1;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
  }
  i0 = 1412u;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  l3 = i0;
  if (i0) {
    i0 = 1836u;
    l1 = i0;
    L110: 
      i0 = l0;
      i1 = l1;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      l4 = i1;
      i2 = l1;
      i3 = 4u;
      i2 += i3;
      l6 = i2;
      i2 = i32_load(Z_envZ_memory, (u64)(i2));
      l5 = i2;
      i1 += i2;
      i0 = i0 == i1;
      if (i0) {goto B109;}
      i0 = l1;
      i0 = i32_load(Z_envZ_memory, (u64)(i0 + 8));
      l1 = i0;
      if (i0) {goto L110;}
    goto B108;
    B109:;
    i0 = l1;
    i0 = i32_load(Z_envZ_memory, (u64)(i0 + 12));
    i1 = 8u;
    i0 &= i1;
    i0 = !(i0);
    if (i0) {
      i0 = l0;
      i1 = l3;
      i0 = i0 > i1;
      i1 = l4;
      i2 = l3;
      i1 = i1 <= i2;
      i0 &= i1;
      if (i0) {
        i0 = l6;
        i1 = l5;
        i2 = l2;
        i1 += i2;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = 1400u;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        i1 = l2;
        i0 += i1;
        l2 = i0;
        i0 = 0u;
        i1 = l3;
        i2 = 8u;
        i1 += i2;
        l1 = i1;
        i0 -= i1;
        i1 = 7u;
        i0 &= i1;
        l0 = i0;
        i0 = 1412u;
        i1 = l3;
        i2 = l1;
        i3 = 7u;
        i2 &= i3;
        if (i2) {
          i2 = l0;
        } else {
          i2 = 0u;
          l0 = i2;
        }
        i1 += i2;
        l1 = i1;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = 1400u;
        i1 = l2;
        i2 = l0;
        i1 -= i2;
        l0 = i1;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = l1;
        i1 = l0;
        i2 = 1u;
        i1 |= i2;
        i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
        i0 = l3;
        i1 = l2;
        i0 += i1;
        i1 = 40u;
        i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
        i0 = 1416u;
        i1 = 1876u;
        i1 = i32_load(Z_envZ_memory, (u64)(i1));
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        goto B106;
      }
    }
    B108:;
    i0 = l0;
    i1 = 1404u;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    i0 = i0 < i1;
    if (i0) {
      i0 = 1404u;
      i1 = l0;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
    }
    i0 = l0;
    i1 = l2;
    i0 += i1;
    l4 = i0;
    i0 = 1836u;
    l1 = i0;
    L117: 
      i0 = l1;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      i1 = l4;
      i0 = i0 == i1;
      if (i0) {goto B116;}
      i0 = l1;
      i0 = i32_load(Z_envZ_memory, (u64)(i0 + 8));
      l1 = i0;
      if (i0) {goto L117;}
      i0 = 1836u;
      l1 = i0;
    goto B115;
    B116:;
    i0 = l1;
    i0 = i32_load(Z_envZ_memory, (u64)(i0 + 12));
    i1 = 8u;
    i0 &= i1;
    if (i0) {
      i0 = 1836u;
      l1 = i0;
    } else {
      i0 = l1;
      i1 = l0;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l1;
      i1 = 4u;
      i0 += i1;
      l1 = i0;
      i1 = l1;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      i2 = l2;
      i1 += i2;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = 0u;
      i1 = l0;
      i2 = 8u;
      i1 += i2;
      l2 = i1;
      i0 -= i1;
      i1 = 7u;
      i0 &= i1;
      l1 = i0;
      i0 = 0u;
      i1 = l4;
      i2 = 8u;
      i1 += i2;
      l6 = i1;
      i0 -= i1;
      i1 = 7u;
      i0 &= i1;
      l8 = i0;
      i0 = l0;
      i1 = l2;
      i2 = 7u;
      i1 &= i2;
      if (i1) {
        i1 = l1;
      } else {
        i1 = 0u;
      }
      i0 += i1;
      l7 = i0;
      i1 = p0;
      i0 += i1;
      l5 = i0;
      i0 = l4;
      i1 = l6;
      i2 = 7u;
      i1 &= i2;
      if (i1) {
        i1 = l8;
      } else {
        i1 = 0u;
      }
      i0 += i1;
      l4 = i0;
      i1 = l7;
      i0 -= i1;
      i1 = p0;
      i0 -= i1;
      l6 = i0;
      i0 = l7;
      i1 = p0;
      i2 = 3u;
      i1 |= i2;
      i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
      i0 = l3;
      i1 = l4;
      i0 = i0 == i1;
      if (i0) {
        i0 = 1400u;
        i1 = 1400u;
        i1 = i32_load(Z_envZ_memory, (u64)(i1));
        i2 = l6;
        i1 += i2;
        p0 = i1;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = 1412u;
        i1 = l5;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = l5;
        i1 = p0;
        i2 = 1u;
        i1 |= i2;
        i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
      } else {
        i0 = 1408u;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        i1 = l4;
        i0 = i0 == i1;
        if (i0) {
          i0 = 1396u;
          i1 = 1396u;
          i1 = i32_load(Z_envZ_memory, (u64)(i1));
          i2 = l6;
          i1 += i2;
          p0 = i1;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          i0 = 1408u;
          i1 = l5;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          i0 = l5;
          i1 = p0;
          i2 = 1u;
          i1 |= i2;
          i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
          i0 = l5;
          i1 = p0;
          i0 += i1;
          i1 = p0;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          goto B121;
        }
        i0 = l4;
        i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
        p0 = i0;
        i1 = 3u;
        i0 &= i1;
        i1 = 1u;
        i0 = i0 == i1;
        if (i0) {
          i0 = p0;
          i1 = 4294967288u;
          i0 &= i1;
          l8 = i0;
          i0 = p0;
          i1 = 3u;
          i0 >>= (i1 & 31);
          l2 = i0;
          i0 = p0;
          i1 = 256u;
          i0 = i0 < i1;
          if (i0) {
            i0 = l4;
            i0 = i32_load(Z_envZ_memory, (u64)(i0 + 12));
            p0 = i0;
            i1 = l4;
            i1 = i32_load(Z_envZ_memory, (u64)(i1 + 8));
            l0 = i1;
            i0 = i0 == i1;
            if (i0) {
              i0 = 1388u;
              i1 = 1388u;
              i1 = i32_load(Z_envZ_memory, (u64)(i1));
              i2 = 1u;
              i3 = l2;
              i2 <<= (i3 & 31);
              i3 = 4294967295u;
              i2 ^= i3;
              i1 &= i2;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
            } else {
              i0 = l0;
              i1 = p0;
              i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
              i0 = p0;
              i1 = l0;
              i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
            }
          } else {
            i0 = l4;
            i0 = i32_load(Z_envZ_memory, (u64)(i0 + 24));
            l3 = i0;
            i0 = l4;
            i0 = i32_load(Z_envZ_memory, (u64)(i0 + 12));
            p0 = i0;
            i1 = l4;
            i0 = i0 == i1;
            if (i0) {
              i0 = l4;
              i1 = 16u;
              i0 += i1;
              l0 = i0;
              i1 = 4u;
              i0 += i1;
              l2 = i0;
              i0 = i32_load(Z_envZ_memory, (u64)(i0));
              p0 = i0;
              if (i0) {
                i0 = l2;
                l0 = i0;
              } else {
                i0 = l0;
                i0 = i32_load(Z_envZ_memory, (u64)(i0));
                p0 = i0;
                i0 = !(i0);
                if (i0) {
                  i0 = 0u;
                  p0 = i0;
                  goto B128;
                }
              }
              L132: 
                i0 = p0;
                i1 = 20u;
                i0 += i1;
                l2 = i0;
                i0 = i32_load(Z_envZ_memory, (u64)(i0));
                l1 = i0;
                if (i0) {
                  i0 = l1;
                  p0 = i0;
                  i0 = l2;
                  l0 = i0;
                  goto L132;
                }
                i0 = p0;
                i1 = 16u;
                i0 += i1;
                l2 = i0;
                i0 = i32_load(Z_envZ_memory, (u64)(i0));
                l1 = i0;
                if (i0) {
                  i0 = l1;
                  p0 = i0;
                  i0 = l2;
                  l0 = i0;
                  goto L132;
                }
              i0 = l0;
              i1 = 0u;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
            } else {
              i0 = l4;
              i0 = i32_load(Z_envZ_memory, (u64)(i0 + 8));
              l0 = i0;
              i1 = p0;
              i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
              i0 = p0;
              i1 = l0;
              i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
            }
            B128:;
            i0 = l3;
            i0 = !(i0);
            if (i0) {goto B125;}
            i0 = l4;
            i0 = i32_load(Z_envZ_memory, (u64)(i0 + 28));
            l0 = i0;
            i1 = 2u;
            i0 <<= (i1 & 31);
            i1 = 1692u;
            i0 += i1;
            l2 = i0;
            i0 = i32_load(Z_envZ_memory, (u64)(i0));
            i1 = l4;
            i0 = i0 == i1;
            if (i0) {
              i0 = l2;
              i1 = p0;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
              i0 = p0;
              if (i0) {goto B135;}
              i0 = 1392u;
              i1 = 1392u;
              i1 = i32_load(Z_envZ_memory, (u64)(i1));
              i2 = 1u;
              i3 = l0;
              i2 <<= (i3 & 31);
              i3 = 4294967295u;
              i2 ^= i3;
              i1 &= i2;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
              goto B125;
            } else {
              i0 = l3;
              i1 = 16u;
              i0 += i1;
              i1 = l3;
              i1 = i32_load(Z_envZ_memory, (u64)(i1 + 16));
              i2 = l4;
              i1 = i1 != i2;
              i2 = 2u;
              i1 <<= (i2 & 31);
              i0 += i1;
              i1 = p0;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
              i0 = p0;
              i0 = !(i0);
              if (i0) {goto B125;}
            }
            B135:;
            i0 = p0;
            i1 = l3;
            i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
            i0 = l4;
            i1 = 16u;
            i0 += i1;
            l2 = i0;
            i0 = i32_load(Z_envZ_memory, (u64)(i0));
            l0 = i0;
            if (i0) {
              i0 = p0;
              i1 = l0;
              i32_store(Z_envZ_memory, (u64)(i0 + 16), i1);
              i0 = l0;
              i1 = p0;
              i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
            }
            i0 = l2;
            i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
            l0 = i0;
            i0 = !(i0);
            if (i0) {goto B125;}
            i0 = p0;
            i1 = l0;
            i32_store(Z_envZ_memory, (u64)(i0 + 20), i1);
            i0 = l0;
            i1 = p0;
            i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
          }
          B125:;
          i0 = l4;
          i1 = l8;
          i0 += i1;
          p0 = i0;
          i0 = l8;
          i1 = l6;
          i0 += i1;
        } else {
          i0 = l4;
          p0 = i0;
          i0 = l6;
        }
        l4 = i0;
        i0 = p0;
        i1 = 4u;
        i0 += i1;
        p0 = i0;
        i1 = p0;
        i1 = i32_load(Z_envZ_memory, (u64)(i1));
        i2 = 4294967294u;
        i1 &= i2;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = l5;
        i1 = l4;
        i2 = 1u;
        i1 |= i2;
        i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
        i0 = l5;
        i1 = l4;
        i0 += i1;
        i1 = l4;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = l4;
        i1 = 3u;
        i0 >>= (i1 & 31);
        l0 = i0;
        i0 = l4;
        i1 = 256u;
        i0 = i0 < i1;
        if (i0) {
          i0 = l0;
          i1 = 3u;
          i0 <<= (i1 & 31);
          i1 = 1428u;
          i0 += i1;
          p0 = i0;
          i0 = 1388u;
          i0 = i32_load(Z_envZ_memory, (u64)(i0));
          l2 = i0;
          i1 = 1u;
          i2 = l0;
          i1 <<= (i2 & 31);
          l0 = i1;
          i0 &= i1;
          if (i0) {
            i0 = p0;
            i1 = 8u;
            i0 += i1;
            l2 = i0;
            i0 = i32_load(Z_envZ_memory, (u64)(i0));
          } else {
            i0 = 1388u;
            i1 = l2;
            i2 = l0;
            i1 |= i2;
            i32_store(Z_envZ_memory, (u64)(i0), i1);
            i0 = p0;
            i1 = 8u;
            i0 += i1;
            l2 = i0;
            i0 = p0;
          }
          l0 = i0;
          i0 = l2;
          i1 = l5;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          i0 = l0;
          i1 = l5;
          i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
          i0 = l5;
          i1 = l0;
          i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
          i0 = l5;
          i1 = p0;
          i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
          goto B121;
        }
        i0 = l4;
        i1 = 8u;
        i0 >>= (i1 & 31);
        p0 = i0;
        if (i0) {
          i0 = 31u;
          i1 = l4;
          i2 = 16777215u;
          i1 = i1 > i2;
          if (i1) {goto B140;}
          i0 = l4;
          i1 = 14u;
          i2 = p0;
          i3 = p0;
          i4 = 1048320u;
          i3 += i4;
          i4 = 16u;
          i3 >>= (i4 & 31);
          i4 = 8u;
          i3 &= i4;
          p0 = i3;
          i2 <<= (i3 & 31);
          l0 = i2;
          i3 = 520192u;
          i2 += i3;
          i3 = 16u;
          i2 >>= (i3 & 31);
          i3 = 4u;
          i2 &= i3;
          l2 = i2;
          i3 = p0;
          i2 |= i3;
          i3 = l0;
          i4 = l2;
          i3 <<= (i4 & 31);
          p0 = i3;
          i4 = 245760u;
          i3 += i4;
          i4 = 16u;
          i3 >>= (i4 & 31);
          i4 = 2u;
          i3 &= i4;
          l0 = i3;
          i2 |= i3;
          i1 -= i2;
          i2 = p0;
          i3 = l0;
          i2 <<= (i3 & 31);
          i3 = 15u;
          i2 >>= (i3 & 31);
          i1 += i2;
          p0 = i1;
          i2 = 7u;
          i1 += i2;
          i0 >>= (i1 & 31);
          i1 = 1u;
          i0 &= i1;
          i1 = p0;
          i2 = 1u;
          i1 <<= (i2 & 31);
          i0 |= i1;
        } else {
          i0 = 0u;
        }
        B140:;
        l0 = i0;
        i1 = 2u;
        i0 <<= (i1 & 31);
        i1 = 1692u;
        i0 += i1;
        p0 = i0;
        i0 = l5;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 28), i1);
        i0 = l5;
        i1 = 16u;
        i0 += i1;
        l2 = i0;
        i1 = 0u;
        i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
        i0 = l2;
        i1 = 0u;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = 1392u;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l2 = i0;
        i1 = 1u;
        i2 = l0;
        i1 <<= (i2 & 31);
        l1 = i1;
        i0 &= i1;
        i0 = !(i0);
        if (i0) {
          i0 = 1392u;
          i1 = l2;
          i2 = l1;
          i1 |= i2;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          i0 = p0;
          i1 = l5;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          i0 = l5;
          i1 = p0;
          i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
          i0 = l5;
          i1 = l5;
          i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
          i0 = l5;
          i1 = l5;
          i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
          goto B121;
        }
        i0 = p0;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        p0 = i0;
        i0 = 25u;
        i1 = l0;
        i2 = 1u;
        i1 >>= (i2 & 31);
        i0 -= i1;
        l2 = i0;
        i0 = l4;
        i1 = l0;
        i2 = 31u;
        i1 = i1 == i2;
        if (i1) {
          i1 = 0u;
        } else {
          i1 = l2;
        }
        i0 <<= (i1 & 31);
        l0 = i0;
        L145: 
          i0 = p0;
          i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
          i1 = 4294967288u;
          i0 &= i1;
          i1 = l4;
          i0 = i0 == i1;
          if (i0) {goto B144;}
          i0 = l0;
          i1 = 1u;
          i0 <<= (i1 & 31);
          l2 = i0;
          i0 = p0;
          i1 = 16u;
          i0 += i1;
          i1 = l0;
          i2 = 31u;
          i1 >>= (i2 & 31);
          i2 = 2u;
          i1 <<= (i2 & 31);
          i0 += i1;
          l0 = i0;
          i0 = i32_load(Z_envZ_memory, (u64)(i0));
          l1 = i0;
          if (i0) {
            i0 = l2;
            l0 = i0;
            i0 = l1;
            p0 = i0;
            goto L145;
          }
        i0 = l0;
        i1 = l5;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = l5;
        i1 = p0;
        i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
        i0 = l5;
        i1 = l5;
        i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
        i0 = l5;
        i1 = l5;
        i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
        goto B121;
        B144:;
        i0 = p0;
        i1 = 8u;
        i0 += i1;
        l0 = i0;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l2 = i0;
        i1 = l5;
        i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
        i0 = l0;
        i1 = l5;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = l5;
        i1 = l2;
        i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
        i0 = l5;
        i1 = p0;
        i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
        i0 = l5;
        i1 = 0u;
        i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
      }
      B121:;
      i0 = l9;
      g4 = i0;
      i0 = l7;
      i1 = 8u;
      i0 += i1;
      goto Bfunc;
    }
    B115:;
    L147: 
      i0 = l1;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l4 = i0;
      i1 = l3;
      i0 = i0 <= i1;
      if (i0) {
        i0 = l4;
        i1 = l1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 4));
        i0 += i1;
        l7 = i0;
        i1 = l3;
        i0 = i0 > i1;
        if (i0) {goto B148;}
      }
      i0 = l1;
      i0 = i32_load(Z_envZ_memory, (u64)(i0 + 8));
      l1 = i0;
      goto L147;
      B148:;
    i0 = 0u;
    i1 = l7;
    i2 = 4294967249u;
    i1 += i2;
    l1 = i1;
    i2 = 8u;
    i1 += i2;
    l4 = i1;
    i0 -= i1;
    i1 = 7u;
    i0 &= i1;
    l6 = i0;
    i0 = l1;
    i1 = l4;
    i2 = 7u;
    i1 &= i2;
    if (i1) {
      i1 = l6;
    } else {
      i1 = 0u;
    }
    i0 += i1;
    l1 = i0;
    i1 = l3;
    i2 = 16u;
    i1 += i2;
    l11 = i1;
    i0 = i0 < i1;
    if (i0) {
      i0 = l3;
      l1 = i0;
    } else {
      i0 = l1;
    }
    i1 = 8u;
    i0 += i1;
    l5 = i0;
    i0 = l1;
    i1 = 24u;
    i0 += i1;
    l4 = i0;
    i0 = l2;
    i1 = 4294967256u;
    i0 += i1;
    l8 = i0;
    i0 = 0u;
    i1 = l0;
    i2 = 8u;
    i1 += i2;
    l10 = i1;
    i0 -= i1;
    i1 = 7u;
    i0 &= i1;
    l6 = i0;
    i0 = 1412u;
    i1 = l0;
    i2 = l10;
    i3 = 7u;
    i2 &= i3;
    if (i2) {
      i2 = l6;
    } else {
      i2 = 0u;
      l6 = i2;
    }
    i1 += i2;
    l10 = i1;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1400u;
    i1 = l8;
    i2 = l6;
    i1 -= i2;
    l6 = i1;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l10;
    i1 = l6;
    i2 = 1u;
    i1 |= i2;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    i0 = l0;
    i1 = l8;
    i0 += i1;
    i1 = 40u;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    i0 = 1416u;
    i1 = 1876u;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l1;
    i1 = 4u;
    i0 += i1;
    l6 = i0;
    i1 = 27u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l5;
    i1 = 1836u;
    j1 = i64_load(Z_envZ_memory, (u64)(i1));
    i64_store(Z_envZ_memory, (u64)(i0), j1);
    i0 = l5;
    i1 = 1844u;
    j1 = i64_load(Z_envZ_memory, (u64)(i1));
    i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
    i0 = 1836u;
    i1 = l0;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1840u;
    i1 = l2;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1848u;
    i1 = 0u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1844u;
    i1 = l5;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l4;
    l0 = i0;
    L153: 
      i0 = l0;
      i1 = 4u;
      i0 += i1;
      l2 = i0;
      i1 = 7u;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l0;
      i1 = 8u;
      i0 += i1;
      i1 = l7;
      i0 = i0 < i1;
      if (i0) {
        i0 = l2;
        l0 = i0;
        goto L153;
      }
    i0 = l1;
    i1 = l3;
    i0 = i0 != i1;
    if (i0) {
      i0 = l6;
      i1 = l6;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      i2 = 4294967294u;
      i1 &= i2;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l3;
      i1 = l1;
      i2 = l3;
      i1 -= i2;
      l6 = i1;
      i2 = 1u;
      i1 |= i2;
      i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
      i0 = l1;
      i1 = l6;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l6;
      i1 = 3u;
      i0 >>= (i1 & 31);
      l2 = i0;
      i0 = l6;
      i1 = 256u;
      i0 = i0 < i1;
      if (i0) {
        i0 = l2;
        i1 = 3u;
        i0 <<= (i1 & 31);
        i1 = 1428u;
        i0 += i1;
        l0 = i0;
        i0 = 1388u;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l1 = i0;
        i1 = 1u;
        i2 = l2;
        i1 <<= (i2 & 31);
        l2 = i1;
        i0 &= i1;
        if (i0) {
          i0 = l0;
          i1 = 8u;
          i0 += i1;
          l1 = i0;
          i0 = i32_load(Z_envZ_memory, (u64)(i0));
        } else {
          i0 = 1388u;
          i1 = l1;
          i2 = l2;
          i1 |= i2;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          i0 = l0;
          i1 = 8u;
          i0 += i1;
          l1 = i0;
          i0 = l0;
        }
        l2 = i0;
        i0 = l1;
        i1 = l3;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = l2;
        i1 = l3;
        i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
        i0 = l3;
        i1 = l2;
        i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
        i0 = l3;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
        goto B106;
      }
      i0 = l6;
      i1 = 8u;
      i0 >>= (i1 & 31);
      l0 = i0;
      if (i0) {
        i0 = l6;
        i1 = 16777215u;
        i0 = i0 > i1;
        if (i0) {
          i0 = 31u;
        } else {
          i0 = l6;
          i1 = 14u;
          i2 = l0;
          i3 = l0;
          i4 = 1048320u;
          i3 += i4;
          i4 = 16u;
          i3 >>= (i4 & 31);
          i4 = 8u;
          i3 &= i4;
          l0 = i3;
          i2 <<= (i3 & 31);
          l2 = i2;
          i3 = 520192u;
          i2 += i3;
          i3 = 16u;
          i2 >>= (i3 & 31);
          i3 = 4u;
          i2 &= i3;
          l1 = i2;
          i3 = l0;
          i2 |= i3;
          i3 = l2;
          i4 = l1;
          i3 <<= (i4 & 31);
          l0 = i3;
          i4 = 245760u;
          i3 += i4;
          i4 = 16u;
          i3 >>= (i4 & 31);
          i4 = 2u;
          i3 &= i4;
          l2 = i3;
          i2 |= i3;
          i1 -= i2;
          i2 = l0;
          i3 = l2;
          i2 <<= (i3 & 31);
          i3 = 15u;
          i2 >>= (i3 & 31);
          i1 += i2;
          l0 = i1;
          i2 = 7u;
          i1 += i2;
          i0 >>= (i1 & 31);
          i1 = 1u;
          i0 &= i1;
          i1 = l0;
          i2 = 1u;
          i1 <<= (i2 & 31);
          i0 |= i1;
        }
      } else {
        i0 = 0u;
      }
      l2 = i0;
      i1 = 2u;
      i0 <<= (i1 & 31);
      i1 = 1692u;
      i0 += i1;
      l0 = i0;
      i0 = l3;
      i1 = l2;
      i32_store(Z_envZ_memory, (u64)(i0 + 28), i1);
      i0 = l3;
      i1 = 0u;
      i32_store(Z_envZ_memory, (u64)(i0 + 20), i1);
      i0 = l11;
      i1 = 0u;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = 1392u;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l1 = i0;
      i1 = 1u;
      i2 = l2;
      i1 <<= (i2 & 31);
      l4 = i1;
      i0 &= i1;
      i0 = !(i0);
      if (i0) {
        i0 = 1392u;
        i1 = l1;
        i2 = l4;
        i1 |= i2;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = l0;
        i1 = l3;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = l3;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
        i0 = l3;
        i1 = l3;
        i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
        i0 = l3;
        i1 = l3;
        i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
        goto B106;
      }
      i0 = l0;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l0 = i0;
      i0 = 25u;
      i1 = l2;
      i2 = 1u;
      i1 >>= (i2 & 31);
      i0 -= i1;
      l1 = i0;
      i0 = l6;
      i1 = l2;
      i2 = 31u;
      i1 = i1 == i2;
      if (i1) {
        i1 = 0u;
      } else {
        i1 = l1;
      }
      i0 <<= (i1 & 31);
      l2 = i0;
      L163: 
        i0 = l0;
        i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
        i1 = 4294967288u;
        i0 &= i1;
        i1 = l6;
        i0 = i0 == i1;
        if (i0) {goto B162;}
        i0 = l2;
        i1 = 1u;
        i0 <<= (i1 & 31);
        l1 = i0;
        i0 = l0;
        i1 = 16u;
        i0 += i1;
        i1 = l2;
        i2 = 31u;
        i1 >>= (i2 & 31);
        i2 = 2u;
        i1 <<= (i2 & 31);
        i0 += i1;
        l2 = i0;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l4 = i0;
        if (i0) {
          i0 = l1;
          l2 = i0;
          i0 = l4;
          l0 = i0;
          goto L163;
        }
      i0 = l2;
      i1 = l3;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l3;
      i1 = l0;
      i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
      i0 = l3;
      i1 = l3;
      i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
      i0 = l3;
      i1 = l3;
      i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
      goto B106;
      B162:;
      i0 = l0;
      i1 = 8u;
      i0 += i1;
      l2 = i0;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l1 = i0;
      i1 = l3;
      i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
      i0 = l2;
      i1 = l3;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l3;
      i1 = l1;
      i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
      i0 = l3;
      i1 = l0;
      i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
      i0 = l3;
      i1 = 0u;
      i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
    }
  } else {
    i0 = 1404u;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l1 = i0;
    i0 = !(i0);
    i1 = l0;
    i2 = l1;
    i1 = i1 < i2;
    i0 |= i1;
    if (i0) {
      i0 = 1404u;
      i1 = l0;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
    }
    i0 = 1836u;
    i1 = l0;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1840u;
    i1 = l2;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1848u;
    i1 = 0u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1424u;
    i1 = 1860u;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1420u;
    i1 = 4294967295u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1440u;
    i1 = 1428u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1436u;
    i1 = 1428u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1448u;
    i1 = 1436u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1444u;
    i1 = 1436u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1456u;
    i1 = 1444u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1452u;
    i1 = 1444u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1464u;
    i1 = 1452u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1460u;
    i1 = 1452u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1472u;
    i1 = 1460u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1468u;
    i1 = 1460u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1480u;
    i1 = 1468u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1476u;
    i1 = 1468u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1488u;
    i1 = 1476u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1484u;
    i1 = 1476u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1496u;
    i1 = 1484u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1492u;
    i1 = 1484u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1504u;
    i1 = 1492u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1500u;
    i1 = 1492u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1512u;
    i1 = 1500u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1508u;
    i1 = 1500u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1520u;
    i1 = 1508u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1516u;
    i1 = 1508u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1528u;
    i1 = 1516u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1524u;
    i1 = 1516u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1536u;
    i1 = 1524u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1532u;
    i1 = 1524u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1544u;
    i1 = 1532u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1540u;
    i1 = 1532u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1552u;
    i1 = 1540u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1548u;
    i1 = 1540u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1560u;
    i1 = 1548u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1556u;
    i1 = 1548u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1568u;
    i1 = 1556u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1564u;
    i1 = 1556u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1576u;
    i1 = 1564u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1572u;
    i1 = 1564u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1584u;
    i1 = 1572u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1580u;
    i1 = 1572u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1592u;
    i1 = 1580u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1588u;
    i1 = 1580u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1600u;
    i1 = 1588u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1596u;
    i1 = 1588u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1608u;
    i1 = 1596u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1604u;
    i1 = 1596u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1616u;
    i1 = 1604u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1612u;
    i1 = 1604u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1624u;
    i1 = 1612u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1620u;
    i1 = 1612u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1632u;
    i1 = 1620u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1628u;
    i1 = 1620u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1640u;
    i1 = 1628u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1636u;
    i1 = 1628u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1648u;
    i1 = 1636u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1644u;
    i1 = 1636u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1656u;
    i1 = 1644u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1652u;
    i1 = 1644u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1664u;
    i1 = 1652u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1660u;
    i1 = 1652u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1672u;
    i1 = 1660u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1668u;
    i1 = 1660u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1680u;
    i1 = 1668u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1676u;
    i1 = 1668u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1688u;
    i1 = 1676u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1684u;
    i1 = 1676u;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l2;
    i1 = 4294967256u;
    i0 += i1;
    l1 = i0;
    i0 = 0u;
    i1 = l0;
    i2 = 8u;
    i1 += i2;
    l4 = i1;
    i0 -= i1;
    i1 = 7u;
    i0 &= i1;
    l2 = i0;
    i0 = 1412u;
    i1 = l0;
    i2 = l4;
    i3 = 7u;
    i2 &= i3;
    if (i2) {
      i2 = l2;
    } else {
      i2 = 0u;
      l2 = i2;
    }
    i1 += i2;
    l4 = i1;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1400u;
    i1 = l1;
    i2 = l2;
    i1 -= i2;
    l2 = i1;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l4;
    i1 = l2;
    i2 = 1u;
    i1 |= i2;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    i0 = l0;
    i1 = l1;
    i0 += i1;
    i1 = 40u;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    i0 = 1416u;
    i1 = 1876u;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    i32_store(Z_envZ_memory, (u64)(i0), i1);
  }
  B106:;
  i0 = 1400u;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  l0 = i0;
  i1 = p0;
  i0 = i0 > i1;
  if (i0) {
    i0 = 1400u;
    i1 = l0;
    i2 = p0;
    i1 -= i2;
    l2 = i1;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 1412u;
    i1 = 1412u;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    l0 = i1;
    i2 = p0;
    i1 += i2;
    l1 = i1;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l1;
    i1 = l2;
    i2 = 1u;
    i1 |= i2;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    i0 = l0;
    i1 = p0;
    i2 = 3u;
    i1 |= i2;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    i0 = l9;
    g4 = i0;
    i0 = l0;
    i1 = 8u;
    i0 += i1;
    goto Bfunc;
  }
  B84:;
  i0 = 1884u;
  i1 = 12u;
  i32_store(Z_envZ_memory, (u64)(i0), i1);
  i0 = l9;
  g4 = i0;
  i0 = 0u;
  Bfunc:;
  FUNC_EPILOGUE;
  return i0;
}

static void _free(u32 p0) {
  u32 l0 = 0, l1 = 0, l2 = 0, l3 = 0, l4 = 0, l5 = 0, l6 = 0, l7 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3, i4;
  i0 = p0;
  i0 = !(i0);
  if (i0) {
    goto Bfunc;
  }
  i0 = 1404u;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  l3 = i0;
  i0 = p0;
  i1 = 4294967288u;
  i0 += i1;
  l0 = i0;
  i1 = p0;
  i2 = 4294967292u;
  i1 += i2;
  i1 = i32_load(Z_envZ_memory, (u64)(i1));
  p0 = i1;
  i2 = 4294967288u;
  i1 &= i2;
  l2 = i1;
  i0 += i1;
  l4 = i0;
  i0 = p0;
  i1 = 1u;
  i0 &= i1;
  if (i0) {
    i0 = l0;
    p0 = i0;
    i0 = l0;
  } else {
    i0 = l0;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l1 = i0;
    i0 = p0;
    i1 = 3u;
    i0 &= i1;
    i0 = !(i0);
    if (i0) {
      goto Bfunc;
    }
    i0 = l0;
    i1 = l1;
    i0 -= i1;
    p0 = i0;
    i1 = l3;
    i0 = i0 < i1;
    if (i0) {
      goto Bfunc;
    }
    i0 = l1;
    i1 = l2;
    i0 += i1;
    l2 = i0;
    i0 = 1408u;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    i1 = p0;
    i0 = i0 == i1;
    if (i0) {
      i0 = p0;
      i1 = l4;
      i2 = 4u;
      i1 += i2;
      l1 = i1;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      l0 = i1;
      i2 = 3u;
      i1 &= i2;
      i2 = 3u;
      i1 = i1 != i2;
      if (i1) {goto B1;}
      i0 = 1396u;
      i1 = l2;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l1;
      i1 = l0;
      i2 = 4294967294u;
      i1 &= i2;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = p0;
      i1 = l2;
      i2 = 1u;
      i1 |= i2;
      i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
      i0 = p0;
      i1 = l2;
      i0 += i1;
      i1 = l2;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      goto Bfunc;
    }
    i0 = l1;
    i1 = 3u;
    i0 >>= (i1 & 31);
    l3 = i0;
    i0 = l1;
    i1 = 256u;
    i0 = i0 < i1;
    if (i0) {
      i0 = p0;
      i0 = i32_load(Z_envZ_memory, (u64)(i0 + 12));
      l1 = i0;
      i1 = p0;
      i1 = i32_load(Z_envZ_memory, (u64)(i1 + 8));
      l0 = i1;
      i0 = i0 == i1;
      if (i0) {
        i0 = 1388u;
        i1 = 1388u;
        i1 = i32_load(Z_envZ_memory, (u64)(i1));
        i2 = 1u;
        i3 = l3;
        i2 <<= (i3 & 31);
        i3 = 4294967295u;
        i2 ^= i3;
        i1 &= i2;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = p0;
        goto B1;
      } else {
        i0 = l0;
        i1 = l1;
        i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
        i0 = l1;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
        i0 = p0;
        goto B1;
      }
      UNREACHABLE;
    }
    i0 = p0;
    i0 = i32_load(Z_envZ_memory, (u64)(i0 + 24));
    l6 = i0;
    i0 = p0;
    i0 = i32_load(Z_envZ_memory, (u64)(i0 + 12));
    l1 = i0;
    i1 = p0;
    i0 = i0 == i1;
    if (i0) {
      i0 = p0;
      i1 = 16u;
      i0 += i1;
      l0 = i0;
      i1 = 4u;
      i0 += i1;
      l3 = i0;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l1 = i0;
      if (i0) {
        i0 = l3;
        l0 = i0;
      } else {
        i0 = l0;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l1 = i0;
        i0 = !(i0);
        if (i0) {
          i0 = 0u;
          l1 = i0;
          goto B8;
        }
      }
      L12: 
        i0 = l1;
        i1 = 20u;
        i0 += i1;
        l3 = i0;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l5 = i0;
        if (i0) {
          i0 = l5;
          l1 = i0;
          i0 = l3;
          l0 = i0;
          goto L12;
        }
        i0 = l1;
        i1 = 16u;
        i0 += i1;
        l3 = i0;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l5 = i0;
        if (i0) {
          i0 = l5;
          l1 = i0;
          i0 = l3;
          l0 = i0;
          goto L12;
        }
      i0 = l0;
      i1 = 0u;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
    } else {
      i0 = p0;
      i0 = i32_load(Z_envZ_memory, (u64)(i0 + 8));
      l0 = i0;
      i1 = l1;
      i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
      i0 = l1;
      i1 = l0;
      i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
    }
    B8:;
    i0 = l6;
    if (i0) {
      i0 = p0;
      i0 = i32_load(Z_envZ_memory, (u64)(i0 + 28));
      l0 = i0;
      i1 = 2u;
      i0 <<= (i1 & 31);
      i1 = 1692u;
      i0 += i1;
      l3 = i0;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      i1 = p0;
      i0 = i0 == i1;
      if (i0) {
        i0 = l3;
        i1 = l1;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = l1;
        i0 = !(i0);
        if (i0) {
          i0 = 1392u;
          i1 = 1392u;
          i1 = i32_load(Z_envZ_memory, (u64)(i1));
          i2 = 1u;
          i3 = l0;
          i2 <<= (i3 & 31);
          i3 = 4294967295u;
          i2 ^= i3;
          i1 &= i2;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          i0 = p0;
          goto B1;
        }
      } else {
        i0 = l6;
        i1 = 16u;
        i0 += i1;
        i1 = l6;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 16));
        i2 = p0;
        i1 = i1 != i2;
        i2 = 2u;
        i1 <<= (i2 & 31);
        i0 += i1;
        i1 = l1;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = p0;
        i1 = l1;
        i1 = !(i1);
        if (i1) {goto B1;}
      }
      i0 = l1;
      i1 = l6;
      i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
      i0 = p0;
      i1 = 16u;
      i0 += i1;
      l3 = i0;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l0 = i0;
      if (i0) {
        i0 = l1;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 16), i1);
        i0 = l0;
        i1 = l1;
        i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
      }
      i0 = l3;
      i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
      l0 = i0;
      if (i0) {
        i0 = l1;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 20), i1);
        i0 = l0;
        i1 = l1;
        i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
        i0 = p0;
      } else {
        i0 = p0;
      }
    } else {
      i0 = p0;
    }
  }
  B1:;
  l1 = i0;
  i0 = p0;
  i1 = l4;
  i0 = i0 >= i1;
  if (i0) {
    goto Bfunc;
  }
  i0 = l4;
  i1 = 4u;
  i0 += i1;
  l3 = i0;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  l0 = i0;
  i1 = 1u;
  i0 &= i1;
  i0 = !(i0);
  if (i0) {
    goto Bfunc;
  }
  i0 = l0;
  i1 = 2u;
  i0 &= i1;
  if (i0) {
    i0 = l3;
    i1 = l0;
    i2 = 4294967294u;
    i1 &= i2;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l1;
    i1 = l2;
    i2 = 1u;
    i1 |= i2;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    i0 = p0;
    i1 = l2;
    i0 += i1;
    i1 = l2;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
  } else {
    i0 = 1412u;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    i1 = l4;
    i0 = i0 == i1;
    if (i0) {
      i0 = 1400u;
      i1 = 1400u;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      i2 = l2;
      i1 += i2;
      p0 = i1;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = 1412u;
      i1 = l1;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l1;
      i1 = p0;
      i2 = 1u;
      i1 |= i2;
      i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
      i0 = l1;
      i1 = 1408u;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      i0 = i0 != i1;
      if (i0) {
        goto Bfunc;
      }
      i0 = 1408u;
      i1 = 0u;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = 1396u;
      i1 = 0u;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      goto Bfunc;
    }
    i0 = 1408u;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    i1 = l4;
    i0 = i0 == i1;
    if (i0) {
      i0 = 1396u;
      i1 = 1396u;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      i2 = l2;
      i1 += i2;
      l2 = i1;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = 1408u;
      i1 = p0;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l1;
      i1 = l2;
      i2 = 1u;
      i1 |= i2;
      i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
      i0 = p0;
      i1 = l2;
      i0 += i1;
      i1 = l2;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      goto Bfunc;
    }
    i0 = l0;
    i1 = 4294967288u;
    i0 &= i1;
    i1 = l2;
    i0 += i1;
    l6 = i0;
    i0 = l0;
    i1 = 3u;
    i0 >>= (i1 & 31);
    l3 = i0;
    i0 = l0;
    i1 = 256u;
    i0 = i0 < i1;
    if (i0) {
      i0 = l4;
      i0 = i32_load(Z_envZ_memory, (u64)(i0 + 12));
      l2 = i0;
      i1 = l4;
      i1 = i32_load(Z_envZ_memory, (u64)(i1 + 8));
      l0 = i1;
      i0 = i0 == i1;
      if (i0) {
        i0 = 1388u;
        i1 = 1388u;
        i1 = i32_load(Z_envZ_memory, (u64)(i1));
        i2 = 1u;
        i3 = l3;
        i2 <<= (i3 & 31);
        i3 = 4294967295u;
        i2 ^= i3;
        i1 &= i2;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
      } else {
        i0 = l0;
        i1 = l2;
        i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
        i0 = l2;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
      }
    } else {
      i0 = l4;
      i0 = i32_load(Z_envZ_memory, (u64)(i0 + 24));
      l7 = i0;
      i0 = l4;
      i0 = i32_load(Z_envZ_memory, (u64)(i0 + 12));
      l2 = i0;
      i1 = l4;
      i0 = i0 == i1;
      if (i0) {
        i0 = l4;
        i1 = 16u;
        i0 += i1;
        l0 = i0;
        i1 = 4u;
        i0 += i1;
        l3 = i0;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l2 = i0;
        if (i0) {
          i0 = l3;
          l0 = i0;
        } else {
          i0 = l0;
          i0 = i32_load(Z_envZ_memory, (u64)(i0));
          l2 = i0;
          i0 = !(i0);
          if (i0) {
            i0 = 0u;
            l2 = i0;
            goto B29;
          }
        }
        L33: 
          i0 = l2;
          i1 = 20u;
          i0 += i1;
          l3 = i0;
          i0 = i32_load(Z_envZ_memory, (u64)(i0));
          l5 = i0;
          if (i0) {
            i0 = l5;
            l2 = i0;
            i0 = l3;
            l0 = i0;
            goto L33;
          }
          i0 = l2;
          i1 = 16u;
          i0 += i1;
          l3 = i0;
          i0 = i32_load(Z_envZ_memory, (u64)(i0));
          l5 = i0;
          if (i0) {
            i0 = l5;
            l2 = i0;
            i0 = l3;
            l0 = i0;
            goto L33;
          }
        i0 = l0;
        i1 = 0u;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
      } else {
        i0 = l4;
        i0 = i32_load(Z_envZ_memory, (u64)(i0 + 8));
        l0 = i0;
        i1 = l2;
        i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
        i0 = l2;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
      }
      B29:;
      i0 = l7;
      if (i0) {
        i0 = l4;
        i0 = i32_load(Z_envZ_memory, (u64)(i0 + 28));
        l0 = i0;
        i1 = 2u;
        i0 <<= (i1 & 31);
        i1 = 1692u;
        i0 += i1;
        l3 = i0;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        i1 = l4;
        i0 = i0 == i1;
        if (i0) {
          i0 = l3;
          i1 = l2;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          i0 = l2;
          i0 = !(i0);
          if (i0) {
            i0 = 1392u;
            i1 = 1392u;
            i1 = i32_load(Z_envZ_memory, (u64)(i1));
            i2 = 1u;
            i3 = l0;
            i2 <<= (i3 & 31);
            i3 = 4294967295u;
            i2 ^= i3;
            i1 &= i2;
            i32_store(Z_envZ_memory, (u64)(i0), i1);
            goto B26;
          }
        } else {
          i0 = l7;
          i1 = 16u;
          i0 += i1;
          i1 = l7;
          i1 = i32_load(Z_envZ_memory, (u64)(i1 + 16));
          i2 = l4;
          i1 = i1 != i2;
          i2 = 2u;
          i1 <<= (i2 & 31);
          i0 += i1;
          i1 = l2;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          i0 = l2;
          i0 = !(i0);
          if (i0) {goto B26;}
        }
        i0 = l2;
        i1 = l7;
        i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
        i0 = l4;
        i1 = 16u;
        i0 += i1;
        l3 = i0;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l0 = i0;
        if (i0) {
          i0 = l2;
          i1 = l0;
          i32_store(Z_envZ_memory, (u64)(i0 + 16), i1);
          i0 = l0;
          i1 = l2;
          i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
        }
        i0 = l3;
        i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
        l0 = i0;
        if (i0) {
          i0 = l2;
          i1 = l0;
          i32_store(Z_envZ_memory, (u64)(i0 + 20), i1);
          i0 = l0;
          i1 = l2;
          i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
        }
      }
    }
    B26:;
    i0 = l1;
    i1 = l6;
    i2 = 1u;
    i1 |= i2;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    i0 = p0;
    i1 = l6;
    i0 += i1;
    i1 = l6;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l1;
    i1 = 1408u;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    i0 = i0 == i1;
    if (i0) {
      i0 = 1396u;
      i1 = l6;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      goto Bfunc;
    } else {
      i0 = l6;
      l2 = i0;
    }
  }
  i0 = l2;
  i1 = 3u;
  i0 >>= (i1 & 31);
  l0 = i0;
  i0 = l2;
  i1 = 256u;
  i0 = i0 < i1;
  if (i0) {
    i0 = l0;
    i1 = 3u;
    i0 <<= (i1 & 31);
    i1 = 1428u;
    i0 += i1;
    p0 = i0;
    i0 = 1388u;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l2 = i0;
    i1 = 1u;
    i2 = l0;
    i1 <<= (i2 & 31);
    l0 = i1;
    i0 &= i1;
    if (i0) {
      i0 = p0;
      i1 = 8u;
      i0 += i1;
      l0 = i0;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
    } else {
      i0 = 1388u;
      i1 = l2;
      i2 = l0;
      i1 |= i2;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = p0;
      i1 = 8u;
      i0 += i1;
      l0 = i0;
      i0 = p0;
    }
    l2 = i0;
    i0 = l0;
    i1 = l1;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l2;
    i1 = l1;
    i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
    i0 = l1;
    i1 = l2;
    i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
    i0 = l1;
    i1 = p0;
    i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
    goto Bfunc;
  }
  i0 = l2;
  i1 = 8u;
  i0 >>= (i1 & 31);
  p0 = i0;
  if (i0) {
    i0 = l2;
    i1 = 16777215u;
    i0 = i0 > i1;
    if (i0) {
      i0 = 31u;
    } else {
      i0 = l2;
      i1 = 14u;
      i2 = p0;
      i3 = p0;
      i4 = 1048320u;
      i3 += i4;
      i4 = 16u;
      i3 >>= (i4 & 31);
      i4 = 8u;
      i3 &= i4;
      p0 = i3;
      i2 <<= (i3 & 31);
      l0 = i2;
      i3 = 520192u;
      i2 += i3;
      i3 = 16u;
      i2 >>= (i3 & 31);
      i3 = 4u;
      i2 &= i3;
      l3 = i2;
      i3 = p0;
      i2 |= i3;
      i3 = l0;
      i4 = l3;
      i3 <<= (i4 & 31);
      p0 = i3;
      i4 = 245760u;
      i3 += i4;
      i4 = 16u;
      i3 >>= (i4 & 31);
      i4 = 2u;
      i3 &= i4;
      l0 = i3;
      i2 |= i3;
      i1 -= i2;
      i2 = p0;
      i3 = l0;
      i2 <<= (i3 & 31);
      i3 = 15u;
      i2 >>= (i3 & 31);
      i1 += i2;
      p0 = i1;
      i2 = 7u;
      i1 += i2;
      i0 >>= (i1 & 31);
      i1 = 1u;
      i0 &= i1;
      i1 = p0;
      i2 = 1u;
      i1 <<= (i2 & 31);
      i0 |= i1;
    }
  } else {
    i0 = 0u;
  }
  l0 = i0;
  i1 = 2u;
  i0 <<= (i1 & 31);
  i1 = 1692u;
  i0 += i1;
  p0 = i0;
  i0 = l1;
  i1 = l0;
  i32_store(Z_envZ_memory, (u64)(i0 + 28), i1);
  i0 = l1;
  i1 = 0u;
  i32_store(Z_envZ_memory, (u64)(i0 + 20), i1);
  i0 = l1;
  i1 = 0u;
  i32_store(Z_envZ_memory, (u64)(i0 + 16), i1);
  i0 = 1392u;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  l3 = i0;
  i1 = 1u;
  i2 = l0;
  i1 <<= (i2 & 31);
  l5 = i1;
  i0 &= i1;
  if (i0) {
    i0 = p0;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    p0 = i0;
    i0 = 25u;
    i1 = l0;
    i2 = 1u;
    i1 >>= (i2 & 31);
    i0 -= i1;
    l3 = i0;
    i0 = l2;
    i1 = l0;
    i2 = 31u;
    i1 = i1 == i2;
    if (i1) {
      i1 = 0u;
    } else {
      i1 = l3;
    }
    i0 <<= (i1 & 31);
    l0 = i0;
    L50: 
      i0 = p0;
      i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
      i1 = 4294967288u;
      i0 &= i1;
      i1 = l2;
      i0 = i0 == i1;
      if (i0) {goto B49;}
      i0 = l0;
      i1 = 1u;
      i0 <<= (i1 & 31);
      l3 = i0;
      i0 = p0;
      i1 = 16u;
      i0 += i1;
      i1 = l0;
      i2 = 31u;
      i1 >>= (i2 & 31);
      i2 = 2u;
      i1 <<= (i2 & 31);
      i0 += i1;
      l0 = i0;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l5 = i0;
      if (i0) {
        i0 = l3;
        l0 = i0;
        i0 = l5;
        p0 = i0;
        goto L50;
      }
    i0 = l0;
    i1 = l1;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l1;
    i1 = p0;
    i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
    i0 = l1;
    i1 = l1;
    i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
    i0 = l1;
    i1 = l1;
    i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
    goto B46;
    B49:;
    i0 = p0;
    i1 = 8u;
    i0 += i1;
    l2 = i0;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l0 = i0;
    i1 = l1;
    i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
    i0 = l2;
    i1 = l1;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l1;
    i1 = l0;
    i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
    i0 = l1;
    i1 = p0;
    i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
    i0 = l1;
    i1 = 0u;
    i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
  } else {
    i0 = 1392u;
    i1 = l3;
    i2 = l5;
    i1 |= i2;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = p0;
    i1 = l1;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l1;
    i1 = p0;
    i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
    i0 = l1;
    i1 = l1;
    i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
    i0 = l1;
    i1 = l1;
    i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
  }
  B46:;
  i0 = 1420u;
  i1 = 1420u;
  i1 = i32_load(Z_envZ_memory, (u64)(i1));
  i2 = 4294967295u;
  i1 += i2;
  p0 = i1;
  i32_store(Z_envZ_memory, (u64)(i0), i1);
  i0 = p0;
  if (i0) {
    goto Bfunc;
  } else {
    i0 = 1844u;
    p0 = i0;
  }
  L53: 
    i0 = p0;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l2 = i0;
    i1 = 8u;
    i0 += i1;
    p0 = i0;
    i0 = l2;
    if (i0) {goto L53;}
  i0 = 1420u;
  i1 = 4294967295u;
  i32_store(Z_envZ_memory, (u64)(i0), i1);
  Bfunc:;
  FUNC_EPILOGUE;
}

static u32 f21(u32 p0, u32 p1) {
  u32 l0 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  i0 = p0;
  if (i0) {
    i0 = p1;
    i1 = p0;
    i0 *= i1;
    l0 = i0;
    i0 = p1;
    i1 = p0;
    i0 |= i1;
    i1 = 65535u;
    i0 = i0 > i1;
    if (i0) {
      i0 = l0;
      i1 = p0;
      i0 = DIV_U(i0, i1);
      i1 = p1;
      i0 = i0 != i1;
      if (i0) {
        i0 = 4294967295u;
        l0 = i0;
      }
    }
  }
  i0 = l0;
  i0 = _malloc(i0);
  p0 = i0;
  i0 = !(i0);
  if (i0) {
    i0 = p0;
    goto Bfunc;
  }
  i0 = p0;
  i1 = 4294967292u;
  i0 += i1;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  i1 = 3u;
  i0 &= i1;
  i0 = !(i0);
  if (i0) {
    i0 = p0;
    goto Bfunc;
  }
  i0 = p0;
  i1 = 0u;
  i2 = l0;
  i0 = _memset(i0, i1, i2);
  i0 = p0;
  Bfunc:;
  FUNC_EPILOGUE;
  return i0;
}

static u32 ___errno_location(void) {
  FUNC_PROLOGUE;
  u32 i0;
  i0 = 1884u;
  FUNC_EPILOGUE;
  return i0;
}

static void runPostSets(void) {
  FUNC_PROLOGUE;
  FUNC_EPILOGUE;
}

static u32 _memcpy(u32 p0, u32 p1, u32 p2) {
  u32 l0 = 0, l1 = 0, l2 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  i0 = p2;
  i1 = 8192u;
  i0 = (u32)((s32)i0 >= (s32)i1);
  if (i0) {
    i0 = p0;
    i1 = p1;
    i2 = p2;
    i0 = (*Z_envZ__emscripten_memcpy_bigZ_iiii)(i0, i1, i2);
    goto Bfunc;
  }
  i0 = p0;
  l1 = i0;
  i0 = p0;
  i1 = p2;
  i0 += i1;
  l0 = i0;
  i0 = p0;
  i1 = 3u;
  i0 &= i1;
  i1 = p1;
  i2 = 3u;
  i1 &= i2;
  i0 = i0 == i1;
  if (i0) {
    L2: 
      i0 = p0;
      i1 = 3u;
      i0 &= i1;
      if (i0) {
        i0 = p2;
        i0 = !(i0);
        if (i0) {
          i0 = l1;
          goto Bfunc;
        }
        i0 = p0;
        i1 = p1;
        i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
        i32_store8(Z_envZ_memory, (u64)(i0), i1);
        i0 = p0;
        i1 = 1u;
        i0 += i1;
        p0 = i0;
        i0 = p1;
        i1 = 1u;
        i0 += i1;
        p1 = i0;
        i0 = p2;
        i1 = 1u;
        i0 -= i1;
        p2 = i0;
        goto L2;
      }
    i0 = l0;
    i1 = 4294967292u;
    i0 &= i1;
    p2 = i0;
    i1 = 64u;
    i0 -= i1;
    l2 = i0;
    L5: 
      i0 = p0;
      i1 = l2;
      i0 = (u32)((s32)i0 <= (s32)i1);
      if (i0) {
        i0 = p0;
        i1 = p1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1));
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = p0;
        i1 = p1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 4));
        i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
        i0 = p0;
        i1 = p1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 8));
        i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
        i0 = p0;
        i1 = p1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 12));
        i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
        i0 = p0;
        i1 = p1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 16));
        i32_store(Z_envZ_memory, (u64)(i0 + 16), i1);
        i0 = p0;
        i1 = p1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 20));
        i32_store(Z_envZ_memory, (u64)(i0 + 20), i1);
        i0 = p0;
        i1 = p1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 24));
        i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
        i0 = p0;
        i1 = p1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 28));
        i32_store(Z_envZ_memory, (u64)(i0 + 28), i1);
        i0 = p0;
        i1 = p1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 32));
        i32_store(Z_envZ_memory, (u64)(i0 + 32), i1);
        i0 = p0;
        i1 = p1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 36));
        i32_store(Z_envZ_memory, (u64)(i0 + 36), i1);
        i0 = p0;
        i1 = p1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 40));
        i32_store(Z_envZ_memory, (u64)(i0 + 40), i1);
        i0 = p0;
        i1 = p1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 44));
        i32_store(Z_envZ_memory, (u64)(i0 + 44), i1);
        i0 = p0;
        i1 = p1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 48));
        i32_store(Z_envZ_memory, (u64)(i0 + 48), i1);
        i0 = p0;
        i1 = p1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 52));
        i32_store(Z_envZ_memory, (u64)(i0 + 52), i1);
        i0 = p0;
        i1 = p1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 56));
        i32_store(Z_envZ_memory, (u64)(i0 + 56), i1);
        i0 = p0;
        i1 = p1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 60));
        i32_store(Z_envZ_memory, (u64)(i0 + 60), i1);
        i0 = p0;
        i1 = 64u;
        i0 += i1;
        p0 = i0;
        i0 = p1;
        i1 = 64u;
        i0 += i1;
        p1 = i0;
        goto L5;
      }
    L7: 
      i0 = p0;
      i1 = p2;
      i0 = (u32)((s32)i0 < (s32)i1);
      if (i0) {
        i0 = p0;
        i1 = p1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1));
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = p0;
        i1 = 4u;
        i0 += i1;
        p0 = i0;
        i0 = p1;
        i1 = 4u;
        i0 += i1;
        p1 = i0;
        goto L7;
      }
  } else {
    i0 = l0;
    i1 = 4u;
    i0 -= i1;
    p2 = i0;
    L9: 
      i0 = p0;
      i1 = p2;
      i0 = (u32)((s32)i0 < (s32)i1);
      if (i0) {
        i0 = p0;
        i1 = p1;
        i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
        i32_store8(Z_envZ_memory, (u64)(i0), i1);
        i0 = p0;
        i1 = p1;
        i1 = i32_load8_s(Z_envZ_memory, (u64)(i1 + 1));
        i32_store8(Z_envZ_memory, (u64)(i0 + 1), i1);
        i0 = p0;
        i1 = p1;
        i1 = i32_load8_s(Z_envZ_memory, (u64)(i1 + 2));
        i32_store8(Z_envZ_memory, (u64)(i0 + 2), i1);
        i0 = p0;
        i1 = p1;
        i1 = i32_load8_s(Z_envZ_memory, (u64)(i1 + 3));
        i32_store8(Z_envZ_memory, (u64)(i0 + 3), i1);
        i0 = p0;
        i1 = 4u;
        i0 += i1;
        p0 = i0;
        i0 = p1;
        i1 = 4u;
        i0 += i1;
        p1 = i0;
        goto L9;
      }
  }
  L11: 
    i0 = p0;
    i1 = l0;
    i0 = (u32)((s32)i0 < (s32)i1);
    if (i0) {
      i0 = p0;
      i1 = p1;
      i1 = i32_load8_s(Z_envZ_memory, (u64)(i1));
      i32_store8(Z_envZ_memory, (u64)(i0), i1);
      i0 = p0;
      i1 = 1u;
      i0 += i1;
      p0 = i0;
      i0 = p1;
      i1 = 1u;
      i0 += i1;
      p1 = i0;
      goto L11;
    }
  i0 = l1;
  Bfunc:;
  FUNC_EPILOGUE;
  return i0;
}

static u32 _memset(u32 p0, u32 p1, u32 p2) {
  u32 l0 = 0, l1 = 0, l2 = 0, l3 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  i0 = p0;
  i1 = p2;
  i0 += i1;
  l1 = i0;
  i0 = p1;
  i1 = 255u;
  i0 &= i1;
  p1 = i0;
  i0 = p2;
  i1 = 67u;
  i0 = (u32)((s32)i0 >= (s32)i1);
  if (i0) {
    L1: 
      i0 = p0;
      i1 = 3u;
      i0 &= i1;
      if (i0) {
        i0 = p0;
        i1 = p1;
        i32_store8(Z_envZ_memory, (u64)(i0), i1);
        i0 = p0;
        i1 = 1u;
        i0 += i1;
        p0 = i0;
        goto L1;
      }
    i0 = l1;
    i1 = 4294967292u;
    i0 &= i1;
    l2 = i0;
    i1 = 64u;
    i0 -= i1;
    l3 = i0;
    i0 = p1;
    i1 = p1;
    i2 = 8u;
    i1 <<= (i2 & 31);
    i0 |= i1;
    i1 = p1;
    i2 = 16u;
    i1 <<= (i2 & 31);
    i0 |= i1;
    i1 = p1;
    i2 = 24u;
    i1 <<= (i2 & 31);
    i0 |= i1;
    l0 = i0;
    L3: 
      i0 = p0;
      i1 = l3;
      i0 = (u32)((s32)i0 <= (s32)i1);
      if (i0) {
        i0 = p0;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = p0;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
        i0 = p0;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 8), i1);
        i0 = p0;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
        i0 = p0;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 16), i1);
        i0 = p0;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 20), i1);
        i0 = p0;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
        i0 = p0;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 28), i1);
        i0 = p0;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 32), i1);
        i0 = p0;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 36), i1);
        i0 = p0;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 40), i1);
        i0 = p0;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 44), i1);
        i0 = p0;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 48), i1);
        i0 = p0;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 52), i1);
        i0 = p0;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 56), i1);
        i0 = p0;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 60), i1);
        i0 = p0;
        i1 = 64u;
        i0 += i1;
        p0 = i0;
        goto L3;
      }
    L5: 
      i0 = p0;
      i1 = l2;
      i0 = (u32)((s32)i0 < (s32)i1);
      if (i0) {
        i0 = p0;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = p0;
        i1 = 4u;
        i0 += i1;
        p0 = i0;
        goto L5;
      }
  }
  L7: 
    i0 = p0;
    i1 = l1;
    i0 = (u32)((s32)i0 < (s32)i1);
    if (i0) {
      i0 = p0;
      i1 = p1;
      i32_store8(Z_envZ_memory, (u64)(i0), i1);
      i0 = p0;
      i1 = 1u;
      i0 += i1;
      p0 = i0;
      goto L7;
    }
  i0 = l1;
  i1 = p2;
  i0 -= i1;
  FUNC_EPILOGUE;
  return i0;
}

static u32 _sbrk(u32 p0) {
  u32 l0 = 0, l1 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  i0 = g3;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  l1 = i0;
  i1 = p0;
  i2 = 15u;
  i1 += i2;
  i2 = 4294967280u;
  i1 &= i2;
  p0 = i1;
  i0 += i1;
  l0 = i0;
  i0 = p0;
  i1 = 0u;
  i0 = (u32)((s32)i0 > (s32)i1);
  i1 = l0;
  i2 = l1;
  i1 = (u32)((s32)i1 < (s32)i2);
  i0 &= i1;
  i1 = l0;
  i2 = 0u;
  i1 = (u32)((s32)i1 < (s32)i2);
  i0 |= i1;
  if (i0) {
    i0 = (*Z_envZ_abortOnCannotGrowMemoryZ_iv)();
    i0 = 12u;
    (*Z_envZ____setErrNoZ_vi)(i0);
    i0 = 4294967295u;
    goto Bfunc;
  }
  i0 = g3;
  i1 = l0;
  i32_store(Z_envZ_memory, (u64)(i0), i1);
  i0 = l0;
  i1 = (*Z_envZ_getTotalMemoryZ_iv)();
  i0 = (u32)((s32)i0 > (s32)i1);
  if (i0) {
    i0 = (*Z_envZ_enlargeMemoryZ_iv)();
    i0 = !(i0);
    if (i0) {
      i0 = g3;
      i1 = l1;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = 12u;
      (*Z_envZ____setErrNoZ_vi)(i0);
      i0 = 4294967295u;
      goto Bfunc;
    }
  }
  i0 = l1;
  Bfunc:;
  FUNC_EPILOGUE;
  return i0;
}

static const u8 data_segment_data_0[] = {
  0xdc, 0x63, 0x7a, 0x21, 0x58, 0x1f, 0x76, 0x5d, 0xd4, 0xdb, 0x72, 0x99, 
  0x50, 0x97, 0x6e, 0xd5, 0xcc, 0x53, 0x6a, 0x11, 0x48, 0x0f, 0x66, 0x4d, 
  0xc4, 0xcb, 0x62, 0x89, 0x40, 0x87, 0x5e, 0xc5, 0xbc, 0x43, 0x5a, 0x01, 
  0x38, 0xff, 0x56, 0x3d, 0xb4, 0xbb, 0x52, 0x79, 0x30, 0x77, 0x4e, 0xb5, 
  0xac, 0x33, 0x4a, 0xf1, 0x28, 0xef, 0x46, 0x2d, 0xa4, 0xab, 0x42, 0x69, 
  0x20, 0x67, 0x3e, 0xa5, 0x9c, 0x23, 0x3a, 0xe1, 0x18, 0xdf, 0x36, 0x1d, 
  0x94, 0x9b, 0x32, 0x59, 0x10, 0x57, 0x2e, 0x95, 0x8c, 0x13, 0x2a, 0xd1, 
  0x08, 0xcf, 0x26, 0x0d, 0x84, 0x8b, 0x22, 0x49, 0x00, 0x47, 0x1e, 0x85, 
  0x7c, 0x03, 0x1a, 0xc1, 0xf8, 0xbf, 0x16, 0xfd, 0x74, 0x7b, 0x12, 0x39, 
  0xf0, 0x37, 0x0e, 0x75, 0x6c, 0xf3, 0x0a, 0xb1, 0xe8, 0xaf, 0x06, 0xed, 
  0x64, 0x6b, 0x02, 0x29, 0xe0, 0x27, 0xfe, 0x65, 0x5c, 0xe3, 0xfa, 0xa1, 
  0xd8, 0x9f, 0xf6, 0xdd, 0x54, 0x5b, 0xf2, 0x19, 0xd0, 0x17, 0xee, 0x55, 
  0x4c, 0xd3, 0xea, 0x91, 0xc8, 0x8f, 0xe6, 0xcd, 0x44, 0x4b, 0xe2, 0x09, 
  0xc0, 0x07, 0xde, 0x45, 0x3c, 0xc3, 0xda, 0x81, 0xb8, 0x7f, 0xd6, 0xbd, 
  0x34, 0x3b, 0xd2, 0xf9, 0xb0, 0xf7, 0xce, 0x35, 0x2c, 0xb3, 0xca, 0x71, 
  0xa8, 0x6f, 0xc6, 0xad, 0x24, 0x2b, 0xc2, 0xe9, 0xa0, 0xe7, 0xbe, 0x25, 
  0x1c, 0xa3, 0xba, 0x61, 0x98, 0x5f, 0xb6, 0x9d, 0x14, 0x1b, 0xb2, 0xd9, 
  0x90, 0xd7, 0xae, 0x15, 0x0c, 0x93, 0xaa, 0x51, 0x88, 0x4f, 0xa6, 0x8d, 
  0x04, 0x0b, 0xa2, 0xc9, 0x80, 0xc7, 0x9e, 0x05, 0xfc, 0x83, 0x9a, 0x41, 
  0x78, 0x3f, 0x96, 0x7d, 0xf4, 0xfb, 0x92, 0xb9, 0x70, 0xb7, 0x8e, 0xf5, 
  0xec, 0x73, 0x8a, 0x31, 0x68, 0x2f, 0x86, 0x6d, 0xe4, 0xeb, 0x82, 0xa9, 
  0x60, 0xa7, 0x7e, 0xe5, 0x7b, 0x20, 0x72, 0x65, 0x74, 0x75, 0x72, 0x6e, 
  0x20, 0x4d, 0x6f, 0x64, 0x75, 0x6c, 0x65, 0x2e, 0x64, 0x28, 0x24, 0x30, 
  0x29, 0x3b, 0x20, 0x7d, 0x00, 0x94, 0x20, 0x85, 0x10, 0xc2, 0xc0, 0x01, 
  0xfb, 0x01, 0xc0, 0xc2, 0x10, 0x85, 0x20, 0x94, 0x01, 0xbb, 0x6b, 0xd9, 
  0xcf, 0x25, 0x71, 0xef, 0x52, 0x52, 0xbd, 0x1b, 0xfc, 0x09, 0x6e, 0x41, 
  0xbe, 0x9b, 0x28, 0xea, 0x83, 0x5c, 0x3f, 0x08, 0x80, 0x7e, 0x13, 0xda, 
  0xfd, 0xe9, 0xd8, 0x84, 0x97, 0x93, 0xb2, 0xac, 0xc6, 0x79, 0xf1, 0x5a, 
  0x70, 0x91, 0xf2, 0xc7, 0x74, 0xb8, 0xa2, 0xf0, 0xa6, 0x2b, 0x39, 0xf2, 
  0x70, 0xc8, 0x87, 0xae, 0x96, 0xc4, 0x0f, 0xbe, 0x85, 0x2e, 0x53, 0xd0, 
  0x8d, 
};

static void init_memory(void) {
  memcpy(&((*Z_envZ_memory).data[1024u]), data_segment_data_0, 361);
}

static void init_table(void) {
  uint32_t offset;
}

/* export: '___errno_location' */
u32 (*WASM_RT_ADD_PREFIX(Z____errno_locationZ_iv))(void);
/* export: '_decryptBlock' */
void (*WASM_RT_ADD_PREFIX(Z__decryptBlockZ_vii))(u32, u32);
/* export: '_free' */
void (*WASM_RT_ADD_PREFIX(Z__freeZ_vi))(u32);
/* export: '_getFlag' */
u32 (*WASM_RT_ADD_PREFIX(Z__getFlagZ_iii))(u32, u32);
/* export: '_malloc' */
u32 (*WASM_RT_ADD_PREFIX(Z__mallocZ_ii))(u32);
/* export: '_memcpy' */
u32 (*WASM_RT_ADD_PREFIX(Z__memcpyZ_iiii))(u32, u32, u32);
/* export: '_memset' */
u32 (*WASM_RT_ADD_PREFIX(Z__memsetZ_iiii))(u32, u32, u32);
/* export: '_sbrk' */
u32 (*WASM_RT_ADD_PREFIX(Z__sbrkZ_ii))(u32);
/* export: '_setDecryptKey' */
void (*WASM_RT_ADD_PREFIX(Z__setDecryptKeyZ_vii))(u32, u32);
/* export: 'establishStackSpace' */
void (*WASM_RT_ADD_PREFIX(Z_establishStackSpaceZ_vii))(u32, u32);
/* export: 'getTempRet0' */
u32 (*WASM_RT_ADD_PREFIX(Z_getTempRet0Z_iv))(void);
/* export: 'runPostSets' */
void (*WASM_RT_ADD_PREFIX(Z_runPostSetsZ_vv))(void);
/* export: 'setTempRet0' */
void (*WASM_RT_ADD_PREFIX(Z_setTempRet0Z_vi))(u32);
/* export: 'setThrew' */
void (*WASM_RT_ADD_PREFIX(Z_setThrewZ_vii))(u32, u32);
/* export: 'stackAlloc' */
u32 (*WASM_RT_ADD_PREFIX(Z_stackAllocZ_ii))(u32);
/* export: 'stackRestore' */
void (*WASM_RT_ADD_PREFIX(Z_stackRestoreZ_vi))(u32);
/* export: 'stackSave' */
u32 (*WASM_RT_ADD_PREFIX(Z_stackSaveZ_iv))(void);

static void init_exports(void) {
  /* export: '___errno_location' */
  WASM_RT_ADD_PREFIX(Z____errno_locationZ_iv) = (&___errno_location);
  /* export: '_decryptBlock' */
  WASM_RT_ADD_PREFIX(Z__decryptBlockZ_vii) = (&_decryptBlock);
  /* export: '_free' */
  WASM_RT_ADD_PREFIX(Z__freeZ_vi) = (&_free);
  /* export: '_getFlag' */
  WASM_RT_ADD_PREFIX(Z__getFlagZ_iii) = (&_getFlag);
  /* export: '_malloc' */
  WASM_RT_ADD_PREFIX(Z__mallocZ_ii) = (&_malloc);
  /* export: '_memcpy' */
  WASM_RT_ADD_PREFIX(Z__memcpyZ_iiii) = (&_memcpy);
  /* export: '_memset' */
  WASM_RT_ADD_PREFIX(Z__memsetZ_iiii) = (&_memset);
  /* export: '_sbrk' */
  WASM_RT_ADD_PREFIX(Z__sbrkZ_ii) = (&_sbrk);
  /* export: '_setDecryptKey' */
  WASM_RT_ADD_PREFIX(Z__setDecryptKeyZ_vii) = (&_setDecryptKey);
  /* export: 'establishStackSpace' */
  WASM_RT_ADD_PREFIX(Z_establishStackSpaceZ_vii) = (&establishStackSpace);
  /* export: 'getTempRet0' */
  WASM_RT_ADD_PREFIX(Z_getTempRet0Z_iv) = (&getTempRet0);
  /* export: 'runPostSets' */
  WASM_RT_ADD_PREFIX(Z_runPostSetsZ_vv) = (&runPostSets);
  /* export: 'setTempRet0' */
  WASM_RT_ADD_PREFIX(Z_setTempRet0Z_vi) = (&setTempRet0);
  /* export: 'setThrew' */
  WASM_RT_ADD_PREFIX(Z_setThrewZ_vii) = (&setThrew);
  /* export: 'stackAlloc' */
  WASM_RT_ADD_PREFIX(Z_stackAllocZ_ii) = (&stackAlloc);
  /* export: 'stackRestore' */
  WASM_RT_ADD_PREFIX(Z_stackRestoreZ_vi) = (&stackRestore);
  /* export: 'stackSave' */
  WASM_RT_ADD_PREFIX(Z_stackSaveZ_iv) = (&stackSave);
}

void WASM_RT_ADD_PREFIX(init)(void) {
  init_func_types();
  init_globals();
  init_memory();
  init_table();
  init_exports();
}
