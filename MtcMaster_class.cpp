//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// Stage Lab MTC Master Class code file
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

#include "MtcMaster_class.h"

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
        break;
    case FR_25:
        currentFRBits = 0x20;
        break;
    case FR_29:
        currentFRBits = 0x40;
        break;
    case FR_30:
        currentFRBits = 0x60;
        break;
    }
}

////////////////////////////////////////////
// Member methods

void MtcMaster::sendMtcPosition(void)
{
    /////////////////////////////////////////////////////////////////////
    // Sends a Full 10 bytes Message with the current MTC position
    // With format : F0 7F <chan> 01 <sub-ID 2> <hours and type> mn sc fr F7

    vector<unsigned char> mtcFullMessage; // Char vector to build the MTC Full message

    mtcFullMessage.push_back(MIDI_SYSEX);     // Midi SisEx message code (F0)
    mtcFullMessage.push_back(0x7F);           // F0 + 7F -> Real Time Universal System Exclusive Header
    mtcFullMessage.push_back(0x7F);           // SysEx channel - 7F -> Everyone
    mtcFullMessage.push_back(0x01);           // SubID01 -> 01 -> MIDI Time Code
    mtcFullMessage.push_back(0x01);           // SubID02 -> 01 -> Full Time Code Message


    mtcFullMessage.push_back((unsigned char)currentFRBits | (mtcTimeVector[3] & 0x1f));
                                                            // Time code Bits + Hours

    mtcFullMessage.push_back((mtcTimeVector[2] & 0xff));   // Minutes
    mtcFullMessage.push_back((mtcTimeVector[1] & 0xff));   // Seconds
    mtcFullMessage.push_back((mtcTimeVector[0] & 0xff));   // Frames
    mtcFullMessage.push_back(MIDI_SYSEX_END);             // SysEx End of full message (F7)

    this->sendMessage( &mtcFullMessage );

    // A small pause to let the devices seek properly
    std::this_thread::sleep_for(std::chrono::milliseconds(4));
}

void MtcMaster::pause()
{
    play();
}

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
        setScheduling(threadObj, SCHED_RR, 99); // Our thread scheduling

        threadObj.detach();
    }
}

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

void MtcMaster::threadedMethod(void)
{
    unsigned char c = 0;    // Working char variable to build the messages
    bool LSB = true;        // Alternating LSB and MSB when building the messages
    unsigned char byteType = 0;    // Message byte type parameter
    vector<unsigned char> mtcMessage {0,0}; // Message char vector

    uint64_t initTime;      // Initial time for the playing loop
    uint64_t currentTime;   // Current time in each round of loop
    uint64_t nextQuarterToSend;

    // First send a full message with the current position
    sendMtcPosition();

    initTime =     // Initial time for the playing loop
        chrono::duration_cast<chrono::nanoseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();

    nextQuarterToSend = initTime; // First "next quarter" is init time

    // The frequency of our frames in miliseconds
    // depends on the Frame Rate we are using
    // FR23.976 = 41,708333333 ms / frame
    // FR25 = 40 ms / frame
    // FR29.97 = 33,366700033 ms / frame
    // FR30 = 33,333333333 ms / frame

    // !!!!!!!!!!! By now we jus round it to a simple division
    // !!!!!!!!!!! to be adjusted later
    uint64_t frameFreq = 1e9 / currentFrameRate;

    struct timespec delay = {0, 0};

    // Mutex lock
    mtx.lock();

    while(playing)
    {
        ////////////////////////////////////////////////////////////////////////////////
        // Each quarter of frame we send a 2 bytes message with different contents
        // and we need 8 messages to send the whoe MTC time code :
        // 2 x system, 2 x hours, 2 x minutes, 2 x seconds & 2 x frames
        // So we complet a MTC whole message after 8 quarters, thus every 2 frames
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

            mtcMessage.push_back(MIDI_TIME_CODE);  // Introducing the MTC code byte (F1)
            mtcMessage.push_back(c);               // Then introducint the rest of the message

            sendMessage( &mtcMessage );             // And then sending the message

            // Calculation of next Quarter to send : last + 1/4 of the nanoseconds per frame
            // nextQuarterToSend = lastQuarterSent + ((uint64_t)frameFreq / 4);
            nextQuarterToSend += ((uint64_t)frameFreq / 4);

            // Getting again the current time from the system
            currentTime = chrono::duration_cast<chrono::nanoseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();

            if (nextQuarterToSend < currentTime)
            {
                // We are out of time for the next quarter
                cerr << "TIME OUT!!!!!!!!!!!!!!!!!!!!!!!!!!!! Diff : " << (nextQuarterToSend - currentTime) << endl;
            }
            else
            {
                // We are on time, let's make a little delay
                // to wait for the next quarter to send
                // NOTE: We subtract another 1e5 nanoseconds more
                // to be a bit in advance
                delay.tv_nsec = nextQuarterToSend - currentTime;

                nanosleep(&delay, NULL);
            }

            // We update the mtcTime after every quarter
            mtcTime += ((uint64_t)frameFreq / 4);;
        }

        // After a whole MTC message, 8 quarters, 2 frames, we update the MtcTimeVector
        fillMtcTimeVector(mtcTime);
    }

    // If the thread has just stopped we adjust the time
    // to the last Time Vector used (+2 frames adjustment)
    // to build the message instead of adjusting the Vector
    // to a new mtcTime
    mtcTime =   (uint64_t) ( mtcTimeVector[0] += 2 ) * frameFreq              // Frames adjusted
                + (uint64_t) mtcTimeVector[1] * (uint64_t)1e9                // + Seconds
                + (uint64_t) mtcTimeVector[2] * 60 * (uint64_t)1e9           // + minutes
                + (uint64_t) mtcTimeVector[3] * 3600 * (uint64_t)1e9 ;       // + hours


    // Before exiting the threaded function
    // we unlock the mutex
    mtx.unlock();
}

void MtcMaster::fillMtcTimeVector(uint64_t nanos)
{
    // The frequency of our frames in miliseconds
    // depends on the Frame Rate we are using
    // FR23.976 = 41,708333333 ms / frame
    // FR25 = 40 ms / frame
    // FR29.97 = 33,366700033 ms / frame
    // FR30 = 33,333333333 ms / frame

    // !!!!!!!!!!! ATTENTION
    // !!!!!!!!!!! By now we jus round it to a simple division
    // !!!!!!!!!!! to be adjusted later
    uint64_t frameFreq = (uint64_t)1e9 / currentFrameRate;

    // First we clear our member vector
    mtcTimeVector.clear();

    // And then fill it again
    mtcTimeVector.push_back((uint64_t)(nanos / frameFreq) % currentFrameRate);     // FRAMES 0
    mtcTimeVector.push_back((uint64_t)(nanos / (uint64_t)1e9) % 60);              // SECONDS 1
    mtcTimeVector.push_back((uint64_t)(nanos / (uint64_t)(60e9)) % 60);         // MINUTES 2
    mtcTimeVector.push_back((uint64_t)(nanos / (uint64_t)(3600e9)) % 24);      // HOURS 3
}

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

uint64_t MtcMaster::getTime(void)
{
    return mtcTime;
}

// Scheduling the threa priority to ensure an accurate timing
void MtcMaster::setScheduling(std::thread &th, int policy, int priority)
{
    sched_param sch_params;
    sch_params.sched_priority = priority;

    if (pthread_setschedparam(th.native_handle(), policy, &sch_params) != 0)
    {
        cout << "Failed to set Thread scheduling : " << error_code() << endl;
        cout << "Policy : " << policy << " Priority : " << sch_params.sched_priority << endl;
    }
}
