;
;  Redistribution and use in source and binary forms, with or without
;  modification, are permitted provided that the following conditions
;  are met:
;    * Redistributions of source code must retain the above copyright
;      notice, this list of conditions and the following disclaimer.
;    * Redistributions in binary form must reproduce the above copyright
;      notice, this list of conditions and the following disclaimer in
;      the documentation and/or other materials provided with the
;      distribution.
;    * Neither the name of Intel Corporation nor the names of its
;      contributors may be used to endorse or promote products derived
;      from this software without specific prior written permission.
;
;  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
;  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
;  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
;  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
;  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
;  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
;  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
;  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
;  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
;  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
;  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;
;;; gf_3vect_mad_avx2_gfni(len, vec, vec_i, mul_array, src, dest);
;;;

%include "reg_sizes.asm"
%include "gf_vect_gfni.inc"
%include "memcpy.asm"

%ifidn __OUTPUT_FORMAT__, elf64
 %define arg0   rdi
 %define arg1   rsi
 %define arg2   rdx
 %define arg3   rcx
 %define arg4   r8
 %define arg5   r9
 %define tmp    r11
 %define tmp2   r10
 %define func(x) x: endbranch
 %define FUNC_SAVE
 %define FUNC_RESTORE
%endif

%ifidn __OUTPUT_FORMAT__, win64
 %define arg0   rcx
 %define arg1   rdx
 %define arg2   r8
 %define arg3   r9
 %define arg4   r12   ; must be saved, loaded and restored
 %define arg5   r13   ; must be saved and restored
 %define tmp    r11
 %define tmp2   r10
 %define stack_size 16*10 + 3*8
 %define arg(x)      [rsp + stack_size + 8 + 8*x]
 %define func(x) proc_frame x

 %macro FUNC_SAVE 0
        sub     rsp, stack_size
        vmovdqa [rsp + 0*16], xmm6
        vmovdqa [rsp + 1*16], xmm7
        vmovdqa [rsp + 2*16], xmm8
        vmovdqa [rsp + 3*16], xmm9
        vmovdqa [rsp + 4*16], xmm10
        vmovdqa [rsp + 5*16], xmm11
        vmovdqa [rsp + 6*16], xmm12
        vmovdqa [rsp + 7*16], xmm13
        vmovdqa [rsp + 8*16], xmm14
        vmovdqa [rsp + 9*16], xmm15
        mov     [rsp + 10*16 + 0*8], r12
        mov     [rsp + 10*16 + 1*8], r13
        end_prolog
        mov     arg4, arg(4)
        mov     arg5, arg(5)
 %endmacro

 %macro FUNC_RESTORE 0
        vmovdqa xmm6, [rsp + 0*16]
        vmovdqa xmm7, [rsp + 1*16]
        vmovdqa xmm8, [rsp + 2*16]
        vmovdqa xmm9, [rsp + 3*16]
        vmovdqa xmm10, [rsp + 4*16]
        vmovdqa xmm11, [rsp + 5*16]
        vmovdqa xmm12, [rsp + 6*16]
        vmovdqa xmm13, [rsp + 7*16]
        vmovdqa xmm14, [rsp + 8*16]
        vmovdqa xmm15, [rsp + 9*16]
        mov     r12,  [rsp + 10*16 + 0*8]
        mov     r13,  [rsp + 10*16 + 1*8]
        add     rsp, stack_size
 %endmacro
%endif

%define len   arg0
%define vec   arg1
%define vec_i arg2
%define mul_array arg3
%define src   arg4
%define dest1 arg5
%define pos   rax
%define dest2 mul_array
%define dest3 vec_i

%ifndef EC_ALIGNED_ADDR
;;; Use Un-aligned load/store
 %define XLDR vmovdqu
 %define XSTR vmovdqu
%else
;;; Use Non-temporal load/stor
 %ifdef NO_NT_LDST
  %define XLDR vmovdqa
  %define XSTR vmovdqa
 %else
  %define XLDR vmovntdqa
  %define XSTR vmovntdq
 %endif
%endif

default rel
[bits 64]
section .text

%define x0l     ymm0
%define x0h     ymm0    ; reuse ymm0
%define xgft1   ymm1
%define xgft2   ymm2
%define xgft3   ymm3
%define xd1l    ymm4
%define xd1h    ymm5
%define xd2l    ymm6
%define xd2h    ymm7
%define xd3l    ymm8
%define xd3h    ymm9

%define xret1l  ymm10
%define xret1h  ymm11
%define xret2l  ymm12
%define xret2h  ymm13
%define xret3l  ymm14
%define xret3h  ymm15

%define x0      x0l
%define xd1     xd1l
%define xd2     xd2l
%define xd3     xd3l
%define xret1   xret1l
%define xret2   xret2l
%define xret3   xret3l

;;
;; Encodes 64 bytes of a single source into 3x 64 bytes (parity disks)
;;
%macro ENCODE_64B_3 0
        ; get next source vector
        XLDR    x0l, [src + pos]        ;; read low 32 bytes
        ; get next dest vectors
        XLDR    xd1l, [dest1 + pos]
        XLDR    xd1h, [dest1 + pos + 32]
        XLDR    xd2l, [dest2 + pos]
        XLDR    xd2h, [dest2 + pos + 32]
        XLDR    xd3l, [dest3 + pos]
        XLDR    xd3h, [dest3 + pos + 32]

        GF_MUL_XOR VEX, x0l, xgft1, xret1l, xd1l, xgft2, xret2l, xd2l, xgft3, xret3l, xd3l

        XLDR    x0h, [src + pos + 32]   ;; read high 32 bytes

        GF_MUL_XOR VEX, x0h, xgft1, xret1h, xd1h, xgft2, xret2h, xd2h, xgft3, xret3h, xd3h

        XSTR    [dest1 + pos], xd1l
        XSTR    [dest1 + pos + 32], xd1h
        XSTR    [dest2 + pos], xd2l
        XSTR    [dest2 + pos + 32], xd2h
        XSTR    [dest3 + pos], xd3l
        XSTR    [dest3 + pos + 32], xd3h
%endmacro

;;
;; Encodes 32 bytes of a single source into 3x 32 bytes (parity disks)
;;
%macro ENCODE_32B_3 0
        ; get next source vector
        XLDR    x0, [src + pos]
        ; get next dest vectors
        XLDR    xd1, [dest1 + pos]
        XLDR    xd2, [dest2 + pos]
        XLDR    xd3, [dest3 + pos]

        GF_MUL_XOR VEX, x0, xgft1, xret1, xd1, xgft2, xret2, xd2, xgft3, xret3, xd3

        XSTR    [dest1 + pos], xd1
        XSTR    [dest2 + pos], xd2
        XSTR    [dest3 + pos], xd3
%endmacro

;;
;; Encodes less than 32 bytes of a single source into 3x parity disks
;;
%macro ENCODE_LT_32B_3 1
%define %%LEN   %1
        ; get next source vector
        simd_load_avx2 x0, src + pos, %%LEN, tmp, tmp2
        ; get next dest vectors
        simd_load_avx2 xd1, dest1 + pos, %%LEN, tmp, tmp2
        simd_load_avx2 xd2, dest2 + pos, %%LEN, tmp, tmp2
        simd_load_avx2 xd3, dest3 + pos, %%LEN, tmp, tmp2

        GF_MUL_XOR VEX, x0, xgft1, xret1, xd1, xgft2, xret2, xd2, xgft3, xret3, xd3

        lea     dest1, [dest1 + pos]
        simd_store_avx2 dest1, xd1, %%LEN, tmp, tmp2
        lea     dest2, [dest2 + pos]
        simd_store_avx2 dest2, xd2, %%LEN, tmp, tmp2
        lea     dest3, [dest3 + pos]
        simd_store_avx2 dest3, xd3, %%LEN, tmp, tmp2
%endmacro

align 16
mk_global gf_3vect_mad_avx2_gfni, function
func(gf_3vect_mad_avx2_gfni)
        FUNC_SAVE

        xor     pos, pos
        shl     vec_i, 3                ;Multiply by 8
        shl     vec, 3                  ;Multiply by 8
        lea     tmp, [mul_array + vec_i]
        vbroadcastsd xgft1, [tmp]
        vbroadcastsd xgft2, [tmp + vec]
        vbroadcastsd xgft3, [tmp + vec*2]
        mov     dest2, [dest1 + 8]      ; reuse mul_array
        mov     dest3, [dest1 + 2*8]    ; reuse vec_i
        mov     dest1, [dest1]

        cmp     len, 64
        jl      .len_lt_64

.loop64:
        ENCODE_64B_3            ;; loop on 64 bytes at a time

        add     pos, 64
        sub     len, 64
        cmp     len, 64
        jge     .loop64

.len_lt_64:
        cmp     len, 32
        jl      .len_lt_32

        ENCODE_32B_3            ;; encode next 32 bytes

        add     pos, 32
        sub     len, 32

.len_lt_32:
        cmp     len, 0
        jle     .exit

        ENCODE_LT_32B_3 len     ;; encode final bytes

.exit:
        vzeroupper

        FUNC_RESTORE
        ret

endproc_frame
