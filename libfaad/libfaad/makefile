# LibFAAD v.2.27
# Dreamcast K:OS Makefile (C) PH3NOM 2011

TARGET = libfaad_2.27.a

KOS_CFLAGS += -I. -I../include

OBJS = bits.o cfft.o decoder.o drc.o \
		     drm_dec.o error.o filtbank.o \
		     ic_predict.o is.o lt_predict.o mdct.o mp4.o ms.o output.o pns.o \
		     ps_dec.o ps_syntax.o \
		     pulse.o specrec.o syntax.o tns.o hcr.o huffman.o \
		     rvlc.o ssr.o ssr_fb.o ssr_ipqf.o common.o \
		     sbr_dct.o sbr_e_nf.o sbr_fbt.o sbr_hfadj.o sbr_hfgen.o \
		     sbr_huff.o sbr_qmf.o sbr_syntax.o sbr_tf_grid.o sbr_dec.o \
		     analysis.h bits.h cfft.h cfft_tab.h common.h \
#		     decoder.h drc.h drm_dec.h error.h fixed.h filtbank.h \
		     drc.h drm_dec.h error.h fixed.h filtbank.h \
		     huffman.h ic_predict.h iq_table.h is.h kbd_win.h lt_predict.h \
		     mdct.h mdct_tab.h mp4.h ms.h output.h pns.h ps_dec.h ps_tables.h \
		     pulse.h rvlc.h \
		     sbr_dct.h sbr_dec.h sbr_e_nf.h sbr_fbt.h sbr_hfadj.h sbr_hfgen.h \
		     sbr_huff.h sbr_noise.h sbr_qmf.h sbr_syntax.h sbr_tf_grid.h \
		     sine_win.h specrec.h ssr.h ssr_fb.h ssr_ipqf.h \
		     ssr_win.h syntax.h structs.h tns.h \
		     sbr_qmf_c.h codebook/hcb.h \
		     codebook/hcb_1.h codebook/hcb_2.h codebook/hcb_3.h codebook/hcb_4.h \
		     codebook/hcb_5.h codebook/hcb_6.h codebook/hcb_7.h codebook/hcb_8.h \
		     codebook/hcb_9.h codebook/hcb_10.h codebook/hcb_11.h codebook/hcb_sf.h

include $(KOS_BASE)/addons/Makefile.prefab
