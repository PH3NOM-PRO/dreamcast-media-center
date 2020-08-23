/*
**
** This file is a part of Dreamcast Media Center
** (C) Josh PH3NOM Pearson 2011-2013
**
*/
/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2005 M. Bakker, Nero AG, http://www.nero.com
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** The "appropriate copyright message" mentioned in section 2c of the GPLv2
** must read: "Code from FAAD2 is copyright (c) Nero AG, www.nero.com"
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Nero AG through Mpeg4AAClicense@nero.com.
**
** $Id: main.c,v 1.85 2008/09/22 17:55:09 menno Exp $
**/

#include <time.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <neaacdec.h>
#include <mp4ff.h>

#include "LibFAAD.h"

/* Had to make a few things global */
static FILE *mp4File;
static mp4ff_callback_t *mp4cb;
static int quiet = 1;

static FifoBuffer abuf;                  
static snddrv_hnd *sndhnd;
static ID3_Data id3;

static unsigned char FAAD_NEED_SAMPLES, FAAD_DEC_STAT;
static unsigned int  FAAD_SAMPLES_DONE;
static Font * FONT;
static DirectoryEntry * fs_entry;

static int LibFAAD_RenderFrame();

void LibFAAD_Exit()
{
    FAAD_DEC_STAT = 0;
    while(FAAD_NEED_SAMPLES) thd_pass();
    thd_sleep(50);
}

void LibFAAD_Status()
{
    return FAAD_DEC_STAT;
}     

/* This callback will handle the AICA Driver */
static void *faad_drv_callback( snd_stream_hnd_t hnd, int pcm_needed, int * pcm_done )
{   
#ifdef DEBUG 
    printf("MPA DRV CB - REQ %i - HAVE %i\n", pcm_needed, abuf.size ); 
#endif

    while( abuf.size < pcm_needed && sndhnd->stat  )
    {
        FAAD_NEED_SAMPLES = 1;
        if(!FAAD_DEC_STAT)//Exit
        {
            memset( sndhnd->drv_buf, 0, pcm_needed );
            sndhnd->drv_ptr = sndhnd->drv_buf;
            *pcm_done = pcm_needed;
            
            return sndhnd->drv_ptr;
        }
        thd_pass();
    }
    FAAD_NEED_SAMPLES = 0;
    
    FAAD_SAMPLES_DONE += pcm_needed;
    ATS = (float)FAAD_SAMPLES_DONE/sndhnd->rate/sndhnd->chan/2.0f;
    
    SNDDRV_lock_mutex()
    memcpy( sndhnd->drv_buf, abuf.buf, pcm_needed );           
    abuf.size -= pcm_needed;
    memmove( abuf.buf, abuf.buf+pcm_needed, abuf.size );
    SNDDRV_unlock_mutex()
    
    sndhnd->drv_ptr = sndhnd->drv_buf;
    *pcm_done = pcm_needed;    
    
    return sndhnd->drv_ptr; 
}

static void faad_drv_thd()
{
    snddrv_hnd_start( sndhnd );   
    while(sndhnd->stat != SNDDRV_STATUS_NULL)
    {
        snddrv_hnd_cb( sndhnd );
        thd_sleep(30);    
    } 
    printf("Faad Driver Thread Complete\n");
}

static void faad_drv_stream( unsigned char *buf, unsigned int size )
{
    SNDDRV_lock_mutex()
    memcpy( abuf.buf+abuf.size, buf, size );
    abuf.size += size;
    SNDDRV_unlock_mutex()
}

static void faad_drv_free()
{
    snddrv_hnd_exit( sndhnd );               /* Exit the AICA Driver */
    
    free( sndhnd );
    sndhnd = NULL;
    
    free(abuf.buf);                 /* Reset the resources */
    abuf.size=0;
    
    FAAD_DEC_STAT = FAAD_NEED_SAMPLES = FAAD_SAMPLES_DONE = 0;
    
    SNDDRV_destroy_mutex()
    
    printf("Driver Free and shnd exited\n");
}

static void faad_drv_init( unsigned int rate, unsigned int chan )
{
    SNDDRV_create_mutex()
     
    FAAD_NEED_SAMPLES = FAAD_SAMPLES_DONE = 0;
    FAAD_DEC_STAT = 1;
    
    abuf.buf  = malloc( PCM_BUF_SIZE );                /* Allocate PCM Buffer */
    abuf.size = 0;

    sndhnd = malloc( sizeof( snddrv_hnd ) );  /* Allocate Sound Driver Handle */
    memset( sndhnd, 0 , sizeof(snddrv_hnd) );
        
    sndhnd->rate = rate;
    sndhnd->chan = chan;
    sndhnd->drv_cb = faad_drv_callback;
    
    thd_create( faad_drv_thd, NULL );

    thd_sleep(20);
}

static void faad_fprintf(FILE *stream, const char *fmt, ...)
{
    va_list ap;

    if (!quiet)
    {
        va_start(ap, fmt);

        vfprintf(stream, fmt, ap);

        va_end(ap);
    }
}

uint32_t read_callback(void *user_data, void *buffer, uint32_t length)
{
    return fread(buffer, 1, length, (FILE*)user_data);
}

uint32_t seek_callback(void *user_data, uint64_t position)
{
    return fseek((FILE*)user_data, position, SEEK_SET);
}

static int fill_buffer(aac_buffer *b)
{
    int bread;

    if (b->bytes_consumed > 0)
    {
        if (b->bytes_into_buffer)
        {
            memmove((void*)b->buffer, (void*)(b->buffer + b->bytes_consumed),
                b->bytes_into_buffer*sizeof(unsigned char));
        }

        if (!b->at_eof)
        {
            bread = fread((void*)(b->buffer + b->bytes_into_buffer), 1,
                b->bytes_consumed, b->infile);

            if (bread != b->bytes_consumed)
                b->at_eof = 1;

            b->bytes_into_buffer += bread;
        }

        b->bytes_consumed = 0;

        if (b->bytes_into_buffer > 3)
        {
            if (memcmp(b->buffer, "TAG", 3) == 0)
                b->bytes_into_buffer = 0;
        }
        if (b->bytes_into_buffer > 11)
        {
            if (memcmp(b->buffer, "LYRICSBEGIN", 11) == 0)
                b->bytes_into_buffer = 0;
        }
        if (b->bytes_into_buffer > 8)
        {
            if (memcmp(b->buffer, "APETAGEX", 8) == 0)
                b->bytes_into_buffer = 0;
        }
    }

    return 1;
}

static void advance_buffer(aac_buffer *b, int bytes)
{
    b->file_offset += bytes;
    b->bytes_consumed = bytes;
    b->bytes_into_buffer -= bytes;
	if (b->bytes_into_buffer < 0)
		b->bytes_into_buffer = 0;
}

static int adts_parse(aac_buffer *b, int *bitrate, float *length)
{
    int frames, frame_length;
    int t_framelength = 0;
    int samplerate;
    float frames_per_sec, bytes_per_frame;

    /* Read all frames to ensure correct time and bitrate */
    for (frames = 0; /* */; frames++)
    {
        fill_buffer(b);

        if (b->bytes_into_buffer > 7)
        {
            /* check syncword */
            if (!((b->buffer[0] == 0xFF)&&((b->buffer[1] & 0xF6) == 0xF0)))
                break;

            if (frames == 0)
                samplerate = adts_sample_rates[(b->buffer[2]&0x3c)>>2];

            frame_length = ((((unsigned int)b->buffer[3] & 0x3)) << 11)
                | (((unsigned int)b->buffer[4]) << 3) | (b->buffer[5] >> 5);

            t_framelength += frame_length;

            if (frame_length > b->bytes_into_buffer)
                break;

            advance_buffer(b, frame_length);
        } else {
            break;
        }
    }

    frames_per_sec = (float)samplerate/1024.0f;
    if (frames != 0)
        bytes_per_frame = (float)t_framelength/(float)(frames*1000);
    else
        bytes_per_frame = 0;
    *bitrate = (int)(8. * bytes_per_frame * frames_per_sec + 0.5);
    if (frames_per_sec != 0)
        *length = (float)frames/frames_per_sec;
    else
        *length = 1;

    return 1;
}

static int decodeAACfile(char *aacfile, char *sndfile, char *adts_fn, int to_stdout,
                  int def_srate, int object_type, int outputFormat, int fileType,
                  int downMatrix, int infoOnly, int adts_out, int old_format,
                  float *song_length)
{
    int tagsize;
    unsigned long samplerate;
    unsigned char channels;
    void *sample_buffer;

    audio_file *aufile;

    FILE *adtsFile;
    unsigned char *adtsData;
    int adtsDataSize;

    NeAACDecHandle hDecoder;
    NeAACDecFrameInfo frameInfo;
    NeAACDecConfigurationPtr config;

    char percents[200];
    int percent, old_percent = -1;
    int bread, fileread;
    int header_type = 0;
    int bitrate = 0;
    float length = 0;

    int first_time = 1;

    aac_buffer b;

    memset(&b, 0, sizeof(aac_buffer));

    if (adts_out)
    {
        adtsFile = fopen(adts_fn, "wb");
        if (adtsFile == NULL)
        {
            faad_fprintf(stderr, "Error opening file: %s\n", adts_fn);
            return 1;
        }
    }

    b.infile = fopen(aacfile, "rb");
    if (b.infile == NULL)
    {
        /* unable to open file */
        faad_fprintf(stderr, "Error opening file: %s\n", aacfile);
        return 1;
    }

    fseek(b.infile, 0, SEEK_END);
    fileread = ftell(b.infile);
    fseek(b.infile, 0, SEEK_SET);

    if (!(b.buffer = (unsigned char*)malloc(FAAD_MIN_STREAMSIZE*MAX_CHANNELS)))
    {
        faad_fprintf(stderr, "Memory allocation error\n");
        return 0;
    }
    memset(b.buffer, 0, FAAD_MIN_STREAMSIZE*MAX_CHANNELS);

    bread = fread(b.buffer, 1, FAAD_MIN_STREAMSIZE*MAX_CHANNELS, b.infile);
    b.bytes_into_buffer = bread;
    b.bytes_consumed = 0;
    b.file_offset = 0;

    if (bread != FAAD_MIN_STREAMSIZE*MAX_CHANNELS)
        b.at_eof = 1;

    tagsize = 0;
    if (!memcmp(b.buffer, "ID3", 3))
    {
        /* high bit is not used */
        tagsize = (b.buffer[6] << 21) | (b.buffer[7] << 14) |
            (b.buffer[8] <<  7) | (b.buffer[9] <<  0);

        tagsize += 10;
        advance_buffer(&b, tagsize);
        fill_buffer(&b);
    }

    hDecoder = NeAACDecOpen();

    /* Set the default object type and samplerate */
    /* This is useful for RAW AAC files */
    config = NeAACDecGetCurrentConfiguration(hDecoder);
    if (def_srate)
        config->defSampleRate = def_srate;
    config->defObjectType = object_type;
    config->outputFormat = outputFormat;
    config->downMatrix = downMatrix;
    config->useOldADTSFormat = old_format;
    //config->dontUpSampleImplicitSBR = 1;
    NeAACDecSetConfiguration(hDecoder, config);

    /* get AAC infos for printing */
    header_type = 0;
    if ((b.buffer[0] == 0xFF) && ((b.buffer[1] & 0xF6) == 0xF0))
    {
        adts_parse(&b, &bitrate, &length);
        fseek(b.infile, tagsize, SEEK_SET);

        bread = fread(b.buffer, 1, FAAD_MIN_STREAMSIZE*MAX_CHANNELS, b.infile);
        if (bread != FAAD_MIN_STREAMSIZE*MAX_CHANNELS)
            b.at_eof = 1;
        else
            b.at_eof = 0;
        b.bytes_into_buffer = bread;
        b.bytes_consumed = 0;
        b.file_offset = tagsize;

        header_type = 1;
    } else if (memcmp(b.buffer, "ADIF", 4) == 0) {
        int skip_size = (b.buffer[4] & 0x80) ? 9 : 0;
        bitrate = ((unsigned int)(b.buffer[4 + skip_size] & 0x0F)<<19) |
            ((unsigned int)b.buffer[5 + skip_size]<<11) |
            ((unsigned int)b.buffer[6 + skip_size]<<3) |
            ((unsigned int)b.buffer[7 + skip_size] & 0xE0);

        length = (float)fileread;
        if (length != 0)
        {
            length = ((float)length*8.f)/((float)bitrate) + 0.5f;
        }

        bitrate = (int)((float)bitrate/1000.0f + 0.5f);

        header_type = 2;
    }

    *song_length = length;

    fill_buffer(&b);
    if ((bread = NeAACDecInit(hDecoder, b.buffer,
        b.bytes_into_buffer, &samplerate, &channels)) < 0)
    {
        /* If some error initializing occured, skip the file */
        faad_fprintf(stderr, "Error initializing decoder library.\n");
        if (b.buffer)
            free(b.buffer);
        NeAACDecClose(hDecoder);
        fclose(b.infile);
        return 1;
    }
    advance_buffer(&b, bread);
    fill_buffer(&b);

    /* print AAC file info */
    faad_fprintf(stderr, "%s file info:\n", aacfile);
    switch (header_type)
    {
    case 0:
        faad_fprintf(stderr, "RAW\n\n");
        break;
    case 1:
        faad_fprintf(stderr, "ADTS, %.3f sec, %d kbps, %d Hz\n\n",
            length, bitrate, samplerate);
        break;
    case 2:
        faad_fprintf(stderr, "ADIF, %.3f sec, %d kbps, %d Hz\n\n",
            length, bitrate, samplerate);
        break;
    }

    if (infoOnly)
    {
        NeAACDecClose(hDecoder);
        fclose(b.infile);
        if (b.buffer)
            free(b.buffer);
        return 0;
    }

    do
    {
    
    if(abuf.size < PCM_BUF_SIZE/2.0f)
    {    
        sample_buffer = NeAACDecDecode(hDecoder, &frameInfo,
            b.buffer, b.bytes_into_buffer);

        /* update buffer indices */
        advance_buffer(&b, frameInfo.bytesconsumed);

        if (frameInfo.error > 0)
        {
            faad_fprintf(stderr, "Error: %s\n",
                NeAACDecGetErrorMessage(frameInfo.error));
        }

        /* open the sound file now that the number of channels are known */
        if (first_time && !frameInfo.error)
        {
            if (!adts_out)
                faad_drv_init(frameInfo.samplerate,frameInfo.channels);

            first_time = 0;
        }

        if ((frameInfo.error == 0) && (frameInfo.samples > 0) && (!adts_out))
        {
            faad_drv_stream( sample_buffer, frameInfo.samples*2  );   
		}

        /* fill buffer */
        fill_buffer(&b);

        if (b.bytes_into_buffer == 0)
            sample_buffer = NULL; /* to make sure it stops now */
    }
    
    if(LibFAAD_RenderFrame())
       FAAD_DEC_STAT=0;
   
    } while (sample_buffer != NULL && FAAD_DEC_STAT );

    NeAACDecClose(hDecoder);

    fclose(b.infile);

    faad_drv_free();
    
    if (b.buffer)
        free(b.buffer);

    return frameInfo.error;
}

static int GetAACTrack(mp4ff_t *infile)
{
    /* find AAC track */
    int i, rc;
    int numTracks = mp4ff_total_tracks(infile);

    for (i = 0; i < numTracks; i++)
    {
        unsigned char *buff = NULL;
        int buff_size = 0;
        mp4AudioSpecificConfig mp4ASC;

        mp4ff_get_decoder_config(infile, i, &buff, &buff_size);

        if (buff)
        {
            rc = NeAACDecAudioSpecificConfig(buff, buff_size, &mp4ASC);
            free(buff);

            if (rc < 0)
                continue;
            return i;
        }
    }

    /* can't decode this */
    return -1;
}

static int decodeMP4file(char *mp4file, char *sndfile, char *adts_fn, int to_stdout,
                  int outputFormat, int fileType, int downMatrix, int noGapless,
                  int infoOnly, int adts_out, float *song_length )
{
    int track;
    unsigned long samplerate;
    unsigned char channels;
    void *sample_buffer;

    mp4ff_t *infile;
    long sampleId, numSamples;

    audio_file *aufile;

    unsigned char *adtsData;
    int adtsDataSize;

    NeAACDecHandle hDecoder;
    NeAACDecConfigurationPtr config;
    NeAACDecFrameInfo frameInfo;
    mp4AudioSpecificConfig mp4ASC;

    unsigned char *buffer;
    int buffer_size;

    char percents[200];
    int percent, old_percent = -1;

    int first_time = 1;

    /* for gapless decoding */
    unsigned int useAacLength = 1;
    unsigned int initial = 1;
    unsigned int framesize;
    unsigned long timescale;
        
    /* initialise the callback structure */
    mp4cb = malloc(sizeof(mp4ff_callback_t));
    
    mp4File = fopen(mp4file, "rb");
    mp4cb->read = read_callback;
    mp4cb->seek = seek_callback;
    mp4cb->user_data = mp4File;

    hDecoder = NeAACDecOpen();

    /* Set configuration */
    config = NeAACDecGetCurrentConfiguration(hDecoder);
    config->outputFormat = outputFormat;
    config->downMatrix = downMatrix;
    NeAACDecSetConfiguration(hDecoder, config);
    
    infile = mp4ff_open_read(mp4cb);
    if (!infile)
    {
        /* unable to open file */
        faad_fprintf(stderr, "Error opening file: %s\n", mp4file);
        return 1;
    }

    if ((track = GetAACTrack(infile)) < 0)
    {
        faad_fprintf(stderr, "Unable to find correct AAC sound track in the MP4 file.\n");
        NeAACDecClose(hDecoder);
        mp4ff_close(infile);
        free(mp4cb);
        fclose(mp4File);
        return 1;
    }

    buffer = NULL;
    buffer_size = 0;
    mp4ff_get_decoder_config(infile, track, &buffer, &buffer_size);
    
    if(NeAACDecInit2(hDecoder, buffer, buffer_size,
                    &samplerate, &channels) < 0)
    {
        /* If some error initializing occured, skip the file */
        faad_fprintf(stderr, "Error initializing decoder library.\n");
        NeAACDecClose(hDecoder);
        mp4ff_close(infile);
        free(mp4cb);
        fclose(mp4File);
        return 1;
    }

    timescale = mp4ff_time_scale(infile, track);
    framesize = 1024;
    useAacLength = 0;

    if (buffer)
    {
        if (NeAACDecAudioSpecificConfig(buffer, buffer_size, &mp4ASC) >= 0)
        {
            if (mp4ASC.frameLengthFlag == 1) framesize = 960;
            if (mp4ASC.sbr_present_flag == 1) framesize *= 2;
        }
        free(buffer);
    }

    numSamples = mp4ff_num_samples(infile, track);
    sampleId = 0;
    
    do {

    if(abuf.size < PCM_BUF_SIZE/2.0f)
    {    
        int rc;
        long dur;
        unsigned int sample_count;
        unsigned int delay = 0;

        /* get acces unit from MP4 file */
        buffer = NULL;
        buffer_size = 0;
         
        dur = mp4ff_get_sample_duration(infile, track, sampleId);
        rc = mp4ff_read_sample(infile, track, sampleId, &buffer,  &buffer_size);
        if (rc == 0)
        {
            faad_fprintf(stderr, "Reading from MP4 file failed.\n");
            NeAACDecClose(hDecoder);
            mp4ff_close(infile);
            free(mp4cb);
            fclose(mp4File);
            return 1;
        }

        sample_buffer = NeAACDecDecode(hDecoder, &frameInfo, buffer, buffer_size);

        if (buffer) free(buffer);
        
        if (!noGapless)
        {
            if (sampleId == 0) dur = 0;

            if (useAacLength || (timescale != samplerate)) {
                sample_count = frameInfo.samples;
            } else {
                sample_count = (unsigned int)(dur * frameInfo.channels);
                if (sample_count > frameInfo.samples)
                    sample_count = frameInfo.samples;

                if (!useAacLength && !initial && (sampleId < numSamples/2) && (sample_count != frameInfo.samples))
                {
                    faad_fprintf(stderr, "MP4 seems to have incorrect frame duration, using values from AAC data.\n");
                    useAacLength = 1;
                    sample_count = frameInfo.samples;
                }
            }

            if (initial && (sample_count < framesize*frameInfo.channels) && (frameInfo.samples > sample_count))
                delay = frameInfo.samples - sample_count;
        } else {
            sample_count = frameInfo.samples;
        }

        /* open the sound file now that the number of channels are known */
        if (first_time && !frameInfo.error)
        {
            if (!adts_out)
                faad_drv_init(frameInfo.samplerate,frameInfo.channels);

            first_time = 0;
        }

        if (sample_count > 0) initial = 0;

        if ((frameInfo.error == 0) && (sample_count > 0) && (!adts_out))
        {
            faad_drv_stream( sample_buffer, sample_count*2  );
        }

        if (frameInfo.error > 0)
            faad_fprintf(stderr, "Warning: %s\n",
                NeAACDecGetErrorMessage(frameInfo.error));

        sampleId++;
    }
    
    if(LibFAAD_RenderFrame())
       FAAD_DEC_STAT=0;
        
    } while ( sampleId < numSamples && FAAD_DEC_STAT );
    
    faad_drv_free();
    
    NeAACDecClose(hDecoder);
    mp4ff_close(infile);

    free(mp4cb);
    fclose(mp4File);

    return frameInfo.error;
}

static int LibFAAD_RenderFrame()
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

int LibFAAD_Decode( char * mp4_file, Font * font, DirectoryEntry * entry  )
{    
    int result;
    int infoOnly = 0;
    int writeToStdio = 0;
    int object_type = MAIN;
    int def_srate = 0;
    int downMatrix = 1;
    int format = 2;
    int outputFormat = FAAD_FMT_16BIT;
    int outfile_set = 1;
    int adts_out = 0;
    int old_format = 0;
    int showHelp = 0;
    int mp4file = 0;
    int noGapless = 0;
    char *fnp;
    char aacFileName[255];
    char audioFileName[255];
    char adtsFileName[64];
    unsigned char header[8];
    float length = 0;
    FILE *hMP4File;

    FONT = font;
    fs_entry = entry;

    unsigned long cap = NeAACDecGetCapabilities();

    /* point to the specified file name */
    strcpy(aacFileName, mp4_file);

    /* check for mp4 file */
    mp4file = 0;
    hMP4File = fopen(aacFileName, "rb");
    if (!hMP4File)
    {
        faad_fprintf(stderr, "Error opening file: %s\n", aacFileName);
        return 1;
    }
    fread(header, 1, 8, hMP4File);
    if (header[4] == 'f' && header[5] == 't' && header[6] == 'y' && header[7] == 'p')
        mp4file = 1;

    fclose(hMP4File);
    
    if (mp4file)
    {
        result = decodeMP4file(aacFileName, audioFileName, adtsFileName, writeToStdio,
            outputFormat, format, downMatrix, noGapless, infoOnly, adts_out, &length);
    }
    else
    {
        result = decodeAACfile(aacFileName, audioFileName, adtsFileName, writeToStdio,
            def_srate, object_type, outputFormat, format, downMatrix, infoOnly, adts_out,
            old_format, &length);
    }
     
    return 0;
}

