/*
** XviD-Play (C) Josh PH3NOM Pearson 2010-2013
**
** This application has evolved quite a bit since I first started work on it.
** After testing various methods, I believe now things are quite optimised.
** In this design, we are using 3 threads
** -Main Thread: Read BitStream Packets from AVI file and Deocde into Output Buffer
** -Audio Thread: Wait for Samples available in output buffer, then stream to AICA
** -Video Thread: Wait for Video Frames available in output buffer, then render to PVR
**
** The big advantange here is that the Main Thread decodes into a rather large
** output buffer, only stalling the decode routine when the output buffers are full.
** This means the decoder can stay well ahead of the current frame being rendered.
**
** Before compiling this, first all of the libs need to be compiled:
   -LibXviD   ( XviD-DC\libxvid\src\makefile )
   -LibMPG123 ( XviD-DC\libmpg123\libmpg123\makefile )
   -LibA52    ( XviD-DC\liba52\liba52\makefile )
   -GL        ( XviD-DC\GL\makefile )
*/

#include "malloc.h"
#include "stdio.h"
#include "string.h"

#include "XviD.h"
#include "LibXVID.h"
#include "LibMPG123.h"
#include "LibA52.h"
#include "LibAVI.h"
#include "Container.h"
#include "Fifo.h"
#include "Input.h"
#include "Render.h"
#include "aica_cmd.h"

#include "Font.h"

/* XviD Structures */
static xvid_dec_stats_t xvid_dec_stats;
static xvid_dec_frame_t xvid_dec_frame;
static void *dec_handle = NULL;
static int XDIM = 0, YDIM = 0;
static unsigned int useful_bytes;

/* AVI File Data */
static FILE * av;
static FifoBuffer *xvid_vbuf;
static FOURCCHDR * fcchdr;
static AVIHeader * avihdr;
static FifoBuffer vbuf = {0,0};
static unsigned int ACODEC;
static float FRAME_TS, AVI_video_rate;
static volatile int XVID_STAT = 0;
static int chunk_type;

/* XviD Bitstream Buffer */
static unsigned char *out_buffer = NULL;
static unsigned char *mp4_ptr = NULL;

static Font * font;



/* XviD Decoder Functions *****************************************************/
     
/* Close decoder to release resources */
static int xvid_stop()
{         
	int ret = xvid_decore(dec_handle, XVID_DEC_DESTROY, NULL, NULL);
	return(ret);
}

/* Init xvid decoder before first run */
static int xvid_dec_init( int use_assembler, int debug_level, int width, int height )
{       
	int ret;

	xvid_gbl_init_t   xvid_gbl_init;
	xvid_dec_create_t xvid_dec_create;

	/* Reset the structure */
	memset(&xvid_gbl_init, 0, sizeof(xvid_gbl_init_t));
	memset(&xvid_dec_create, 0, sizeof(xvid_dec_create_t));

	/* XviD core initialization */
	xvid_gbl_init.version = XVID_VERSION;
	xvid_gbl_init.cpu_flags = XVID_CPU_FORCE;	
    xvid_gbl_init.debug = debug_level;
	xvid_global(NULL, 0, &xvid_gbl_init, NULL);
	xvid_dec_create.version = XVID_VERSION;

	/* Image dimensions */
	xvid_dec_create.width = width;
	xvid_dec_create.height = height;

	ret = xvid_decore(NULL, XVID_DEC_CREATE, &xvid_dec_create, NULL);

	dec_handle = xvid_dec_create.handle;

	return(ret);
}

/* Save some XviD structures before first run */
static void xvid_frame_init( unsigned char *ostream, xvid_dec_stats_t *xvid_dec_stats )
{
	/* Reset all structures */
	memset(&xvid_dec_frame, 0, sizeof(xvid_dec_frame_t));
	memset(xvid_dec_stats, 0, sizeof(xvid_dec_stats_t));

	/* Set version */ 
	xvid_dec_frame.version = XVID_VERSION;
    xvid_dec_stats->version = XVID_VERSION;
    	
    /* General flags to set */ 
	xvid_dec_frame.general          = 0;
	
	/* Output frame structure */ 
	xvid_dec_frame.output.plane[0]  = ostream;
	xvid_dec_frame.output.stride[0] = XDIM*2;
	xvid_dec_frame.output.csp = XVID_CSP_UYVY;  
}

void xvid_print_info()
{
    printf(":========================================\n");
    printf(": XviD-DC - A Sega Dreamcast XviD Player\n");
    printf(": (c) Josh 'PH3NOM' Pearson 2010-2013\n");
    printf(":========================================\n");
    printf(": Video Decoder: libXviD build 1.3.0\n");
    printf(":========================================\n");
    printf(": LibAVI Information :\n");
    printf(":  VIDEO Resolution  : %ix%i\n", XDIM , YDIM );
	printf(":  VIDEO FrameRate   : %.3f fps\n", AVI_video_rate );
    printf(":  VIDEO Length : %.2f secconds\n", avihdr->dwTotalFrames / AVI_video_rate );
    if( ACODEC == ACODEC_AC3 )
       printf(":  AUDIO CODEC : LibA52 - Dolby Digital\n" );
    else if( ACODEC == ACODEC_MP3 )
       printf(":  AUDIO CODEC : LibMPG123 - Mpeg Audio\n" );
    printf(":  AUDIO Channels    : %i\n", avihdr->dwAudio->nChannels );
	printf(":  AUDIO Frequency   : %i\n", avihdr->dwAudio->nSamplesPerSec);
    printf(":========================================\n");
}

static int XVID_STREAM_INIT( char * avifile )
{
    /* Open the AVI file */
    av = fopen( avifile, "rb");
    if( av == NULL )
    {  
       printf("LibXVID: FILE I/O ERROR\n");
       return 0;
    }  
     
    avihdr = LibAVI_open( av );

    if( avihdr->dwWidth < 1 || avihdr->dwWidth > 1024 
        || avihdr->dwHeight < 1 || avihdr->dwHeight > 1024 )
    {
        printf("LibXVID: AVI Dimension ERROR\n");
        return 0;
    }
    
    /* Get the nedded specifications of the AVI file */
    AVI_video_rate = micro2fps(avihdr->dwMicroSecPerFrame);
    FRAME_TS = avihdr->dwMicroSecPerFrame/1000.0f;
    ACODEC = avihdr->dwAudio->wFormatTag;         
    XDIM = avihdr->dwWidth;
    YDIM = avihdr->dwHeight;    
    
    /* Set up the buffers for the en/decoded video packet */
    if( avihdr->dwSuggestedBufferSize>1024 )
      vbuf.buf = malloc(avihdr->dwSuggestedBufferSize);  
    else
      vbuf.buf = malloc(720*480*2);   
    out_buffer = memalign(32, XDIM*YDIM*2); 
    fcchdr     = malloc(sizeof(FOURCCHDR));
    xvid_vbuf = &vbuf;
        
    /* Initialize the XviD deocder */
	int status = xvid_dec_init(0, 0, XDIM, YDIM);
	if( status )
    {
		printf("Decore INIT problem, return value %d\n", status);			
  	    if (dec_handle)
        {
	  	   status = xvid_stop();
		   if( status )    
			  printf("decore RELEASE problem return value %d\n", status);
        }
	}    

    /* Initialize the XviD frame output */
    xvid_frame_init(out_buffer, &xvid_dec_stats);        
    xvid_print_info();    
    
    /* Initialize Audio Decoder in a Wait-to-Start mode for synchronization */  
    switch(ACODEC)
    {
        case ACODEC_AC3:
             LibA52_WaitStart();
             LibA52_Init( avihdr->dwAudio->nSamplesPerSec,
                          avihdr->dwAudio->nChannels > 2 ?
                          2 : avihdr->dwAudio->nChannels );
             break;
             
        case ACODEC_MP3:
             LibMPG123_WaitStart();
             LibMPG123_Init();
             break;
    }

    /* Initialize the Render Routine */
    RenderInit( XDIM, YDIM, PVR_TXRFMT_YUV422 | PVR_TXRFMT_NONTWIDDLED,
                ACODEC == ACODEC_AC3 ? AUDIO_CODEC_AC3 : AUDIO_CODEC_MPEG,
                FRAME_TS, font );
    
    return 1;
}

static int XVID_INIT( char * avifile )
{
    /* Open the AVI file */
    av = fopen( avifile, "rb");
    if( av == NULL )
    {  
       printf("LibXVID: FILE I/O ERROR\n");
       return 0;
    }  
     
    avihdr = LibAVI_open( av );

    if( avihdr->dwWidth < 1 || avihdr->dwWidth > 1024 
        || avihdr->dwHeight < 1 || avihdr->dwHeight > 1024 )
    {
        printf("LibXVID: AVI Dimension ERROR\n");
        return 0;
    }
    
    /* Get the nedded specifications of the AVI file */
    AVI_video_rate = micro2fps(avihdr->dwMicroSecPerFrame);    
    XDIM = avihdr->dwWidth;
    YDIM = avihdr->dwHeight;    
    
    /* Set up the buffers for the en/decoded video packet */
    if( avihdr->dwSuggestedBufferSize>1024 )
      vbuf.buf = malloc(avihdr->dwSuggestedBufferSize);  
    else
      vbuf.buf = malloc(720*480*2);   
    out_buffer = memalign(32, XDIM*YDIM*2); 
    fcchdr     = malloc(sizeof(FOURCCHDR));
    xvid_vbuf = &vbuf;
        
    /* Initialize the XviD deocder */
	int status = xvid_dec_init(0, 0, XDIM, YDIM);
	if( status )
    {
		printf("Decore INIT problem, return value %d\n", status);			
  	    if (dec_handle)
        {
	  	   status = xvid_stop();
		   if( status )    
			  printf("decore RELEASE problem return value %d\n", status);
        }
	}    

    /* Initialize the XviD frame output */
    xvid_frame_init(out_buffer, &xvid_dec_stats);        
    //xvid_print_info();    
    
    return 1;
}

/* Decode a single frame */
static inline void LibXVID_DecodeFrame( )
{ 
     int used_bytes = 0;       
     do{                       
	    xvid_dec_frame.bitstream = mp4_ptr;      
        xvid_dec_frame.length    = useful_bytes; 
        used_bytes = xvid_decore(dec_handle, XVID_DEC_DECODE, &xvid_dec_frame, &xvid_dec_stats); 
		if(used_bytes > 0) {            
		    mp4_ptr += used_bytes;      
		    useful_bytes -= used_bytes; 
		} 
     } while (xvid_dec_stats.type <= 0 && useful_bytes > 1); 
}

/* Main Decoder Thread */
static void XVID_STREAM_DECODE()
{
    XVID_STAT   = 1;
    unsigned int frame=0;
    
    RenderStart();
         
    while( XVID_STAT )
    {          
        chunk_type = LibAVI_read_chunk( av, fcchdr, (unsigned int *)xvid_vbuf->buf );

        if(fcchdr->dwSize<1)
        {
            XVID_STAT=0;
            continue;
        }
            
        switch( chunk_type )
        {
            case AVI_CHUNK_AUDS:                 
                 
                 if(++frame%2==0)
                     if(HandleInput((avihdr->dwAudio->nChannels==2) ?
                              AICA_STEREO : AICA_MONO ))
                     {
                         XVID_STAT = 0;
                         continue;   
                     }
                 switch( ACODEC )
                 {   
                     case ACODEC_AC3:
                          LibA52_decode_chunk( xvid_vbuf->buf, xvid_vbuf->buf + fcchdr->dwSize );
                          break;
                         
                     case ACODEC_MP3:
                          //{
                          //unsigned int st = GetTime();
                          LibMPG123_DecodeChunk( xvid_vbuf->buf, fcchdr->dwSize );
                          //printf("MP3 Chunk Time: %i\n", GetTime()-st);
                          //}
                          break;     
                 }
                 break;
                
            case AVI_CHUNK_VIDS:                                         
                 useful_bytes = fcchdr->dwSize;
                 mp4_ptr = (unsigned char*)xvid_vbuf->buf;
                 
                 LibXVID_DecodeFrame();
                                  
                 RenderPush(out_buffer);
                 
                 break;
                     
            default:
                XVID_STAT=0;
                return;
        }            
    }
    printf("Xvid Main Loop Complete\n");
}

static void XVID_STREAM_DESTROY()
{
    xvid_stop();
    
    printf("Xvid Stop()\n");
    
    RenderDestroy();
    
    printf("Render Destroy()\n"); 
    
    switch(ACODEC)
    {
        case ACODEC_AC3:
             LibA52_Exit();
             break;
             
        case ACODEC_MP3:
             LibMPG123_Exit();
             break;
    }
    
    printf("Audio Decoder Destroyed\n");

    if(out_buffer) free(out_buffer);
    out_buffer=NULL;
    if(vbuf.buf) free(vbuf.buf);
    vbuf.buf=NULL;
    if(fcchdr) free(fcchdr);
    fcchdr=NULL;
}

unsigned char LibXVID_Decode( char * avifile, Font * f )
{ 
    font = f;
    if(!XVID_STREAM_INIT(avifile))
    {
        printf("LibXVID ERROR: Unable to open avi file\n");
        return 0;
    }
    
    XVID_STREAM_DECODE();

    XVID_STREAM_DESTROY();
}
