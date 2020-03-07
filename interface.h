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

#if defined(__cplusplus)
}
#endif

#endif // INTERFACE_H_INCLUDED
