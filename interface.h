///////////////////////////////////////////////
// Header file for the C bindings interface

#ifndef INTERFACE_H_INCLUDED
#define INTERFACE_H_INCLUDED

#include "MtcMaster_class.h"

#if defined(__cplusplus)
extern "C" {
#endif

// Constructor and destructor bindings
void* MTCSender_create();
void MTCSender_release(void* mtcsender);

// Main MTC control functions
void MTCSender_openPort(void* mtcsender, unsigned int portnumber, const char* portname);

void MTCSender_play(void* mtcsender);

void MTCSender_stop(void* mtcsender);

void MTCSender_pause(void* mtcsender);

void MTCSender_setTime(void* mtcsender, uint64_t nanos);

uint64_t MTCSender_getTime(void* mtcsender);

// Frame rate: 24, 25, 29 (29.97 drop-frame), 30
// Note: Currently fixed at 25 fps, but this allows future flexibility
void MTCSender_setFrameRate(void* mtcsender, int fps);

int MTCSender_getFrameRate(void* mtcsender);

#if defined(__cplusplus)
}
#endif

#endif // INTERFACE_H_INCLUDED
