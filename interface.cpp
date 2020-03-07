#include "interface.h"

/*extern "C"*/ void* MTCSender_create() {
    return new MtcMaster;
}

/*extern "C"*/ void MTCSender_release(void* mtcsender) {
    delete static_cast<MtcMaster*>(mtcsender);
}

/*extern "C"*/ void MTCSender_play(void* mtcsender) {
    static_cast<MtcMaster*>(mtcsender)->play();
}

/*extern "C"*/ void MTCSender_stop(void* mtcsender) {
    static_cast<MtcMaster*>(mtcsender)->stop();
}

/*extern "C"*/ void MTCSender_pause(void* mtcsender) {
    static_cast<MtcMaster*>(mtcsender)->pause();
}

/*extern "C"*/ void MTCSender_setTime(void* mtcsender, uint64_t nanos) {
    static_cast<MtcMaster*>(mtcsender)->setTime(nanos);
}
