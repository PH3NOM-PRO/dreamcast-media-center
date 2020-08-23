# LibXviD KOS Makefile (C) PH3NOM 2011

KOS_CFLAGS += -I. -I../

OBJS += decoder.o
OBJS += xvid.o
OBJS += bitstream/bitstream.o
OBJS += bitstream/cbp.o
OBJS += bitstream/mbcoding.o
OBJS += dct/fdct.o
OBJS += dct/idct.o
OBJS += dct/simple_idct.o
OBJS += image/colorspace.o
OBJS += image/font.o
OBJS += image/image.o
OBJS += image/interpolate8x8.o
OBJS += image/postprocessing.o
OBJS += image/qpel.o
OBJS += image/reduced.o
OBJS += motion/estimation_bvop.o
OBJS += motion/estimation_common.o
OBJS += motion/estimation_gmc.o
OBJS += motion/estimation_pvop.o
OBJS += motion/estimation_rd_based.o
OBJS += motion/estimation_rd_based_bvop.o
OBJS += motion/gmc.o
OBJS += motion/motion_comp.o
OBJS += motion/sad.o
OBJS += motion/vop_type_decision.o
OBJS += prediction/mbprediction.o
OBJS += quant/quant_h263.o
OBJS += quant/quant_matrix.o
OBJS += quant/quant_mpeg.o
OBJS += utils/emms.o
OBJS += utils/mbtransquant.o
OBJS += utils/mem_align.o
OBJS += utils/mem_transfer.o
OBJS += utils/timer.o

INCS += -I. -DLITTLE_ENDIAN=1
CFLAGS += -O3 -m4-single-only -ffast-math

all: libxvidcore_1.3.0.a

libxvidcore_1.3.0.a: $(OBJS)
	$(KOS_AR) rcs libxvidcore_1.3.0.a $(OBJS)

include $(KOS_BASE)/Makefile.rules
