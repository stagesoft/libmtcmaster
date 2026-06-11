#include "interface.h"

/*extern "C"*/ void* MTCSender_create() {
    return new MtcMaster();
}

/*extern "C"*/ void MTCSender_release(void* mtcsender) {
    delete static_cast<MtcMaster*>(mtcsender);
}

/*extern "C"*/ void MTCSender_openPort(void* mtcsender, unsigned int portnumber, const char* portname) {
    std::string sportname(portname);
    static_cast<MtcMaster*>(mtcsender)->openPort(portnumber, sportname);
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

/*extern "C"*/ uint64_t MTCSender_getTime(void* mtcsender) {
    return static_cast<MtcMaster*>(mtcsender)->getTime();
}

/*extern "C"*/ void MTCSender_setFrameRate(void* mtcsender, int fps) {
    FrameRate fr;
    switch(fps) {
        case 24: fr = FR_24; break;
        case 25: fr = FR_25; break;
        case 29: fr = FR_29; break;  // 29.97 drop-frame
        case 30: fr = FR_30; break;
        default: fr = FR_25; break;  // Default to 25 fps
    }
    static_cast<MtcMaster*>(mtcsender)->setFrameRate(fr);
}

/*extern "C"*/ int MTCSender_getFrameRate(void* mtcsender) {
    return static_cast<int>(static_cast<MtcMaster*>(mtcsender)->getFrameRate());
}

/*extern "C"*/ void MTCSender_setFullFrameResyncInterval(void* mtcsender, unsigned int frames) {
    static_cast<MtcMaster*>(mtcsender)->setFullFrameResyncInterval(frames);
}
