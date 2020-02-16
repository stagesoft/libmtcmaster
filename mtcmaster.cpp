

#include "mtcmaster.h"


using std::vector;
using std::string;

        MTCSender::MTCSender() {
            midiOut = new RtMidiOut();
            midiOut->openVirtualPort("MTC out");
            threadRunning = false;
            threadDone = true;
        }

        MTCSender::~MTCSender() {
            delete midiOut;
        }

        void MTCSender::play(){
            setTime(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
            MTCSender::startThread();
        }

        void MTCSender::pause(){
           MTCSender::stopThread();
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
            setTime(1);
        }

        void MTCSender::stop(){
            MTCSender::stopThread();
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Temp fix, we should check thread as finished
            setTime(0);

        }

        void MTCSender::startThread(){
         //   std::unique_lock<std::mutex> lck(mutex);
            if(threadRunning || !threadDone){
                std::cout << "Thread allready running" << std::endl;
                return;
            }

            threadDone = false;
            threadRunning = true;
      //      this->mutexBlocks = true;

            std::thread threadObj(&MTCSender::threadedFunction, this);
            setScheduling(threadObj, SCHED_RR, 99);
            threadObj.detach();

        }


        void MTCSender::stopThread(){
            threadRunning = false;
            threadDone = true;
        }

        bool MTCSender::isThreadRunning() const{
            return threadRunning;
        }


        void MTCSender::setTime(uint64_t nanos = 1) {
            if (nanos == 1) {
                sendMtcPosition();
            }else {
                mtc_time = nanos;
                nanosToTime(mtc_time);
                sendMtcPosition();
            }
            std::cout << " - sending mtc position:" << timeAsString() << " - " << mtc_time << std::endl;
        }



        bool MTCSender::sendMtcPosition(){
            vector<unsigned char> mtc_full;
            mtc_full.push_back(MIDI_SYSEX);
            mtc_full.push_back(0x7F);
            mtc_full.push_back(0x7F); // SysEx channel
            mtc_full.push_back(0x01); //
            mtc_full.push_back(0x01); //
            mtc_full.push_back(0x20 | (mtc_time_arr[3] & 0xf)); // hour (only high byte)
            mtc_full.push_back((mtc_time_arr[2] & 0xff)); // Minute
            mtc_full.push_back((mtc_time_arr[1] & 0xff)); // Second
            mtc_full.push_back((mtc_time_arr[0] & 0xff)); // Frame
            mtc_full.push_back(MIDI_SYSEX_END);

            midiOut->sendMessage( &mtc_full );

            return true;

              // Program change: 192, 5           

        }

       void MTCSender::nanosToTime(uint64_t nanos) {

                int millis = nanos / 1000000;
                mtc_time_arr.clear();
                mtc_time_arr.push_back((millis / 40) % 25); // FRAMES 0
                mtc_time_arr.push_back(( millis /  1000) % 60); // SECONDS 1
                mtc_time_arr.push_back(((millis / (1000*60)) % 60)); // MINUTES 2
                mtc_time_arr.push_back(((millis / (1000*60*60)) % 24)); // HOURS 3


        }

        string MTCSender::timeAsString()
        {

                string timeAsString = "";
                if( mtc_time_arr[3] < 10 ) timeAsString = timeAsString + "0";
                timeAsString = timeAsString + std::to_string(mtc_time_arr[3]);
                timeAsString = timeAsString + ":";

                if( mtc_time_arr[2] < 10 ) timeAsString = timeAsString + "0";
                timeAsString = timeAsString + std::to_string(mtc_time_arr[2]);
                timeAsString = timeAsString + ":";

                if( mtc_time_arr[1] < 10 ) timeAsString = timeAsString + "0";
                timeAsString = timeAsString + std::to_string(mtc_time_arr[1]);
                timeAsString = timeAsString + ":";


                if( mtc_time_arr[0] < 10 )   timeAsString = timeAsString + "0";

                timeAsString = timeAsString + std::to_string(mtc_time_arr[0]);

                return timeAsString;
        }

        string MTCSender::gettimeAsString (){
            return timeAsString();

        }

        void MTCSender::setScheduling(std::thread &th, int policy, int priority){
            sched_param sch_params;
            sch_params.sched_priority = priority;
            if(pthread_setschedparam(th.native_handle(), policy, &sch_params)) {
                std::cout << "Failed to set Thread scheduling : " << std::endl;
            }
        }

        void MTCSender::threadedFunction(){


                uint64_t elapsed = 0;
                vector<unsigned char> mtc_quarter {0,0};
                vector<unsigned char> mtc_full1;
                unsigned char mtc_timecode_bits = 0;
                switch ((int) ceil (frames_per_second)) {
                case 24:
                    mtc_timecode_bits = 0;
                    break;

                case 25:
                    mtc_timecode_bits = 0x20;
                    std::cout << "25 fps" << std::endl;
                    break;
// TODO Implement other fps options
//
//                case 30:
//                default:
//                    if (timecode_drop_frames()) {
//                        mtc_timecode_bits = 0x40;
//                    } else {
//                        mtc_timecode_bits =  0x60;
//                    }
//                    break;
                };

                struct timespec delay;
                delay.tv_sec = 0;

                time_zero = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                lastQuarterSent = time_zero;
         //       time_zero -= mtc_time;


                int type = 0;
                mtc_quarter.clear();

                while(isThreadRunning()){


                    unsigned char c = 0;

                            switch (type) {
                            case 0:
                                c = 0x00 | ((unsigned char) mtc_time_arr[0] & 0xf);
                                break;
                            case 1:
                                c = 0x10 | ((mtc_time_arr[0] & 0xf0) >> 4);
                                break;
                            case 2:
                                c = 0x20 | (mtc_time_arr[1] & 0xf);
                                break;
                            case 3:
                                c = 0x30 | ((mtc_time_arr[1] & 0xf0) >> 4);
                                break;
                            case 4:
                                c = 0x40 | (mtc_time_arr[2] & 0xf);
                                break;
                            case 5:
                                c = 0x50 | ((mtc_time_arr[2] & 0xf0) >> 4);
                                break;
                            case 6:
                                c = 0x60 | ((mtc_timecode_bits | mtc_time_arr[3]) & 0xf);
                                break;
                            case 7:
                                c = 0x70 | (((mtc_timecode_bits | mtc_time_arr[3]) & 0xf0) >> 4);
                                break;
                            }

                        // log timecode
                          //    std::cout << "\r" << timeAsString() << std::flush;

                            if (++type == 8) {
                                type = 0;
                            }

                        mtc_quarter.push_back(MIDI_TIME_CODE);
                        mtc_quarter.push_back(c);

                        
                        midiOut->sendMessage( &mtc_quarter );

                        mtc_quarter.clear();


                        time_now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

                        nextQuarterToSend = lastQuarterSent + 10000000;
                        lastQuarterSent = nextQuarterToSend;

                        if (nextQuarterToSend < time_now) {
                            std::cout << std::dec << "TIME OUT!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
                        }

                        elapsed = (time_now - time_zero)+ nextQuarterToSend - time_now;
                        delay.tv_nsec = nextQuarterToSend - time_now ;
                        nanosToTime(mtc_time + elapsed);
                        nanosleep(&delay, NULL);
                }


                mtc_time += elapsed;
                std::cout << " - mtc time de pause:" << timeAsString() << " - " << mtc_time << std::endl;
                nanosToTime(mtc_time);
                mtc_time = (uint64_t)mtc_time_arr[0]*40*1000000 + (uint64_t)mtc_time_arr[1]*1000*1000000 + (uint64_t)mtc_time_arr[2]*60*1000*1000000 + (uint64_t)mtc_time_arr[3]*3600*1000*1000000 ; // redondeo para empezar al princio del frame
                std::cout << " -  mtc time redonde" << timeAsString() << " - " << mtc_time << std::endl;
        };


