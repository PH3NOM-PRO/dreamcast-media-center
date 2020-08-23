/*
**
** (C) Josh 'PH3NOM' Pearson 2011
**
*/

#ifndef LIB_FLAC_H
#define LIB_FLAC_H

#include "aica_cmd.h"
#include "fifo.h"
#include "snddrv.h"
#include "dcmc.h"

volatile int decoder_status;
#define DEC_STATUS_NULL      0x00
#define DEC_STATUS_OK        0x01
#define DEC_STATUS_PAUSING   0x02
#define DEC_STATUS_PAUSED    0x03
#define DEC_STATUS_DONE      0x04

/* Function Protocols */

#endif
