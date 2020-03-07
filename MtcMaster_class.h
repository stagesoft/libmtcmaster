//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// Stage Lab MTC Master Class header file
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

#ifndef MTCMASTER_CLASS_H_INCLUDED
#define MTCMASTER_CLASS_H_INCLUDED

#include <vector>
#include <chrono>
#include <iostream>
#include <thread>
#include <sys/time.h>
#include <sys/resource.h>
#include <mutex>
#include <iomanip>
#include <cstring>
#include <pthread.h>
#include "rtmidi/RtMidi.h"

// #include "ScreenMan_class.h"

using namespace std;

enum MidiStatus
{

    MIDI_UNKNOWN            = 0x00,

    // channel voice messages
    MIDI_NOTE_OFF           = 0x80,
    MIDI_NOTE_ON            = 0x90,
    MIDI_CONTROL_CHANGE     = 0xB0,
    MIDI_PROGRAM_CHANGE     = 0xC0,
    MIDI_PITCH_BEND         = 0xE0,
    MIDI_AFTERTOUCH         = 0xD0, // aka channel pressure
    MIDI_POLY_AFTERTOUCH    = 0xA0, // aka key pressure

    // system messages
    MIDI_SYSEX              = 0xF0,
    MIDI_TIME_CODE          = 0xF1,
    MIDI_SONG_POS_POINTER   = 0xF2,
    MIDI_SONG_SELECT        = 0xF3,
    MIDI_TUNE_REQUEST       = 0xF6,
    MIDI_SYSEX_END          = 0xF7,
    MIDI_TIME_CLOCK         = 0xF8,
    MIDI_START              = 0xFA,
    MIDI_CONTINUE           = 0xFB,
    MIDI_STOP               = 0xFC,
    MIDI_ACTIVE_SENSING     = 0xFE,
    MIDI_SYSTEM_RESET       = 0xFF
};


enum FrameRate
{
    FR_24 = 24,
    FR_25 = 25,
    FR_29 = 29,
    FR_30 = 30
}; // Frame rate constants

class MtcMaster : public RtMidiOut
{
    //////////////////////////////////////////
    public:
    //////////////////////////////////////////

    //////////////////////////////////////////
    // Constructors and destructors
    MtcMaster(RtMidi::Api api = UNIX_JACK,
             const string& clientName = "MtcMaster",
             // const string& portName = "SLMTCPort",
             FrameRate setUpFR = FR_25);

    inline virtual ~MtcMaster( void ) { instanceCount--; };
    //////////////////////////////////////////

    //////////////////////////////////////////
    // Member variables
    vector<unsigned char> mtcTimeVector; // MTC Time char vector
    static unsigned short instanceCount; // Static counter of number of instances of the class

    //////////////////////////////////////////
    // Member methods
    void sendMtcPosition();
    void play();
    void pause();
    void stop();
    void setTime(uint64_t nanos);
    uint64_t getTime(void);
    void fillMtcTimeVector(uint64_t nanos);
    string mtcTimeVectorString(void);
    string getApiString(void);

    inline bool getPlaying(void) { return playing; }
    inline void setPlaying(bool status) { playing = status; }
    inline FrameRate getFrameRate(void) { return currentFrameRate; }
    inline void setFrameRate(FrameRate FR) { currentFrameRate = FR; }
    inline uint64_t getMtcTime (void) { return mtcTime; };

    //////////////////////////////////////////
    private:
    //////////////////////////////////////////

    //////////////////////////////////////////
    // Private variables
    uint64_t mtcTime;               // MTC time
    FrameRate currentFrameRate;      // Current Frame Rate
    unsigned char currentFRBits;   // Current Time code bits
    static std::mutex mtx;          // Mutex to lock the threaded function
    static bool playing;            // Flag to know if the object is playing

    //////////////////////////////////////////
    // Private methods
    void threadedMethod(void);
    void setScheduling(std::thread &th, int policy, int priority);
};

#endif // MTCMASTER_CLASS_H_INCLUDED
