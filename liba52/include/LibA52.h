#ifndef LibA52_H
#define LibA52_H

#include <kos/mutex.h>

mutex_t * a52mut;
#define A52_create_mutex()  { a52mut = mutex_create(); }
#define A52_lock_mutex()    { mutex_lock( a52mut );    }
#define A52_unlock_mutex()  { mutex_unlock( a52mut );  }
#define A52_destroy_mutex() { mutex_destroy( a52mut ); }

struct LibA52_Info
{
    int sample_rate;
    int channels;
}LibA52_Info;       

volatile int A52_DEC_STAT;

int LibA52_Init( int rate, int channels );

void LibA52_decode_chunk (unsigned char * start, unsigned char * end);

int LibA52_Exit();

void LibA52_VolumeUp();

void LibA52_VolumeDown();

void LibA52_WaitStart();

void LibA52_Start();

#endif
