/*
Module._decryptBlock(ctx=malloc(10 * 16), block=16-bytes)
    ctx:
        0x00-0x8f: ?
        0x90-0x9f : xoré avec le block
g4 = stack
l32 = base ptr : MEM_as_u64[l23..l23+0x10] = local block
*/
Function _decryptBlock void (u32 p0, u32 p1)
    l23 = g4
    g4 = g4 + 0x10

    MEM_as_u64[var<l23_bp>] := (MEM_as_u64[param:p0 + 0x90]) ^ (MEM_as_u64[param:p1])
    MEM_as_u64[var<l23_bp> + 8] := (MEM_as_u64[param:p0 + 0x98]) ^ (MEM_as_u64[param:p1 + 8])
  /*
    j1 = (MEM_as_u64[param:p0 + 0x98]) ^ (MEM_as_u64[param:p1 + 8])
    j2 = MEM_as_u64[param:p1 + 8]
    p0 = param:p0
    p1 = param:p1*/
    l0 = var<l23_bp>
    l1 = 0
    l3 = var<l23_bp> + 0xf
    l4 = var<l23_bp> + 8
    l5 = var<l23_bp> + 1
    l6 = var<l23_bp> + 2
    l7 = var<l23_bp> + 3
    l8 = var<l23_bp> + 4
    l9 = var<l23_bp> + 5
    l10 = var<l23_bp> + 6
    l11 = var<l23_bp> + 7
    l12 = var<l23_bp> + 8
    l13 = var<l23_bp> + 9
    l14 = var<l23_bp> + 0xa
    l15 = var<l23_bp> + 0xb
    l16 = var<l23_bp> + 0xc
    l17 = var<l23_bp> + 0xd
    l18 = var<l23_bp> + 0xe
    // temporary registers
    l19 = 0
    l20 = 0
    l21 = 0
    l24 = 0
    l25 = 0
    l26 = var<param:p1> + 8
    l27 = 0
    l28 = 0

    repeat_9_times { // for (l2 = 8; l2 >= 0; l2--) {
  
        repeat_16_times { //for (l24 = 0; l24+1 != 0x10; l24++) {

            l19_curbyte = block[0] // (s32)MEM_as_s8[var<l23_bp>]

            for (l25 = 1; l25 != 0x10; l25++) { // l1, saved in l25 ; bounds imprecise... do { ...} while (l25 != 15)
                //MEM_as_u8[var<l23_bp> + var<l1>] := (s32)MEM_as_s8[var<l23_bp> + (var<l1> + 1)]
                l1 = block[l25-1] = block[l25] // l1 = (s32)MEM_as_s8[var<l23_bp> + (var<l1> + 1)]

                /* polynom with LSFR ? */
                //l22_polynom_byte = MEM_as_s8[0x519 + (l25-1)] // = (s32)MEM_as_s8[var<l1> + 0x519]

                l21_newbyte = 0
                for (l22_polynom_byte = MEM_as_s8[0x519 + (l25-1)]; l22_polynom_byte; l22_polynom_byte >>= 1) {
                    l21_newbyte = l21_newbyte ^ ((l22_polynom_byte & 1) ? l1 : 0)
                    l1 = l1 & 0xff
                    l1 = (( (l1 & 0x80) ? 0xc3 : 0) ^ ( (l1 & 0xff) << 1 )) & 0xff
                    // l22 = (var<l22> & 0xff) >> 1
                } //while (l22);
                /* end of polynom */

                l19_curbyte = l21 ^ l19_curbyte
            }
            block[15] = l19_curbyte // MEM_as_u8[(var<l23_bp> + 0xf)] := l19_curbyte
        }
        
        // sbox!
        MEM_as_u8[var<l23_bp>] := Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[((u32)MEM_as_u8[var<l23_bp>]) + 0x400])
        MEM_as_u8[var<l23_bp> + 1] := Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[((u32)MEM_as_u8[var<l23_bp> + 1]) + 0x400])
        ...
        MEM_as_u8[var<l23_bp> + 0xf] := Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[((u32)MEM_as_u8[var<l23_bp> + 0xf]) + 0x400])

        /* Save last block, with xor with ctx! */
        l27 = MEM_as_u64[var<l23_bp>] := (MEM_as_u64[var<l23_bp>]) ^ (MEM_as_u64[var<p0> + (var<l2_loop1idx_8_to_1> << 4)])
        l28 = MEM_as_u64[(var<l23_bp> + 8)] := (MEM_as_u64[(var<l23_bp> + 8)]) ^ (MEM_as_u64[(var<p0> + (var<l2_loop1idx_8_to_1> << 4)) + 8])
    }

    /* Copy block back to caller block WITH UN-DEOBFUSCATED sbox */
    MEM_as_u8[var<l23_bp>] := (s32)MEM_as_s8[(var<l27> & 0xff) + 0x400]
    MEM_as_u8[(var<l23_bp> + 1)] := (s32)MEM_as_s8[((var<l27> >> 8) & 0xff) + 0x400]
    MEM_as_u8[(var<l23_bp> + 2)] := (s32)MEM_as_s8[((var<l27> >> 0x10) & 0xff) + 0x400]
    MEM_as_u8[(var<l23_bp> + 3)] := (s32)MEM_as_s8[((var<l27> >> 0x18) & 0xff) + 0x400]
    MEM_as_u8[(var<l23_bp> + 4)] := (s32)MEM_as_s8[((var<l27> >> 0x20) & 0xff) + 0x400]
    MEM_as_u8[(var<l23_bp> + 5)] := (s32)MEM_as_s8[((var<l27> >> 0x28) & 0xff) + 0x400]
    MEM_as_u8[(var<l23_bp> + 6)] := (s32)MEM_as_s8[((var<l27> >> 0x30) & 0xff) + 0x400]
    MEM_as_u8[(var<l23_bp> + 7)] := (s32)MEM_as_s8[((var<l27> >> 0x38) & 0xffffffff) + 0x400]
    MEM_as_u8[(var<l23_bp> + 8)] := (s32)MEM_as_s8[(var<l28> & 0xff) + 0x400]
    MEM_as_u8[(var<l23_bp> + 9)] := (s32)MEM_as_s8[((var<l28> >> 8) & 0xff) + 0x400]
    MEM_as_u8[(var<l23_bp> + 0xa)] := (s32)MEM_as_s8[((var<l28> >> 0x10) & 0xff) + 0x400]
    MEM_as_u8[(var<l23_bp> + 0xb)] := (s32)MEM_as_s8[((var<l28> >> 0x18) & 0xff) + 0x400]
    MEM_as_u8[(var<l23_bp> + 0xc)] := (s32)MEM_as_s8[((var<l28> >> 0x20) & 0xff) + 0x400]
    MEM_as_u8[(var<l23_bp> + 0xd)] := (s32)MEM_as_s8[((var<l28> >> 0x28) & 0xff) + 0x400]
    MEM_as_u8[(var<l23_bp> + 0xe)] := (s32)MEM_as_s8[((var<l28> >> 0x30) & 0xff) + 0x400]
    MEM_as_u8[(var<l23_bp> + 0xf)] := (s32)MEM_as_s8[((var<l28> >> 0x38) & 0xffffffff) + 0x400]
    MEM_as_u64[var<p1>] := MEM_as_u64[var<l23_bp>]
    MEM_as_u64[var<param:p1> + 8] := MEM_as_u64[(var<l23_bp> + 8)]
