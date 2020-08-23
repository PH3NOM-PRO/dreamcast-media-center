# LibFLAC for Dreamcast/KOS makefile by PH3NOM 2011

# Global header file includes 
KOS_CFLAGS += -I. -I../../include
KOS_CFLAGS += -I. -Iinclude

# Optimizations for Dreamcast compiler
KOS_CFLAGS += -O3 -m4-single-only -DDREAMCAST

# Were building LibFLAC as a static library
TARGET = libflac.a 

# Standard C version of LibFLAC
OBJS += bitmath.o
OBJS += bitreader.o
OBJS += bitwriter.o
OBJS += cpu.o
OBJS += crc.o
OBJS += fixed.o
OBJS += float.o
OBJS += format.o
OBJS += lpc.o
OBJS += md5.o
OBJS += memory.o
OBJS += metadata_iterators.o
OBJS += metadata_object.o
OBJS += ogg_decoder_aspect.o
OBJS += ogg_encoder_aspect.o
OBJS += ogg_helper.o
OBJS += ogg_mapping.o
OBJS += stream_decoder.o
OBJS += stream_encoder.o
OBJS += stream_encoder_framing.o
OBJS += window.o

include $(KOS_BASE)/addons/Makefile.prefab 