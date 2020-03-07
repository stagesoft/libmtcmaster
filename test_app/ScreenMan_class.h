//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// Screen Manager Class header file
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

#ifndef SCREENMAN_CLASS_H_INCLUDED
#define SCREENMAN_CLASS_H_INCLUDED

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

#define ESC 27
#define CLRSCR cout << "\033[2J"
#define LOCATE(x,y,s) (cout <<"\033["<< x <<';'<< y <<'H'<<s)

using namespace std;

class ScreenMan
{
    //////////////////////////////////////////
    public:
    //////////////////////////////////////////

    //////////////////////////////////////////
    // Constructors and destructors
    ScreenMan(const bool clearScreen = true) { if (clearScreen) cout << "\033[2J"; };

    // Public members

    // Public methods
    inline short getRow(void) { return cursorRow; };
    inline short getCol(void) { return cursorCol; };
    inline void setRowCol(const short row, const short col) { cursorRow = row; cursorCol = col; };

    inline void clrScr(void) { cout << "\033[2J"; };

    inline void mvCursor(const short row = cursorRow, const short col = cursorCol)
        { cout <<"\033["<< row <<';'<< col <<'H'; };
    inline void printAt(const short row = cursorRow, const short col = cursorCol, const string str = "")
        { cout <<"\033["<< row <<';'<< col <<'H'<<str<<endl; };

    //////////////////////////////////////////
    private:
    //////////////////////////////////////////
    static short cursorRow;
    static short cursorCol;
};

#endif // ScreenMan_CLASS_H_INCLUDED
