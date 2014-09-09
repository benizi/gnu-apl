/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2014  Dr. Jürgen Sauermann

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../config.h"          // for HAVE_ macros from configure
#include "UserPreferences.hh"   // for preferences and command line options

#if defined(__sun) && defined(__SVR4)
# define NOMACROS
#endif

// check if ncurses (or curses) is usable and #include the proper header file.
// Usable means that one of the header files ncurses.h or curses.h AND a
// curses library are available
//
#if HAVE_LIBCURSES
# if HAVE_NCURSES_H

# include <ncurses.h>
#define CURSES_USABLE 1

# elif HAVE_CURSES_H

# include <curses.h>
#define CURSES_USABLE 1

# endif
#endif

// Then check for term.h and #include it
//
#if HAVE_TERM_H
# include <term.h>
#else
# undef CURSES_USABLE
#endif

// curses on Solaris #defines erase() and tab() which conflicts with e.g.
// vector::erase() and others
//
#ifdef erase
# undef erase
#endif

#ifdef tab
# undef tab
#endif

#include "Command.hh"
#include "Common.hh"
#include "DiffOut.hh"
#include "InputFile.hh"
#include "main.hh"
#include "LineInput.hh"
#include "Output.hh"
#include "PrintOperator.hh"
#include "Svar_DB.hh"

bool Output::use_curses = false;   // possibly overridden by uprefs
bool Output::colors_enabled = false;
bool Output::colors_changed = false;

int Output::color_CIN_foreground = 0;
int Output::color_CIN_background = 7;
int Output::color_COUT_foreground = 0;
int Output::color_COUT_background = 8;
int Output::color_CERR_foreground = 5;
int Output::color_CERR_background = 8;
int Output::color_UERR_foreground = 5;
int Output::color_UERR_background = 8;

/// a filebuf for stderr output
class ErrOut : public filebuf
{
protected:
   /// overloaded filebuf::overflow()
   virtual int overflow(int c);

public:
   /** a helper function telling whether the constructor for CERR was called
       if CERR is used before its constructor was called (which can happen in
       when constructors of static objects are called and use CERR) then a
       segmentation fault would occur.

       We avoid that by using get_CERR() instead of CERR in such constructors.
       get_CERR() checks \b used and returns cerr instead of CERR if it is
       false.
    **/
   filebuf * use()   { used = true;   return this; }

   /// true iff the constructor for CERR was called
   static bool used;   // set when CERR is constructed

} CERR_filebuf;   ///< a filebuf for CERR

bool ErrOut::used = false;

DiffOut DOUT_filebuf(false);
DiffOut UERR_filebuf(true);

// Android defines its own CIN, COUT, CERR, and UERR ostreams
#ifndef WANT_ANDROID

CinOut CIN_filebuf;
CIN_ostream CIN;

ostream COUT(&DOUT_filebuf);
ostream CERR(CERR_filebuf.use());
ostream UERR(&UERR_filebuf);

#endif

extern ostream & get_CERR();
ostream & get_CERR()
{
   return ErrOut::used ? CERR : cerr;
};

Output::ColorMode Output::color_mode = COLM_UNDEF;

/// CSI sequence for ANSI/VT100 escape sequences (ESC [)
#define CSI "\x1b["

/// VT100 escape sequence to change to cin color
char Output::color_CIN[MAX_ESC_LEN] = CSI "0;30;47m";

/// VT100 escape sequence to change to cout color
char Output::color_COUT[MAX_ESC_LEN] = CSI "0;30;48;2;255;255;255m";

/// VT100 escape sequence to change to cerr color
char Output::color_CERR[MAX_ESC_LEN] = CSI "0;35;48;2;255;255;255m";

/// VT100 escape sequence to change to uerr color
char Output::color_UERR[MAX_ESC_LEN] = CSI "0;35;48;2;255;255;255m";

/// VT100 escape sequence to reset colors to their default
char Output::color_RESET[MAX_ESC_LEN] = CSI "0;30;48;2;255;255;255m";

/// VT100 escape sequence to clear to end of line
char Output::clear_EOL[MAX_ESC_LEN] = CSI "K";

/// VT100 escape sequence to clear to end of screen
char Output::clear_EOS[MAX_ESC_LEN] = CSI "J";

/// VT100 escape sequence to exit attribute mode
char Output::exit_attr_mode[MAX_ESC_LEN] = CSI "m" "\x1B"  "(B";

char Output::ESC_CursorUp[MAX_ESC_LEN]    = { 0x1B, 0x5B, 0x41, 0 };
char Output::ESC_CursorDown[MAX_ESC_LEN]  = { 0x1B, 0x5B, 0x42, 0 };
char Output::ESC_CursorRight[MAX_ESC_LEN] = { 0x1B, 0x5B, 0x43, 0 };
char Output::ESC_CursorLeft[MAX_ESC_LEN]  = { 0x1B, 0x5B, 0x44, 0 };
char Output::ESC_CursorEnd[MAX_ESC_LEN]   = { 0x1B, 0x5B, 0x46, 0 };
char Output::ESC_CursorHome[MAX_ESC_LEN]  = { 0x1B, 0x5B, 0x48, 0 };
char Output::ESC_InsertMode[MAX_ESC_LEN]  = CSI "2~";
char Output::ESC_Delete[MAX_ESC_LEN]      = CSI "3~";

//-----------------------------------------------------------------------------
int
CinOut::overflow(int c)
{
   if (!InputFile::echo_current_file())   return 0;

   Output::set_color_mode(Output::COLM_INPUT);
   cerr << (char)c;
   return 0;
}
//-----------------------------------------------------------------------------
int
ErrOut::overflow(int c)
{
   Output::set_color_mode(Output::COLM_ERROR);
   cerr << (char)c;
   return 0;
}
//-----------------------------------------------------------------------------
void
Output::init(bool logit)
{
   if (!isatty(fileno(stdout))) cout.setf(ios::unitbuf);

#if CURSES_USABLE

   if (!use_curses)
      {
        if (logit)   CERR << "using ANSI terminal ESC sequences" << endl;
        return;
      }

   if (logit)   CERR << "initializing ESC sequences from libcurses"
                           << endl;

int errors = 0;

const int ret = setupterm(0, STDOUT_FILENO, 0);
   if (ret != 0)   ++errors;

   // read some ESC sequences
   //
   errors += read_ESC_sequence(clear_EOL, MAX_ESC_LEN, 0,
                               clr_eol);
   errors += read_ESC_sequence(clear_EOS, MAX_ESC_LEN, 0,
                               clr_eos);
   errors += read_ESC_sequence(exit_attr_mode, MAX_ESC_LEN, 0,
                               exit_attribute_mode);

   errors += read_ESC_sequence(color_CIN, MAX_ESC_LEN, 0,
                               set_foreground, color_CIN_foreground);
   errors += read_ESC_sequence(color_CIN, MAX_ESC_LEN, 1,
                               set_background, color_CIN_background);

   errors += read_ESC_sequence(color_COUT, MAX_ESC_LEN, 0,
                               set_foreground, color_COUT_foreground);
   errors += read_ESC_sequence(color_COUT, MAX_ESC_LEN, 1,
                               set_background, color_COUT_background);

   errors += read_ESC_sequence(color_CERR, MAX_ESC_LEN, 0,
                               set_foreground, color_CERR_foreground);
   errors += read_ESC_sequence(color_CERR, MAX_ESC_LEN, 1,
                               set_background, color_CERR_background);

   errors += read_ESC_sequence(color_UERR, MAX_ESC_LEN, 0,
                               set_foreground, color_UERR_foreground);
   errors += read_ESC_sequence(color_UERR, MAX_ESC_LEN, 1,
                               set_background, color_UERR_background);

   // cursor keys. This does not work currently because the keys reported
   // by: key_up, key_down, etc are different from the hardwired VT100 keys
   //
   // The other group: cursor_up, cursor_down, etc is closer but cursor_down
   // is linefeed
   //
#if 0
   errors += read_ESC_sequence(ESC_CursorUp,    MAX_ESC_LEN, 0, key_up);
   errors += read_ESC_sequence(ESC_CursorDown,  MAX_ESC_LEN, 0, key_down);
   errors += read_ESC_sequence(ESC_CursorLeft,  MAX_ESC_LEN, 0, key_left);
   errors += read_ESC_sequence(ESC_CursorRight, MAX_ESC_LEN, 0, key_right);
   errors += read_ESC_sequence(ESC_CursorEnd,   MAX_ESC_LEN, 0, key_end);
   errors += read_ESC_sequence(ESC_CursorHome,  MAX_ESC_LEN, 0, key_home);
   errors += read_ESC_sequence(ESC_InsertMode,  MAX_ESC_LEN, 0, key_ic);
   errors += read_ESC_sequence(ESC_Delete,      MAX_ESC_LEN, 0, key_dc);

   ESCmap::refresh_lengths();
#endif

   if (errors)
      {
        CERR <<
"\n*** use of libcurses was requested, but something went wrong during its\n"
"initialization. Expect garbled output and non-functional keys." << endl;
         use_curses = false;
      }
#endif
}
//-----------------------------------------------------------------------------
int
Output::read_ESC_sequence(char * dest, int destlen, int append, 
                          const char * str, int p1, int p2)
{
#if CURSES_USABLE
   if (str == 0)
      {
        CERR << "capability not in terminal description" << endl;
        return 1;
      }

   if (str == (char *)-1)
      {
        CERR << "capability not a string capability" << endl;
        return 1;
      }

// CERR << "BEFORE: ";
// for (int i = 0; i < strlen(dest); ++i)   CERR << " " << HEX2(dest[i]);
// CERR << endl;

   if (!append)   *dest = 0;

const int offset = strlen(dest);
const char * seq = tparm(str, p1, p2, 0, 0, 0, 0, 0, 0, 0);
const int seq_len = strlen(seq);

   if (seq_len + offset >= (destlen - 1))
      {
        CERR << "ESC sequence too long" << endl;
        return 1;
      }

   strncpy(dest + offset, seq, destlen - offset - 1);

//   CERR << "AFTER:  ";
//   for (int i = 0; i < strlen(dest); ++i)   CERR << " " << HEX2(dest[i]);
//   CERR << endl;

   return 0;

#else
   return 1;   // should not happen
#endif
}
//-----------------------------------------------------------------------------
int
Output::putc_stderr(TPUTS_arg3 ch)
{
   cerr << (char)ch;
   return ch;
}
//-----------------------------------------------------------------------------
int
Output::putc_stdout(TPUTS_arg3 ch)
{
   cout << (char)ch;
   return ch;
}
//-----------------------------------------------------------------------------
void
Output::reset_dout()
{
   DOUT_filebuf.reset();
}
//-----------------------------------------------------------------------------
void
Output::reset_colors()
{
   if (!colors_changed)   return;

   if (use_curses)
      {
        cout << exit_attr_mode << clear_EOL;
        cerr << exit_attr_mode << clear_EOL;
      }
   else
      {
        cout << color_RESET << clear_EOL;
        cerr << color_RESET << clear_EOL;
      }
}
//-----------------------------------------------------------------------------
void
Output::set_color_mode(Output::ColorMode mode)
{
   if (!colors_enabled)      return;   // colors disabled
   if (color_mode == mode)   return;   // no change in color mode

   // mode changed
   //
   color_mode = mode;
   colors_changed = true;   // to reset them in reset_colors()

   switch(color_mode)
      {
        case COLM_INPUT:  cerr << color_CIN  << clear_EOL;   break;

        case COLM_OUTPUT:
             if (use_curses)   cout << exit_attr_mode << clear_EOL;
             else              cout << color_COUT << clear_EOL;
             break;

        case COLM_ERROR:  cerr << color_CERR << clear_EOL;   break;

        case COLM_UERROR: cout << color_UERR << clear_EOL;   break;

        default: break;
      }
}
//-----------------------------------------------------------------------------
void 
Output::toggle_color(const UCS_string & arg)
{
int a = 0;
   while (a < arg.size() && arg[a] < UNI_ASCII_SPACE)   ++a;

   if (arg.starts_iwith("ON"))         colors_enabled = true;
   else if (arg.starts_iwith("OFF"))   colors_enabled = false;
   else                                colors_enabled = !colors_enabled;
}
//=============================================================================
void
CIN_ostream::set_cursor(int y, int x)
{
   if (uprefs.raw_cin)   return;

   if (y < 0)
      {
        // y < 0 means from bottom upwards
        //
        *this << "\x1B[30;" << (1 + x) << "H\x1B[99B";
        if (y < -1)   *this << "\x1B[" << (-(y + 1)) << "A";
      }
   else
      {
        *this << "\x1B[" << (1 + y) << ";" << (1 + x) << 'H' << std::flush;
      }
}
//-----------------------------------------------------------------------------
