/*
https://github.com/peterix/dfhack
Copyright (c) 2011 Petr Mrázek <peterix@gmail.com>

A thread-safe logging console with a line editor for windows.

Based on linenoise win32 port,
copyright 2010, Jon Griffiths <jon_p_griffiths at yahoo dot com>.
All rights reserved.
Based on linenoise, copyright 2010, Salvatore Sanfilippo <antirez at gmail dot com>.
The original linenoise can be found at: http://github.com/antirez/linenoise

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of Redis nor the names of its contributors may be used
    to endorse or promote products derived from this software without
    specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/


#include <windows.h>
#include <conio.h>
#include <stdarg.h>

#include <process.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <fstream>
#include <istream>
#include <string>

#include "Console.h"
#include "Hooks.h"
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <deque>
using namespace DFHack;

#include "tinythread.h"
using namespace tthread;

// FIXME: maybe make configurable with an ini option?
#define MAX_CONSOLE_LINES 999;

namespace DFHack
{
    class Private : public std::stringbuf
    {
    public:
        Private() : basic_stringbuf<char>::basic_stringbuf()
        {
            dfout_C = 0;
            rawmode = 0;
            console_in = 0;
            console_out = 0;
            ConsoleWindow = 0;
            default_attributes = 0;
            state = con_unclaimed;
            raw_cursor = 0;
        };
        virtual ~Private()
        {
            //sync();
        }
    protected:
        int sync()
        {
            print (str().c_str());
            // Clear the string buffer
            str(std::string());
            return 0;
        }
    public:
        /// Print a formatted string, like printf
        int  print(const char * format, ...)
        {
            va_list args;
            va_start( args, format );
            int ret = vprint( format, args );
            va_end( args );
            return ret;
        }
        int  vprint(const char * format, va_list vl)
        {
            if(state == con_lineedit)
            {
                clearline();
                int ret = vfprintf( dfout_C, format, vl );
                prompt_refresh();
                return ret;
            }
            else return vfprintf( dfout_C, format, vl );
        }
        int  vprinterr(const char * format, va_list vl)
        {
            if(state == con_lineedit)
            {
                color(Console::COLOR_LIGHTRED);
                clearline();
                int ret = vfprintf( dfout_C, format, vl );
                reset_color();
                prompt_refresh();
                return ret;
            }
            else
            {
                color(Console::COLOR_LIGHTRED);
                int ret = vfprintf( dfout_C, format, vl );
                reset_color();
                return ret;
            }
        }
        /// Print a formatted string, like printf, in red
        int  printerr(const char * format, ...)
        {
            va_list args;
            va_start( args, format );
            int ret = vprinterr( format, args );
            va_end( args );
            return ret;
        }
        int get_columns(void)
        {
            CONSOLE_SCREEN_BUFFER_INFO inf = { 0 };
            GetConsoleScreenBufferInfo(console_out, &inf);
            return (size_t)inf.dwSize.X;
        }
        int get_rows(void)
        {
            CONSOLE_SCREEN_BUFFER_INFO inf = { 0 };
            GetConsoleScreenBufferInfo(console_out, &inf);
            return (size_t)inf.dwSize.Y;
        }
        void clear()
        {
            system("cls");
        }
        void clearline()
        {
            CONSOLE_SCREEN_BUFFER_INFO inf = { 0 };
            GetConsoleScreenBufferInfo(console_out, &inf);
            // Blank to EOL
            char* tmp = (char*)malloc(inf.dwSize.X);
            memset(tmp, ' ', inf.dwSize.X);
            output(tmp, inf.dwSize.X, 0, inf.dwCursorPosition.Y);
            free(tmp);
            COORD coord = {0, inf.dwCursorPosition.Y}; // Windows uses 0-based coordinates
            SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
        }
        void gotoxy(int x, int y)
        {
            COORD coord = {x-1, y-1}; // Windows uses 0-based coordinates
            SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
        }

        void color(int index)
        {
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, index);
        }

        void reset_color( void )
        {
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, default_attributes);
        }

        void cursor(bool enable)
        {
            if(enable)
            {
                HANDLE hConsoleOutput;
                CONSOLE_CURSOR_INFO structCursorInfo;
                hConsoleOutput = GetStdHandle( STD_OUTPUT_HANDLE );
                GetConsoleCursorInfo( hConsoleOutput, &structCursorInfo ); // Get current cursor size
                structCursorInfo.bVisible = TRUE;
                SetConsoleCursorInfo( hConsoleOutput, &structCursorInfo );
            }
            else
            {
                HANDLE hConsoleOutput;
                CONSOLE_CURSOR_INFO structCursorInfo;
                hConsoleOutput = GetStdHandle( STD_OUTPUT_HANDLE );
                GetConsoleCursorInfo( hConsoleOutput, &structCursorInfo ); // Get current cursor size
                structCursorInfo.bVisible = FALSE;
                SetConsoleCursorInfo( hConsoleOutput, &structCursorInfo );
            }
        }

        void output(const char* str, size_t len, int x, int y)
        {
            COORD pos = { (SHORT)x, (SHORT)y };
            DWORD count = 0;
            WriteConsoleOutputCharacterA(console_out, str, len, pos, &count);
        }

        void prompt_refresh()
        {
            size_t cols = get_columns();
            size_t plen = prompt.size();
            const char * buf = raw_buffer.c_str();
            size_t len = raw_buffer.size();

            while ((plen + raw_cursor) >= cols)
            {
                buf++;
                len--;
                raw_cursor--;
            }
            while (plen + len > cols)
            {
                len--;
            }

            CONSOLE_SCREEN_BUFFER_INFO inf = { 0 };
            GetConsoleScreenBufferInfo(console_out, &inf);
            output(prompt.c_str(), plen, 0, inf.dwCursorPosition.Y);
            output(buf, len, plen, inf.dwCursorPosition.Y);
            if (plen + len < (size_t)inf.dwSize.X)
            {
                // Blank to EOL
                char* tmp = (char*)malloc(inf.dwSize.X - (plen + len));
                memset(tmp, ' ', inf.dwSize.X - (plen + len));
                output(tmp, inf.dwSize.X - (plen + len), len + plen, inf.dwCursorPosition.Y);
                free(tmp);
            }
            inf.dwCursorPosition.X = (SHORT)(raw_cursor + plen);
            SetConsoleCursorPosition(console_out, inf.dwCursorPosition);
        }

        int prompt_loop(mutex * lock, CommandHistory & history)
        {
            raw_buffer.clear(); // make sure the buffer is empty!
            size_t plen = prompt.size();
            raw_cursor = 0;
            int history_index = 0;
            // The latest history entry is always our current buffer, that
            // initially is just an empty string.
            const std::string empty;
            history.add(empty);

            CONSOLE_SCREEN_BUFFER_INFO inf = { 0 };
            GetConsoleScreenBufferInfo(console_out, &inf);
            size_t cols = inf.dwSize.X;
            output(prompt.c_str(), plen, 0, inf.dwCursorPosition.Y);
            inf.dwCursorPosition.X = (SHORT)plen;
            SetConsoleCursorPosition(console_out, inf.dwCursorPosition);

            while (1)
            {
                INPUT_RECORD rec;
                DWORD count;
                lock->unlock();
                ReadConsoleInputA(console_in, &rec, 1, &count);
                lock->lock();
                if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown)
                    continue;
                switch (rec.Event.KeyEvent.wVirtualKeyCode)
                {
                case VK_RETURN: // enter
                    history.remove();
                    return raw_buffer.size();
                case VK_BACK:   // backspace
                    if (raw_cursor > 0 && raw_buffer.size() > 0)
                    {
                        raw_buffer.erase(raw_cursor-1,1);
                        raw_cursor--;
                        prompt_refresh();
                    }
                    break;
                case VK_LEFT: // left arrow
                    if (raw_cursor > 0)
                    {
                        raw_cursor--;
                        prompt_refresh();
                    }
                    break;
                case VK_RIGHT: // right arrow
                    if (raw_cursor != raw_buffer.size())
                    {
                        raw_cursor++;
                        prompt_refresh();
                    }
                    break;
                case VK_UP:
                case VK_DOWN:
                    // up and down arrow: history
                    if (history.size() > 1)
                    {
                        // Update the current history entry before to
                        // overwrite it with tne next one.
                        history[history_index] = raw_buffer;
                        // Show the new entry
                        history_index += (rec.Event.KeyEvent.wVirtualKeyCode == VK_UP) ? 1 : -1;
                        if (history_index < 0)
                        {
                            history_index = 0;
                            break;
                        }
                        else if (history_index >= history.size())
                        {
                            history_index = history.size()-1;
                            break;
                        }
                        raw_buffer = history[history_index];
                        raw_cursor = raw_buffer.size();
                        prompt_refresh();
                    }
                    break;
                case VK_DELETE:
                    // delete
                    if (raw_buffer.size() > 0 && raw_cursor < raw_buffer.size())
                    {
                        raw_buffer.erase(raw_cursor,1);
                        prompt_refresh();
                    }
                    break;
                case VK_HOME:
                    raw_cursor = 0;
                    prompt_refresh();
                    break;
                case VK_END:
                    raw_cursor = raw_buffer.size();
                    prompt_refresh();
                    break;
                default:
                    if (rec.Event.KeyEvent.uChar.AsciiChar < ' ' ||
                        rec.Event.KeyEvent.uChar.AsciiChar > '~')
                        continue;
                    if (raw_buffer.size() == raw_cursor)
                        raw_buffer.append(1,rec.Event.KeyEvent.uChar.AsciiChar);
                    else
                        raw_buffer.insert(raw_cursor,1,rec.Event.KeyEvent.uChar.AsciiChar);
                    raw_cursor++;
                    prompt_refresh();
                    break;
                }
            }
        }
        int lineedit(const std::string & prompt, std::string & output, mutex * lock, CommandHistory & ch)
        {
            output.clear();
            int count;
            state = con_lineedit;
            this->prompt = prompt;
            count = prompt_loop(lock, ch);
            if(count != -1)
                output = raw_buffer;
            state = con_unclaimed;
            print("\n");
            return count;
        }

        FILE * dfout_C;
        int rawmode;
        HANDLE console_in;
        HANDLE console_out;
        HWND ConsoleWindow;
        HWND MainWindow;
        WORD default_attributes;
        // current state
        enum console_state
        {
            con_unclaimed,
            con_lineedit
        } state;
        std::string prompt;     // current prompt string
        std::string raw_buffer; // current raw mode buffer
        int raw_cursor;         // cursor position in the buffer
    };
}


Console::Console():std::ostream(0), std::ios(0)
{
    d = 0;
    wlock = 0;
    inited = false;
}

Console::~Console()
{
}
/*
// DOESN'T WORK - locks up DF!
void ForceForegroundWindow(HWND window)
{
    DWORD nForeThread, nAppThread;

    nForeThread = GetWindowThreadProcessId(GetForegroundWindow(), 0);
    nAppThread = ::GetWindowThreadProcessId(window,0);

    if(nForeThread != nAppThread)
    {
        AttachThreadInput(nForeThread, nAppThread, true);
        BringWindowToTop(window);
        ShowWindow(window,3);
        AttachThreadInput(nForeThread, nAppThread, false);
    }
    else
    {
        BringWindowToTop(window);
        ShowWindow(window,3);
    }
}
*/
bool Console::init(bool)
{
    d = new Private();
    int                        hConHandle;
    long                       lStdHandle;
    CONSOLE_SCREEN_BUFFER_INFO coninfo;
    FILE                       *fp;
    DWORD  oldMode, newMode;
    DWORD dwTheardId;

    HWND h = ::GetTopWindow(0 );
    while ( h )
    {
        DWORD pid;
        dwTheardId = ::GetWindowThreadProcessId( h,&pid);
        if ( pid == GetCurrentProcessId() )
        {
            // here h is the handle to the window
            break;
        }
        h = ::GetNextWindow( h , GW_HWNDNEXT);
    }
    d->MainWindow = h;

    // Allocate a console!
    AllocConsole();
    d->ConsoleWindow = GetConsoleWindow();
    wlock = new mutex();
    HMENU  hm = GetSystemMenu(d->ConsoleWindow,false);
    DeleteMenu(hm, SC_CLOSE, MF_BYCOMMAND);

    // set the screen buffer to be big enough to let us scroll text
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
    d->default_attributes = coninfo.wAttributes;
    coninfo.dwSize.Y = MAX_CONSOLE_LINES;  // How many lines do you want to have in the console buffer
    SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

    // redirect unbuffered STDOUT to the console
    d->console_out = GetStdHandle(STD_OUTPUT_HANDLE);
    lStdHandle = (long)d->console_out;
    hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
    d->dfout_C = _fdopen( hConHandle, "w" );
    setvbuf( d->dfout_C, NULL, _IONBF, 0 );

    // redirect unbuffered STDIN to the console
    d->console_in = GetStdHandle(STD_INPUT_HANDLE);
    lStdHandle = (long)d->console_in;
    hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
    fp = _fdopen( hConHandle, "r" );
    *stdin = *fp;
    setvbuf( stdin, NULL, _IONBF, 0 );
    GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),&oldMode);
    newMode = oldMode | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT;
    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),newMode);
    SetConsoleCtrlHandler(NULL,true);
    std::ios::sync_with_stdio();

    // make our own weird streams so our IO isn't redirected
    rdbuf(d);
    std::cin.tie(this);
    clear();
    inited = true;
    // DOESN'T WORK - locks up DF!
    // ForceForegroundWindow(d->MainWindow);
    return true;
}
// FIXME: looks awfully empty, doesn't it?
bool Console::shutdown(void)
{
    lock_guard <mutex> g(*wlock);
    FreeConsole();
    inited = false;
    return true;
}
int Console::print( const char* format, ... )
{
    va_list args;
    lock_guard <mutex> g(*wlock);
    int ret;
    if(!inited) ret = -1;
    else
    {
        va_start( args, format );
        ret = d->vprint(format, args);
        va_end(args);
    }
    return ret;
}

int Console::printerr( const char* format, ... )
{
    va_list args;
    lock_guard <mutex> g(*wlock);
    int ret;
    // also mirror in error log
    if(!inited)
    {
        va_start( args, format );
        ret = vfprintf(stderr, format, args);
        va_end(args);
    }
    else
    {
        va_start( args, format );
        ret = d->vprinterr(format, args);
        vfprintf(stderr, format, args);
        va_end(args);
    }
    return ret;
}

int Console::get_columns(void)
{
    lock_guard <mutex> g(*wlock);
    int ret = -1;
    if(inited)
        ret = d->get_columns();
    return ret;
}

int Console::get_rows(void)
{
    lock_guard <mutex> g(*wlock);
    int ret = -1;
    if(inited)
        ret = d->get_rows();
    return ret;
}

void Console::clear()
{
    lock_guard <mutex> g(*wlock);
    if(inited)
        d->clear();
}

void Console::gotoxy(int x, int y)
{
    lock_guard <mutex> g(*wlock);
    if(inited)
        d->gotoxy(x,y);
}

void Console::color(color_value index)
{
    lock_guard <mutex> g(*wlock);
    if(inited)
        d->color(index);
}

void Console::reset_color( void )
{
    lock_guard <mutex> g(*wlock);
    if(inited)
        d->reset_color();
}

void Console::cursor(bool enable)
{
    lock_guard <mutex> g(*wlock);
    if(inited)
        d->cursor(enable);
}

int Console::lineedit(const std::string & prompt, std::string & output, CommandHistory & ch)
{
    wlock->lock();
    int ret = -2;
    if(inited)
        ret = d->lineedit(prompt,output,wlock,ch);
    wlock->unlock();
    return ret;
}

void Console::msleep (unsigned int msec)
{
    Sleep(msec);
}
