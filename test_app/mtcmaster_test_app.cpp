//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// Just a small program to show the SLMtcMaster Class
// working and to test it.
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

// Std libraries
// #include <iostream>
// #include <stdio.h>
// #include <sys/resource.h>
// #include <iomanip>
// #include <string>
// #include <chrono>
// #include <thread>

// Library classes
#include "../MtcMaster_class.h"

// Local classes
#include "ScreenMan_class.h"

// Own header file
#include "mtcmaster_test_app.h"

/////////////////////////////////////////////////////////
// Escape characters defines / substitutions
#define ESC 27

using namespace std;

int main()
{
    string key(""); // Our key presses

    ScreenMan screen;

    screen.clrScr();
    screen.printAt(0, 0);

	MtcMaster *myMtcMaster = new MtcMaster(); // Our MTC Master Object
    cout << "Initializing..." << endl;
    cout << "Number of MTC Master instances: " << myMtcMaster->instanceCount << endl;
    cout << "Current API: " << myMtcMaster->getApiString() << endl;
	cout << "Current Frame Rate: " << (unsigned short)myMtcMaster->getFrameRate() << endl;
    myMtcMaster->openPort(0, "SLMTCPort");


	screen.printAt(10, 0, "Status: ");
	screen.mvCursor(19, 0);
    cout << "<1-9> para \"Avanzar n minutos\" - <-> para \"Retroceder 1 minuto\"" << endl;
    cout << "<P> para \"Play/Pause\" - <S> para \"Stop\"  - <Esc> para Salir" << endl;
    cout << "Any other key shows current MTC position" << endl;

	do
	{
        switch(key[0])
        {
        case 'P':
        case 'p':
            if (! myMtcMaster->getPlaying())
            {
                screen.printAt(10, 11,"Playing           ");
                cout << setw(30) << left  << "Playing MTC time from: " << myMtcMaster->mtcTimeVectorString() << " -> " << myMtcMaster->getMtcTime() << " nanosecs" << endl;
                myMtcMaster->play();
            }
            else
            {
                myMtcMaster->play();
                screen.printAt(10, 11,"Paused          ");
                cout << setw(30) << left  << "Paused MTC time at: " << myMtcMaster->mtcTimeVectorString() << " -> " << myMtcMaster->getMtcTime() << " nanosecs" << endl;
            }
            break;

        case ESC:
            screen.printAt(10, 11,"Let's get out of here!    ");
            break;

        case 'S':
        case 's':
            myMtcMaster->stop();
            screen.printAt(10, 11,"Stopped                ");
            cout << setw(30) << left << "Resetting MTC position: " << myMtcMaster->mtcTimeVectorString() << " -> " << myMtcMaster->getMtcTime() << " nanosecs" << endl;
            break;

        case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
            myMtcMaster->setTime(myMtcMaster->getTime() + (uint64_t)((key[0] - '0') * 60e9));
            break;

        case '-':
            myMtcMaster->setTime(myMtcMaster->getTime() - (uint64_t)60e9);
            break;

        default:
            screen.mvCursor(11, 0);
            cout << setw(30) << left << "Current MTC position: " << myMtcMaster->mtcTimeVectorString() << " -> " << myMtcMaster->getMtcTime() << " nanosecs" << endl;
            break;
        }

        screen.mvCursor(22,0);
        cin.sync(); cin.clear(std::iostream::iostate::_S_goodbit);
        getline(cin, key);
        cin.sync(); cin.clear(std::iostream::iostate::_S_goodbit);
        screen.printAt(22,0, "   ");

	} while (key[0] != ESC);

	screen.clrScr();
	screen.mvCursor(0,0);

	myMtcMaster->closePort();

	delete myMtcMaster;

    return 0;
}

