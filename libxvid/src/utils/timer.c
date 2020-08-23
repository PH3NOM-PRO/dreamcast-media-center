/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Timer functions (used for internal debugging)  -
 *
 *  Copyright(C) 2002 Michael Militzer <isibaar@xvid.org>
 *
 *  This program is free software ; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation ; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY ; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program ; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * $Id: timer.c,v 1.9 2004/03/22 22:36:24 edgomez Exp $
 *
 ****************************************************************************/

#include <stdio.h>
#include <time.h>
#include "timer.h"

#if defined(_PROFILING_)
/*
    determine cpu frequency
	not very precise but sufficient
*/
double
get_freq()
{
	return (double) 200.0;
}

/* set everything to zero */
void
init_timer()
{
}

void
start_timer()
{
}

void
start_global_timer()
{
}

void
stop_dct_timer()
{
}

void
stop_idct_timer()
{
}

void
stop_quant_timer()
{
}

void
stop_iquant_timer()
{
}

void
stop_motion_timer()
{
}

void
stop_comp_timer()
{
}

void
stop_edges_timer()
{
}

void
stop_inter_timer()
{
}

void
stop_conv_timer()
{
}

void
stop_transfer_timer()
{
}

void
stop_prediction_timer()
{
}

void
stop_coding_timer()
{
}

void
stop_interlacing_timer()
{
}

void
stop_global_timer()
{
}

/*
    write log file with some timer information
*/
void
write_timer()
{
}

#endif
