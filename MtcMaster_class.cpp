//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// Stage Lab MTC Master Class code file
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

#include "MtcMaster_class.h"
#include <time.h>  // For clock_nanosleep, clock_gettime

using namespace std;

////////////////////////////////////////////
// Initializing static class members
unsigned short MtcMaster::instanceCount = 0; // Instance counter
std::mutex MtcMaster::mtx;
bool MtcMaster::playing = false;

////////////////////////////////////////////
// Contrstuctors and Destructors
MtcMaster::MtcMaster(   RtMidi::Api api,
                            const string& clientName,
                            // const string& portName,
                            FrameRate setUpFR)
                            :   // Members initialization
                            RtMidiOut(api, clientName),
                            mtcTimeVector(4, 0),
                            mtcTime(0),
                            currentFrameRate(setUpFR)
{
    instanceCount++;

    switch(setUpFR)
    {
    case FR_24:
        currentFRBits = 0x00;
        // Frame period: 1/24 second = 41666666.67 ns
        frameFreqNanos = 41666667;
        break;
    case FR_25:
        currentFRBits = 0x20;
        // Frame period: 1/25 second = 40000000 ns
        frameFreqNanos = 40000000;
        break;
    case FR_29:
        currentFRBits = 0x40;
        // Frame period for 29.97 drop-frame: 1001/30000 second = 33366666.67 ns
        // More accurate than 1e9/29.97
        frameFreqNanos = 33366667;
        break;
    case FR_30:
        currentFRBits = 0x60;
        // Frame period: 1/30 second = 33333333.33 ns
        frameFreqNanos = 33333333;
        break;
    default:
        currentFRBits = 0x20;
        frameFreqNanos = 40000000;
        break;
    }

    try {
        openPort(0, "MTCPort");
    }
    catch ( RtMidiError &error ) {
        error.printMessage();
        exit( EXIT_FAILURE );
    }

    try {
        logFileStream.open("/run/log/libmtcmaster.log", std::ofstream::out | std::ofstream::app);
    }
    catch (system_error &error) {
        exit( EXIT_FAILURE );
    }

    logFileStream << endl << endl << "________________________________________" << endl << endl;
    logTime();
    logFileStream << "MTC Master Initialized" << endl << endl;
}

MtcMaster::~MtcMaster(void) {
    instanceCount--;

    logFileStream << endl;
    logTime();
    logFileStream << "MTC Master finished succesfully" << endl << endl;

    logFileStream.close();
}

////////////////////////////////////////////
// Member methods

//--------------------------------------------------------------
void MtcMaster::sendMtcPosition(void)
{
    /////////////////////////////////////////////////////////////////////
    // Sends a Full 10 bytes Message with the current MTC position
    // With format : F0 7F <chan> 01 <sub-ID 2> <hours and type> mn sc fr F7

    vector<unsigned char> mtcFullMessage; // Char vector to build the MTC Full message

    mtcFullMessage.push_back(MIDI_STATUS_SYSEX);    // Midi SisEx message code (F0)
    mtcFullMessage.push_back(0x7F);                 // F0 + 7F -> Real Time Universal System Exclusive Header
    mtcFullMessage.push_back(0x7F);                 // SysEx channel - 7F -> Everyone
    mtcFullMessage.push_back(0x01);                 // SubID01 -> 01 -> MIDI Time Code
    mtcFullMessage.push_back(0x01);                 // SubID02 -> 01 -> Full Time Code Message


    mtcFullMessage.push_back((unsigned char)currentFRBits | (mtcTimeVector[3] & 0x1f));
                                                            // Time code Bits + Hours

    mtcFullMessage.push_back((mtcTimeVector[2] & 0xff));    // Minutes
    mtcFullMessage.push_back((mtcTimeVector[1] & 0xff));    // Seconds
    mtcFullMessage.push_back((mtcTimeVector[0] & 0xff));    // Frames
    mtcFullMessage.push_back(MIDI_STATUS_SYSEX_END);        // SysEx End of full message (F7)

    this->sendMessage( &mtcFullMessage );

    // A small pause to let the devices seek properly
    std::this_thread::sleep_for(std::chrono::milliseconds(4));
}

//--------------------------------------------------------------
void MtcMaster::pause()
{
    play();
}

//--------------------------------------------------------------
void MtcMaster::play()
{
    if (playing)
    {
        // If the master is already playing, don't play again
        // instead call the pause method and pause, then

        // Just changing the flag the thread stops
        setPlaying(false);

        // We wait for the thread to unlock the control mutex
        // which means that is has finished the main loop
        while(!mtx.try_lock());
        mtx.unlock();
    }
    else
    {
        // If not really playing the, play and thread the method
        // to be running independently till the flag changes

        setPlaying(true);       // Mark the playing flag as true

        // A small pause to let the player seek properly
        //std::this_thread::sleep_for(std::chrono::milliseconds(4));

        thread threadObj(&MtcMaster::threadedMethod, this); // Our thread object
        try {
            setScheduling(threadObj, SCHED_RR, 99); // Our thread scheduling
        }
        catch ( RtMidiError &error ) {
            error.printMessage();
        }

        threadObj.detach();
    }
}

//--------------------------------------------------------------
void MtcMaster::stop()
{
    // Just changing the flag the thread stops
    setPlaying(false);

    // We wait for the thread to unlock the control mutex
    // which means that is has finished the main loop
    while(!mtx.try_lock());
    mtx.unlock();

    // As we are stopping we just reset the time to 0
    setTime(0);

    // A small pause to let the system devices seek properly
    //std::this_thread::sleep_for(std::chrono::milliseconds(150));
}

//--------------------------------------------------------------
void MtcMaster::setTime(uint64_t nanosecs = 0)
{
    bool wasPlaying = playing;

    if (playing)    // If we are playing...
    {
        // We pause by calling the play function
        play();
    }

    // If the parameter is actually a number of nanoseconds
    // we take that amount to send the MTC
    mtcTime = nanosecs;         // We store that amount into our member variable
    fillMtcTimeVector(mtcTime); // and fill in our member MTC Time vector
    sendMtcPosition();

    // If it was playing, let's play again
    if (wasPlaying) play();
}

//--------------------------------------------------------------
// Helper function to add nanoseconds to a timespec
static inline void timespec_add_ns(struct timespec *ts, uint64_t ns)
{
    ts->tv_nsec += ns;
    while (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000L;
    }
}

//--------------------------------------------------------------
void MtcMaster::threadedMethod(void)
{
    unsigned char c = 0;    // Working char variable to build the messages
    bool LSB = true;        // Alternating LSB and MSB when building the messages
    unsigned char byteType = 0;    // Message byte type parameter
    vector<unsigned char> mtcMessage {0,0}; // Message char vector

    // First send a full message with the current position
    sendMtcPosition();

    // The quarter frame period in nanoseconds
    uint64_t quarterFreq = frameFreqNanos / 4;

    // Use absolute time scheduling with clock_nanosleep for jitter-free timing
    struct timespec nextQuarterTime;
    clock_gettime(CLOCK_MONOTONIC, &nextQuarterTime);

    // Mutex lock
    mtx.lock();

    while(playing)
    {
        ////////////////////////////////////////////////////////////////////////////////
        // Each quarter of frame we send a 2 bytes message with different contents
        // and we need 8 messages to send the whole MTC time code :
        // 2 x system, 2 x hours, 2 x minutes, 2 x seconds & 2 x frames
        // So we complete a MTC whole message after 8 quarters, thus every 2 frames
        //
        // The structure of the message is as follows:
        // First byte = (F1) - Second byte = (<message>)
        //
        // F1 = Currently unused and undefined System Common status byte
        // <message> = 0nnn dddd
        //
        // nnn = Message type (0 to 7)
        // dddd = 4 bits of binary data for this Message Type
        // 0 = Frame count LS nibble
        // 1 = Frame count MS nibble
        // FRAME COUNT: xxx yyyyy
        //          xxx undefined
        //          yyyyy frame count (0-29)
        // 2 = Seconds count LS nibble
        // 3 = Seconds count MS nibble
        // SECONDS COUNT: xx yyyyyy
        //          xx undefined
        //          yyyyyy seconds count (0-59)
        // 4 = Minutes count LS nibble
        // 5 = Minutes count MS nibble
        // MINUTES COUNT: xx yyyyyy
        //          xx undefined
        //          yyyyyy minutes count (0-59)
        // 6 = Hours count LS nibble
        // 7 = Hours count MS nibble and SMPTE Type
        // HOURS COUNT: x yy zzzzz
        //          x undefined
        //          yy Time Code Type:
        //                    0 = 24 Frames/Second
        //                    1 = 25 Frames/Second
        //                    2 = 30 Frames/Second (Drop-Frame)
        //                    3 = 30 Frames/Second (Non-Drop)
        //          zzzzz Hours count (0-23)

        c = 0;        // working char reset

        /////////////////////////////////////////////////////////////////
        // 8 steps loop to build and send the 8 MTC messages that
        // compose the whole time code
        for (byteType = 0; byteType <= 7; ++byteType)
        {
            // MTC message container initialization
            mtcMessage.clear();

            if (LSB)
            {
                c = (byteType << 4) | (mtcTimeVector[byteType / 2] & 0x0f);

                LSB = false; // Reset LSB flag to alternate
            }
            else
            {
                if (byteType < 7)
                {
                    c = (byteType << 4) | ((mtcTimeVector[byteType / 2] & 0xf0) >> 4);
                }
                else
                {
                    c = 0x70 | ((((unsigned char)currentFRBits | mtcTimeVector[3]) & 0xf0) >> 4);
                }

                LSB = true; // Reset LSB flag to alternate
            }

            mtcMessage.push_back(MIDI_STATUS_TIME_CODE);  // Introducing the MTC code byte (F1)
            mtcMessage.push_back(c);               // Then introducing the rest of the message

            sendMessage( &mtcMessage );             // And then sending the message

            // Calculate the absolute time for the next quarter frame
            timespec_add_ns(&nextQuarterTime, quarterFreq);

            // Use clock_nanosleep with TIMER_ABSTIME for precise, jitter-free timing
            // This eliminates drift because we sleep until an absolute time,
            // not for a relative duration. Any overshoot is automatically compensated.
            int ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &nextQuarterTime, NULL);
            
            if (ret != 0 && ret != EINTR) {
                // Log timing errors (but don't spam the log)
                static int errorCount = 0;
                if (errorCount++ < 10) {
                    logTime();
                    logFileStream << "clock_nanosleep error: " << ret << endl;
                }
            }

            // Check if we're falling behind (for logging purposes)
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            int64_t drift_ns = (now.tv_sec - nextQuarterTime.tv_sec) * 1000000000LL + 
                               (now.tv_nsec - nextQuarterTime.tv_nsec);
            
            if (drift_ns > 1000000) {  // More than 1ms behind
                logTime();
                logFileStream << "TIME OUT!!! Behind by: " << drift_ns << " ns" << endl;
            }

            // We update the mtcTime after every quarter
            mtcTime += quarterFreq;
        }

        // After a whole MTC message, 8 quarters, 2 frames, we update the MtcTimeVector
        fillMtcTimeVector(mtcTime);
        
        // Network resync: send periodic full frame messages for receivers to resync
        // This helps recover from packet loss over network (rtpmidid, etc.)
        framesSinceLastFullFrame += 2;  // We just sent 2 frames worth of quarter frames
        if (fullFrameResyncInterval > 0 && framesSinceLastFullFrame >= fullFrameResyncInterval) {
            sendMtcPosition();  // Send full frame SYSEX
            framesSinceLastFullFrame = 0;
        }
    }

    // If the thread has just stopped we adjust the time
    // to the last Time Vector used (+2 frames adjustment)
    // to build the message instead of adjusting the Vector
    // to a new mtcTime
    mtcTime =   (uint64_t) ( mtcTimeVector[0] += 2 ) * frameFreqNanos              // Frames adjusted
                + (uint64_t) mtcTimeVector[1] * (uint64_t)1e9                // + Seconds
                + (uint64_t) mtcTimeVector[2] * 60 * (uint64_t)1e9           // + minutes
                + (uint64_t) mtcTimeVector[3] * 3600 * (uint64_t)1e9 ;       // + hours


    // Before exiting the threaded function
    // we unlock the mutex
    mtx.unlock();
}

//--------------------------------------------------------------
void MtcMaster::fillMtcTimeVector(uint64_t nanos)
{
    // First we clear our member vector
    mtcTimeVector.clear();

    // And then fill it again using the precise frame frequency
    mtcTimeVector.push_back((uint64_t)(nanos / frameFreqNanos) % currentFrameRate);     // FRAMES 0
    mtcTimeVector.push_back((uint64_t)(nanos / (uint64_t)1e9) % 60);              // SECONDS 1
    mtcTimeVector.push_back((uint64_t)(nanos / (uint64_t)(60e9)) % 60);         // MINUTES 2
    mtcTimeVector.push_back((uint64_t)(nanos / (uint64_t)(3600e9)) % 24);      // HOURS 3
}

//--------------------------------------------------------------
string MtcMaster::mtcTimeVectorString(void)
{
    string timeAsString("");

    if( mtcTimeVector[3] < 10 ) timeAsString += "0";

    timeAsString += to_string(mtcTimeVector[3]);
    timeAsString += "h:";

    if( mtcTimeVector[2] < 10 ) timeAsString += "0";
    timeAsString += to_string(mtcTimeVector[2]);
    timeAsString += "m:";

    if( mtcTimeVector[1] < 10 ) timeAsString += "0";
    timeAsString += to_string(mtcTimeVector[1]);
    timeAsString += "s:";

    if( mtcTimeVector[0] < 10 )   timeAsString += "0";
    timeAsString += to_string(mtcTimeVector[0]);
    timeAsString += "f";

    return timeAsString;
}

//--------------------------------------------------------------
string MtcMaster::getApiString(void)
{
    switch(getCurrentApi())
    {
    case RtMidi::Api::LINUX_ALSA:
        return  "Linux ALSA";
    case RtMidi::Api::MACOSX_CORE:
        return "MacOSX Core";
    case RtMidi::Api::RTMIDI_DUMMY:
        return "RtMidi Dummy";
    case RtMidi::Api::UNIX_JACK:
        return "Unix Jack";
    case RtMidi::Api::WINDOWS_MM:
        return "Windows MM";
    case RtMidi::Api::UNSPECIFIED:
    default:
        return "Unspecified";
    }

}

//--------------------------------------------------------------
uint64_t MtcMaster::getTime(void)
{
    return mtcTime;
}

//--------------------------------------------------------------
// Scheduling the thread priority to ensure an accurate timing
void MtcMaster::setScheduling(std::thread &th, int policy, int priority)
{
    sched_param sch_params;
    sch_params.sched_priority = priority;

    string errorText = "";

    if (pthread_setschedparam(th.native_handle(), policy, &sch_params) != 0)
    {

        logTime();
        logFileStream  << "Failed to set Thread scheduling : " << error_code() << endl;
        logFileStream << "Policy : " << policy << " Priority : " << sch_params.sched_priority << endl;

        throw RtMidiError("[ERR] libmtcmaster thread scheduling error", RtMidiError::Type::THREAD_ERROR);
    }
}

//--------------------------------------------------------------
void MtcMaster::logTime(void) {
    time_t now = time(nullptr);

    logFileStream << put_time(localtime(&now), "%F %T") << " ";
}

//--------------------------------------------------------------
void MtcMaster::subtractNanos(const uint64_t diff) {
    if (mtcTime > diff) {
        mtcTime -= diff;
    } 
    else {
        mtcTime = 0;
    }

    setTime(mtcTime);
}

//--------------------------------------------------------------
void MtcMaster::addNanos(const uint64_t diff) {
    // Let's say we cannot go further than 24 hours. We'll see...
    if (mtcTime + diff > 86400000000000) {
        mtcTime = 0;
    }
    else {
        mtcTime += diff;
    }

    setTime(mtcTime);
}

//--------------------------------------------------------------
void MtcMaster::setFrameRate(FrameRate FR) {
    currentFrameRate = FR;
    
    // Update frame frequency and MTC bits for the new frame rate
    switch(FR)
    {
    case FR_24:
        currentFRBits = 0x00;
        frameFreqNanos = 41666667;  // 1/24 second
        break;
    case FR_25:
        currentFRBits = 0x20;
        frameFreqNanos = 40000000;  // 1/25 second
        break;
    case FR_29:
        currentFRBits = 0x40;
        frameFreqNanos = 33366667;  // 1001/30000 second (29.97 drop-frame)
        break;
    case FR_30:
        currentFRBits = 0x60;
        frameFreqNanos = 33333333;  // 1/30 second
        break;
    default:
        currentFRBits = 0x20;
        frameFreqNanos = 40000000;
        break;
    }
}
