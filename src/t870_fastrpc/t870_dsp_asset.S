    .section .rodata.t870_dsp,"a",@progbits
    .balign 16
    .global t870_dsp_blob_start
    .type t870_dsp_blob_start, %object
t870_dsp_blob_start:
    .incbin "src/t870_fastrpc/libt870_map_collision_skel.so"
    .global t870_dsp_blob_end
    .type t870_dsp_blob_end, %object
t870_dsp_blob_end:
    .size t870_dsp_blob_start, t870_dsp_blob_end - t870_dsp_blob_start

    .section .note.GNU-stack,"",@progbits
