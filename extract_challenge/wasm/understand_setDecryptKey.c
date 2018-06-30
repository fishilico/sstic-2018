Function _setDecryptKey void (u32 p0, u32 p1) // Module._setDecryptKey(ctx, key);                // ctx malloc(10 * 16), key malloc(32)
  p0 = var<param:p0_ctx>
  p1 = var<param:p1>
  i0 = var<param:p0_ctx>
  i1 = var<param:p1>
  f13(var<i0>, var<i1>)

Function f13 void (u32 p0, u32 p1)
    l6_framebuffer = var<g4>
    g4 = var<g4> + 0x40 // stack
    // Swap key blocks
    /*
    MEM_as_u64[var<l6_framebuffer> + 0x20] := MEM_as_u64[var<param:p1>]
    MEM_as_u64[var<l6_framebuffer> + 0x28] := MEM_as_u64[var<param:p1> + 8]
    MEM_as_u64[var<l6_framebuffer> + 0x10] := MEM_as_u64[var<param:p1> + 0x10]
    MEM_as_u64[var<l6_framebuffer> + 0x18] := MEM_as_u64[var<param:p1> + 0x18]
    */
    memcpy16(buffer@20, key[0:16])
    memcpy16(buffer@10, key[16:32])
    // Initial ctx
    /*
    l30 = MEM_as_u64[var<l6_framebuffer> + 0x20]
    MEM_as_u64[var<param:p0_ctx>] := MEM_as_u64[var<l6_framebuffer> + 0x20]
    l31 = MEM_as_u64[var<l6_framebuffer> + 0x28]
    MEM_as_u64[var<param:p0_ctx> + 8] := MEM_as_u64[var<l6_framebuffer> + 0x28]
    l32 = MEM_as_u64[var<l6_framebuffer> + 0x10]
    MEM_as_u64[var<param:p0_ctx> + 0x10] := MEM_as_u64[var<l6_framebuffer> + 0x10]
    l33 = MEM_as_u64[var<l6_framebuffer> + 0x18]
    MEM_as_u64[var<param:p0_ctx> + 0x18] := MEM_as_u64[var<l6_framebuffer> + 0x18]
    */
    l30,31,32,33 = (buffer@20, buffer@10)
    ctx[0x00..0x20] = (buffer@20, buffer@10) // = key

    l0_buffer@00 = var<l6_framebuffer>
    l1_buffer@10 = var<l6_framebuffer> + 0x10
    l2_buffer@20 = var<l6_framebuffer> + 0x20
    l3_buffer@30 = var<l6_framebuffer> + 0x30
    l4 = 0
    l5 = 0
    l7 = 1
    l8 = 0
    l9 = var<l0_buffer@00> + 8
    l10 = var<l0_buffer@00> + 0xf
    l11 = 0
    l12 = var<l3_buffer@30> + 0xf
    l13 = var<l6_framebuffer> + 0xa
    l14 = var<l6_framebuffer> + 0xb
    l15 = var<l6_framebuffer> + 0xc
    l16 = var<l6_framebuffer> + 0xd
    l17 = var<l6_framebuffer> + 0xe
    l18 = var<l3_buffer@30> + 8
    l19 = var<l6_framebuffer> + 1
    l20 = var<l6_framebuffer> + 2
    l21 = var<l6_framebuffer> + 3
    l22 = var<l6_framebuffer> + 4
    l23 = var<l6_framebuffer> + 5
    l24 = var<l6_framebuffer> + 6
    l25 = var<l6_framebuffer> + 7
    l26 = var<l6_framebuffer> + 8
    l27 = var<l6_framebuffer> + 9
    l28 = 0
    l29 = 0

    for (l7 = 1; l7 != 0x21; l7++) { // L0:
        /**
         * Encrypt [0,0,0,0,..., l7 & 0xff]
         */
        ZeroMemory(var<l3_buffer@30>, 16)
        MEM_as_u8[var<l3_buffer@30> + 0xf] := var<l7> & 0xff
        for (l8 = 0; l8 != 0x10; l8++) { // L1:
            // start: l2_curbyte = MEM_as_u8[var<l12>] = var<l7> & 0xff
            // at the loop block end: l2 = (s32)MEM_as_s8[var<l12>]
            // l12 = var<l6_framebuffer> + 0x3f = var<l3_buffer@30> + 0xf
            l2_curbyte = MEM_as_u8[var<l3_buffer@30> + 0xf]
            for (l1=0xe; l1 >= 0; l1--) { //L2:
                p1 = (s32)MEM_as_s8[var<l3_buffer@30> + l1]
                MEM_as_u8[var<l3_buffer@30> + (l1 + 1)] := (s32)MEM_as_s8[var<l3_buffer@30> + l1]
                l4_newbyte = 0
                l5_polynom_byte = MEM_data_@519_as_u8[l1] // (s32)MEM_as_s8[l1 + 0x519]
                do { //L3:
                    l4_newbyte = ((l5_polynom_byte & 1) ? p1 : 0) ^ l4_newbyte
                    p1 = (((p1 & 0x80) ? 0xc3 : 0) ^ (p1 << 1)) & 0xff
                    l5_polynom_byte = (l5_polynom_byte & 0xff) >> 1
                } while (l5_polynom_byte);
                l2_curbyte = l4_newbyte ^ l2_curbyte
            }
            MEM_as_u8[var<l3_buffer@30>] := l2_curbyte
        }
        // xor l3_buffer@30 with first key
        l28 = (MEM_as_u64[var<l3_buffer@30>]) ^ var<l30>
        l29 = (MEM_as_u64[var<l3_buffer@30> + 8]) ^ var<l31>
        MEM_as_u64[var<l0_buffer@00> + 8] := (MEM_as_u64[var<l3_buffer@30> + 8]) ^ var<l31>

        // S-box
        MEM_as_u8[var<l0_buffer@00>] := Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[(var<l28> & 0xff) + 0x400])
        MEM_as_u8[var<l0_buffer@00> + 1] := Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[((var<l28> >> 8) & 0xff) + 0x400])
        MEM_as_u8[var<l0_buffer@00> + 2] := Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[((var<l28> >> 0x10) & 0xff) + 0x400])
        MEM_as_u8[var<l0_buffer@00> + 3] := Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[((var<l28> >> 0x18) & 0xff) + 0x400])
        MEM_as_u8[var<l0_buffer@00> + 4] :=  Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[((var<l28> >> 0x20) & 0xff) + 0x400])
        MEM_as_u8[var<l0_buffer@00> + 5] := Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[((var<l28> >> 0x28) & 0xff) + 0x400])
        MEM_as_u8[var<l0_buffer@00> + 6] := Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[((var<l28> >> 0x30) & 0xff) + 0x400])
        MEM_as_u8[var<l0_buffer@00> + 7] := Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[((var<l28> >> 0x38) & 0xffffffff) + 0x400])

        MEM_as_u8[var<l0_buffer@00> + 8] := Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[(var<l29> & 0xff) + 0x400])
        MEM_as_u8[var<l0_buffer@00> + 9] := Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[((var<l29> >> 8) & 0xff) + 0x400])
        MEM_as_u8[var<l0_buffer@00> + 0xa] := Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[((u32)MEM_as_u8[var<l0_buffer@00> + 0xa]) + 0x400])
        MEM_as_u8[var<l0_buffer@00> + 0xb] := Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[((u32)MEM_as_u8[var<l0_buffer@00> + 0xb]) + 0x400])
        MEM_as_u8[var<l0_buffer@00> + 0xc] := Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[((u32)MEM_as_u8[var<l0_buffer@00> + 0xc]) + 0x400])
        MEM_as_u8[var<l0_buffer@00> + 0xd] := Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[((u32)MEM_as_u8[var<l0_buffer@00> + 0xd]) + 0x400])
        MEM_as_u8[var<l0_buffer@00> + 0xe] := Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[((u32)MEM_as_u8[var<l0_buffer@00> + 0xe]) + 0x400])
        MEM_as_u8[var<l0_buffer@00> + 0xf] := Z_envZ__emscripten_asm_const_iiZ_iii(0, (u32)MEM_as_u8[((u32)MEM_as_u8[var<l0_buffer@00> + 0xf]) + 0x400])

        l2_curbyte = MEM_as_u8[var<l0_buffer@00> + 0xf]
        l8 = 0
        L8:
            i0 = 0xe
            l1 = 0xe
            for (l1=0xe; l1 >= 0; l1--) { //L9:
                p1 = (s32)MEM_as_s8[var<l0_buffer@00> + l1]
                MEM_as_u8[var<l0_buffer@00> + (l1 + 1)] := (s32)MEM_as_s8[var<l0_buffer@00> + l1]

                l4_newbyte = 0
                l5_polynom_byte = MEM_data_@519_as_u8[l1] // (s32)MEM_as_s8[l1 + 0x519]
                do { //L10:
                    l4_newbyte = ((l5_polynom_byte & 1) ? p1 : 0) ^ l4_newbyte
                    p1 = (((p1 & 0x80) ? 0xc3 : 0) ^ (p1 << 1)) & 0xff
                    l5_polynom_byte = (l5_polynom_byte & 0xff) >> 1
                } while (l5_polynom_byte);
                l2_curbyte = l4_newbyte ^ l2_curbyte
            }
            MEM_as_u8[var<l0_buffer@00>] := l2_curbyte
            l8 += 1
            if (l8 != 0x10) {
                l2_curbyte = (s32)MEM_as_s8[var<l0_buffer@00> + 0xf]
                goto L8
            }

        l28 = MEM_as_u64[var<l0_buffer@00>] := (MEM_as_u64[var<l0_buffer@00>]) ^ var<l32>
        l29 = MEM_as_u64[var<l0_buffer@00> + 8] := (MEM_as_u64[var<l0_buffer@00> + 8]) ^ var<l33>

        if (!(var<l7> & 7)) { // Special every 8 steps (l7 = 8, 16, 24, 32)
            // write l28,29,30,31 into ctx[(l7 / 4) * 16 ... + 32]
            // l7 =  8 => write ctx[0x20..0x40]
            // l7 = 16 => write ctx[0x40..0x60]
            // l7 = 24 => write ctx[0x60..0x80]
            // l7 = 32 => write ctx[0x80..0xa0] // MAX
            MEM_as_u64[var<param:p0_ctx> + ((var<l7> >>s 2) << 4)] := var<l28>
            MEM_as_u64[(var<param:p0_ctx> + ((var<l7> >>s 2) << 4)) + 8] := var<l29>
            MEM_as_u64[var<param:p0_ctx> + (((var<l7> >>s 2) + 1) << 4)] := var<l30>
            MEM_as_u64[(var<param:p0_ctx> + (((var<l7> >>s 2) + 1) << 4)) + 8] := var<l31>
        }
        i0 = var<l7> + 1

        // l30,31,32,33 = l28,29,30,31
        l32 = var<l30>
        l33 = var<l31>
        l30 = var<l28>
        l31 = var<l29>
    }

