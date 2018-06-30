Function f17 u32 (u32 p0 = framepointer)
    p0 = var<param:p0>
    l0 = 0
    l1 = 0
    l3 = 0
    l4 = 0

    l2 = calloc(1, 4)
    l0 = _malloc(0x100)
    *l2 := l0

    // Read 64-bit int from 8 octets, LittleEndian
    /*j0 = (((((((((u32)MEM_as_u8[var<p0> + 0xe]) << 0x30) | (((u32)MEM_as_u8[var<p0> + 0xf]) << 0x38)) | (((u32)MEM_as_u8[var<p0> + 0xd]) << 0x28)) | (((u32)MEM_as_u8[var<p0> + 0xc]) << 0x20)) | (((u32)MEM_as_u8[var<p0> + 0xb]) << 0x18)) | (((u32)MEM_as_u8[var<p0> + 0xa]) << 0x10)) | (((u32)MEM_as_u8[var<p0> + 9]) << 8)) | ((u32)MEM_as_u8[var<p0> + 8])
    j1 = (((((((((u32)MEM_as_u8[var<p0> + 6]) << 0x30) | (((u32)MEM_as_u8[var<p0> + 7]) << 0x38)) | (((u32)MEM_as_u8[var<p0> + 5]) << 0x28)) | (((u32)MEM_as_u8[var<p0> + 4]) << 0x20)) | (((u32)MEM_as_u8[var<p0> + 3]) << 0x18)) | (((u32)MEM_as_u8[var<p0> + 2]) << 0x10)) | (((u32)MEM_as_u8[var<p0> + 1]) << 8)) | ((u32)MEM_as_u8[var<p0>])
    i1 = (u32)MEM_as_u8[var<p0> + 6]
    j2 = (u32)MEM_as_u8[var<p0>]
    l4 = (((((((((u32)MEM_as_u8[var<p0> + 0xe]) << 0x30) | (((u32)MEM_as_u8[var<p0> + 0xf]) << 0x38)) | (((u32)MEM_as_u8[var<p0> + 0xd]) << 0x28)) | (((u32)MEM_as_u8[var<p0> + 0xc]) << 0x20)) | (((u32)MEM_as_u8[var<p0> + 0xb]) << 0x18)) | (((u32)MEM_as_u8[var<p0> + 0xa]) << 0x10)) | (((u32)MEM_as_u8[var<p0> + 9]) << 8)) | ((u32)MEM_as_u8[var<p0> + 8])
    i2 = (u32)MEM_as_u8[var<p0>]
    l3 = (((((((((u32)MEM_as_u8[var<p0> + 6]) << 0x30) | (((u32)MEM_as_u8[var<p0> + 7]) << 0x38)) | (((u32)MEM_as_u8[var<p0> + 5]) << 0x28)) | (((u32)MEM_as_u8[var<p0> + 4]) << 0x20)) | (((u32)MEM_as_u8[var<p0> + 3]) << 0x18)) | (((u32)MEM_as_u8[var<p0> + 2]) << 0x10)) | (((u32)MEM_as_u8[var<p0> + 1]) << 8)) | ((u32)MEM_as_u8[var<p0>])
    */
    l4_highbytes = struct_unpack("<Q", &p0[8..0xf])
    l3_lowbytes = struct_unpack("<Q", &p0[0..7])

    //MEM_as_u64[var<l0>] := (((((((((u32)MEM_as_u8[var<p0> + 6]) << 0x30) | (((u32)MEM_as_u8[var<p0> + 7]) << 0x38)) | (((u32)MEM_as_u8[var<p0> + 5]) << 0x28)) | (((u32)MEM_as_u8[var<p0> + 4]) << 0x20)) | (((u32)MEM_as_u8[var<p0> + 3]) << 0x18)) | (((u32)MEM_as_u8[var<p0> + 2]) << 0x10)) | (((u32)MEM_as_u8[var<p0> + 1]) << 8)) | ((u32)MEM_as_u8[var<p0>])
    *(u64*)l0 = l3;


    /*
    for (l1 = 0; l1 != 0x1f; l1++) {
      i2 = var<l1>

      j1 = ((var<l4> >> 8) | (var<l4> << 0x38)) + var<l3>
      j1 = var<j1> ^ sign_extend_32_to_64(var<l1>)
      l4 = var<j1>
      j1 = var<j1> ^ ((var<l3> << 3) | (var<l3> >> 0x3d))
      l3 = MEM_as_u64[var<l0> + ((var<l1> + 1) << 3)] := var<j1>
    }
    */
    for (i = 0; i != 0x1f; i++) {
      i2 = i

      l4_highbytes = (ROR64(l4_highbytes, 8) + l3_lowbytes) ^ i
      l3_lowbytes = l4_highbytes ^ ROL64(l3_lowbytes, 3)
      //MEM_as_u64[var<l0> + ((i + 1) * 8)] := l3_lowbytes
      ((u64*)l0)[i + 1] = l3_lowbytes // l0 has 32 8-byte integers => 256 bytes
    }
    return var<i2>




Function f16 u32 (u32 p0, u32 p1, u32 p2, u32 p3) (var<p0>=ctx, var<l2>=block48, var<l1> + 0x10 = output buffer, 0x30)
  p0 = var<param:p0_ctx>
  p1 = var<param:p1_inbuf>
  p2 = var<param:p2_outbuf>
  p3 = var<param:p3_size>
  l0 = 0
  l1 = 0
  l2 = 0
  l3 = 0
  l4 = 0
  l5 = 0
  l6 = 0
  l7 = 0
  l8 = 0
  l9 = 0
  l10 = 0
  l11 = 0

  l3_numblocks = I32_DIV_S(var<param:p3_size>, 0x10) // number of blocks

  i0 = 0
  p3 = 0
  L2:
    curblock_in = &inbuf[p3 * 16] // (u32)MEM_as_u8[(var<p1> + (var<p3> << 4)) + ...]
    i0 = curblock_in[6]
    l4 = MEM_as_u32[var<p0>]
    i1 = curblock_in[0]
    i2 = 4
    l5 = var<p3> << 4
    l1 = var<p1> + (var<p3> << 4)
    l2 = (var<p1> + (var<p3> << 4)) + 8
    /*
    j0 = ((((((((curblock_in[6]) << 0x30) | ((curblock_in[7]) << 0x38)) | ((curblock_in[5]) << 0x28)) | ((curblock_in[4]) << 0x20)) | ((curblock_in[3]) << 0x18)) | ((curblock_in[2]) << 0x10)) | ((curblock_in[1]) << 8)) | (curblock_in[0])
    j1 = curblock_in[0]
    j2 = 8
    l8 = (((((((((u32)MEM_as_u8[(var<p1> + (var<p3> << 4)) + 0xe]) << 0x30) | (((u32)MEM_as_u8[(var<p1> + (var<p3> << 4)) + 0xf]) << 0x38)) | (((u32)MEM_as_u8[(var<p1> + (var<p3> << 4)) + 0xd]) << 0x28)) | (((u32)MEM_as_u8[(var<p1> + (var<p3> << 4)) + 0xc]) << 0x20)) | (((u32)MEM_as_u8[(var<p1> + (var<p3> << 4)) + 0xb]) << 0x18)) | (((u32)MEM_as_u8[(var<p1> + (var<p3> << 4)) + 0xa]) << 0x10)) | ((curblock_in[9]) << 8)) | (curblock_in[8])
    l9 = ((((((((curblock_in[6]) << 0x30) | ((curblock_in[7]) << 0x38)) | ((curblock_in[5]) << 0x28)) | ((curblock_in[4]) << 0x20)) | ((curblock_in[3]) << 0x18)) | ((curblock_in[2]) << 0x10)) | ((curblock_in[1]) << 8)) | (curblock_in[0])
    */
    l9_lowbytes, l8_highbytes = struct.unpack("<QQ", curblock_in)
    for (l0 = 0x20; l0 > 0; l0--) { // L3:
        l7 = l8_highbytes ^ l9_lowbytes
        l11 = l7 >> 3
        l9_lowbytes = ROR64(l8_highbytes ^ l9_lowbytes, 3)
        l6 = ((l4_ctx_as_u64[(l0-1)*8]) ^ l8_highbytes) - l9_lowbytes
        l8_highbytes = ROL64(l6, 8)
    }
    /*
    MEM_as_u8[var<p2> + var<l5>] := var<l7> >> 3
    MEM_as_u8[(var<p2> + var<l5>) + 1] := var<l7> >> 0xb
    MEM_as_u8[(var<p2> + var<l5>) + 2] := var<l7> >> 0x13
    MEM_as_u8[(var<p2> + var<l5>) + 3] := var<l7> >> 0x1b
    MEM_as_u8[(var<p2> + var<l5>) + 4] := var<l7> >> 0x23
    MEM_as_u8[(var<p2> + var<l5>) + 5] := var<l7> >> 0x2b
    MEM_as_u8[(var<p2> + var<l5>) + 6] := var<l7> >> 0x33
    MEM_as_u8[(var<p2> + var<l5>) + 7] := var<l9> >> 0x38
    MEM_as_u8[(var<p2> + var<l5>) + 8] := var<l6> >> 0x38
    MEM_as_u8[(var<p2> + var<l5>) + 9] := var<l6>
    MEM_as_u8[(var<p2> + var<l5>) + 0xa] := var<l6> >> 8
    MEM_as_u8[(var<p2> + var<l5>) + 0xb] := var<l6> >> 0x10
    MEM_as_u8[(var<p2> + var<l5>) + 0xc] := var<l6> >> 0x18
    MEM_as_u8[(var<p2> + var<l5>) + 0xd] := var<l6> >> 0x20
    MEM_as_u8[(var<p2> + var<l5>) + 0xe] := var<l6> >> 0x28
    MEM_as_u8[(var<p2> + var<l5>) + 0xf] := var<l6> >> 0x30
    */
    outbuf[p3 * 16:...+16] = pack("<QQ", l9, l8)

    if (++p3 != l3_numblocks) {
            goto L2
    }
  i0 = 0
  p0 = 0
  Bfunc:
  return var<i0>

