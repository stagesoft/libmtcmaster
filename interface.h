#include "mtcmaster.h"
#if defined(__cplusplus)
extern "C" {
#endif

void* MTCSender_create();
void MTCSender_release(void* mtcsender);
void MTCSender_play(void* mtcsender);
void MTCSender_stop(void* mtcsender);
void MTCSender_pause(void* mtcsender);
void MTCSender_setTime(void* mtcsender, uint64_t nanos);

#if defined(__cplusplus)
}
#endif
