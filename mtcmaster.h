#include <thread>
#include <pthread.h>
#include <iostream>
#include <string.h>
#include <vector>
#include <chrono>
#include <cmath>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstdlib>
#include "RtMidi.h"



#include "midi_constants.h"

using std::vector;
using std::string;

class MTCSender  {

        uint64_t lastQuarterSent;
        uint64_t nextQuarterToSend;
        uint64_t time_zero = 0;
        uint64_t time_now = 0;
        uint64_t mtc_time = 0;
        vector<int> mtc_time_arr { 0,0,0,0 };
        int frames_per_second = 25;
        std::atomic<bool> threadRunning;
        std::atomic<bool> threadDone;

        ///< \brief Should the mutex block?
        std::atomic<bool> mutexBlocks;

        std::string name;
        std::condition_variable condition;

        mutable std::mutex mutex;
        RtMidiOut *midiOut = 0;

  public:
        MTCSender();
        ~MTCSender();
        void play();
        void pause();
        void stop();
        void setTime(uint64_t nanos);
  private:
        void startThread();
        void stopThread();
        bool isThreadRunning() const;
        bool sendMtcPosition();
        void nanosToTime(uint64_t nanos);
        string timeAsString();
        string gettimeAsString ();
        void threadedFunction();
        static void setScheduling(std::thread &th, int policy, int priority);
        sched_param sch_params;

};
