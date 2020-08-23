/*
**
** flac_dec.c (C) Josh 'PH3NOM' Pearson 2011
**
*/
/* example_c_decode_file - Simple FLAC file decoder using libFLAC
 * Copyright (C) 2007  Josh Coalson
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*
 * This example shows how to use libFLAC to decode a FLAC file to a WAVE
 * file.  It only supports 16-bit stereo files.
 *
 * Complete API documentation can be found at:
 *   http://flac.sourceforge.net/api/
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include "FLAC/stream_decoder.h"

#include "LibFLAC.h"

static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
static void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);

static FLAC__uint64 total_samples = 0;
static unsigned sample_rate = 0;
static unsigned channels = 0;
static unsigned bps = 0;

FLAC__uint32 total_size, total_dec_size;
FLAC__StreamDecoder *decoder;
FLAC__StreamDecoderInitStatus init_status;
FLAC__bool ok;
char FLAC__file[256];

static snddrv_hnd *sndhnd;
static FifoBuffer * pcm_buf;

static Font * FONT;
static DirectoryEntry * fs_entry;

static int LibFLAC_RenderFrame();

/* This callback will handle the AICA Driver */
static void *flac_drv_callback( snd_stream_hnd_t hnd, int pcm_needed, int * pcm_done )
{   
    while( pcm_buf->size < pcm_needed )
        thd_pass();
    
    SNDDRV_lock_mutex()
    memcpy( sndhnd->drv_buf, pcm_buf->buf, pcm_needed );
    pcm_buf->size -= pcm_needed;
    memmove( pcm_buf->buf, pcm_buf->buf+pcm_needed, pcm_buf->size );
    SNDDRV_unlock_mutex()
        
    sndhnd->drv_ptr = sndhnd->drv_buf;
    *pcm_done = pcm_needed;   
        
    return sndhnd->drv_ptr;
}

static void flac_drv_thd()
{
    snddrv_hnd_start( sndhnd );   
    while(sndhnd->stat != SNDDRV_STATUS_NULL)
    {
        snddrv_hnd_cb( sndhnd );
        thd_sleep(50);    
    }
}

/* Callback from FLAC decoder */
FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
    static FLAC__int8 s8buffer[FLAC__MAX_BLOCK_SIZE * FLAC__MAX_CHANNELS * sizeof(FLAC__int32)]; /* WATCHOUT: can be up to 2 megs */
    FLAC__int16  *s16buffer = (FLAC__int16  *)s8buffer;
    unsigned wide_samples = frame->header.blocksize, wide_sample, sample;
       
    /* Mix the left and right buffers into a stereo stream */
    for(sample = wide_sample = 0; wide_sample < wide_samples; wide_sample++) {
         s16buffer[sample++] = (FLAC__int16)(buffer[0][wide_sample]);
         s16buffer[sample++] = (FLAC__int16)(buffer[1][wide_sample]);
     }
    
	(void)decoder;

	if(total_samples == 0) {
		fprintf(stderr, "ERROR: this example only works for FLAC files that have a total_samples count in STREAMINFO\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
	if(channels != 2 || bps != 16) {
		fprintf(stderr, "ERROR: this example only supports 16bit stereo streams\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
    
    unsigned int bytes = frame->header.blocksize * sizeof(FLAC__int32);
    total_dec_size += bytes;
    
    SNDDRV_lock_mutex()
    memcpy(pcm_buf->buf+pcm_buf->size, s16buffer, bytes );
    pcm_buf->size += bytes;
    SNDDRV_unlock_mutex()
    
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

/* If metadata frames are decoded */
void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	(void)decoder, (void)client_data;

	/* print some stats */
	if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
		/* save for later */
		total_samples = metadata->data.stream_info.total_samples;
		sample_rate = metadata->data.stream_info.sample_rate;
		channels = metadata->data.stream_info.channels;
		bps = metadata->data.stream_info.bits_per_sample;
  
	}

    total_size = (FLAC__uint32)(total_samples * channels * (bps/8));

}

/* In case some decoder error occurs */
void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	(void)decoder, (void)client_data;

	fprintf(stderr, "Got error callback: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}

/* Decode the stream in its own thread */
static int flac_callback( ) {
        
	ok = true;
	decoder = 0;
            
	if((decoder = FLAC__stream_decoder_new()) == NULL) {
		//fprintf(stderr, "ERROR: allocating decoder\n");
		return 1;
	}
    
	(void)FLAC__stream_decoder_set_md5_checking(decoder, true);

	init_status = FLAC__stream_decoder_init_file(decoder, FLAC__file, write_callback, metadata_callback, error_callback, /*client_data=*/ NULL/*pcm_buf*/);
	if(init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
		//fprintf(stderr, "ERROR: initializing decoder: %s\n", FLAC__StreamDecoderInitStatusString[init_status]);
		ok = false;
	}

    FLAC__stream_decoder_process_single(decoder);
		
	return 0;
}

unsigned int LibFLAC_PcmHave()
{
    return pcm_buf->size;
}  

static int LibFLAC_RenderFrame()
{
    glKosBeginFrame(); /* Now, Lets render some stuff */
    
    DCMC_RenderGlTexture( DCMC_TexID(BgndMusic), 0,60,640,360 );
    
    DCMC_RenderTbn( fs_entry, 220,80,200,320);

    float width = cstr_len(fs_entry->Name)*16.0f;
    
    float x = ((vid_mode->width)/2.0f)-(width/2.0f);

    glBindTexture( GL_TEXTURE_2D, FONT->TexId );  
    glBegin(GL_QUADS);
        FontPrintString( FONT, fs_entry->Name, x, 436.0f, 16.0f, 16.0f );
    glEnd();
    
    glKosFinishFrame();

    Input cont; /* Handle User Input */
    DCE_SetInput( &cont, 0 );
    DCE_GetInput( &cont );
        
    if(cont.rt>0)
        AICA_VolumeIncrease(sndhnd->chan==2 ? AICA_STEREO : AICA_MONO);
    if(cont.lt>0)
        AICA_VolumeDecrease(sndhnd->chan==2 ? AICA_STEREO : AICA_MONO);
    if(cont.st)
        return 1;
        
    return 0;
}

int LibFLAC_Decode( char * flac_file, Font * font, DirectoryEntry * entry  )
{   
    /* Start the FLAC stream */
    sprintf( FLAC__file, "%s", flac_file );
    FONT = font;
    fs_entry = entry;
    
    flac_callback( ); 
            
    /* Wait for the stream to start */
    while( sample_rate < 1 && channels < 1 )
        FLAC__stream_decoder_process_single(decoder); 

    printf("\nFLAC - File Attributes:\n\n");
	printf("sample rate    : %u Hz\n", sample_rate);
	printf("channels       : %u\n", channels);

    pcm_buf = malloc( sizeof(FifoBuffer) );
    memset( pcm_buf, 0, sizeof(FifoBuffer) );
    pcm_buf->buf = malloc(PCM_BUF_SIZE);

    sndhnd = malloc( sizeof( snddrv_hnd ) );
    memset( sndhnd, 0 , sizeof(snddrv_hnd) );
    
    sndhnd->rate = sample_rate;
	sndhnd->chan = channels;
	sndhnd->drv_cb = flac_drv_callback;
    
    SNDDRV_create_mutex()
      
    thd_create( flac_drv_thd, NULL );
    
    total_dec_size = 0;
    
	/* Decode the stream untill EOF or flac_stop is called */
    while ( !LibFLAC_RenderFrame() )
    {    
        /* Check for EOF  */
        if( total_dec_size >= total_size && total_size > 0 )
            break; 
        if(LibFLAC_PcmHave() < 65535*2 )
            FLAC__stream_decoder_process_single(decoder);		
    }

    snddrv_hnd_exit( sndhnd );               /* Exit the AICA Driver */
    free( sndhnd ); sndhnd = NULL;
    free(pcm_buf->buf);               
    free(pcm_buf);

    /* Exit the Decoder  */   
    FLAC__stream_decoder_finish(decoder);
	FLAC__stream_decoder_delete(decoder);   

    /* Free the other resources */
    total_dec_size = total_size = total_samples = 0;
    sample_rate = channels =  bps = 0;
    sq_clr( FLAC__file, 256 );
    
    return 0;
}
