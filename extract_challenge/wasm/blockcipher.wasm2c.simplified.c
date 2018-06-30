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
  l0 = g4;
  i1 = p0;
  i0 = g4 + p0;
  g4 = i0;
  i1 = 0xfffffff0;
  i0 = (g4 + 0xf) & 0xfffffff0;
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
  g4 = p0;
  FUNC_EPILOGUE;
}

static void establishStackSpace(u32 p0, u32 p1) {
  FUNC_PROLOGUE;
  u32 i0;
  i0 = p1;
  g4 = p0;
  g5 = p1;
  FUNC_EPILOGUE;
}

static void setThrew(u32 p0, u32 p1) {
  FUNC_PROLOGUE;
  u32 i0;
  i0 = g6;
  i0 = !(i0);
  if (i0) {
    i0 = p1;
    g6 = p0;
    g7 = p1;
  }
  FUNC_EPILOGUE;
}

static void setTempRet0(u32 p0) {
  FUNC_PROLOGUE;
  u32 i0;
  i0 = p0;
  g8 = p0;
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
  l6 = g4;
  i1 = 0x40;
  i0 = g4 + 0x40;
  g4 = i0;
  i1 = p1;
  i0 = l6 + 0x20;
  l2 = l6 + 0x20;
  j1 = MEM_as_u64[p1];
  MEM_as_u64[l6 + 0x20] := MEM_as_u64[p1];
  i0 = l2;
  i1 = p1;
  j1 = MEM_as_u64[p1 + 0x8];
  i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
  i0 = l6 + 0x10;
  l1 = l6 + 0x10;
  i2 = 0x10;
  i1 = p1 + 0x10;
  p1 = i1;
  j1 = MEM_as_u64[i1];
  MEM_as_u64[i0] := MEM_as_u64[i1];
  i0 = l1;
  i1 = p1;
  j1 = MEM_as_u64[p1 + 0x8];
  i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
  i0 = p0;
  i1 = l2;
  j1 = MEM_as_u64[l2];
  l30 = MEM_as_u64[l2];
  MEM_as_u64[p0] := MEM_as_u64[l2];
  i0 = p0;
  i1 = l2;
  j1 = MEM_as_u64[l2 + 0x8];
  l31 = MEM_as_u64[l2 + 0x8];
  i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
  i0 = p0;
  i1 = l1;
  j1 = MEM_as_u64[l1];
  l32 = MEM_as_u64[l1];
  i64_store(Z_envZ_memory, (u64)(i0 + 16), j1);
  i0 = p0;
  i1 = l1;
  j1 = MEM_as_u64[l1 + 0x8];
  l33 = MEM_as_u64[l1 + 0x8];
  i64_store(Z_envZ_memory, (u64)(i0 + 24), j1);
  l3 = l6 + 0x30;
  l18 = (l6 + 0x30) + 0x8;
  l12 = (l6 + 0x30) + 0xf;
  l0 = l6;
  l9 = l6 + 0x8;
  l19 = l6 + 0x1;
  l20 = l6 + 0x2;
  l21 = l6 + 0x3;
  l22 = l6 + 0x4;
  l23 = l6 + 0x5;
  l24 = l6 + 0x6;
  l25 = l6 + 0x7;
  l26 = l6 + 0x8;
  l27 = l6 + 0x9;
  l13 = l6 + 0xa;
  l14 = l6 + 0xb;
  l15 = l6 + 0xc;
  l16 = l6 + 0xd;
  l17 = l6 + 0xe;
  i1 = 0xf;
  l10 = l6 + 0xf;
  i0 = 0x1;
  l7 = 0x1;
  L0:
    i0 = l3;
    j1 = 0x0;
    MEM_as_u64[l3] := 0x0;
    i0 = l3;
    j1 = 0x0;
    i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
    i0 = l12;
    i2 = 0xff;
    i1 = l7 & 0xff;
    l2 = l7 & 0xff;
    MEM_as_u8[l12] := l7 & 0xff;
    i0 = 0x0;
    l8 = 0x0;
    L1:
      i0 = 0xe;
      l1 = 0xe;
      L2:
        i2 = l1;
        i0 = l3 + (l1 + 0x1);
        i1 = (s32)MEM_as_s8[l3 + l1];
        p1 = (s32)MEM_as_s8[l3 + l1];
        MEM_as_u8[l3 + (l1 + 0x1)] := (s32)MEM_as_s8[l3 + l1];
        l4 = 0x0;
        i1 = 0x519;
        i0 = (s32)MEM_as_s8[l1 + 0x519];
        l5 = (s32)MEM_as_s8[l1 + 0x519];
        L3:
          i1 = 0x1;
          i0 = l5 & 0x1;
          if (i0) {
            i0 = p1;
          } else {
            i0 = 0x0;
          }
          i1 = l4;
          i0 = i0 ^ l4;
          l4 = i0;
          i1 = 0xff;
          i0 = p1 & 0xff;
          p1 = i0;
          i1 = 0x1;
          i0 <<= (i1 & 31);
          l11 = i0;
          i1 = 0x80;
          i0 = p1 & 0x80;
          if (i0) {
            i0 = 0xc3;
          } else {
            i0 = 0x0;
          }
          i0 = i0 ^ l11;
          i1 = 0xff;
          i0 = i0 & i1;
          p1 = i0 & i1;
          i0 = l5 & 0xff;
          i1 = 0x1;
          i0 >>= (i1 & 31);
          l5 = i0;
          if (i0) {goto L3;}
        i1 = l2;
        i0 = l4 ^ l2;
        l2 = i0;
        i0 = l1;
        p1 = l1 + 0xffffffff;
        i1 = 0x0;
        i0 = (u32)((s32)i0 > (s32)i1);
        if (i0) {
          i0 = p1;
          l1 = p1;
          goto L2;
        }
      i0 = l3;
      i1 = l2;
      MEM_as_u8[l3] := l2;
      i1 = 0x1;
      i0 = l8 + 0x1;
      l8 = i0;
      i1 = 0x10;
      i0 = i0 != i1;
      if (i0) {
        i0 = (s32)MEM_as_s8[l12];
        l2 = (s32)MEM_as_s8[l12];
        goto L1;
      }
    i0 = l9;
    j0 = MEM_as_u64[l3] ^ l30;
    l28 = MEM_as_u64[l3] ^ l30;
    i1 = l18;
    j2 = l31;
    j1 = MEM_as_u64[l18] ^ l31;
    l29 = MEM_as_u64[l18] ^ l31;
    MEM_as_u64[l9] := MEM_as_u64[l18] ^ l31;
    i0 = l0;
    i1 = 0x0;
    j2 = l28;
    i2 = (u32)(j2);
    i2 = i2 & 0xff;
    i3 = 0x400;
    i2 = i2 + i3;
    i2 = (u32)MEM_as_u8[i2];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l19;
    i1 = 0x0;
    j2 = l28;
    j3 = 0x8;
    j2 >>= (j3 & 63);
    i2 = (u32)(j2);
    i2 = i2 & 0xff;
    i3 = 0x400;
    i2 = i2 + i3;
    i2 = (u32)MEM_as_u8[i2];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l20;
    i1 = 0x0;
    j2 = l28;
    j3 = 0x10;
    j2 >>= (j3 & 63);
    i2 = (u32)(j2);
    i2 = i2 & 0xff;
    i3 = 0x400;
    i2 = i2 + i3;
    i2 = (u32)MEM_as_u8[i2];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l21;
    i1 = 0x0;
    j2 = l28;
    j3 = 0x18;
    j2 >>= (j3 & 63);
    i2 = (u32)(j2);
    i2 = i2 & 0xff;
    i3 = 0x400;
    i2 = i2 + i3;
    i2 = (u32)MEM_as_u8[i2];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l22;
    i1 = 0x0;
    j2 = l28;
    j3 = 0x20;
    j2 >>= (j3 & 63);
    i2 = (u32)(j2);
    i2 = i2 & 0xff;
    i3 = 0x400;
    i2 = i2 + i3;
    i2 = (u32)MEM_as_u8[i2];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l23;
    i1 = 0x0;
    j2 = l28;
    j3 = 0x28;
    j2 >>= (j3 & 63);
    i2 = (u32)(j2);
    i2 = i2 & 0xff;
    i3 = 0x400;
    i2 = i2 + i3;
    i2 = (u32)MEM_as_u8[i2];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l24;
    i1 = 0x0;
    j2 = l28;
    j3 = 0x30;
    j2 >>= (j3 & 63);
    i2 = (u32)(j2);
    i2 = i2 & 0xff;
    i3 = 0x400;
    i2 = i2 + i3;
    i2 = (u32)MEM_as_u8[i2];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l25;
    i1 = 0x0;
    j2 = l28;
    j3 = 0x38;
    j2 >>= (j3 & 63);
    i2 = (u32)(j2);
    i3 = 0x400;
    i2 = i2 + 0x400;
    i2 = (u32)MEM_as_u8[i2];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l26;
    i1 = 0x0;
    j2 = l29;
    i2 = (u32)(j2);
    i2 = i2 & 0xff;
    i3 = 0x400;
    i2 = i2 + i3;
    i2 = (u32)MEM_as_u8[i2];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l27;
    i1 = 0x0;
    j2 = l29;
    j3 = 0x8;
    j2 >>= (j3 & 63);
    i2 = (u32)(j2);
    i2 = i2 & 0xff;
    i3 = 0x400;
    i2 = i2 + i3;
    i2 = (u32)MEM_as_u8[i2];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l13;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l13] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l14;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l14] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l15;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l15] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l16;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l16] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l17;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l17] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l10;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l10] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    i2 = 0xff;
    i1 = i1 & 0xff;
    l2 = i1 & 0xff;
    MEM_as_u8[i0] := i1 & 0xff;
    i0 = 0x0;
    l8 = 0x0;
    L8:
      i0 = 0xe;
      l1 = 0xe;
      L9:
        i2 = l1;
        i0 = l0 + (l1 + 0x1);
        i1 = (s32)MEM_as_s8[l0 + l1];
        p1 = (s32)MEM_as_s8[l0 + l1];
        MEM_as_u8[l0 + (l1 + 0x1)] := (s32)MEM_as_s8[l0 + l1];
        l4 = 0x0;
        i1 = 0x519;
        i0 = (s32)MEM_as_s8[l1 + 0x519];
        l5 = (s32)MEM_as_s8[l1 + 0x519];
        L10:
          i1 = 0x1;
          i0 = l5 & 0x1;
          if (i0) {
            i0 = p1;
          } else {
            i0 = 0x0;
          }
          i1 = l4;
          i0 = i0 ^ l4;
          l4 = i0;
          i1 = 0xff;
          i0 = p1 & 0xff;
          p1 = i0;
          i1 = 0x1;
          i0 <<= (i1 & 31);
          l11 = i0;
          i1 = 0x80;
          i0 = p1 & 0x80;
          if (i0) {
            i0 = 0xc3;
          } else {
            i0 = 0x0;
          }
          i0 = i0 ^ l11;
          i1 = 0xff;
          i0 = i0 & i1;
          p1 = i0 & i1;
          i0 = l5 & 0xff;
          i1 = 0x1;
          i0 >>= (i1 & 31);
          l5 = i0;
          if (i0) {goto L10;}
        i1 = l2;
        i0 = l4 ^ l2;
        l2 = i0;
        i0 = l1;
        p1 = l1 + 0xffffffff;
        i1 = 0x0;
        i0 = (u32)((s32)i0 > (s32)i1);
        if (i0) {
          i0 = p1;
          l1 = p1;
          goto L9;
        }
      i0 = l0;
      i1 = l2;
      MEM_as_u8[l0] := l2;
      i1 = 0x1;
      i0 = l8 + 0x1;
      l8 = i0;
      i1 = 0x10;
      i0 = i0 != i1;
      if (i0) {
        i0 = (s32)MEM_as_s8[l10];
        l2 = (s32)MEM_as_s8[l10];
        goto L8;
      }
    i0 = l0;
    i1 = l0;
    j2 = l32;
    j1 = MEM_as_u64[l0] ^ l32;
    l28 = MEM_as_u64[l0] ^ l32;
    MEM_as_u64[l0] := MEM_as_u64[l0] ^ l32;
    i0 = l9;
    i1 = l9;
    j2 = l33;
    j1 = MEM_as_u64[l9] ^ l33;
    l29 = MEM_as_u64[l9] ^ l33;
    MEM_as_u64[l9] := MEM_as_u64[l9] ^ l33;
    i1 = 0x7;
    i0 = l7 & 0x7;
    i0 = !(i0);
    if (i0) {
      i0 = p0;
      i1 = l7;
      i2 = 0x2;
      i1 = (u32)((s32)i1 >> (i2 & 31));
      p1 = i1;
      i2 = 0x4;
      i1 <<= (i2 & 31);
      i0 = i0 + i1;
      j1 = l28;
      MEM_as_u64[i0 + i1] := l28;
      i0 = p0;
      i1 = p1;
      i2 = 0x4;
      i1 <<= (i2 & 31);
      i0 = i0 + i1;
      j1 = l29;
      i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
      i0 = p0;
      i2 = 0x1;
      i1 = p1 + 0x1;
      p1 = i1;
      i2 = 0x4;
      i1 <<= (i2 & 31);
      i0 = i0 + i1;
      j1 = l30;
      MEM_as_u64[i0 + i1] := l30;
      i0 = p0;
      i1 = p1;
      i2 = 0x4;
      i1 <<= (i2 & 31);
      i0 = i0 + i1;
      j1 = l31;
      i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
    }
    i1 = 0x1;
    i0 = l7 + 0x1;
    l7 = i0;
    i1 = 0x21;
    i0 = i0 != i1;
    if (i0) {
      j0 = l28;
      l33 = l31;
      l32 = l30;
      l30 = j0;
      j0 = l29;
      l31 = l29;
      goto L0;
    }
  i0 = l6;
  g4 = l6;
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
  l23 = g4;
  i1 = 0x10;
  i0 = g4 + 0x10;
  g4 = i0;
  i0 = l23;
  l0 = l23;
  i1 = p0;
  i2 = p1;
  j2 = MEM_as_u64[p1];
  j1 = (MEM_as_u64[p0 + 0x90]) ^ MEM_as_u64[p1];
  MEM_as_u64[l23] := (MEM_as_u64[p0 + 0x90]) ^ MEM_as_u64[p1];
  i1 = p0;
  i0 = l0 + 0x8;
  l4 = l0 + 0x8;
  i3 = 0x8;
  i2 = p1 + 0x8;
  l26 = p1 + 0x8;
  j2 = MEM_as_u64[p1 + 0x8];
  j1 = (MEM_as_u64[p0 + 0x98]) ^ (MEM_as_u64[p1 + 0x8]);
  MEM_as_u64[l0 + 0x8] := (MEM_as_u64[p0 + 0x98]) ^ (MEM_as_u64[p1 + 0x8]);
  l3 = l0 + 0xf;
  l5 = l0 + 0x1;
  l6 = l0 + 0x2;
  l7 = l0 + 0x3;
  l8 = l0 + 0x4;
  l9 = l0 + 0x5;
  l10 = l0 + 0x6;
  l11 = l0 + 0x7;
  l12 = l0 + 0x8;
  l13 = l0 + 0x9;
  l14 = l0 + 0xa;
  l15 = l0 + 0xb;
  l16 = l0 + 0xc;
  l17 = l0 + 0xd;
  i1 = 0xe;
  l18 = l0 + 0xe;
  i0 = 0x8;
  l2 = 0x8;
  L0:
    i0 = 0x0;
    l24 = 0x0;
    L1:
      i0 = (s32)MEM_as_s8[l0];
      l1 = 0x0;
      l19 = (s32)MEM_as_s8[l0];
      L2:
        i0 = l0 + l1;
        i3 = 0x1;
        i2 = l1 + 0x1;
        l25 = l1 + 0x1;
        i1 = (s32)MEM_as_s8[l0 + (l1 + 0x1)];
        l20 = (s32)MEM_as_s8[l0 + (l1 + 0x1)];
        MEM_as_u8[l0 + l1] := (s32)MEM_as_s8[l0 + (l1 + 0x1)];
        l21 = 0x0;
        i1 = 0x519;
        i0 = l20;
        l22 = (s32)MEM_as_s8[l1 + 0x519];
        l1 = i0;
        L3:
          i1 = 0x1;
          i0 = l22 & 0x1;
          if (i0) {
            i0 = l1;
          } else {
            i0 = 0x0;
          }
          i1 = l21;
          i0 = i0 ^ l21;
          l21 = i0;
          i1 = 0xff;
          i0 = l1 & 0xff;
          l1 = i0;
          i1 = 0x1;
          i0 <<= (i1 & 31);
          l20 = i0;
          i1 = 0x80;
          i0 = l1 & 0x80;
          if (i0) {
            i0 = 0xc3;
          } else {
            i0 = 0x0;
          }
          i0 = i0 ^ l20;
          i1 = 0xff;
          i0 = i0 & i1;
          l1 = i0 & i1;
          i0 = l22 & 0xff;
          i1 = 0x1;
          i0 >>= (i1 & 31);
          l22 = i0;
          if (i0) {goto L3;}
        i1 = l19;
        i0 = l21 ^ l19;
        l19 = i0;
        i0 = l25;
        i1 = 0xf;
        i0 = i0 != i1;
        if (i0) {
          i0 = l25;
          l1 = l25;
          goto L2;
        }
      i0 = l3;
      i1 = l19;
      MEM_as_u8[l3] := l19;
      i1 = 0x1;
      i0 = l24 + 0x1;
      l24 = i0;
      i1 = 0x10;
      i0 = i0 != i1;
      if (i0) {goto L1;}
    i0 = l0;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l0] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l5;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l5] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l6;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l6] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l7;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l7] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l8;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l8] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l9;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l9] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l10;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l10] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l11;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l11] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l12;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l12] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l13;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l13] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l14;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l14] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l15;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l15] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l16;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l16] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l17;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l17] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l18;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l18] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l3;
    i1 = 0x0;
    i3 = 0x400;
    i2 = (u32)MEM_as_u8[(u32)MEM_as_u8[l3] + 0x400];
    i1 = (*Z_envZ__emscripten_asm_const_iiZ_iii)(i1, i2);
    MEM_as_u8[i0] := i1;
    i0 = l0;
    i1 = l0;
    j1 = MEM_as_u64[l0];
    i2 = p0;
    i3 = l2;
    i4 = 0x4;
    i3 <<= (i4 & 31);
    i2 = i2 + i3;
    j2 = MEM_as_u64[i2 + i3];
    j1 = j1 ^ (MEM_as_u64[i2 + i3]);
    l27 = j1 ^ (MEM_as_u64[i2 + i3]);
    MEM_as_u64[i0] := j1 ^ (MEM_as_u64[i2 + i3]);
    i0 = l4;
    i1 = l4;
    j1 = MEM_as_u64[l4];
    i2 = p0;
    i3 = l2;
    i4 = 0x4;
    i3 <<= (i4 & 31);
    i2 = i2 + i3;
    j2 = MEM_as_u64[(i2 + i3) + 0x8];
    j1 = j1 ^ (MEM_as_u64[(i2 + i3) + 0x8]);
    l28 = j1 ^ (MEM_as_u64[(i2 + i3) + 0x8]);
    MEM_as_u64[i0] := j1 ^ (MEM_as_u64[(i2 + i3) + 0x8]);
    i0 = l2;
    l1 = l2 + 0xffffffff;
    i1 = 0x0;
    i0 = (u32)((s32)i0 > (s32)i1);
    if (i0) {
      i0 = l1;
      l2 = l1;
      goto L0;
    }
  i0 = l0;
  j1 = l27;
  i1 = (u32)(j1);
  i1 = i1 & 0xff;
  i2 = 0x400;
  i1 = i1 + i2;
  i1 = (s32)MEM_as_s8[i1];
  MEM_as_u8[i0] := (s32)MEM_as_s8[i1];
  i0 = l5;
  j1 = l27;
  j2 = 0x8;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i1 = i1 & 0xff;
  i2 = 0x400;
  i1 = i1 + i2;
  i1 = (s32)MEM_as_s8[i1];
  MEM_as_u8[i0] := (s32)MEM_as_s8[i1];
  i0 = l6;
  j1 = l27;
  j2 = 0x10;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i1 = i1 & 0xff;
  i2 = 0x400;
  i1 = i1 + i2;
  i1 = (s32)MEM_as_s8[i1];
  MEM_as_u8[i0] := (s32)MEM_as_s8[i1];
  i0 = l7;
  j1 = l27;
  j2 = 0x18;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i1 = i1 & 0xff;
  i2 = 0x400;
  i1 = i1 + i2;
  i1 = (s32)MEM_as_s8[i1];
  MEM_as_u8[i0] := (s32)MEM_as_s8[i1];
  i0 = l8;
  j1 = l27;
  j2 = 0x20;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i1 = i1 & 0xff;
  i2 = 0x400;
  i1 = i1 + i2;
  i1 = (s32)MEM_as_s8[i1];
  MEM_as_u8[i0] := (s32)MEM_as_s8[i1];
  i0 = l9;
  j1 = l27;
  j2 = 0x28;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i1 = i1 & 0xff;
  i2 = 0x400;
  i1 = i1 + i2;
  i1 = (s32)MEM_as_s8[i1];
  MEM_as_u8[i0] := (s32)MEM_as_s8[i1];
  i0 = l10;
  j1 = l27;
  j2 = 0x30;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i1 = i1 & 0xff;
  i2 = 0x400;
  i1 = i1 + i2;
  i1 = (s32)MEM_as_s8[i1];
  MEM_as_u8[i0] := (s32)MEM_as_s8[i1];
  i0 = l11;
  j1 = l27;
  j2 = 0x38;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i2 = 0x400;
  i1 = i1 + 0x400;
  i1 = (s32)MEM_as_s8[i1];
  MEM_as_u8[i0] := (s32)MEM_as_s8[i1];
  i0 = l12;
  j1 = l28;
  i1 = (u32)(j1);
  i1 = i1 & 0xff;
  i2 = 0x400;
  i1 = i1 + i2;
  i1 = (s32)MEM_as_s8[i1];
  MEM_as_u8[i0] := (s32)MEM_as_s8[i1];
  i0 = l13;
  j1 = l28;
  j2 = 0x8;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i1 = i1 & 0xff;
  i2 = 0x400;
  i1 = i1 + i2;
  i1 = (s32)MEM_as_s8[i1];
  MEM_as_u8[i0] := (s32)MEM_as_s8[i1];
  i0 = l14;
  j1 = l28;
  j2 = 0x10;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i1 = i1 & 0xff;
  i2 = 0x400;
  i1 = i1 + i2;
  i1 = (s32)MEM_as_s8[i1];
  MEM_as_u8[i0] := (s32)MEM_as_s8[i1];
  i0 = l15;
  j1 = l28;
  j2 = 0x18;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i1 = i1 & 0xff;
  i2 = 0x400;
  i1 = i1 + i2;
  i1 = (s32)MEM_as_s8[i1];
  MEM_as_u8[i0] := (s32)MEM_as_s8[i1];
  i0 = l16;
  j1 = l28;
  j2 = 0x20;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i1 = i1 & 0xff;
  i2 = 0x400;
  i1 = i1 + i2;
  i1 = (s32)MEM_as_s8[i1];
  MEM_as_u8[i0] := (s32)MEM_as_s8[i1];
  i0 = l17;
  j1 = l28;
  j2 = 0x28;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i1 = i1 & 0xff;
  i2 = 0x400;
  i1 = i1 + i2;
  i1 = (s32)MEM_as_s8[i1];
  MEM_as_u8[i0] := (s32)MEM_as_s8[i1];
  i0 = l18;
  j1 = l28;
  j2 = 0x30;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i1 = i1 & 0xff;
  i2 = 0x400;
  i1 = i1 + i2;
  i1 = (s32)MEM_as_s8[i1];
  MEM_as_u8[i0] := (s32)MEM_as_s8[i1];
  i0 = l3;
  j1 = l28;
  j2 = 0x38;
  j1 >>= (j2 & 63);
  i1 = (u32)(j1);
  i2 = 0x400;
  i1 = i1 + 0x400;
  i1 = (s32)MEM_as_s8[i1];
  MEM_as_u8[i0] := (s32)MEM_as_s8[i1];
  i0 = p1;
  i1 = l0;
  j1 = MEM_as_u64[l0];
  MEM_as_u64[p1] := MEM_as_u64[l0];
  i0 = l26;
  i1 = l4;
  j1 = MEM_as_u64[l4];
  MEM_as_u64[l26] := MEM_as_u64[l4];
  i0 = l23;
  g4 = l23;
  FUNC_EPILOGUE;
}

static u32 f16(u32 p0, u32 p1, u32 p2, u32 p3) {
  u32 l0 = 0, l1 = 0, l2 = 0, l3 = 0, l4 = 0, l5 = 0;
  u64 l6 = 0, l7 = 0, l8 = 0, l9 = 0, l10 = 0, l11 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  u64 j0, j1, j2, j3;
  i1 = 0xf;
  i0 = p3 & 0xf;
  if (i0) {
    i0 = 0xffffffff;
    goto Bfunc;
  }
  i0 = p3;
  i1 = 0x10;
  i0 = I32_DIV_S(i0, i1);
  l3 = i0;
  i0 = p3;
  i1 = 0xf;
  i0 = (u32)((s32)i0 <= (s32)i1);
  if (i0) {
    i0 = 0x0;
    goto Bfunc;
  }
  i0 = 0x0;
  p3 = 0x0;
  L2:
    i0 = p0;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l4 = i0;
    i0 = p1;
    l0 = 0x20;
    i1 = p3;
    i2 = 0x4;
    i1 <<= (i2 & 31);
    l5 = i1;
    i0 = i0 + i1;
    l1 = i0 + i1;
    i1 = 0x8;
    i0 = i0 + 0x8;
    l2 = i0 + 0x8;
    i0 = i32_load8_u(Z_envZ_memory, (u64)(i0 + 6));
    j0 = (u64)(i0);
    j1 = 0x30;
    j0 <<= (j1 & 63);
    i1 = l2;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 7));
    j1 = (u64)(i1);
    j2 = 0x38;
    j1 <<= (j2 & 63);
    j0 = j0 | j1;
    i1 = l2;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 5));
    j1 = (u64)(i1);
    j2 = 0x28;
    j1 <<= (j2 & 63);
    j0 = j0 | j1;
    i1 = l2;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 4));
    j1 = (u64)(i1);
    j2 = 0x20;
    j1 <<= (j2 & 63);
    j0 = j0 | j1;
    i1 = l2;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 3));
    j1 = (u64)(i1);
    j2 = 0x18;
    j1 <<= (j2 & 63);
    j0 = j0 | j1;
    i1 = l2;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 2));
    j1 = (u64)(i1);
    j2 = 0x10;
    j1 <<= (j2 & 63);
    j0 = j0 | j1;
    i1 = l2;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 1));
    j1 = (u64)(i1);
    j2 = 0x8;
    j1 <<= (j2 & 63);
    j0 = j0 | j1;
    i1 = (u32)MEM_as_u8[l2];
    j1 = (u64)(i1);
    j0 = j0 | j1;
    l8 = j0 | j1;
    i0 = l1;
    i0 = i32_load8_u(Z_envZ_memory, (u64)(i0 + 6));
    j0 = (u64)(i0);
    j1 = 0x30;
    j0 <<= (j1 & 63);
    i1 = l1;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 7));
    j1 = (u64)(i1);
    j2 = 0x38;
    j1 <<= (j2 & 63);
    j0 = j0 | j1;
    i1 = l1;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 5));
    j1 = (u64)(i1);
    j2 = 0x28;
    j1 <<= (j2 & 63);
    j0 = j0 | j1;
    i1 = l1;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 4));
    j1 = (u64)(i1);
    j2 = 0x20;
    j1 <<= (j2 & 63);
    j0 = j0 | j1;
    i1 = l1;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 3));
    j1 = (u64)(i1);
    j2 = 0x18;
    j1 <<= (j2 & 63);
    j0 = j0 | j1;
    i1 = l1;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 2));
    j1 = (u64)(i1);
    j2 = 0x10;
    j1 <<= (j2 & 63);
    j0 = j0 | j1;
    i1 = l1;
    i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 1));
    j1 = (u64)(i1);
    j2 = 0x8;
    j1 <<= (j2 & 63);
    j0 = j0 | j1;
    i1 = (u32)MEM_as_u8[l1];
    j1 = (u64)(i1);
    j0 = j0 | j1;
    l9 = j0 | j1;
    L3:
      i0 = l4;
      i1 = l0 + 0xffffffff;
      l1 = l0 + 0xffffffff;
      i2 = 0x3;
      i1 <<= (i2 & 31);
      i0 = i0 + i1;
      j0 = (MEM_as_u64[i0 + i1]) ^ l8;
      j1 = l8 ^ l9;
      l7 = l8 ^ l9;
      j2 = 0x3;
      j1 >>= (j2 & 63);
      l11 = j1;
      j2 = l7;
      j3 = 0x3d;
      j2 <<= (j3 & 63);
      j1 = j1 | j2;
      l9 = j1 | j2;
      j0 = j0 - (j1 | j2);
      l6 = j0 - (j1 | j2);
      j1 = 0x38;
      j0 >>= (j1 & 63);
      l10 = j0;
      j0 = l6;
      j1 = 0x8;
      j0 <<= (j1 & 63);
      j1 = l10;
      j0 = j0 | l10;
      l8 = j0 | l10;
      i0 = l0;
      i1 = 0x1;
      i0 = (u32)((s32)i0 > (s32)i1);
      if (i0) {
        i0 = l1;
        l0 = l1;
        goto L3;
      }
    i1 = l5;
    i0 = p2 + l5;
    l0 = p2 + l5;
    j1 = l11;
    i64_store8(Z_envZ_memory, (u64)(i0), j1);
    i0 = l0;
    j1 = l7;
    j2 = 0xb;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 1), j1);
    i0 = l0;
    j1 = l7;
    j2 = 0x13;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 2), j1);
    i0 = l0;
    j1 = l7;
    j2 = 0x1b;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 3), j1);
    i0 = l0;
    j1 = l7;
    j2 = 0x23;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 4), j1);
    i0 = l0;
    j1 = l7;
    j2 = 0x2b;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 5), j1);
    i0 = l0;
    j1 = l7;
    j2 = 0x33;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 6), j1);
    i0 = l0;
    j1 = l9;
    j2 = 0x38;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 7), j1);
    i1 = 0x8;
    i0 = l0 + 0x8;
    l0 = i0;
    j1 = l10;
    i64_store8(Z_envZ_memory, (u64)(i0), j1);
    i0 = l0;
    j1 = l6;
    i64_store8(Z_envZ_memory, (u64)(i0 + 1), j1);
    i0 = l0;
    j1 = l6;
    j2 = 0x8;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 2), j1);
    i0 = l0;
    j1 = l6;
    j2 = 0x10;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 3), j1);
    i0 = l0;
    j1 = l6;
    j2 = 0x18;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 4), j1);
    i0 = l0;
    j1 = l6;
    j2 = 0x20;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 5), j1);
    i0 = l0;
    j1 = l6;
    j2 = 0x28;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 6), j1);
    i0 = l0;
    j1 = l6;
    j2 = 0x30;
    j1 >>= (j2 & 63);
    i64_store8(Z_envZ_memory, (u64)(i0 + 7), j1);
    i1 = 0x1;
    i0 = p3 + 0x1;
    p3 = i0;
    i1 = l3;
    i0 = i0 != i1;
    if (i0) {goto L2;}
    i0 = 0x0;
    p0 = 0x0;
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
    i0 = 0x0;
    goto Bfunc;
  }
  i0 = 0x1;
  i1 = 0x4;
  i0 = f21(i0, i1);
  l2 = i0;
  i0 = !(i0);
  if (i0) {
    i0 = 0x0;
    goto Bfunc;
  }
  i0 = l2;
  i1 = 0x100;
  i1 = _malloc(i1);
  l0 = i1;
  i32_store(Z_envZ_memory, (u64)(i0), i1);
  i0 = l0;
  i0 = !(i0);
  if (i0) {
    i0 = 0x0;
    goto Bfunc;
  }
  i0 = p0;
  i0 = i32_load8_u(Z_envZ_memory, (u64)(i0 + 14));
  j0 = (u64)(i0);
  j1 = 0x30;
  j0 <<= (j1 & 63);
  i1 = p0;
  i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 15));
  j1 = (u64)(i1);
  j2 = 0x38;
  j1 <<= (j2 & 63);
  j0 = j0 | j1;
  i1 = p0;
  i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 13));
  j1 = (u64)(i1);
  j2 = 0x28;
  j1 <<= (j2 & 63);
  j0 = j0 | j1;
  i1 = p0;
  i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 12));
  j1 = (u64)(i1);
  j2 = 0x20;
  j1 <<= (j2 & 63);
  j0 = j0 | j1;
  i1 = p0;
  i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 11));
  j1 = (u64)(i1);
  j2 = 0x18;
  j1 <<= (j2 & 63);
  j0 = j0 | j1;
  i1 = p0;
  i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 10));
  j1 = (u64)(i1);
  j2 = 0x10;
  j1 <<= (j2 & 63);
  j0 = j0 | j1;
  i1 = p0;
  i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 9));
  j1 = (u64)(i1);
  j2 = 0x8;
  j1 <<= (j2 & 63);
  j0 = j0 | j1;
  i1 = p0;
  i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 8));
  j1 = (u64)(i1);
  j0 = j0 | j1;
  l4 = j0 | j1;
  i0 = l0;
  i1 = p0;
  i1 = i32_load8_u(Z_envZ_memory, (u64)(i1 + 6));
  j1 = (u64)(i1);
  j2 = 0x30;
  j1 <<= (j2 & 63);
  i2 = p0;
  i2 = i32_load8_u(Z_envZ_memory, (u64)(i2 + 7));
  j2 = (u64)(i2);
  j3 = 0x38;
  j2 <<= (j3 & 63);
  j1 = j1 | j2;
  i2 = p0;
  i2 = i32_load8_u(Z_envZ_memory, (u64)(i2 + 5));
  j2 = (u64)(i2);
  j3 = 0x28;
  j2 <<= (j3 & 63);
  j1 = j1 | j2;
  i2 = p0;
  i2 = i32_load8_u(Z_envZ_memory, (u64)(i2 + 4));
  j2 = (u64)(i2);
  j3 = 0x20;
  j2 <<= (j3 & 63);
  j1 = j1 | j2;
  i2 = p0;
  i2 = i32_load8_u(Z_envZ_memory, (u64)(i2 + 3));
  j2 = (u64)(i2);
  j3 = 0x18;
  j2 <<= (j3 & 63);
  j1 = j1 | j2;
  i2 = p0;
  i2 = i32_load8_u(Z_envZ_memory, (u64)(i2 + 2));
  j2 = (u64)(i2);
  j3 = 0x10;
  j2 <<= (j3 & 63);
  j1 = j1 | j2;
  i2 = p0;
  i2 = i32_load8_u(Z_envZ_memory, (u64)(i2 + 1));
  j2 = (u64)(i2);
  j3 = 0x8;
  j2 <<= (j3 & 63);
  j1 = j1 | j2;
  i2 = (u32)MEM_as_u8[p0];
  j2 = (u64)(i2);
  j1 = j1 | j2;
  l3 = j1 | j2;
  MEM_as_u64[i0] := j1 | j2;
  L3:
    i0 = l0;
    i1 = l1 + 0x1;
    p0 = l1 + 0x1;
    i2 = 0x3;
    i1 <<= (i2 & 31);
    i0 = i0 + i1;
    j1 = l4;
    j2 = 0x8;
    j1 >>= (j2 & 63);
    j2 = l4;
    j3 = 0x38;
    j2 <<= (j3 & 63);
    j1 = j1 | j2;
    j2 = l3;
    j1 = j1 + l3;
    i2 = l1;
    j2 = (u64)(s64)(s32)(i2);
    j1 = j1 ^ j2;
    l4 = j1 ^ j2;
    j2 = l3;
    j3 = 0x3;
    j2 <<= (j3 & 63);
    j3 = l3;
    j4 = 0x3d;
    j3 >>= (j4 & 63);
    j2 = j2 | j3;
    j1 = j1 ^ (j2 | j3);
    l3 = j1 ^ (j2 | j3);
    MEM_as_u64[i0] := j1 ^ (j2 | j3);
    i0 = p0;
    i1 = 0x1f;
    i0 = i0 != i1;
    if (i0) {
      i0 = p0;
      l1 = p0;
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
  l1 = g4;
  i1 = 0x70;
  i0 = g4 + 0x70;
  g4 = i0;
  i0 = l1 + 0x40;
  l2 = l1 + 0x40;
  i1 = 0x529;
  j1 = MEM_as_u64[0x529];
  MEM_as_u64[l1 + 0x40] := MEM_as_u64[0x529];
  i0 = l2;
  i1 = 0x531;
  j1 = MEM_as_u64[0x531];
  i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
  i0 = l2;
  i1 = 0x539;
  j1 = MEM_as_u64[0x539];
  i64_store(Z_envZ_memory, (u64)(i0 + 16), j1);
  i0 = l2;
  i1 = 0x541;
  j1 = MEM_as_u64[0x541];
  i64_store(Z_envZ_memory, (u64)(i0 + 24), j1);
  i0 = l2;
  i1 = 0x549;
  j1 = MEM_as_u64[0x549];
  i64_store(Z_envZ_memory, (u64)(i0 + 32), j1);
  i0 = l2;
  i1 = 0x551;
  j1 = MEM_as_u64[0x551];
  i64_store(Z_envZ_memory, (u64)(i0 + 40), j1);
  i0 = l1;
  l0 = l1;
  i1 = 0x559;
  j1 = MEM_as_u64[0x559];
  MEM_as_u64[l1] := MEM_as_u64[0x559];
  i0 = l0;
  i1 = 0x561;
  j1 = MEM_as_u64[0x561];
  i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
  i0 = p0;
  i1 = 0x5571c18;
  i0 = i0 != i1;
  if (i0) {
    g4 = l1;
    i0 = 0x0;
    goto Bfunc;
  }
  i0 = l0;
  i0 = f17(i0);
  p0 = i0;
  i0 = !(i0);
  if (i0) {
    g4 = l1;
    i0 = 0x0;
    goto Bfunc;
  }
  i0 = p0;
  i1 = l2;
  i2 = l1 + 0x10;
  l0 = l1 + 0x10;
  i3 = 0x30;
  i0 = f16(i0, i1, i2, i3);
  i0 = p1;
  i1 = l0;
  j1 = MEM_as_u64[l0];
  MEM_as_u64[p1] := MEM_as_u64[l0];
  i0 = p1;
  i1 = l0;
  j1 = MEM_as_u64[l0 + 0x8];
  i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
  i0 = p1;
  i1 = l0;
  j1 = MEM_as_u64[l0 + 0x10];
  i64_store(Z_envZ_memory, (u64)(i0 + 16), j1);
  i0 = p1;
  i1 = l0;
  j1 = MEM_as_u64[l0 + 0x18];
  i64_store(Z_envZ_memory, (u64)(i0 + 24), j1);
  i0 = p1;
  i1 = l0;
  j1 = MEM_as_u64[l0 + 0x20];
  i64_store(Z_envZ_memory, (u64)(i0 + 32), j1);
  i0 = p1;
  i1 = l0;
  i1 = i32_load(Z_envZ_memory, (u64)(i1 + 40));
  i32_store(Z_envZ_memory, (u64)(i0 + 40), i1);
  i0 = p0;
  _free(i0);
  g4 = l1;
  i0 = 0x1;
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
  l9 = g4;
  i1 = 0x10;
  i0 = g4 + 0x10;
  g4 = i0;
  i0 = p0;
  l7 = l9;
  i1 = 0xf5;
  i0 = i0 < i1;
  if (i0) {
    i1 = 0xfffffff8;
    l2 = (p0 + 0xb) & 0xfffffff8;
    i0 = 0x56c;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l5 = i0;
    i1 = p0;
    i2 = 0xb;
    i1 = i1 < i2;
    if (i1) {
      i1 = 0x10;
      l2 = 0x10;
    } else {
      i1 = l2;
    }
    i2 = 0x3;
    i1 >>= (i2 & 31);
    p0 = i1;
    i0 >>= (i1 & 31);
    l0 = i0;
    i1 = 0x3;
    i0 = i0 & i1;
    if (i0) {
      i1 = p0;
      i0 = ((l0 & 0x1) ^ 0x1) + p0;
      p0 = i0;
      i1 = 0x3;
      i0 <<= (i1 & 31);
      i0 = i0 + 0x594;
      l0 = i0 + 0x594;
      i1 = 0x8;
      i0 = i0 + i1;
      l4 = i0 + i1;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l2 = i0;
      i1 = 0x8;
      i0 = i0 + i1;
      l3 = i0 + i1;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l1 = i0;
      i1 = l0;
      i0 = i0 == i1;
      if (i0) {
        i0 = 0x56c;
        i1 = l5;
        i2 = 0x1;
        i3 = p0;
        i2 <<= (i3 & 31);
        i3 = 0xffffffff;
        i2 = i2 ^ 0xffffffff;
        i1 = i1 & (i2 ^ 0xffffffff);
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
      i2 = 0x3;
      i1 <<= (i2 & 31);
      p0 = i1;
      i2 = 0x3;
      i1 = i1 | i2;
      i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
      i1 = 0x4;
      i0 = (l2 + p0) + 0x4;
      p0 = i0;
      i1 = i0;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      i2 = 0x1;
      i1 = i1 | 0x1;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l3;
      g4 = l9;
      goto Bfunc;
    }
    i0 = l2;
    i1 = 0x574;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    l8 = i1;
    i0 = i0 > i1;
    if (i0) {
      i0 = l0;
      if (i0) {
        i0 = l0;
        i1 = p0;
        i0 <<= (i1 & 31);
        i1 = 0x2;
        i2 = p0;
        i1 <<= (i2 & 31);
        p0 = i1;
        i3 = i1;
        i2 = 0x0 - i1;
        i1 = i1 | i2;
        i0 = i0 & (i1 | i2);
        p0 = i0 & (i1 | i2);
        i2 = p0;
        i0 = i0 & (0x0 - p0);
        i1 = 0xffffffff;
        i0 = i0 + i1;
        l0 = i0 + i1;
        i1 = 0xc;
        i0 >>= (i1 & 31);
        i1 = 0x10;
        i0 = i0 & 0x10;
        p0 = i0 & 0x10;
        i0 = l0;
        i1 = p0;
        i0 >>= (i1 & 31);
        l0 = i0;
        i1 = 0x5;
        i0 >>= (i1 & 31);
        i1 = p0;
        i0 = i0 & 0x8;
        l1 = i0 & 0x8;
        i0 = i0 | i1;
        i1 = l0;
        i2 = l1;
        i1 >>= (i2 & 31);
        p0 = i1;
        i2 = 0x2;
        i1 >>= (i2 & 31);
        i2 = 0x4;
        i1 = i1 & 0x4;
        l0 = i1 & 0x4;
        i0 = i0 | (i1 & 0x4);
        i1 = p0;
        i2 = l0;
        i1 >>= (i2 & 31);
        p0 = i1;
        i2 = 0x1;
        i1 >>= (i2 & 31);
        i2 = 0x2;
        i1 = i1 & 0x2;
        l0 = i1 & 0x2;
        i0 = i0 | (i1 & 0x2);
        i1 = p0;
        i2 = l0;
        i1 >>= (i2 & 31);
        p0 = i1;
        i2 = 0x1;
        i1 >>= (i2 & 31);
        i2 = 0x1;
        i1 = i1 & 0x1;
        l0 = i1 & 0x1;
        i0 = i0 | (i1 & 0x1);
        i1 = p0;
        i2 = l0;
        i1 >>= (i2 & 31);
        i0 = i0 + i1;
        l1 = i0 + i1;
        i1 = 0x3;
        i0 <<= (i1 & 31);
        i0 = i0 + 0x594;
        p0 = i0 + 0x594;
        i1 = 0x8;
        i0 = i0 + i1;
        l3 = i0 + i1;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l0 = i0;
        i1 = 0x8;
        i0 = i0 + i1;
        l6 = i0 + i1;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l4 = i0;
        i1 = p0;
        i0 = i0 == i1;
        if (i0) {
          i0 = 0x56c;
          i1 = l5;
          i2 = 0x1;
          i3 = l1;
          i2 <<= (i3 & 31);
          i3 = 0xffffffff;
          i2 = i2 ^ 0xffffffff;
          i1 = i1 & (i2 ^ 0xffffffff);
          p0 = i1 & (i2 ^ 0xffffffff);
          i32_store(Z_envZ_memory, (u64)(i0), i1);
        } else {
          i0 = l4;
          i1 = p0;
          i32_store(Z_envZ_memory, (u64)(i0 + 12), i1);
          i0 = l3;
          i1 = l4;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          i0 = l5;
          p0 = l5;
        }
        i0 = l0;
        i2 = 0x3;
        i1 = l2 | 0x3;
        i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
        i1 = l1;
        i0 = l0 + l2;
        l3 = l0 + l2;
        i2 = 0x3;
        i1 <<= (i2 & 31);
        l1 = i1;
        i2 = l2;
        i1 = i1 - i2;
        l4 = i1 - i2;
        i2 = 0x1;
        i1 = i1 | 0x1;
        i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
        i1 = l4;
        i0 = l0 + l1;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = l8;
        if (i0) {
          i0 = 0x580;
          i0 = i32_load(Z_envZ_memory, (u64)(i0));
          l1 = i0;
          i0 = l8;
          i1 = 0x3;
          i0 >>= (i1 & 31);
          l2 = i0;
          i1 = 0x3;
          i0 <<= (i1 & 31);
          i1 = 0x594;
          i0 = i0 + 0x594;
          l0 = i0 + 0x594;
          i0 = p0;
          i1 = 0x1;
          i2 = l2;
          i1 <<= (i2 & 31);
          l2 = i1;
          i0 = i0 & i1;
          if (i0) {
            i1 = 0x8;
            i0 = l0 + 0x8;
            l2 = l0 + 0x8;
            i0 = i32_load(Z_envZ_memory, (u64)(i0));
          } else {
            i0 = 0x56c;
            i2 = l2;
            i1 = p0 | l2;
            i32_store(Z_envZ_memory, (u64)(i0), i1);
            i1 = 0x8;
            i0 = l0;
            l2 = l0 + 0x8;
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
        i0 = 0x574;
        i1 = l4;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = 0x580;
        i1 = l3;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = l6;
        g4 = l9;
        goto Bfunc;
      }
      i0 = 0x570;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l11 = i0;
      if (i0) {
        i2 = l11;
        i0 = (l11 & (0x0 - l11)) + 0xffffffff;
        l0 = (l11 & (0x0 - l11)) + 0xffffffff;
        i1 = 0xc;
        i0 >>= (i1 & 31);
        i1 = 0x10;
        i0 = i0 & 0x10;
        p0 = i0 & 0x10;
        i0 = l0;
        i1 = p0;
        i0 >>= (i1 & 31);
        l0 = i0;
        i1 = 0x5;
        i0 >>= (i1 & 31);
        i1 = p0;
        i0 = i0 & 0x8;
        l1 = i0 & 0x8;
        i0 = i0 | i1;
        i1 = l0;
        i2 = l1;
        i1 >>= (i2 & 31);
        p0 = i1;
        i2 = 0x2;
        i1 >>= (i2 & 31);
        i2 = 0x4;
        i1 = i1 & 0x4;
        l0 = i1 & 0x4;
        i0 = i0 | (i1 & 0x4);
        i1 = p0;
        i2 = l0;
        i1 >>= (i2 & 31);
        p0 = i1;
        i2 = 0x1;
        i1 >>= (i2 & 31);
        i2 = 0x2;
        i1 = i1 & 0x2;
        l0 = i1 & 0x2;
        i0 = i0 | (i1 & 0x2);
        i1 = p0;
        i2 = l0;
        i1 >>= (i2 & 31);
        p0 = i1;
        i2 = 0x1;
        i1 >>= (i2 & 31);
        i2 = 0x1;
        i1 = i1 & 0x1;
        l0 = i1 & 0x1;
        i0 = i0 | (i1 & 0x1);
        i1 = p0;
        i2 = l0;
        i1 >>= (i2 & 31);
        i0 = i0 + i1;
        i1 = 0x2;
        i0 <<= (i1 & 31);
        i1 = 0x69c;
        i0 = i0 + 0x69c;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l1 = i0;
        i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
        i1 = l2;
        i0 = i0 & 0xfffffff8;
        i0 = i0 - i1;
        l0 = i0 - i1;
        i1 = l1;
        i0 = l1 + 0x10;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 16));
        i1 = !(i1);
        i2 = 0x2;
        i1 <<= (i2 & 31);
        i0 = i0 + i1;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        p0 = i0;
        if (i0) {
          L12:
            i0 = p0;
            i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
            i1 = l2;
            i0 = i0 & 0xfffffff8;
            i0 = i0 - i1;
            l4 = i0 - i1;
            i1 = l0;
            i0 = i0 < i1;
            l3 = i0;
            if (i0) {
              i0 = l4;
              l0 = l4;
            }
            i0 = l3;
            if (i0) {
              i0 = p0;
              l1 = p0;
            }
            i1 = p0;
            i0 = p0 + 0x10;
            i1 = i32_load(Z_envZ_memory, (u64)(i1 + 16));
            i1 = !(i1);
            i2 = 0x2;
            i1 <<= (i2 & 31);
            i0 = i0 + i1;
            i0 = i32_load(Z_envZ_memory, (u64)(i0));
            p0 = i0;
            if (i0) {goto L12;}
            i0 = l0;
            l4 = l0;
        } else {
          i0 = l0;
          l4 = l0;
        }
        i1 = l1;
        i0 = l1 + l2;
        l10 = l1 + l2;
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
            i1 = 0x14;
            i0 = l1 + 0x14;
            l0 = l1 + 0x14;
            i0 = i32_load(Z_envZ_memory, (u64)(i0));
            p0 = i0;
            i0 = !(i0);
            if (i0) {
              i1 = 0x10;
              i0 = l1 + 0x10;
              l0 = l1 + 0x10;
              i0 = i32_load(Z_envZ_memory, (u64)(i0));
              p0 = i0;
              i0 = !(i0);
              if (i0) {
                i0 = 0x0;
                p0 = 0x0;
                goto B16;
              }
            }
            L20:
              i1 = 0x14;
              i0 = p0 + 0x14;
              l3 = p0 + 0x14;
              i0 = i32_load(Z_envZ_memory, (u64)(i0));
              l6 = i0;
              if (i0) {
                i0 = l3;
                p0 = l6;
                l0 = l3;
                goto L20;
              }
              i1 = 0x10;
              i0 = p0 + 0x10;
              l3 = p0 + 0x10;
              i0 = i32_load(Z_envZ_memory, (u64)(i0));
              l6 = i0;
              if (i0) {
                i0 = l3;
                p0 = l6;
                l0 = l3;
                goto L20;
              }
            i0 = l0;
            i1 = 0x0;
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
            i2 = 0x2;
            i1 <<= (i2 & 31);
            i2 = 0x69c;
            i1 = i1 + 0x69c;
            l3 = i1 + 0x69c;
            i1 = i32_load(Z_envZ_memory, (u64)(i1));
            i0 = i0 == i1;
            if (i0) {
              i0 = l3;
              i1 = p0;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
              i0 = p0;
              i0 = !(i0);
              if (i0) {
                i0 = 0x570;
                i1 = l11;
                i2 = 0x1;
                i3 = l0;
                i2 <<= (i3 & 31);
                i3 = 0xffffffff;
                i2 = i2 ^ 0xffffffff;
                i1 = i1 & (i2 ^ 0xffffffff);
                i32_store(Z_envZ_memory, (u64)(i0), i1);
                goto B23;
              }
            } else {
              i1 = l7;
              i0 = l7 + 0x10;
              i1 = i32_load(Z_envZ_memory, (u64)(i1 + 16));
              i2 = l1;
              i1 = i1 != i2;
              i2 = 0x2;
              i1 <<= (i2 & 31);
              i0 = i0 + i1;
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
          i1 = 0x10;
          i0 = i0 < i1;
          if (i0) {
            i0 = l1;
            p0 = l4 + l2;
            i2 = 0x3;
            i1 = (l4 + l2) | 0x3;
            i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
            i1 = 0x4;
            i0 = (l1 + p0) + 0x4;
            p0 = i0;
            i1 = i0;
            i1 = i32_load(Z_envZ_memory, (u64)(i1));
            i2 = 0x1;
            i1 = i1 | 0x1;
            i32_store(Z_envZ_memory, (u64)(i0), i1);
          } else {
            i0 = l1;
            i2 = 0x3;
            i1 = l2 | 0x3;
            i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
            i0 = l10;
            i2 = 0x1;
            i1 = l4 | 0x1;
            i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
            i1 = l4;
            i0 = l10 + l4;
            i32_store(Z_envZ_memory, (u64)(i0), i1);
            i0 = l8;
            if (i0) {
              i0 = 0x580;
              i0 = i32_load(Z_envZ_memory, (u64)(i0));
              l3 = i0;
              i0 = l8;
              i1 = 0x3;
              i0 >>= (i1 & 31);
              l0 = i0;
              i1 = 0x3;
              i0 <<= (i1 & 31);
              i1 = 0x594;
              i0 = i0 + 0x594;
              p0 = i0 + 0x594;
              i0 = l5;
              i1 = 0x1;
              i2 = l0;
              i1 <<= (i2 & 31);
              l0 = i1;
              i0 = i0 & i1;
              if (i0) {
                i1 = 0x8;
                i0 = p0 + 0x8;
                l2 = p0 + 0x8;
                i0 = i32_load(Z_envZ_memory, (u64)(i0));
              } else {
                i0 = 0x56c;
                i2 = l0;
                i1 = l5 | l0;
                i32_store(Z_envZ_memory, (u64)(i0), i1);
                i1 = 0x8;
                i0 = p0;
                l2 = p0 + 0x8;
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
            i0 = 0x574;
            i1 = l4;
            i32_store(Z_envZ_memory, (u64)(i0), i1);
            i0 = 0x580;
            i1 = l10;
            i32_store(Z_envZ_memory, (u64)(i0), i1);
          }
          g4 = l9;
          i1 = 0x8;
          i0 = l1 + 0x8;
          goto Bfunc;
        } else {
          i0 = l2;
          p0 = l2;
        }
      } else {
        i0 = l2;
        p0 = l2;
      }
    } else {
      i0 = l2;
      p0 = l2;
    }
  } else {
    i0 = p0;
    i1 = 0xffffffbf;
    i0 = i0 > i1;
    if (i0) {
      i0 = 0xffffffff;
      p0 = 0xffffffff;
    } else {
      i1 = 0xb;
      i0 = p0 + 0xb;
      p0 = i0;
      i1 = 0xfffffff8;
      i0 = i0 & i1;
      l1 = i0 & i1;
      i0 = 0x570;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l4 = i0;
      if (i0) {
        i0 = p0;
        i1 = 0x8;
        i0 >>= (i1 & 31);
        p0 = i0;
        if (i0) {
          i0 = l1;
          i1 = 0xffffff;
          i0 = i0 > i1;
          if (i0) {
            i0 = 0x1f;
          } else {
            i0 = l1;
            i1 = 0xe;
            i2 = p0;
            i3 = p0 + 0xfff00;
            i4 = 0x10;
            i3 >>= (i4 & 31);
            i4 = 0x8;
            i3 = i3 & 0x8;
            p0 = i3 & 0x8;
            i2 <<= (i3 & 31);
            l0 = i2;
            i3 = 0x7f000;
            i2 = i2 + i3;
            i3 = 0x10;
            i2 >>= (i3 & 31);
            i3 = p0;
            i2 = i2 & 0x4;
            l2 = i2 & 0x4;
            i2 = i2 | i3;
            i3 = l0;
            i4 = l2;
            i3 <<= (i4 & 31);
            p0 = i3;
            i4 = 0x3c000;
            i3 = i3 + i4;
            i4 = 0x10;
            i3 >>= (i4 & 31);
            i4 = 0x2;
            i3 = i3 & 0x2;
            l0 = i3 & 0x2;
            i2 = i2 | (i3 & 0x2);
            i1 = i1 - (i2 | (i3 & 0x2));
            i2 = p0;
            i3 = l0;
            i2 <<= (i3 & 31);
            i3 = 0xf;
            i2 >>= (i3 & 31);
            i1 = i1 + i2;
            p0 = i1 + i2;
            i2 = 0x7;
            i1 = i1 + 0x7;
            i0 >>= (i1 & 31);
            i1 = p0;
            i0 = i0 & 0x1;
            i2 = 0x1;
            i1 <<= (i2 & 31);
            i0 = i0 | i1;
          }
        } else {
          i0 = 0x0;
        }
        l8 = i0;
        i0 = l8;
        l2 = 0x0 - l1;
        i1 = 0x2;
        i0 <<= (i1 & 31);
        i1 = 0x69c;
        i0 = i0 + 0x69c;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        p0 = i0;
        if (i0) {
          i0 = 0x19;
          i1 = l8;
          i2 = 0x1;
          i1 >>= (i2 & 31);
          i0 = i0 - i1;
          l3 = i0 - i1;
          i0 = l1;
          l0 = 0x0;
          i1 = l8;
          i2 = 0x1f;
          i1 = i1 == i2;
          if (i1) {
            i1 = 0x0;
          } else {
            i1 = l3;
          }
          i0 <<= (i1 & 31);
          l6 = i0;
          i0 = 0x0;
          l3 = 0x0;
          L40:
            i0 = p0;
            i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
            i1 = l1;
            i0 = i0 & 0xfffffff8;
            i0 = i0 - i1;
            l5 = i0 - i1;
            i1 = l2;
            i0 = i0 < i1;
            if (i0) {
              i0 = l5;
              if (i0) {
                i0 = l5;
                l0 = p0;
                l2 = l5;
              } else {
                i0 = p0;
                l2 = 0x0;
                l0 = p0;
                goto B37;
              }
            }
            i0 = p0;
            i0 = i32_load(Z_envZ_memory, (u64)(i0 + 20));
            l5 = i0;
            i0 = !(i0);
            i1 = l5;
            i3 = l6;
            i2 = p0 + 0x10;
            i4 = 0x1f;
            i3 >>= (i4 & 31);
            i4 = 0x2;
            i3 <<= (i4 & 31);
            i2 = i2 + i3;
            i2 = i32_load(Z_envZ_memory, (u64)(i2));
            p0 = i2;
            i1 = i1 == i2;
            i0 = i0 | i1;
            i0 = !(i0);
            if (i0) {
              i0 = l5;
              l3 = l5;
            }
            i0 = l6;
            i1 = p0;
            i1 = !(i1);
            l5 = i1;
            i2 = 0x1;
            i1 = i1 ^ i2;
            i0 <<= (i1 & 31);
            l6 = i0;
            i0 = l5;
            i0 = !(i0);
            if (i0) {goto L40;}
            i0 = l0;
            p0 = l0;
        } else {
          i0 = 0x0;
          p0 = 0x0;
        }
        i1 = p0;
        i0 = l3 | p0;
        if (i0) {
          i0 = l3;
        } else {
          i0 = l4;
          i1 = 0x2;
          i2 = l8;
          i1 <<= (i2 & 31);
          p0 = i1;
          i3 = i1;
          i2 = 0x0 - i1;
          i1 = i1 | i2;
          i0 = i0 & (i1 | i2);
          p0 = i0 & (i1 | i2);
          i0 = !(i0);
          if (i0) {
            i0 = l1;
            p0 = l1;
            goto B0;
          }
          i2 = p0;
          i0 = (p0 & (0x0 - p0)) + 0xffffffff;
          l3 = (p0 & (0x0 - p0)) + 0xffffffff;
          i1 = 0xc;
          i0 >>= (i1 & 31);
          i1 = 0x10;
          i0 = i0 & 0x10;
          l0 = i0 & 0x10;
          i0 = l3;
          p0 = 0x0;
          i1 = l0;
          i0 >>= (i1 & 31);
          l3 = i0;
          i1 = 0x5;
          i0 >>= (i1 & 31);
          i1 = l0;
          i0 = i0 & 0x8;
          l6 = i0 & 0x8;
          i0 = i0 | i1;
          i1 = l3;
          i2 = l6;
          i1 >>= (i2 & 31);
          l0 = i1;
          i2 = 0x2;
          i1 >>= (i2 & 31);
          i2 = 0x4;
          i1 = i1 & 0x4;
          l3 = i1 & 0x4;
          i0 = i0 | (i1 & 0x4);
          i1 = l0;
          i2 = l3;
          i1 >>= (i2 & 31);
          l0 = i1;
          i2 = 0x1;
          i1 >>= (i2 & 31);
          i2 = 0x2;
          i1 = i1 & 0x2;
          l3 = i1 & 0x2;
          i0 = i0 | (i1 & 0x2);
          i1 = l0;
          i2 = l3;
          i1 >>= (i2 & 31);
          l0 = i1;
          i2 = 0x1;
          i1 >>= (i2 & 31);
          i2 = 0x1;
          i1 = i1 & 0x1;
          l3 = i1 & 0x1;
          i0 = i0 | (i1 & 0x1);
          i1 = l0;
          i2 = l3;
          i1 >>= (i2 & 31);
          i0 = i0 + i1;
          i1 = 0x2;
          i0 <<= (i1 & 31);
          i1 = 0x69c;
          i0 = i0 + 0x69c;
          i0 = i32_load(Z_envZ_memory, (u64)(i0));
        }
        l0 = i0;
        if (i0) {goto B37;}
        i0 = p0;
        l3 = p0;
        goto B36;
        B37:;
        L46:
          i0 = l0;
          i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
          i1 = l1;
          i0 = i0 & 0xfffffff8;
          i0 = i0 - i1;
          l3 = i0 - i1;
          i1 = l2;
          i0 = i0 < i1;
          l6 = i0;
          if (i0) {
            i0 = l3;
            l2 = l3;
          }
          i0 = l6;
          if (i0) {
            i0 = l0;
            p0 = l0;
          }
          i1 = l0;
          i0 = l0 + 0x10;
          i1 = i32_load(Z_envZ_memory, (u64)(i1 + 16));
          i1 = !(i1);
          i2 = 0x2;
          i1 <<= (i2 & 31);
          i0 = i0 + i1;
          i0 = i32_load(Z_envZ_memory, (u64)(i0));
          l0 = i0;
          if (i0) {goto L46;}
          i0 = p0;
          l3 = p0;
        B36:;
        i0 = l3;
        if (i0) {
          i0 = l2;
          i1 = 0x574;
          i1 = i32_load(Z_envZ_memory, (u64)(i1));
          i2 = l1;
          i1 = i1 - l1;
          i0 = i0 < i1;
          if (i0) {
            i1 = l3;
            i0 = l3 + l1;
            l7 = l3 + l1;
            i0 = i0 <= i1;
            if (i0) {
              g4 = l9;
              i0 = 0x0;
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
              i1 = 0x14;
              i0 = l3 + 0x14;
              l0 = l3 + 0x14;
              i0 = i32_load(Z_envZ_memory, (u64)(i0));
              p0 = i0;
              i0 = !(i0);
              if (i0) {
                i1 = 0x10;
                i0 = l3 + 0x10;
                l0 = l3 + 0x10;
                i0 = i32_load(Z_envZ_memory, (u64)(i0));
                p0 = i0;
                i0 = !(i0);
                if (i0) {
                  i0 = 0x0;
                  p0 = 0x0;
                  goto B52;
                }
              }
              L56:
                i1 = 0x14;
                i0 = p0 + 0x14;
                l6 = p0 + 0x14;
                i0 = i32_load(Z_envZ_memory, (u64)(i0));
                l5 = i0;
                if (i0) {
                  i0 = l6;
                  p0 = l5;
                  l0 = l6;
                  goto L56;
                }
                i1 = 0x10;
                i0 = p0 + 0x10;
                l6 = p0 + 0x10;
                i0 = i32_load(Z_envZ_memory, (u64)(i0));
                l5 = i0;
                if (i0) {
                  i0 = l6;
                  p0 = l5;
                  l0 = l6;
                  goto L56;
                }
              i0 = l0;
              i1 = 0x0;
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
              i2 = 0x2;
              i1 <<= (i2 & 31);
              i2 = 0x69c;
              i1 = i1 + 0x69c;
              l6 = i1 + 0x69c;
              i1 = i32_load(Z_envZ_memory, (u64)(i1));
              i0 = i0 == i1;
              if (i0) {
                i0 = l6;
                i1 = p0;
                i32_store(Z_envZ_memory, (u64)(i0), i1);
                i0 = p0;
                i0 = !(i0);
                if (i0) {
                  i0 = 0x570;
                  i1 = l4;
                  i2 = 0x1;
                  i3 = l0;
                  i2 <<= (i3 & 31);
                  i3 = 0xffffffff;
                  i2 = i2 ^ 0xffffffff;
                  i1 = i1 & (i2 ^ 0xffffffff);
                  p0 = i1 & (i2 ^ 0xffffffff);
                  i32_store(Z_envZ_memory, (u64)(i0), i1);
                  goto B59;
                }
              } else {
                i1 = l8;
                i0 = l8 + 0x10;
                i1 = i32_load(Z_envZ_memory, (u64)(i1 + 16));
                i2 = l3;
                i1 = i1 != i2;
                i2 = 0x2;
                i1 <<= (i2 & 31);
                i0 = i0 + i1;
                i1 = p0;
                i32_store(Z_envZ_memory, (u64)(i0), i1);
                i0 = p0;
                i0 = !(i0);
                if (i0) {
                  i0 = l4;
                  p0 = l4;
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
            i1 = 0x10;
            i0 = i0 < i1;
            if (i0) {
              i0 = l3;
              p0 = l2 + l1;
              i2 = 0x3;
              i1 = (l2 + l1) | 0x3;
              i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
              i1 = 0x4;
              i0 = (l3 + p0) + 0x4;
              p0 = i0;
              i1 = i0;
              i1 = i32_load(Z_envZ_memory, (u64)(i1));
              i2 = 0x1;
              i1 = i1 | 0x1;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
            } else {
              i0 = l3;
              i2 = 0x3;
              i1 = l1 | 0x3;
              i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
              i0 = l7;
              i2 = 0x1;
              i1 = l2 | 0x1;
              i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
              i1 = l2;
              i0 = l7 + l2;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
              i0 = l2;
              i1 = 0x3;
              i0 >>= (i1 & 31);
              l0 = i0;
              i0 = l2;
              i1 = 0x100;
              i0 = i0 < i1;
              if (i0) {
                i0 = l0;
                i1 = 0x3;
                i0 <<= (i1 & 31);
                i1 = 0x594;
                i0 = i0 + 0x594;
                p0 = i0 + 0x594;
                i0 = 0x56c;
                i0 = i32_load(Z_envZ_memory, (u64)(i0));
                l2 = i0;
                i1 = 0x1;
                i2 = l0;
                i1 <<= (i2 & 31);
                l0 = i1;
                i0 = i0 & i1;
                if (i0) {
                  i1 = 0x8;
                  i0 = p0 + 0x8;
                  l2 = p0 + 0x8;
                  i0 = i32_load(Z_envZ_memory, (u64)(i0));
                } else {
                  i0 = 0x56c;
                  i2 = l0;
                  i1 = l2 | l0;
                  i32_store(Z_envZ_memory, (u64)(i0), i1);
                  i1 = 0x8;
                  i0 = p0;
                  l2 = p0 + 0x8;
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
              i1 = 0x8;
              i0 >>= (i1 & 31);
              l0 = i0;
              if (i0) {
                i0 = l2;
                i1 = 0xffffff;
                i0 = i0 > i1;
                if (i0) {
                  i0 = 0x1f;
                } else {
                  i0 = l2;
                  i1 = 0xe;
                  i2 = l0;
                  i3 = l0 + 0xfff00;
                  i4 = 0x10;
                  i3 >>= (i4 & 31);
                  i4 = 0x8;
                  i3 = i3 & 0x8;
                  l0 = i3 & 0x8;
                  i2 <<= (i3 & 31);
                  l1 = i2;
                  i3 = 0x7f000;
                  i2 = i2 + i3;
                  i3 = 0x10;
                  i2 >>= (i3 & 31);
                  i3 = l0;
                  i2 = i2 & 0x4;
                  l4 = i2 & 0x4;
                  i2 = i2 | i3;
                  i3 = l1;
                  i4 = l4;
                  i3 <<= (i4 & 31);
                  l0 = i3;
                  i4 = 0x3c000;
                  i3 = i3 + i4;
                  i4 = 0x10;
                  i3 >>= (i4 & 31);
                  i4 = 0x2;
                  i3 = i3 & 0x2;
                  l1 = i3 & 0x2;
                  i2 = i2 | (i3 & 0x2);
                  i1 = i1 - (i2 | (i3 & 0x2));
                  i2 = l0;
                  i3 = l1;
                  i2 <<= (i3 & 31);
                  i3 = 0xf;
                  i2 >>= (i3 & 31);
                  i1 = i1 + i2;
                  l0 = i1 + i2;
                  i2 = 0x7;
                  i1 = i1 + 0x7;
                  i0 >>= (i1 & 31);
                  i1 = l0;
                  i0 = i0 & 0x1;
                  i2 = 0x1;
                  i1 <<= (i2 & 31);
                  i0 = i0 | i1;
                }
              } else {
                i0 = 0x0;
              }
              l0 = i0;
              i1 = 0x2;
              i0 <<= (i1 & 31);
              i1 = 0x69c;
              i0 = i0 + 0x69c;
              l1 = i0 + 0x69c;
              i0 = l7;
              i1 = l0;
              i32_store(Z_envZ_memory, (u64)(i0 + 28), i1);
              i0 = l7 + 0x10;
              l4 = l7 + 0x10;
              i1 = 0x0;
              i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
              i0 = l4;
              i1 = 0x0;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
              i0 = p0;
              i1 = 0x1;
              i2 = l0;
              i1 <<= (i2 & 31);
              l4 = i1;
              i0 = i0 & i1;
              i0 = !(i0);
              if (i0) {
                i0 = 0x570;
                i2 = l4;
                i1 = p0 | l4;
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
              i0 = 0x19;
              i1 = l0;
              i2 = 0x1;
              i1 >>= (i2 & 31);
              i0 = i0 - i1;
              l1 = i0 - i1;
              i0 = l2;
              i1 = l0;
              i2 = 0x1f;
              i1 = i1 == i2;
              if (i1) {
                i1 = 0x0;
              } else {
                i1 = l1;
              }
              i0 <<= (i1 & 31);
              l0 = i0;
              L75:
                i0 = p0;
                i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
                i1 = l2;
                i0 = i0 & 0xfffffff8;
                i0 = i0 == i1;
                if (i0) {goto B74;}
                i0 = l0;
                i1 = 0x1;
                i0 <<= (i1 & 31);
                l1 = i0;
                i1 = l0;
                i0 = p0 + 0x10;
                i2 = 0x1f;
                i1 >>= (i2 & 31);
                i2 = 0x2;
                i1 <<= (i2 & 31);
                i0 = i0 + i1;
                l0 = i0 + i1;
                i0 = i32_load(Z_envZ_memory, (u64)(i0));
                l4 = i0;
                if (i0) {
                  i0 = l4;
                  l0 = l1;
                  p0 = l4;
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
              i1 = 0x8;
              i0 = p0 + 0x8;
              l0 = p0 + 0x8;
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
              i1 = 0x0;
              i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
            }
            B66:;
            g4 = l9;
            i1 = 0x8;
            i0 = l3 + 0x8;
            goto Bfunc;
          } else {
            i0 = l1;
            p0 = l1;
          }
        } else {
          i0 = l1;
          p0 = l1;
        }
      } else {
        i0 = l1;
        p0 = l1;
      }
    }
  }
  B0:;
  i0 = 0x574;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  l2 = i0;
  i1 = p0;
  i0 = i0 >= i1;
  if (i0) {
    i0 = 0x580;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l0 = i0;
    i0 = l2 - p0;
    l1 = l2 - p0;
    i1 = 0xf;
    i0 = i0 > i1;
    if (i0) {
      i0 = 0x580;
      i2 = p0;
      i1 = l0 + p0;
      l4 = l0 + p0;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = 0x574;
      i1 = l1;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l4;
      i2 = 0x1;
      i1 = l1 | 0x1;
      i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
      i1 = l1;
      i0 = l0 + l2;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l0;
      i2 = 0x3;
      i1 = p0 | 0x3;
      i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    } else {
      i0 = 0x574;
      i1 = 0x0;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = 0x580;
      i1 = 0x0;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l0;
      i2 = 0x3;
      i1 = l2 | 0x3;
      i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
      i1 = (l0 + l2) + 0x4;
      i0 = (l0 + l2) + 0x4;
      p0 = (l0 + l2) + 0x4;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      i2 = 0x1;
      i1 = i1 | 0x1;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
    }
    g4 = l9;
    i1 = 0x8;
    i0 = l0 + 0x8;
    goto Bfunc;
  }
  i0 = 0x578;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  l2 = i0;
  i1 = p0;
  i0 = i0 > i1;
  if (i0) {
    i0 = 0x578;
    i2 = p0;
    i1 = l2 - p0;
    l2 = i1;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x584;
    i1 = 0x584;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    l0 = i1;
    i2 = p0;
    i1 = i1 + i2;
    l1 = i1 + i2;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l1;
    i2 = 0x1;
    i1 = l2 | 0x1;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    i0 = l0;
    i2 = 0x3;
    i1 = p0 | 0x3;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    g4 = l9;
    i1 = 0x8;
    i0 = l0 + 0x8;
    goto Bfunc;
  }
  i0 = 0x744;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  if (i0) {
    i0 = 0x74c;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
  } else {
    i0 = 0x74c;
    i1 = 0x1000;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x748;
    i1 = 0x1000;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x750;
    i1 = 0xffffffff;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x754;
    i1 = 0xffffffff;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x758;
    i1 = 0x0;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x728;
    i1 = 0x0;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x744;
    i2 = 0x55555558;
    i1 = (l7 & 0xfffffff0) ^ 0x55555558;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x1000;
  }
  l0 = i0;
  i2 = 0x2f;
  i1 = p0 + 0x2f;
  l3 = p0 + 0x2f;
  i0 = i0 + i1;
  l6 = i0 + i1;
  i2 = l0;
  i1 = p0;
  l5 = 0x0 - l0;
  i0 = i0 & (0x0 - l0);
  l4 = i0 & (0x0 - l0);
  i0 = i0 <= i1;
  if (i0) {
    g4 = l9;
    i0 = 0x0;
    goto Bfunc;
  }
  i0 = 0x724;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  l0 = i0;
  if (i0) {
    i0 = 0x71c;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l1 = i0;
    i1 = l4;
    i0 = i0 + i1;
    l7 = i0 + i1;
    i1 = l1;
    i0 = i0 <= i1;
    i1 = l7;
    i2 = l0;
    i1 = i1 > i2;
    i0 = i0 | i1;
    if (i0) {
      g4 = l9;
      i0 = 0x0;
      goto Bfunc;
    }
  }
  i1 = 0x30;
  l7 = p0 + 0x30;
  i0 = 0x728;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  i1 = 0x4;
  i0 = i0 & 0x4;
  if (i0) {
    i0 = 0x0;
    l2 = 0x0;
  } else {
    i0 = 0x584;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l0 = i0;
    i0 = !(i0);
    if (i0) {goto B89;}
    i0 = 0x72c;
    l1 = 0x72c;
    L90:
      i0 = l1;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l8 = i0;
      i1 = l0;
      i0 = i0 <= i1;
      if (i0) {
        i0 = l8;
        i2 = 0x4;
        i1 = l1 + 0x4;
        l8 = i1;
        i1 = i32_load(Z_envZ_memory, (u64)(i1));
        i0 = i0 + i1;
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
    i1 = l5;
    i0 = (l6 - l2) & l5;
    l2 = i0;
    i1 = 0x7fffffff;
    i0 = i0 < i1;
    if (i0) {
      i0 = l2;
      i0 = _sbrk(i0);
      l0 = i0;
      i1 = l1;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      i2 = l8;
      i2 = i32_load(Z_envZ_memory, (u64)(i2));
      i1 = i1 + i2;
      i0 = i0 == i1;
      if (i0) {
        i0 = l0;
        i1 = 0xffffffff;
        i0 = i0 != i1;
        if (i0) {goto B85;}
      } else {
        goto B88;
      }
    } else {
      i0 = 0x0;
      l2 = 0x0;
    }
    goto B87;
    B89:;
    i0 = 0x0;
    i0 = _sbrk(i0);
    l0 = i0;
    i1 = 0xffffffff;
    i0 = i0 == i1;
    if (i0) {
      i0 = 0x0;
      l2 = 0x0;
    } else {
      i0 = 0x748;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l1 = i0;
      i1 = 0xffffffff;
      i0 = i0 + i1;
      l6 = i0 + i1;
      l2 = l0;
      i0 = i0 + l0;
      i2 = l1;
      i1 = 0x0 - l1;
      i0 = i0 & i1;
      i1 = l2;
      i0 = i0 - l2;
      l1 = i0 - l2;
      i1 = l2;
      i0 = l6 & l2;
      if (i0) {
        i0 = l1;
      } else {
        i0 = 0x0;
      }
      i0 = i0 + l4;
      l2 = i0 + l4;
      i1 = 0x71c;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      l6 = i1;
      i0 = i0 + i1;
      l1 = i0 + i1;
      i0 = l2;
      i1 = p0;
      i0 = i0 > i1;
      i1 = l2;
      i2 = 0x7fffffff;
      i1 = i1 < i2;
      i0 = i0 & i1;
      if (i0) {
        i0 = 0x724;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l5 = i0;
        if (i0) {
          i0 = l1;
          i1 = l6;
          i0 = i0 <= i1;
          i1 = l1;
          i2 = l5;
          i1 = i1 > i2;
          i0 = i0 | i1;
          if (i0) {
            i0 = 0x0;
            l2 = 0x0;
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
        l0 = l1;
        goto B88;
      } else {
        i0 = 0x0;
        l2 = 0x0;
      }
    }
    goto B87;
    B88:;
    i0 = l7;
    i1 = l2;
    i0 = i0 > i1;
    i1 = l2;
    i2 = 0x7fffffff;
    i1 = i1 < i2;
    i2 = l0;
    i3 = 0xffffffff;
    i2 = i2 != i3;
    i1 = i1 & i2;
    i0 = i0 & (i1 & i2);
    i0 = !(i0);
    if (i0) {
      i0 = l0;
      i1 = 0xffffffff;
      i0 = i0 == i1;
      if (i0) {
        i0 = 0x0;
        l2 = 0x0;
        goto B87;
      } else {
        goto B85;
      }
      UNREACHABLE;
    }
    i0 = l3 - l2;
    i1 = 0x74c;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    l1 = i1;
    i0 = i0 + i1;
    i2 = l1;
    i1 = 0x0 - l1;
    i0 = i0 & (0x0 - l1);
    l1 = i0;
    i1 = 0x7fffffff;
    i0 = i0 >= i1;
    if (i0) {goto B85;}
    i1 = l2;
    i0 = l1;
    l3 = 0x0 - l2;
    i0 = _sbrk(i0);
    i1 = 0xffffffff;
    i0 = i0 == i1;
    if (i0) {
      i0 = l3;
      i0 = _sbrk(i0);
      i0 = 0x0;
      l2 = 0x0;
    } else {
      i1 = l2;
      i0 = l1 + l2;
      l2 = i0;
      goto B85;
    }
    B87:;
    i0 = 0x728;
    i1 = 0x728;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    i2 = 0x4;
    i1 = i1 | 0x4;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
  }
  i0 = l4;
  i1 = 0x7fffffff;
  i0 = i0 < i1;
  if (i0) {
    i0 = l4;
    i0 = _sbrk(i0);
    l0 = i0;
    i1 = 0x0;
    i1 = _sbrk(i1);
    l1 = i1;
    i0 = i0 < i1;
    i1 = l0;
    i2 = 0xffffffff;
    i1 = i1 != i2;
    i2 = l1;
    i3 = 0xffffffff;
    i2 = i2 != i3;
    i1 = i1 & i2;
    i0 = i0 & (i1 & i2);
    l4 = i0 & (i1 & i2);
    i1 = l0;
    i0 = l1 - l0;
    l1 = i0;
    i2 = 0x28;
    i1 = p0 + 0x28;
    i0 = i0 > i1;
    l3 = i0;
    if (i0) {
      i0 = l1;
      l2 = l1;
    }
    i0 = l0;
    i1 = 0xffffffff;
    i0 = i0 == i1;
    i0 = i0 | (l3 ^ 0x1);
    i2 = 0x1;
    i1 = l4 ^ 0x1;
    i0 = i0 | i1;
    i0 = !(i0);
    if (i0) {goto B85;}
  }
  goto B84;
  B85:;
  i0 = 0x71c;
  i1 = 0x71c;
  i1 = i32_load(Z_envZ_memory, (u64)(i1));
  i2 = l2;
  i1 = i1 + l2;
  l1 = i1 + l2;
  i32_store(Z_envZ_memory, (u64)(i0), i1);
  i0 = l1;
  i1 = 0x720;
  i1 = i32_load(Z_envZ_memory, (u64)(i1));
  i0 = i0 > i1;
  if (i0) {
    i0 = 0x720;
    i1 = l1;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
  }
  i0 = 0x584;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  l3 = i0;
  if (i0) {
    i0 = 0x72c;
    l1 = 0x72c;
    L110:
      i0 = l0;
      i1 = l1;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      l4 = i1;
      i3 = 0x4;
      i2 = l1 + 0x4;
      l6 = l1 + 0x4;
      i2 = i32_load(Z_envZ_memory, (u64)(i2));
      l5 = i2;
      i1 = i1 + i2;
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
    i1 = 0x8;
    i0 = i0 & 0x8;
    i0 = !(i0);
    if (i0) {
      i0 = l0;
      i1 = l3;
      i0 = i0 > i1;
      i1 = l4;
      i2 = l3;
      i1 = i1 <= i2;
      i0 = i0 & i1;
      if (i0) {
        i0 = l6;
        i2 = l2;
        i1 = l5 + l2;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = 0x578;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        i1 = l2;
        i0 = i0 + l2;
        l2 = i0;
        l1 = l3 + 0x8;
        i1 = l3;
        l0 = (0x0 - (l3 + 0x8)) & 0x7;
        i0 = 0x584;
        i3 = 0x7;
        i2 = (l3 + 0x8) & 0x7;
        if (i2) {
          i2 = l0;
        } else {
          i2 = 0x0;
          l0 = 0x0;
        }
        i1 = i1 + i2;
        l1 = i1 + i2;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = 0x578;
        i2 = l0;
        i1 = l2 - l0;
        l0 = i1;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = l1;
        i2 = 0x1;
        i1 = l0 | 0x1;
        i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
        i0 = l3 + l2;
        i1 = 0x28;
        i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
        i0 = 0x588;
        i1 = 0x754;
        i1 = i32_load(Z_envZ_memory, (u64)(i1));
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        goto B106;
      }
    }
    B108:;
    i0 = l0;
    i1 = 0x57c;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    i0 = i0 < i1;
    if (i0) {
      i0 = 0x57c;
      i1 = l0;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
    }
    i1 = l2;
    l4 = l0 + l2;
    i0 = 0x72c;
    l1 = 0x72c;
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
      i0 = 0x72c;
      l1 = 0x72c;
    goto B115;
    B116:;
    i0 = l1;
    i0 = i32_load(Z_envZ_memory, (u64)(i0 + 12));
    i1 = 0x8;
    i0 = i0 & 0x8;
    if (i0) {
      i0 = 0x72c;
      l1 = 0x72c;
    } else {
      i0 = l1;
      i1 = l0;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i1 = 0x4;
      i0 = l1 + 0x4;
      l1 = i0;
      i1 = i0;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      i2 = l2;
      i1 = i1 + l2;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      l2 = l0 + 0x8;
      l1 = (0x0 - (l0 + 0x8)) & 0x7;
      l6 = l4 + 0x8;
      i0 = l0;
      l8 = (0x0 - (l4 + 0x8)) & 0x7;
      i2 = 0x7;
      i1 = (l0 + 0x8) & 0x7;
      if (i1) {
        i1 = l1;
      } else {
        i1 = 0x0;
      }
      i0 = i0 + i1;
      l7 = i0 + i1;
      i1 = p0;
      i0 = i0 + p0;
      l5 = i0 + p0;
      i0 = l4;
      i2 = 0x7;
      i1 = l6 & 0x7;
      if (i1) {
        i1 = l8;
      } else {
        i1 = 0x0;
      }
      i0 = i0 + i1;
      l4 = i0 + i1;
      i1 = p0;
      i0 = i0 - l7;
      i0 = i0 - i1;
      l6 = i0 - i1;
      i0 = l7;
      i2 = 0x3;
      i1 = p0 | 0x3;
      i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
      i0 = l3;
      i1 = l4;
      i0 = i0 == i1;
      if (i0) {
        i0 = 0x578;
        i1 = 0x578;
        i1 = i32_load(Z_envZ_memory, (u64)(i1));
        i2 = l6;
        i1 = i1 + l6;
        p0 = i1 + l6;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = 0x584;
        i1 = l5;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = l5;
        i2 = 0x1;
        i1 = p0 | 0x1;
        i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
      } else {
        i0 = 0x580;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        i1 = l4;
        i0 = i0 == i1;
        if (i0) {
          i0 = 0x574;
          i1 = 0x574;
          i1 = i32_load(Z_envZ_memory, (u64)(i1));
          i2 = l6;
          i1 = i1 + l6;
          p0 = i1 + l6;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          i0 = 0x580;
          i1 = l5;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          i0 = l5;
          i2 = 0x1;
          i1 = p0 | 0x1;
          i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
          i1 = p0;
          i0 = l5 + p0;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          goto B121;
        }
        i0 = l4;
        i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
        p0 = i0;
        i1 = 0x3;
        i0 = i0 & i1;
        i1 = 0x1;
        i0 = i0 == i1;
        if (i0) {
          i0 = p0;
          l8 = p0 & 0xfffffff8;
          i1 = 0x3;
          i0 >>= (i1 & 31);
          l2 = i0;
          i0 = p0;
          i1 = 0x100;
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
              i0 = 0x56c;
              i1 = 0x56c;
              i1 = i32_load(Z_envZ_memory, (u64)(i1));
              i2 = 0x1;
              i3 = l2;
              i2 <<= (i3 & 31);
              i3 = 0xffffffff;
              i2 = i2 ^ 0xffffffff;
              i1 = i1 & (i2 ^ 0xffffffff);
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
              l0 = l4 + 0x10;
              i1 = 0x4;
              i0 = (l4 + 0x10) + 0x4;
              l2 = (l4 + 0x10) + 0x4;
              i0 = i32_load(Z_envZ_memory, (u64)(i0));
              p0 = i0;
              if (i0) {
                i0 = l2;
                l0 = l2;
              } else {
                i0 = l0;
                i0 = i32_load(Z_envZ_memory, (u64)(i0));
                p0 = i0;
                i0 = !(i0);
                if (i0) {
                  i0 = 0x0;
                  p0 = 0x0;
                  goto B128;
                }
              }
              L132:
                i1 = 0x14;
                i0 = p0 + 0x14;
                l2 = p0 + 0x14;
                i0 = i32_load(Z_envZ_memory, (u64)(i0));
                l1 = i0;
                if (i0) {
                  i0 = l2;
                  p0 = l1;
                  l0 = l2;
                  goto L132;
                }
                i1 = 0x10;
                i0 = p0 + 0x10;
                l2 = p0 + 0x10;
                i0 = i32_load(Z_envZ_memory, (u64)(i0));
                l1 = i0;
                if (i0) {
                  i0 = l2;
                  p0 = l1;
                  l0 = l2;
                  goto L132;
                }
              i0 = l0;
              i1 = 0x0;
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
            i1 = 0x2;
            i0 <<= (i1 & 31);
            i1 = 0x69c;
            i0 = i0 + 0x69c;
            l2 = i0 + 0x69c;
            i0 = i32_load(Z_envZ_memory, (u64)(i0));
            i1 = l4;
            i0 = i0 == i1;
            if (i0) {
              i0 = l2;
              i1 = p0;
              i32_store(Z_envZ_memory, (u64)(i0), i1);
              i0 = p0;
              if (i0) {goto B135;}
              i0 = 0x570;
              i1 = 0x570;
              i1 = i32_load(Z_envZ_memory, (u64)(i1));
              i2 = 0x1;
              i3 = l0;
              i2 <<= (i3 & 31);
              i3 = 0xffffffff;
              i2 = i2 ^ 0xffffffff;
              i1 = i1 & (i2 ^ 0xffffffff);
              i32_store(Z_envZ_memory, (u64)(i0), i1);
              goto B125;
            } else {
              i1 = l3;
              i0 = l3 + 0x10;
              i1 = i32_load(Z_envZ_memory, (u64)(i1 + 16));
              i2 = l4;
              i1 = i1 != i2;
              i2 = 0x2;
              i1 <<= (i2 & 31);
              i0 = i0 + i1;
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
            i1 = 0x10;
            i0 = l4 + 0x10;
            l2 = l4 + 0x10;
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
          i1 = l6;
          p0 = l4 + l8;
          i0 = l8 + l6;
        } else {
          i0 = l6;
          p0 = l4;
        }
        l4 = i0;
        i1 = 0x4;
        i0 = p0 + 0x4;
        p0 = i0;
        i1 = i0;
        i1 = i32_load(Z_envZ_memory, (u64)(i1));
        i2 = 0xfffffffe;
        i1 = i1 & 0xfffffffe;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = l5;
        i2 = 0x1;
        i1 = l4 | 0x1;
        i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
        i1 = l4;
        i0 = l5 + l4;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = l4;
        i1 = 0x3;
        i0 >>= (i1 & 31);
        l0 = i0;
        i0 = l4;
        i1 = 0x100;
        i0 = i0 < i1;
        if (i0) {
          i0 = l0;
          i1 = 0x3;
          i0 <<= (i1 & 31);
          i1 = 0x594;
          i0 = i0 + 0x594;
          p0 = i0 + 0x594;
          i0 = 0x56c;
          i0 = i32_load(Z_envZ_memory, (u64)(i0));
          l2 = i0;
          i1 = 0x1;
          i2 = l0;
          i1 <<= (i2 & 31);
          l0 = i1;
          i0 = i0 & i1;
          if (i0) {
            i1 = 0x8;
            i0 = p0 + 0x8;
            l2 = p0 + 0x8;
            i0 = i32_load(Z_envZ_memory, (u64)(i0));
          } else {
            i0 = 0x56c;
            i2 = l0;
            i1 = l2 | l0;
            i32_store(Z_envZ_memory, (u64)(i0), i1);
            i1 = 0x8;
            i0 = p0;
            l2 = p0 + 0x8;
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
        i1 = 0x8;
        i0 >>= (i1 & 31);
        p0 = i0;
        if (i0) {
          i0 = 0x1f;
          i1 = l4;
          i2 = 0xffffff;
          i1 = i1 > i2;
          if (i1) {goto B140;}
          i0 = l4;
          i1 = 0xe;
          i2 = p0;
          i3 = p0 + 0xfff00;
          i4 = 0x10;
          i3 >>= (i4 & 31);
          i4 = 0x8;
          i3 = i3 & 0x8;
          p0 = i3 & 0x8;
          i2 <<= (i3 & 31);
          l0 = i2;
          i3 = 0x7f000;
          i2 = i2 + i3;
          i3 = 0x10;
          i2 >>= (i3 & 31);
          i3 = p0;
          i2 = i2 & 0x4;
          l2 = i2 & 0x4;
          i2 = i2 | i3;
          i3 = l0;
          i4 = l2;
          i3 <<= (i4 & 31);
          p0 = i3;
          i4 = 0x3c000;
          i3 = i3 + i4;
          i4 = 0x10;
          i3 >>= (i4 & 31);
          i4 = 0x2;
          i3 = i3 & 0x2;
          l0 = i3 & 0x2;
          i2 = i2 | (i3 & 0x2);
          i1 = i1 - (i2 | (i3 & 0x2));
          i2 = p0;
          i3 = l0;
          i2 <<= (i3 & 31);
          i3 = 0xf;
          i2 >>= (i3 & 31);
          i1 = i1 + i2;
          p0 = i1 + i2;
          i2 = 0x7;
          i1 = i1 + 0x7;
          i0 >>= (i1 & 31);
          i1 = p0;
          i0 = i0 & 0x1;
          i2 = 0x1;
          i1 <<= (i2 & 31);
          i0 = i0 | i1;
        } else {
          i0 = 0x0;
        }
        B140:;
        l0 = i0;
        i1 = 0x2;
        i0 <<= (i1 & 31);
        i1 = 0x69c;
        i0 = i0 + 0x69c;
        p0 = i0 + 0x69c;
        i0 = l5;
        i1 = l0;
        i32_store(Z_envZ_memory, (u64)(i0 + 28), i1);
        i0 = l5 + 0x10;
        l2 = l5 + 0x10;
        i1 = 0x0;
        i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
        i0 = l2;
        i1 = 0x0;
        i32_store(Z_envZ_memory, (u64)(i0), i1);
        i0 = 0x570;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l2 = i0;
        i1 = 0x1;
        i2 = l0;
        i1 <<= (i2 & 31);
        l1 = i1;
        i0 = i0 & i1;
        i0 = !(i0);
        if (i0) {
          i0 = 0x570;
          i2 = l1;
          i1 = l2 | l1;
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
        i0 = 0x19;
        i1 = l0;
        i2 = 0x1;
        i1 >>= (i2 & 31);
        i0 = i0 - i1;
        l2 = i0 - i1;
        i0 = l4;
        i1 = l0;
        i2 = 0x1f;
        i1 = i1 == i2;
        if (i1) {
          i1 = 0x0;
        } else {
          i1 = l2;
        }
        i0 <<= (i1 & 31);
        l0 = i0;
        L145:
          i0 = p0;
          i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
          i1 = l4;
          i0 = i0 & 0xfffffff8;
          i0 = i0 == i1;
          if (i0) {goto B144;}
          i0 = l0;
          i1 = 0x1;
          i0 <<= (i1 & 31);
          l2 = i0;
          i1 = l0;
          i0 = p0 + 0x10;
          i2 = 0x1f;
          i1 >>= (i2 & 31);
          i2 = 0x2;
          i1 <<= (i2 & 31);
          i0 = i0 + i1;
          l0 = i0 + i1;
          i0 = i32_load(Z_envZ_memory, (u64)(i0));
          l1 = i0;
          if (i0) {
            i0 = l1;
            l0 = l2;
            p0 = l1;
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
        i1 = 0x8;
        i0 = p0 + 0x8;
        l0 = p0 + 0x8;
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
        i1 = 0x0;
        i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
      }
      B121:;
      g4 = l9;
      i1 = 0x8;
      i0 = l7 + 0x8;
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
        i0 = i0 + i1;
        l7 = i0 + i1;
        i1 = l3;
        i0 = i0 > i1;
        if (i0) {goto B148;}
      }
      i0 = l1;
      i0 = i32_load(Z_envZ_memory, (u64)(i0 + 8));
      l1 = i0;
      goto L147;
      B148:;
    l1 = l7 + 0xffffffd1;
    l4 = (l7 + 0xffffffd1) + 0x8;
    i0 = l7 + 0xffffffd1;
    l6 = (0x0 - ((l7 + 0xffffffd1) + 0x8)) & 0x7;
    i2 = 0x7;
    i1 = ((l7 + 0xffffffd1) + 0x8) & 0x7;
    if (i1) {
      i1 = l6;
    } else {
      i1 = 0x0;
    }
    i0 = i0 + i1;
    l1 = i0 + i1;
    i2 = 0x10;
    i1 = l3 + 0x10;
    l11 = l3 + 0x10;
    i0 = i0 < i1;
    if (i0) {
      i0 = l3;
      l1 = l3;
    } else {
      i0 = l1;
    }
    i1 = 0x8;
    i0 = i0 + 0x8;
    l5 = i0 + 0x8;
    l4 = l1 + 0x18;
    l8 = l2 + 0xffffffd8;
    l10 = l0 + 0x8;
    i1 = l0;
    l6 = (0x0 - (l0 + 0x8)) & 0x7;
    i0 = 0x584;
    i3 = 0x7;
    i2 = (l0 + 0x8) & 0x7;
    if (i2) {
      i2 = l6;
    } else {
      i2 = 0x0;
      l6 = 0x0;
    }
    i1 = i1 + i2;
    l10 = i1 + i2;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x578;
    i2 = l6;
    i1 = l8 - l6;
    l6 = i1;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l10;
    i2 = 0x1;
    i1 = l6 | 0x1;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    i0 = l0 + l8;
    i1 = 0x28;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    i0 = 0x588;
    i1 = 0x754;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l1 + 0x4;
    l6 = l1 + 0x4;
    i1 = 0x1b;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l5;
    i1 = 0x72c;
    j1 = MEM_as_u64[0x72c];
    MEM_as_u64[l5] := MEM_as_u64[0x72c];
    i0 = l5;
    i1 = 0x734;
    j1 = MEM_as_u64[0x734];
    i64_store(Z_envZ_memory, (u64)(i0 + 8), j1);
    i0 = 0x72c;
    i1 = l0;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x730;
    i1 = l2;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x738;
    i1 = 0x0;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x734;
    i1 = l5;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l4;
    l0 = l4;
    L153:
      i0 = l0 + 0x4;
      l2 = l0 + 0x4;
      i1 = 0x7;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i1 = l7;
      i0 = l0 + 0x8;
      i0 = i0 < i1;
      if (i0) {
        i0 = l2;
        l0 = l2;
        goto L153;
      }
    i0 = l1;
    i1 = l3;
    i0 = i0 != i1;
    if (i0) {
      i0 = l6;
      i1 = l6;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      i2 = 0xfffffffe;
      i1 = i1 & 0xfffffffe;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l3;
      l6 = l1 - l3;
      i2 = 0x1;
      i1 = (l1 - l3) | 0x1;
      i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
      i0 = l1;
      i1 = l6;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l6;
      i1 = 0x3;
      i0 >>= (i1 & 31);
      l2 = i0;
      i0 = l6;
      i1 = 0x100;
      i0 = i0 < i1;
      if (i0) {
        i0 = l2;
        i1 = 0x3;
        i0 <<= (i1 & 31);
        i1 = 0x594;
        i0 = i0 + 0x594;
        l0 = i0 + 0x594;
        i0 = 0x56c;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l1 = i0;
        i1 = 0x1;
        i2 = l2;
        i1 <<= (i2 & 31);
        l2 = i1;
        i0 = i0 & i1;
        if (i0) {
          i1 = 0x8;
          i0 = l0 + 0x8;
          l1 = l0 + 0x8;
          i0 = i32_load(Z_envZ_memory, (u64)(i0));
        } else {
          i0 = 0x56c;
          i2 = l2;
          i1 = l1 | l2;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          i1 = 0x8;
          i0 = l0;
          l1 = l0 + 0x8;
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
      i1 = 0x8;
      i0 >>= (i1 & 31);
      l0 = i0;
      if (i0) {
        i0 = l6;
        i1 = 0xffffff;
        i0 = i0 > i1;
        if (i0) {
          i0 = 0x1f;
        } else {
          i0 = l6;
          i1 = 0xe;
          i2 = l0;
          i3 = l0 + 0xfff00;
          i4 = 0x10;
          i3 >>= (i4 & 31);
          i4 = 0x8;
          i3 = i3 & 0x8;
          l0 = i3 & 0x8;
          i2 <<= (i3 & 31);
          l2 = i2;
          i3 = 0x7f000;
          i2 = i2 + i3;
          i3 = 0x10;
          i2 >>= (i3 & 31);
          i3 = l0;
          i2 = i2 & 0x4;
          l1 = i2 & 0x4;
          i2 = i2 | i3;
          i3 = l2;
          i4 = l1;
          i3 <<= (i4 & 31);
          l0 = i3;
          i4 = 0x3c000;
          i3 = i3 + i4;
          i4 = 0x10;
          i3 >>= (i4 & 31);
          i4 = 0x2;
          i3 = i3 & 0x2;
          l2 = i3 & 0x2;
          i2 = i2 | (i3 & 0x2);
          i1 = i1 - (i2 | (i3 & 0x2));
          i2 = l0;
          i3 = l2;
          i2 <<= (i3 & 31);
          i3 = 0xf;
          i2 >>= (i3 & 31);
          i1 = i1 + i2;
          l0 = i1 + i2;
          i2 = 0x7;
          i1 = i1 + 0x7;
          i0 >>= (i1 & 31);
          i1 = l0;
          i0 = i0 & 0x1;
          i2 = 0x1;
          i1 <<= (i2 & 31);
          i0 = i0 | i1;
        }
      } else {
        i0 = 0x0;
      }
      l2 = i0;
      i1 = 0x2;
      i0 <<= (i1 & 31);
      i1 = 0x69c;
      i0 = i0 + 0x69c;
      l0 = i0 + 0x69c;
      i0 = l3;
      i1 = l2;
      i32_store(Z_envZ_memory, (u64)(i0 + 28), i1);
      i0 = l3;
      i1 = 0x0;
      i32_store(Z_envZ_memory, (u64)(i0 + 20), i1);
      i0 = l11;
      i1 = 0x0;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = 0x570;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l1 = i0;
      i1 = 0x1;
      i2 = l2;
      i1 <<= (i2 & 31);
      l4 = i1;
      i0 = i0 & i1;
      i0 = !(i0);
      if (i0) {
        i0 = 0x570;
        i2 = l4;
        i1 = l1 | l4;
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
      i0 = 0x19;
      i1 = l2;
      i2 = 0x1;
      i1 >>= (i2 & 31);
      i0 = i0 - i1;
      l1 = i0 - i1;
      i0 = l6;
      i1 = l2;
      i2 = 0x1f;
      i1 = i1 == i2;
      if (i1) {
        i1 = 0x0;
      } else {
        i1 = l1;
      }
      i0 <<= (i1 & 31);
      l2 = i0;
      L163:
        i0 = l0;
        i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
        i1 = l6;
        i0 = i0 & 0xfffffff8;
        i0 = i0 == i1;
        if (i0) {goto B162;}
        i0 = l2;
        i1 = 0x1;
        i0 <<= (i1 & 31);
        l1 = i0;
        i1 = l2;
        i0 = l0 + 0x10;
        i2 = 0x1f;
        i1 >>= (i2 & 31);
        i2 = 0x2;
        i1 <<= (i2 & 31);
        i0 = i0 + i1;
        l2 = i0 + i1;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l4 = i0;
        if (i0) {
          i0 = l4;
          l2 = l1;
          l0 = l4;
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
      i1 = 0x8;
      i0 = l0 + 0x8;
      l2 = l0 + 0x8;
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
      i1 = 0x0;
      i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
    }
  } else {
    i0 = 0x57c;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l1 = i0;
    i0 = !(i0);
    i1 = l0;
    i2 = l1;
    i1 = i1 < i2;
    i0 = i0 | i1;
    if (i0) {
      i0 = 0x57c;
      i1 = l0;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
    }
    i0 = 0x72c;
    i1 = l0;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x730;
    i1 = l2;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x738;
    i1 = 0x0;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x590;
    i1 = 0x744;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x58c;
    i1 = 0xffffffff;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5a0;
    i1 = 0x594;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x59c;
    i1 = 0x594;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5a8;
    i1 = 0x59c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5a4;
    i1 = 0x59c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5b0;
    i1 = 0x5a4;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5ac;
    i1 = 0x5a4;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5b8;
    i1 = 0x5ac;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5b4;
    i1 = 0x5ac;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5c0;
    i1 = 0x5b4;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5bc;
    i1 = 0x5b4;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5c8;
    i1 = 0x5bc;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5c4;
    i1 = 0x5bc;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5d0;
    i1 = 0x5c4;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5cc;
    i1 = 0x5c4;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5d8;
    i1 = 0x5cc;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5d4;
    i1 = 0x5cc;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5e0;
    i1 = 0x5d4;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5dc;
    i1 = 0x5d4;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5e8;
    i1 = 0x5dc;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5e4;
    i1 = 0x5dc;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5f0;
    i1 = 0x5e4;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5ec;
    i1 = 0x5e4;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5f8;
    i1 = 0x5ec;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5f4;
    i1 = 0x5ec;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x600;
    i1 = 0x5f4;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x5fc;
    i1 = 0x5f4;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x608;
    i1 = 0x5fc;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x604;
    i1 = 0x5fc;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x610;
    i1 = 0x604;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x60c;
    i1 = 0x604;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x618;
    i1 = 0x60c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x614;
    i1 = 0x60c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x620;
    i1 = 0x614;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x61c;
    i1 = 0x614;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x628;
    i1 = 0x61c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x624;
    i1 = 0x61c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x630;
    i1 = 0x624;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x62c;
    i1 = 0x624;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x638;
    i1 = 0x62c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x634;
    i1 = 0x62c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x640;
    i1 = 0x634;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x63c;
    i1 = 0x634;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x648;
    i1 = 0x63c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x644;
    i1 = 0x63c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x650;
    i1 = 0x644;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x64c;
    i1 = 0x644;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x658;
    i1 = 0x64c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x654;
    i1 = 0x64c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x660;
    i1 = 0x654;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x65c;
    i1 = 0x654;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x668;
    i1 = 0x65c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x664;
    i1 = 0x65c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x670;
    i1 = 0x664;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x66c;
    i1 = 0x664;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x678;
    i1 = 0x66c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x674;
    i1 = 0x66c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x680;
    i1 = 0x674;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x67c;
    i1 = 0x674;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x688;
    i1 = 0x67c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x684;
    i1 = 0x67c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x690;
    i1 = 0x684;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x68c;
    i1 = 0x684;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x698;
    i1 = 0x68c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x694;
    i1 = 0x68c;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    l1 = l2 + 0xffffffd8;
    i2 = 0x8;
    l4 = l0 + 0x8;
    i1 = 0x7;
    i0 = (0x0 - (l0 + 0x8)) & 0x7;
    l2 = i0;
    i0 = 0x584;
    i1 = l0;
    i3 = 0x7;
    i2 = l4 & 0x7;
    if (i2) {
      i2 = l2;
    } else {
      i2 = 0x0;
      l2 = 0x0;
    }
    i1 = i1 + i2;
    l4 = i1 + i2;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x578;
    i2 = l2;
    i1 = l1 - l2;
    l2 = i1;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l4;
    i2 = 0x1;
    i1 = l2 | 0x1;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    i0 = l0 + l1;
    i1 = 0x28;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    i0 = 0x588;
    i1 = 0x754;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    i32_store(Z_envZ_memory, (u64)(i0), i1);
  }
  B106:;
  i0 = 0x578;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  l0 = i0;
  i1 = p0;
  i0 = i0 > i1;
  if (i0) {
    i0 = 0x578;
    i2 = p0;
    i1 = l0 - p0;
    l2 = l0 - p0;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = 0x584;
    i1 = 0x584;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    l0 = i1;
    i2 = p0;
    i1 = i1 + i2;
    l1 = i1 + i2;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l1;
    i2 = 0x1;
    i1 = l2 | 0x1;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    i0 = l0;
    i2 = 0x3;
    i1 = p0 | 0x3;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    g4 = l9;
    i1 = 0x8;
    i0 = l0 + 0x8;
    goto Bfunc;
  }
  B84:;
  i0 = 0x75c;
  i1 = 0xc;
  i32_store(Z_envZ_memory, (u64)(i0), i1);
  g4 = l9;
  i0 = 0x0;
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
  i0 = 0x57c;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  l3 = i0;
  i0 = p0 + 0xfffffff8;
  l0 = p0 + 0xfffffff8;
  i2 = 0xfffffffc;
  i1 = p0 + 0xfffffffc;
  i1 = i32_load(Z_envZ_memory, (u64)(i1));
  p0 = i1;
  i2 = 0xfffffff8;
  i1 = i1 & i2;
  l2 = i1 & i2;
  i0 = i0 + (i1 & i2);
  l4 = i0 + (i1 & i2);
  i1 = 0x1;
  i0 = p0 & 0x1;
  if (i0) {
    i0 = l0;
    p0 = l0;
  } else {
    i0 = l0;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l1 = i0;
    i1 = 0x3;
    i0 = p0 & 0x3;
    i0 = !(i0);
    if (i0) {
      goto Bfunc;
    }
    i1 = l3;
    i0 = l0 - l1;
    p0 = l0 - l1;
    i0 = i0 < i1;
    if (i0) {
      goto Bfunc;
    }
    i1 = l2;
    i0 = l1 + l2;
    l2 = i0;
    i0 = 0x580;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    i1 = p0;
    i0 = i0 == i1;
    if (i0) {
      i0 = p0;
      i2 = 0x4;
      i1 = l4 + 0x4;
      l1 = l4 + 0x4;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      l0 = i1;
      i2 = 0x3;
      i1 = i1 & i2;
      i2 = 0x3;
      i1 = i1 != i2;
      if (i1) {goto B1;}
      i0 = 0x574;
      i1 = l2;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l1;
      i2 = 0xfffffffe;
      i1 = l0 & 0xfffffffe;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = p0;
      i2 = 0x1;
      i1 = l2 | 0x1;
      i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
      i1 = l2;
      i0 = p0 + l2;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      goto Bfunc;
    }
    i0 = l1;
    i1 = 0x3;
    i0 >>= (i1 & 31);
    l3 = i0;
    i0 = l1;
    i1 = 0x100;
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
        i0 = 0x56c;
        i1 = 0x56c;
        i1 = i32_load(Z_envZ_memory, (u64)(i1));
        i2 = 0x1;
        i3 = l3;
        i2 <<= (i3 & 31);
        i3 = 0xffffffff;
        i2 = i2 ^ 0xffffffff;
        i1 = i1 & (i2 ^ 0xffffffff);
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
      l0 = p0 + 0x10;
      i1 = 0x4;
      i0 = (p0 + 0x10) + 0x4;
      l3 = (p0 + 0x10) + 0x4;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l1 = i0;
      if (i0) {
        i0 = l3;
        l0 = l3;
      } else {
        i0 = l0;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l1 = i0;
        i0 = !(i0);
        if (i0) {
          i0 = 0x0;
          l1 = 0x0;
          goto B8;
        }
      }
      L12:
        i1 = 0x14;
        i0 = l1 + 0x14;
        l3 = l1 + 0x14;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l5 = i0;
        if (i0) {
          i0 = l3;
          l1 = l5;
          l0 = l3;
          goto L12;
        }
        i1 = 0x10;
        i0 = l1 + 0x10;
        l3 = l1 + 0x10;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l5 = i0;
        if (i0) {
          i0 = l3;
          l1 = l5;
          l0 = l3;
          goto L12;
        }
      i0 = l0;
      i1 = 0x0;
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
      i1 = 0x2;
      i0 <<= (i1 & 31);
      i1 = 0x69c;
      i0 = i0 + 0x69c;
      l3 = i0 + 0x69c;
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
          i0 = 0x570;
          i1 = 0x570;
          i1 = i32_load(Z_envZ_memory, (u64)(i1));
          i2 = 0x1;
          i3 = l0;
          i2 <<= (i3 & 31);
          i3 = 0xffffffff;
          i2 = i2 ^ 0xffffffff;
          i1 = i1 & (i2 ^ 0xffffffff);
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          i0 = p0;
          goto B1;
        }
      } else {
        i1 = l6;
        i0 = l6 + 0x10;
        i1 = i32_load(Z_envZ_memory, (u64)(i1 + 16));
        i2 = p0;
        i1 = i1 != i2;
        i2 = 0x2;
        i1 <<= (i2 & 31);
        i0 = i0 + i1;
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
      i1 = 0x10;
      i0 = p0 + 0x10;
      l3 = p0 + 0x10;
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
  i1 = 0x4;
  i0 = l4 + 0x4;
  l3 = l4 + 0x4;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  l0 = i0;
  i1 = 0x1;
  i0 = i0 & i1;
  i0 = !(i0);
  if (i0) {
    goto Bfunc;
  }
  i1 = 0x2;
  i0 = l0 & 0x2;
  if (i0) {
    i0 = l3;
    i2 = 0xfffffffe;
    i1 = l0 & 0xfffffffe;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l1;
    i2 = 0x1;
    i1 = l2 | 0x1;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    i1 = l2;
    i0 = p0 + l2;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
  } else {
    i0 = 0x584;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    i1 = l4;
    i0 = i0 == i1;
    if (i0) {
      i0 = 0x578;
      i1 = 0x578;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      i2 = l2;
      i1 = i1 + l2;
      p0 = i1 + l2;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = 0x584;
      i1 = l1;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l1;
      i2 = 0x1;
      i1 = p0 | 0x1;
      i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
      i0 = l1;
      i1 = 0x580;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      i0 = i0 != i1;
      if (i0) {
        goto Bfunc;
      }
      i0 = 0x580;
      i1 = 0x0;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = 0x574;
      i1 = 0x0;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      goto Bfunc;
    }
    i0 = 0x580;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    i1 = l4;
    i0 = i0 == i1;
    if (i0) {
      i0 = 0x574;
      i1 = 0x574;
      i1 = i32_load(Z_envZ_memory, (u64)(i1));
      i2 = l2;
      i1 = i1 + l2;
      l2 = i1;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = 0x580;
      i1 = p0;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i0 = l1;
      i2 = 0x1;
      i1 = l2 | 0x1;
      i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
      i1 = l2;
      i0 = p0 + l2;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      goto Bfunc;
    }
    i0 = l0;
    l6 = (l0 & 0xfffffff8) + l2;
    i1 = 0x3;
    i0 >>= (i1 & 31);
    l3 = i0;
    i0 = l0;
    i1 = 0x100;
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
        i0 = 0x56c;
        i1 = 0x56c;
        i1 = i32_load(Z_envZ_memory, (u64)(i1));
        i2 = 0x1;
        i3 = l3;
        i2 <<= (i3 & 31);
        i3 = 0xffffffff;
        i2 = i2 ^ 0xffffffff;
        i1 = i1 & (i2 ^ 0xffffffff);
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
        l0 = l4 + 0x10;
        i1 = 0x4;
        i0 = (l4 + 0x10) + 0x4;
        l3 = (l4 + 0x10) + 0x4;
        i0 = i32_load(Z_envZ_memory, (u64)(i0));
        l2 = i0;
        if (i0) {
          i0 = l3;
          l0 = l3;
        } else {
          i0 = l0;
          i0 = i32_load(Z_envZ_memory, (u64)(i0));
          l2 = i0;
          i0 = !(i0);
          if (i0) {
            i0 = 0x0;
            l2 = 0x0;
            goto B29;
          }
        }
        L33:
          i1 = 0x14;
          i0 = l2 + 0x14;
          l3 = l2 + 0x14;
          i0 = i32_load(Z_envZ_memory, (u64)(i0));
          l5 = i0;
          if (i0) {
            i0 = l3;
            l2 = l5;
            l0 = l3;
            goto L33;
          }
          i1 = 0x10;
          i0 = l2 + 0x10;
          l3 = l2 + 0x10;
          i0 = i32_load(Z_envZ_memory, (u64)(i0));
          l5 = i0;
          if (i0) {
            i0 = l3;
            l2 = l5;
            l0 = l3;
            goto L33;
          }
        i0 = l0;
        i1 = 0x0;
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
        i1 = 0x2;
        i0 <<= (i1 & 31);
        i1 = 0x69c;
        i0 = i0 + 0x69c;
        l3 = i0 + 0x69c;
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
            i0 = 0x570;
            i1 = 0x570;
            i1 = i32_load(Z_envZ_memory, (u64)(i1));
            i2 = 0x1;
            i3 = l0;
            i2 <<= (i3 & 31);
            i3 = 0xffffffff;
            i2 = i2 ^ 0xffffffff;
            i1 = i1 & (i2 ^ 0xffffffff);
            i32_store(Z_envZ_memory, (u64)(i0), i1);
            goto B26;
          }
        } else {
          i1 = l7;
          i0 = l7 + 0x10;
          i1 = i32_load(Z_envZ_memory, (u64)(i1 + 16));
          i2 = l4;
          i1 = i1 != i2;
          i2 = 0x2;
          i1 <<= (i2 & 31);
          i0 = i0 + i1;
          i1 = l2;
          i32_store(Z_envZ_memory, (u64)(i0), i1);
          i0 = l2;
          i0 = !(i0);
          if (i0) {goto B26;}
        }
        i0 = l2;
        i1 = l7;
        i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
        i1 = 0x10;
        i0 = l4 + 0x10;
        l3 = l4 + 0x10;
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
    i2 = 0x1;
    i1 = l6 | 0x1;
    i32_store(Z_envZ_memory, (u64)(i0 + 4), i1);
    i1 = l6;
    i0 = p0 + l6;
    i32_store(Z_envZ_memory, (u64)(i0), i1);
    i0 = l1;
    i1 = 0x580;
    i1 = i32_load(Z_envZ_memory, (u64)(i1));
    i0 = i0 == i1;
    if (i0) {
      i0 = 0x574;
      i1 = l6;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      goto Bfunc;
    } else {
      i0 = l6;
      l2 = l6;
    }
  }
  i0 = l2;
  i1 = 0x3;
  i0 >>= (i1 & 31);
  l0 = i0;
  i0 = l2;
  i1 = 0x100;
  i0 = i0 < i1;
  if (i0) {
    i0 = l0;
    i1 = 0x3;
    i0 <<= (i1 & 31);
    i1 = 0x594;
    i0 = i0 + 0x594;
    p0 = i0 + 0x594;
    i0 = 0x56c;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l2 = i0;
    i1 = 0x1;
    i2 = l0;
    i1 <<= (i2 & 31);
    l0 = i1;
    i0 = i0 & i1;
    if (i0) {
      i1 = 0x8;
      i0 = p0 + 0x8;
      l0 = p0 + 0x8;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
    } else {
      i0 = 0x56c;
      i2 = l0;
      i1 = l2 | l0;
      i32_store(Z_envZ_memory, (u64)(i0), i1);
      i1 = 0x8;
      i0 = p0;
      l0 = p0 + 0x8;
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
  i1 = 0x8;
  i0 >>= (i1 & 31);
  p0 = i0;
  if (i0) {
    i0 = l2;
    i1 = 0xffffff;
    i0 = i0 > i1;
    if (i0) {
      i0 = 0x1f;
    } else {
      i0 = l2;
      i1 = 0xe;
      i2 = p0;
      i3 = p0 + 0xfff00;
      i4 = 0x10;
      i3 >>= (i4 & 31);
      i4 = 0x8;
      i3 = i3 & 0x8;
      p0 = i3 & 0x8;
      i2 <<= (i3 & 31);
      l0 = i2;
      i3 = 0x7f000;
      i2 = i2 + i3;
      i3 = 0x10;
      i2 >>= (i3 & 31);
      i3 = p0;
      i2 = i2 & 0x4;
      l3 = i2 & 0x4;
      i2 = i2 | i3;
      i3 = l0;
      i4 = l3;
      i3 <<= (i4 & 31);
      p0 = i3;
      i4 = 0x3c000;
      i3 = i3 + i4;
      i4 = 0x10;
      i3 >>= (i4 & 31);
      i4 = 0x2;
      i3 = i3 & 0x2;
      l0 = i3 & 0x2;
      i2 = i2 | (i3 & 0x2);
      i1 = i1 - (i2 | (i3 & 0x2));
      i2 = p0;
      i3 = l0;
      i2 <<= (i3 & 31);
      i3 = 0xf;
      i2 >>= (i3 & 31);
      i1 = i1 + i2;
      p0 = i1 + i2;
      i2 = 0x7;
      i1 = i1 + 0x7;
      i0 >>= (i1 & 31);
      i1 = p0;
      i0 = i0 & 0x1;
      i2 = 0x1;
      i1 <<= (i2 & 31);
      i0 = i0 | i1;
    }
  } else {
    i0 = 0x0;
  }
  l0 = i0;
  i1 = 0x2;
  i0 <<= (i1 & 31);
  i1 = 0x69c;
  i0 = i0 + 0x69c;
  p0 = i0 + 0x69c;
  i0 = l1;
  i1 = l0;
  i32_store(Z_envZ_memory, (u64)(i0 + 28), i1);
  i0 = l1;
  i1 = 0x0;
  i32_store(Z_envZ_memory, (u64)(i0 + 20), i1);
  i0 = l1;
  i1 = 0x0;
  i32_store(Z_envZ_memory, (u64)(i0 + 16), i1);
  i0 = 0x570;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  l3 = i0;
  i1 = 0x1;
  i2 = l0;
  i1 <<= (i2 & 31);
  l5 = i1;
  i0 = i0 & i1;
  if (i0) {
    i0 = p0;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    p0 = i0;
    i0 = 0x19;
    i1 = l0;
    i2 = 0x1;
    i1 >>= (i2 & 31);
    i0 = i0 - i1;
    l3 = i0 - i1;
    i0 = l2;
    i1 = l0;
    i2 = 0x1f;
    i1 = i1 == i2;
    if (i1) {
      i1 = 0x0;
    } else {
      i1 = l3;
    }
    i0 <<= (i1 & 31);
    l0 = i0;
    L50:
      i0 = p0;
      i0 = i32_load(Z_envZ_memory, (u64)(i0 + 4));
      i1 = l2;
      i0 = i0 & 0xfffffff8;
      i0 = i0 == i1;
      if (i0) {goto B49;}
      i0 = l0;
      i1 = 0x1;
      i0 <<= (i1 & 31);
      l3 = i0;
      i1 = l0;
      i0 = p0 + 0x10;
      i2 = 0x1f;
      i1 >>= (i2 & 31);
      i2 = 0x2;
      i1 <<= (i2 & 31);
      i0 = i0 + i1;
      l0 = i0 + i1;
      i0 = i32_load(Z_envZ_memory, (u64)(i0));
      l5 = i0;
      if (i0) {
        i0 = l5;
        l0 = l3;
        p0 = l5;
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
    i1 = 0x8;
    i0 = p0 + 0x8;
    l2 = p0 + 0x8;
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
    i1 = 0x0;
    i32_store(Z_envZ_memory, (u64)(i0 + 24), i1);
  } else {
    i0 = 0x570;
    i2 = l5;
    i1 = l3 | l5;
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
  i0 = 0x58c;
  i1 = 0x58c;
  i1 = i32_load(Z_envZ_memory, (u64)(i1));
  i2 = 0xffffffff;
  i1 = i1 + 0xffffffff;
  p0 = i1 + 0xffffffff;
  i32_store(Z_envZ_memory, (u64)(i0), i1);
  i0 = p0;
  if (i0) {
    goto Bfunc;
  } else {
    i0 = 0x734;
    p0 = 0x734;
  }
  L53:
    i0 = p0;
    i0 = i32_load(Z_envZ_memory, (u64)(i0));
    l2 = i0;
    i1 = 0x8;
    i0 = i0 + i1;
    p0 = i0 + i1;
    i0 = l2;
    if (i0) {goto L53;}
  i0 = 0x58c;
  i1 = 0xffffffff;
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
    l0 = p1 * p0;
    i0 = p1 | p0;
    i1 = 0xffff;
    i0 = i0 > i1;
    if (i0) {
      i0 = l0;
      i1 = p0;
      i0 = DIV_U(i0, i1);
      i1 = p1;
      i0 = i0 != i1;
      if (i0) {
        i0 = 0xffffffff;
        l0 = 0xffffffff;
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
  i1 = 0xfffffffc;
  i0 = p0 + 0xfffffffc;
  i0 = i32_load(Z_envZ_memory, (u64)(i0));
  i1 = 0x3;
  i0 = i0 & 0x3;
  i0 = !(i0);
  if (i0) {
    i0 = p0;
    goto Bfunc;
  }
  i0 = p0;
  i1 = 0x0;
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
  i0 = 0x75c;
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
  i1 = 0x2000;
  i0 = (u32)((s32)i0 >= (s32)i1);
  if (i0) {
    i0 = p0;
    i1 = p1;
    i2 = p2;
    i0 = (*Z_envZ__emscripten_memcpy_bigZ_iiii)(i0, i1, i2);
    goto Bfunc;
  }
  l1 = p0;
  l0 = p0 + p2;
  i0 = p0 & 0x3;
  i2 = 0x3;
  i1 = p1 & 0x3;
  i0 = i0 == i1;
  if (i0) {
    L2:
      i1 = 0x3;
      i0 = p0 & 0x3;
      if (i0) {
        i0 = p2;
        i0 = !(i0);
        if (i0) {
          i0 = l1;
          goto Bfunc;
        }
        i0 = p0;
        i1 = (s32)MEM_as_s8[p1];
        MEM_as_u8[p0] := (s32)MEM_as_s8[p1];
        i1 = 0x1;
        i0 = p0 + 0x1;
        p0 = i0;
        i1 = 0x1;
        i0 = p1 + 0x1;
        p1 = i0;
        i1 = 0x1;
        i0 = p2 - 0x1;
        p2 = i0;
        goto L2;
      }
    p2 = l0 & 0xfffffffc;
    i1 = 0x40;
    i0 = (l0 & 0xfffffffc) - 0x40;
    l2 = (l0 & 0xfffffffc) - 0x40;
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
        i1 = 0x40;
        i0 = p0 + 0x40;
        p0 = i0;
        i1 = 0x40;
        i0 = p1 + 0x40;
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
        i1 = 0x4;
        i0 = p0 + 0x4;
        p0 = i0;
        i1 = 0x4;
        i0 = p1 + 0x4;
        p1 = i0;
        goto L7;
      }
  } else {
    i1 = 0x4;
    i0 = l0 - 0x4;
    p2 = l0 - 0x4;
    L9:
      i0 = p0;
      i1 = p2;
      i0 = (u32)((s32)i0 < (s32)i1);
      if (i0) {
        i0 = p0;
        i1 = (s32)MEM_as_s8[p1];
        MEM_as_u8[p0] := (s32)MEM_as_s8[p1];
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
        i1 = 0x4;
        i0 = p0 + 0x4;
        p0 = i0;
        i1 = 0x4;
        i0 = p1 + 0x4;
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
      i1 = (s32)MEM_as_s8[p1];
      MEM_as_u8[p0] := (s32)MEM_as_s8[p1];
      i1 = 0x1;
      i0 = p0 + 0x1;
      p0 = i0;
      i1 = 0x1;
      i0 = p1 + 0x1;
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
  l1 = p0 + p2;
  i1 = 0xff;
  i0 = p1 & 0xff;
  p1 = i0;
  i0 = p2;
  i1 = 0x43;
  i0 = (u32)((s32)i0 >= (s32)i1);
  if (i0) {
    L1:
      i1 = 0x3;
      i0 = p0 & 0x3;
      if (i0) {
        i0 = p0;
        i1 = p1;
        MEM_as_u8[p0] := p1;
        i1 = 0x1;
        i0 = p0 + 0x1;
        p0 = i0;
        goto L1;
      }
    l2 = l1 & 0xfffffffc;
    i1 = p1;
    i0 = p1;
    l3 = (l1 & 0xfffffffc) - 0x40;
    i2 = 0x8;
    i1 <<= (i2 & 31);
    i0 = i0 | i1;
    i1 = p1;
    i2 = 0x10;
    i1 <<= (i2 & 31);
    i0 = i0 | i1;
    i1 = p1;
    i2 = 0x18;
    i1 <<= (i2 & 31);
    i0 = i0 | i1;
    l0 = i0 | i1;
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
        i1 = 0x40;
        i0 = p0 + 0x40;
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
        i1 = 0x4;
        i0 = p0 + 0x4;
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
      MEM_as_u8[p0] := p1;
      i1 = 0x1;
      i0 = p0 + 0x1;
      p0 = i0;
      goto L7;
    }
  i1 = p2;
  i0 = l1 - p2;
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
  i2 = 0xfffffff0;
  i1 = (p0 + 0xf) & 0xfffffff0;
  p0 = i1;
  i0 = i0 + i1;
  l0 = i0 + i1;
  i0 = p0;
  i1 = 0x0;
  i0 = (u32)((s32)i0 > (s32)i1);
  i1 = l0;
  i2 = l1;
  i1 = (u32)((s32)i1 < (s32)i2);
  i0 = i0 & i1;
  i1 = l0;
  i2 = 0x0;
  i1 = (u32)((s32)i1 < (s32)i2);
  i0 = i0 | i1;
  if (i0) {
    i0 = (*Z_envZ_abortOnCannotGrowMemoryZ_iv)();
    i0 = 0xc;
    (*Z_envZ____setErrNoZ_vi)(i0);
    i0 = 0xffffffff;
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
      i0 = 0xc;
      (*Z_envZ____setErrNoZ_vi)(i0);
      i0 = 0xffffffff;
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
