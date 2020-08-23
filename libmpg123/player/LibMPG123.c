/*
**
** This file is a part of Dreamcast Media Center
** (C) Josh PH3NOM Pearson 2011-2013
**
*/
/*
**  LibMPG123 streaming decoder (C) Josh 'PH3NOM' Pearson.
**  This decoder demonstrates the use of mpg123_open_feed() decoding.
**  In this mode, we must manually feed the input buffer.
**  This algorithm is based around decoding "Chunks" of bitstream data.
**  I.E., reading chunks of bitstream from an interleaved AVI or MPEG file.
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "stdio.h"

#include "mpg123.h"

#include "LibMPG123.h"

#include "fifo.h"
#include "snddrv.h"

/* Global variables for the LibMPG123 decoder */
static mpg123_handle *mh;
static mpg123_id3v1 *v1;
static mpg123_id3v2 *v2;

static int enc, err, MP3_DEC_STAT = 0;
static FifoBuffer abuf;                  
static snddrv_hnd *sndhnd;

static volatile int MPG123_WAIT_START = 0;
static unsigned int SAMPLES = 0;

//#define DEBUG

unsigned int LibMPG123_PcmHave()
{
    return abuf.size;
}

unsigned int LibMPG123_PcmMax()
{
    return PCM_BUF_SIZE;
}

void LibMPG123_WaitStart()
{
    MPG123_WAIT_START = 1;
}

void LibMPG123_Start()
{
    MPG123_WAIT_START = 0;
}

float LibMPG123_ATS()
{
    return ATS;
}

unsigned int LibMPG123_Rate()
{
    return sndhnd->rate;
}

unsigned int LibMPG123_Chan()
{
    return sndhnd->chan;
}

unsigned int LibMPG123_Samples()
{
    return SAMPLES;
} 
 
/* This callback will handle the AICA Driver */
static void *mpa_drv_callback( snd_stream_hnd_t hnd, int pcm_needed, int * pcm_done )
{   
#ifdef DEBUG 
    printf("MPA DRV CB - REQ %i - HAVE %i\n", pcm_needed, abuf.size ); 
#endif
    while(MPG123_WAIT_START) thd_pass();

    while( abuf.size < pcm_needed )
    {
        //printf("HAVE: %i - REQ: %i\n", abuf.size, pcm_needed );
        MP3_NEED_SAMPLES = 1;
        if(!MP3_DEC_STAT)
        {
            memset( sndhnd->drv_buf, 0, pcm_needed );
            sndhnd->drv_ptr = sndhnd->drv_buf;
            *pcm_done = pcm_needed;
            
            return sndhnd->drv_ptr;
        }
        thd_pass();
    }
    MP3_NEED_SAMPLES = 0;
    
    SAMPLES += pcm_needed;
    ATS = (float)SAMPLES/sndhnd->rate/sndhnd->chan/2.0f;
    
    MPA_lock_mutex()
    memcpy( sndhnd->drv_buf, abuf.buf, pcm_needed );           
    abuf.size -= pcm_needed;
    memmove( abuf.buf, abuf.buf+pcm_needed, abuf.size );
    MPA_unlock_mutex()
    
    sndhnd->drv_ptr = sndhnd->drv_buf;
    *pcm_done = pcm_needed;    
    
    return sndhnd->drv_ptr; 
}

static void mpa_drv_thd()
{
    snddrv_hnd_start( sndhnd );   
    while(sndhnd->stat != SNDDRV_STATUS_NULL)
    {
        snddrv_hnd_cb( sndhnd );
        thd_sleep(80);    
    } 
}

void LibMPG123_VolumeUp()
{
    snddrv_hnd_volume_up( sndhnd );
}

void LibMPG123_VolumeDown()
{
    snddrv_hnd_volume_down( sndhnd );
}

static ID3_Data id3;

void LibMPG123_PrintID3()
{    
	if(v1!=NULL)
	{
        sprintf(id3.track,  " Track: %s", v1->title );
	    sprintf(id3.artist, "Artist: %s", v1->artist );
	    sprintf(id3.album,  " Album: %s", v1->album );
    }
	if(v2!=NULL)
	{
        sprintf(id3.track,  " Track: %s", v2->title );
	    sprintf(id3.artist, "Artist: %s", v2->artist );
	    sprintf(id3.album,  " Album: %s", v2->album );
	    sprintf(id3.genre,  " Genre: %s", v2->genre);
    }
    printf("%s\n", id3.track);
    printf("%s\n", id3.artist);
    printf("%s\n", id3.album);
    if(v2!=NULL)
    printf("%s\n", id3.genre);
}

	
/* Decode a chunk of MPEG Audio, given a bitstream buffer and its size */
int LibMPG123_DecodeChunk( uint8 * mpabuf, int size )
{
    MPA_lock_mutex()
    err = mpg123_decode(mh, mpabuf, size, abuf.buf+abuf.size, 163840, &sndhnd->samples_done);
    abuf.size+=sndhnd->samples_done;
    MPA_unlock_mutex()
    
    switch( err )
    {
        case MPG123_NEED_MORE:   /* Status OK - No need to waste time */
             break;
                 
        case MPG123_ERR:         /* Check for MPG123 Error Code */
             printf("err = %s\n", mpg123_strerror(mh));
             return 0;
     
        case MPG123_NEW_FORMAT:  /* First frame initialize structures */
             /*
             {
                int meta = 0;
                meta = mpg123_meta_check(mh);
                if(meta & MPG123_ID3 && mpg123_id3(mh, &v1, &v2) == MPG123_OK)
	                LibMPG123_PrintID3();
             } */ // Spits out garbage so for now, comment this bitch out
             
             err = mpg123_getformat(mh, &sndhnd->rate, &sndhnd->chan, &enc);

             while(err!=MPG123_NEED_MORE && err!=MPG123_ERR )
             { 

               MPA_lock_mutex()                            
               err = mpg123_decode(mh, NULL, 0, abuf.buf+abuf.size, 163840, &sndhnd->samples_done);
               abuf.size+=sndhnd->samples_done;
               MPA_unlock_mutex()
             } 
             /* Start the AICA Driver */
             sndhnd->drv_cb = mpa_drv_callback;
             thd_create( mpa_drv_thd, NULL );
    }
     
    return 1;    
}

/* Exit LibMPG123 */
int LibMPG123_Exit( )
{
    MP3_DEC_STAT = 0; thd_sleep(50);
    snddrv_hnd_exit( sndhnd );               /* Exit the AICA Driver */
    free( sndhnd );
    
    free(abuf.buf);                 /* Reset the resources */
    enc=err=0;
                  
    mpg123_close(mh);               /* Release the MPG123 handle */    
    mpg123_exit();  
       
    MPA_destroy_mutex()
    
    return 1; 
}

/* Initialize LibMPG123 -  Do this before calling LibMPG123_decode_chunk() */
int LibMPG123_Init( )
{
    MP3_DEC_STAT = 1;
    abuf.buf=malloc( PCM_BUF_SIZE );                   /* Allocate PCM Buffer */
    abuf.size=0;
    SAMPLES = 0;
    
    sndhnd = malloc( sizeof( snddrv_hnd ) );  /* Allocate Sound Driver Handle */
    memset( sndhnd, 0 , sizeof(snddrv_hnd) );
   
    mpg123_init();                            /* Initialize the MPG123 handle */
    mh = mpg123_new(NULL, &err);
    assert(mh != NULL);
       
    err = mpg123_open_feed(mh);           /* Start LibMPG123 in open_feed mode */
    assert(err == MPG123_OK);   
    
    MPA_create_mutex()
    
    MP3_NEED_SAMPLES = ATS = SAMPLES = 0;
    v1 = v2 = NULL;
    
    return 1;
}
