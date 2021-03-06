/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2016  Dr. Jürgen Sauermann

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

#include <math.h>
#include <string.h>

#include "Common.hh"
#include "FloatCell.hh"
#include "Heapsort.hh"
#include "Output.hh"
#include "PrintBuffer.hh"
#include "PrintOperator.hh"
#include "UCS_string.hh"
#include "UTF8_string.hh"
#include "Value.icc"

ShapeItem UCS_string::total_count = 0;
ShapeItem UCS_string::total_id = 0;

//-----------------------------------------------------------------------------
UCS_string::UCS_string()
{
   create(LOC);
}
//-----------------------------------------------------------------------------
UCS_string::UCS_string(Unicode uni)
   : Simple_string<Unicode, false>(1, uni)
{
  create(LOC);
}
//-----------------------------------------------------------------------------
UCS_string::UCS_string(const Unicode * data, size_t len)
   : Simple_string<Unicode, false>(data, len)
{
   create(LOC);
}
//-----------------------------------------------------------------------------
UCS_string::UCS_string(size_t len, Unicode uni)
   : Simple_string<Unicode, false>(len, uni)
{
   create(LOC);
}
//-----------------------------------------------------------------------------
UCS_string::UCS_string(const UCS_string & ucs)
   : Simple_string<Unicode, false>(ucs)
{
   create(LOC);
}
//-----------------------------------------------------------------------------
UCS_string::UCS_string(const UCS_string & ucs, size_t pos, size_t len)
   : Simple_string<Unicode, false>(ucs, pos, len)
{
   create(LOC);
}
//-----------------------------------------------------------------------------
UCS_string::UCS_string(const char * cstring)
{
   create(LOC);

const int len = strlen(cstring);
   extend(len + 1);

   while (items_valid < len)   items[items_valid++] = Unicode(*cstring++);
   items[items_valid] = UNI_ASCII_NUL;
}
//-----------------------------------------------------------------------------
UCS_string::UCS_string(const UTF8_string & utf)
{
   create(LOC);

   Log(LOG_char_conversion)
      CERR << "UCS_string::UCS_string(): utf = " << utf << endl;

   for (int i = 0; i < utf.size();)
      {
        const uint32_t b0 = utf[i++];
        uint32_t bx = b0;
        uint32_t more;
        if      ((b0 & 0x80) == 0x00)   { more = 0;             }
        else if ((b0 & 0xE0) == 0xC0)   { more = 1; bx &= 0x1F; }
        else if ((b0 & 0xF0) == 0xE0)   { more = 2; bx &= 0x0F; }
        else if ((b0 & 0xF8) == 0xF0)   { more = 3; bx &= 0x0E; }
        else if ((b0 & 0xFC) == 0xF8)   { more = 4; bx &= 0x07; }
        else if ((b0 & 0xFE) == 0xFC)   { more = 5; bx &= 0x03; }
        else
           {
             utf.dump_hex(CERR << "Bad UTF8 string: ", 40)
                               << " at " << LOC <<  endl;
             Backtrace::show(__FILE__, __LINE__);
             return;
           }

        uint32_t uni = 0;
        for (; more; --more)
            {
              if (i >= utf.size())
                 {
                   utf.dump_hex(CERR << "Truncated UTF8 string: ", 40)
                      << " len " << utf.size() << " at " << LOC <<  endl;
                   if (utf.size() >= 40)
                      {
                         const UTF8_string end(&utf[utf.size() - 10], 10);
                         end.dump_hex(CERR << endl << "(ending with : ", 20)
                                           << ")" << endl;
                      }
                   return;
                 }

              const UTF8 subc = utf[i++];
              if ((subc & 0xC0) != 0x80)
                 {
                   utf.dump_hex(CERR << "Bad UTF8 string: ", 40)
                      << " len " << utf.size() << " at " << LOC <<  endl;
                   if (utf.size() >= 40)
                      {
                         const UTF8_string end(&utf[utf.size() - 10], 10);
                         end.dump_hex(CERR << endl << "(ending with : ", 20)
                                           << ")" << endl;
                      }
                   Backtrace::show(__FILE__, __LINE__);
                   return;
                 }

              bx  <<= 6;
              uni <<= 6;
              uni |= subc & 0x3F;
            }

         append(Unicode(bx | uni));
      }

   Log(LOG_char_conversion)
      CERR << "UCS_string::UCS_string(): ucs = " << *this << endl;
}
//-----------------------------------------------------------------------------
UCS_string::UCS_string(APL_Float value, bool & scaled,
                       const PrintContext & pctx)
{
   create(LOC);

int quad_pp = pctx.get_PP();
   if (quad_pp > MAX_Quad_PP)   quad_pp = MAX_Quad_PP;
   if (quad_pp < MIN_Quad_PP)   quad_pp = MIN_Quad_PP;

const bool negative = (value < 0);
   if (negative)   value = -value;

int expo = 0;

   if (value >= 10.0)   // large number, positive exponent
      {
       while (value >= 1e16)
             {
               if (value > 1e307)
                  {
                    if (negative)   append_utf8("¯∞");
                    else            append_utf8("∞");
                    FloatCell::map_FC(*this);
                    return;
                  }
               value *= 1e-16;   expo += 16;
             }
       while (value >= 1e4)    { value *= 1e-4;    expo +=  4; }
       while (value >= 1e1)    { value *= 1e-1;    ++expo;     }
      }
   else if (value < 1.0)   // small number, negative exponent
      {
       while (value < 1e-16)
             {
               if (value < 1e-305)   // very small number: make it 0
                  {
                    append(UNI_ASCII_0);
                    return;
                  }
               value *= 1e16;   expo -= 16;
             }
       while (value < 1e-4)    { value *= 1e4;    expo -=  4; }
       while (value < 1.0)     { value *= 10.0;   --expo;     }
      }

   // In theory, at this point, 1.0 ≤ value < 10.0. In reality value can
   // be outside, though, due to rounding errors.

   // create a string with quad_pp + 1 significant digits.
   // The last digit is used for rounding and then discarded.
   //
UCS_string digits;
   loop(d, (quad_pp + 2))
      {
        if (value >= 10.0)
           {
             digits.append((Unicode)(10 + '0'));
             value = 0.0;
           }
        else if (value < 0.0)
           {
             digits.append((Unicode)('0'));
             value = 0.0;
           }
        else
           {
             const int dig = (int)value;
             value -= dig;
             value *= 10;
             digits.append((Unicode)(dig + '0'));
           }
      }

   if (digits[0] != '0')   digits.pop();

   // round last digit
   //
const Unicode last = digits.last();
   digits.pop();

   if (last >= '5')   digits.last() = (Unicode)(digits.last() + 1);
 
   // adjust carries of 2nd to last digit
   //
   for (int d = digits.size() - 1; d > 0; --d)   // all but first
       {
        if (digits[d] > '9')
           {
             digits[d] =     (Unicode)(digits[d]     - 10);
             digits[d - 1] = (Unicode)(digits[d - 1] +  1);
           }
       }

   // adjust carry of 1st digit
   //
   if (digits[0] > '9')
      {
        digits[0] = (Unicode)(digits[0] - 10);
        digits.insert_before(0, UNI_ASCII_1);
        ++expo;
        digits.pop();
      }

   // remove trailing zeros
   //
   while (digits.size() && digits.last() == UNI_ASCII_0)   digits.pop();

   // force scaled format if:
   //
   // value < .00001     ( value has ≥ 5 leading zeroes)
   // value > 10⋆quad_pp ( integer larger than ⎕PP)
   //
   if ((expo < -6) || (expo > quad_pp))   scaled = true;

   if (negative)   append(UNI_OVERBAR);

   if (scaled)
      {
        append(digits[0]);       // integer part
        if (digits.size() > 1)   // fractional part
           {
             append(UNI_ASCII_FULLSTOP);
             loop(d, (digits.size() - 1))   append(digits[d + 1]);
           }
        if (expo < 0)
           {
             append(UNI_ASCII_E);
             append(UNI_OVERBAR);
             append_number(-expo);
           }
        else if (expo > 0)
           {
             append(UNI_ASCII_E);
             append_number(expo);
           }
        else if (!(pctx.get_style() & PST_NO_EXPO_0)) // expo == 0
           {
             append(UNI_ASCII_E);
             append(UNI_ASCII_0);
           }
      }
   else
      {
        if (expo < 0)   // 0.000...
           {
             append(UNI_ASCII_0);
             append(UNI_ASCII_FULLSTOP);
             loop(e, (-(expo + 1)))   append(UNI_ASCII_0);
             append(digits);
           }
        else   // expo >= 0
           {
             loop(e, expo + 1)
                {
                  if (e < digits.size())   append(digits[e]);
                  else                     append(UNI_ASCII_0);
                }

             if ((expo + 1) < digits.size())   // there are fractional digits
                {
                  append(UNI_ASCII_FULLSTOP);
                  for (int e = expo + 1; e < digits.size(); ++e)
                     {
                       if (e < digits.size())   append(digits[e]);
                       else                     break;
                     }
                }
           }
      }

   FloatCell::map_FC(*this);
}
//-----------------------------------------------------------------------------
UCS_string::UCS_string(const PrintBuffer & pb, Rank rank, int quad_PW)
{
   create(LOC);

   if (pb.get_height() == 0)   return;      // empty PrintBuffer

const int total_width = pb.get_width(0);

Simple_string<int, false> breakpoints;
   breakpoints.reserve(2*total_width/quad_PW);

   // print rows, breaking at breakpoints
   //
   loop(row, pb.get_height())
       {
         if (row)   append(UNI_ASCII_LF);   // end previous row
         int col = 0;
         int b = 0;

         while (col < total_width)
            {
              int chunk_len;
              if (row == 0)   // first row: set up breakpoints
                 {
                   chunk_len = pb.get_line(0).compute_chunk_length(quad_PW,col);
                   breakpoints.append(chunk_len);
                 }
              else
                 {
                   chunk_len = breakpoints[b++];
                 }

              if (col)   append_utf8("\n      ");
              UCS_string trow(pb.get_line(row), col, chunk_len);
              trow.remove_trailing_padchars();
              append(trow);

              col += chunk_len;
            }
       }

   // replace pad chars with blanks.
   //
   loop(u, size())
       {
         if (is_iPAD_char(at(u)))   at(u) = UNI_ASCII_SPACE;
       }
}
//-----------------------------------------------------------------------------
int
UCS_string::compute_chunk_length(int quad_PW, int col) const
{
int chunk_len = quad_PW;

   if (col)   chunk_len -= 6;   // subsequent line inden

int pos = col + chunk_len;
   if (pos >= size())   return size() - col;

   while (--pos > col)
      {
         const Unicode uni = at(pos);
         if (uni == UNI_iPAD_U2 || uni == UNI_iPAD_U3)
            {
               chunk_len = pos - col + 1;
               break;
            }
      }

   return chunk_len;
}
//-----------------------------------------------------------------------------
void
UCS_string::remove_trailing_padchars()
{
   // remove trailing pad chars from align() and append_string(),
   // but leave other pad chars intact.
   // But only if the line has no frame (vert).
   //

   // If the line contains UNI_iPAD_L0 (higher dimension separator)
   // then discard all chars.
   //
   loop(u, size())
       {
         if (at(u) == UNI_LINE_VERT)    break;
         if (at(u) == UNI_LINE_VERT2)   break;
         if (at(u) == UNI_iPAD_L0)
            {
              shrink(0);
              return;
            }
       }

   while (size())
      {
        const Unicode last = back();
        if (last == UNI_iPAD_L0 ||
            last == UNI_iPAD_L1 ||
            last == UNI_iPAD_L2 ||
            last == UNI_iPAD_L3 ||
            last == UNI_iPAD_L4 ||
            last == UNI_iPAD_U7)
            pop_back();
        else
            break;
      }
}
//-----------------------------------------------------------------------------
void
UCS_string::remove_trailing_whitespaces()
{
   while (size() && last() <= UNI_ASCII_SPACE)   pop();
}
//-----------------------------------------------------------------------------
void
UCS_string::remove_leading_whitespaces()
{
int count = 0;
   loop(s, size())
      {
        if (at(s) <= UNI_ASCII_SPACE)   ++count;
        else                            break;
      }

   if (count == 0)        return;      // no leading whitspaces
   if (count == size())   shrink(0);   // only whitespaces
   else                   *this = UCS_string(*this, count, size() - count);
}
//-----------------------------------------------------------------------------
void
UCS_string::split_ws(UCS_string & rest)
{
   remove_leading_and_trailing_whitespaces();

   loop(clen, size())
       {
         if (Avec::is_white(at(clen)))   // whilespace: end of command
            {
              ShapeItem arg = clen;
              while (arg < size() && Avec::is_white(at(arg)))   ++arg;
              while (arg < size())   rest.append(at(arg++));
              shrink(clen);
              return;
            }
       }
}
//-----------------------------------------------------------------------------
/// constructor
UCS_string::UCS_string(const Value & value)
   : Simple_string<Unicode, false>(0, 0)
{
   create(LOC);

   if (value.get_rank() > 1) RANK_ERROR;

const ShapeItem ec = value.element_count();
   reserve(ec);

   loop(e, ec)   append(value.get_ravel(e).get_char_value());
}
//-----------------------------------------------------------------------------
UCS_string::UCS_string(istream & in)
   : Simple_string<Unicode, false>(0, 0)
{
   create(LOC);

   for (;;)
      {
        const Unicode uni = UTF8_string::getc(in);
        if (uni == Invalid_Unicode)   return;
        if (uni == UNI_ASCII_LF)      return;
        append(uni);
      }
}
//-----------------------------------------------------------------------------
void
UCS_string::copy_black(UCS_string & dest, int & idx) const
{
   while (idx < size() && operator[](idx) <= ' ')   ++idx;
   while (idx < size() && operator[](idx) >  ' ')   dest.append(operator[](idx++));
   while (idx < size() && operator[](idx) <= ' ')   ++idx;
}
//-----------------------------------------------------------------------------
ShapeItem
UCS_string::LF_count() const
{
ShapeItem count = 0;
   loop(u, size())   if (at(u) == UNI_ASCII_LF)   ++count;
   return count;
}
//-----------------------------------------------------------------------------
ShapeItem
UCS_string::substr_pos(const UCS_string & sub) const
{
const ShapeItem start_positions = 1 + size() - sub.size();
   loop(start, start_positions)
      {
        bool mismatch = false;
        loop(u, sub.size())
           {
             if (at(start + u) != sub[u])
                {
                  mismatch = true;
                  break;
                }
           }

        if (!mismatch)   return start;   // found sub at start
      }

   return -1;   // not found
}
//-----------------------------------------------------------------------------
bool 
UCS_string::has_black() const
{
   loop(s, size())   if (!Avec::is_white(at(s)))   return true;
   return false;
}
//-----------------------------------------------------------------------------
bool 
UCS_string::starts_with(const char * prefix) const
{
   loop(s, size())
      {
        const char pc = *prefix++;
        if (pc == 0)   return true;   // prefix matches this string.

        const Unicode uni = at(s);
        if (uni != Unicode(pc))   return false;
      }

   // strings agree, but prefix is longer
   //
   return false;   
}
//-----------------------------------------------------------------------------
bool 
UCS_string::starts_with(const UCS_string & prefix) const
{
   if (prefix.size() > size())   return false;

   loop(p, prefix.size())   if (at(p) != prefix[p])   return false;

   return true;
}
//-----------------------------------------------------------------------------
bool 
UCS_string::starts_iwith(const char * prefix) const
{
   loop(s, size())
      {
        char pc = *prefix++;
        if (pc == 0)   return true;   // prefix matches this string.
        if (pc >= 'a' && pc <= 'z')   pc -= 'a' - 'A';

        int uni = at(s);
        if (uni >= 'a' && uni <= 'z')   uni -= 'a' - 'A';

        if (uni != Unicode(pc))   return false;
      }

   return *prefix == 0;   
}
//-----------------------------------------------------------------------------
bool 
UCS_string::starts_iwith(const UCS_string & prefix) const
{
   if (prefix.size() > size())   return false;

   loop(p, prefix.size())
      {
        int c1 = at(p);
        int c2 = prefix[p];
        if (c1 >= 'a' && c1 <= 'z')   c1 -= 'a' - 'A';
        if (c2 >= 'a' && c2 <= 'z')   c2 -= 'a' - 'A';
        if (c1 != c2)   return false;
      }

   return true;
}
//-----------------------------------------------------------------------------
UCS_string
UCS_string::no_pad() const
{
UCS_string ret;
   loop(s, size())
      {
        Unicode uni = at(s);
        if (is_iPAD_char(uni))   uni = UNI_ASCII_SPACE;
        ret.append(uni);
      }

   return ret;
}
//-----------------------------------------------------------------------------
void
UCS_string::map_pad()
{
   loop(s, size())
      {
        if (is_iPAD_char(at(s)))   at(s) = UNI_ASCII_SPACE;
      }
}
//-----------------------------------------------------------------------------
UCS_string
UCS_string::remove_pad() const
{
UCS_string ret;
   loop(s, size())
      {
        Unicode uni = at(s);
        if (!is_iPAD_char(uni))   ret.append(uni);
      }

   return ret;
}
//-----------------------------------------------------------------------------
UCS_string
UCS_string::reverse() const
{
UCS_string ret;
   for (int s = size(); s > 0;)   ret.append(at(--s));
   return ret;
}
//-----------------------------------------------------------------------------
bool
UCS_string::is_comment_or_label() const
{
   if (size() == 0)                          return false;
   if (at(0) == UNI_ASCII_NUMBER_SIGN)       return true;   // comment
   if (at(0) == UNI_COMMENT)                 return true;   // comment
   loop(t, size())
       {
         if (at(t) == UNI_ASCII_COLON)       return true;   // label
         if (!Avec::is_symbol_char(at(t)))   return false;
       }

   return false;
}
//-----------------------------------------------------------------------------
void
UCS_string::append_utf8(const UTF8 * str)
{
const size_t len = strlen((const char *)str);
const UTF8_string utf(str, len);
const UCS_string ucs(utf);

   append(ucs);
}
//-----------------------------------------------------------------------------
void
UCS_string::append_quoted(const UCS_string & other)
{
   append(UNI_ASCII_DOUBLE_QUOTE);
   loop(s, other.size())
       {
          const Unicode uni = other[s];
          if (uni == UNI_ASCII_DOUBLE_QUOTE)   append(UNI_ASCII_BACKSLASH);
          append(uni);
       }
   append(UNI_ASCII_DOUBLE_QUOTE);
}
//-----------------------------------------------------------------------------
void
UCS_string::append_ascii(const char * str)
{
   while (*str)   append(Unicode(*str++));
}
//-----------------------------------------------------------------------------
void
UCS_string::append_number(ShapeItem num)
{
char cc[40];
   snprintf(cc, sizeof(cc) - 1, "%lld", (long long)num);
   loop(c, sizeof(cc))
      {
        if (cc[c])   append(Unicode(cc[c]));
        else         break;
      }
}
//-----------------------------------------------------------------------------
void
UCS_string::append_hex(ShapeItem num, bool uppercase)
{
const char * format = uppercase ? "%llX" : "%llx";
char cc[40];
   snprintf(cc, sizeof(cc) - 1, format, (long long)num);
   loop(c, sizeof(cc))
      {
        if (cc[c])   append(Unicode(cc[c]));
        else         break;
      }
}
//-----------------------------------------------------------------------------
void
UCS_string::append_shape(const Shape & shape)
{
   loop(r, shape.get_rank())
       {
         if (r)   append(UNI_ASCII_SPACE);
         ShapeItem s = shape.get_shape_item(r);
         if (s < 0)
            {
              s = -s;
              append(UNI_OVERBAR);
            }
         append_number(s);
       }
}
//-----------------------------------------------------------------------------
void
UCS_string::append_float(double num)
{
char cc[60];
   snprintf(cc, sizeof(cc) - 1, "%lf", num);
   loop(c, sizeof(cc))
      {
        if (cc[c])   append(Unicode(cc[c]));
        else         break;
      }
}
//-----------------------------------------------------------------------------
size_t
UCS_string::to_vector(UCS_string_vector & result) const
{
size_t max_len = 0;

   result.shrink(0);
   if (size() == 0)   return max_len;

   result.append(UCS_string());
   loop(s, size())
      {
        const Unicode uni = at(s);
        if (uni == UNI_ASCII_LF)    // line done
           {
             const size_t len = result.last().size();
             if (max_len < len)   max_len = len;

             if (s < size() - 1)   // more coming
                result.append(UCS_string());
           }
        else
           {
             if (uni != UNI_ASCII_CR)         // ignore \r.
                result[result.size() - 1].append(uni);
           }
      }

   // if the last line lacked a \n we check max_len here again.
const size_t len = result.last().size();
   if (max_len < len)   max_len = len;

   return max_len;
}
//-----------------------------------------------------------------------------
int
UCS_string::atoi() const
{
int ret = 0;
bool negative = false;

   loop(s, size())
      {
        const Unicode uni = at(s);

        if (!ret && Avec::is_white(uni))   continue;   // leading whitespace

        if (uni == UNI_ASCII_MINUS || uni == UNI_OVERBAR)
           {
             negative = true;
             continue;
           }

        if (uni < UNI_ASCII_0)                break;      // non-digit
        if (uni > UNI_ASCII_9)                break;      // non-digit

        ret *= 10;
        ret += uni - UNI_ASCII_0;
      }

   return negative ? -ret : ret;
}
//-----------------------------------------------------------------------------
ostream &
operator << (ostream & os, Unicode uni)
{       
   if (uni < 0x80)      return os << (char)uni;
        
   if (uni < 0x800)     return os << (char)(0xC0 | (uni >> 6))
                                  << (char)(0x80 | (uni & 0x3F));
        
   if (uni < 0x10000)    return os << (char)(0xE0 | (uni >> 12))
                                   << (char)(0x80 | (uni >>  6 & 0x3F))
                                   << (char)(0x80 | (uni       & 0x3F));

   if (uni < 0x200000)   return os << (char)(0xF0 | (uni >> 18))
                                   << (char)(0x80 | (uni >> 12 & 0x3F))
                                   << (char)(0x80 | (uni >>  6 & 0x3F))
                                   << (char)(0x80 | (uni       & 0x3F));

   if (uni < 0x4000000)  return os << (char)(0xF8 | (uni >> 24))
                                   << (char)(0x80 | (uni >> 18 & 0x3F))
                                   << (char)(0x80 | (uni >> 12 & 0x3F))
                                   << (char)(0x80 | (uni >>  6 & 0x3F))
                                   << (char)(0x80 | (uni       & 0x3F));

   return os << (char)(0xFC | (uni >> 30))
             << (char)(0x80 | (uni >> 24 & 0x3F))
             << (char)(0x80 | (uni >> 18 & 0x3F))
             << (char)(0x80 | (uni >> 12 & 0x3F))
             << (char)(0x80 | (uni >>  6 & 0x3F))
             << (char)(0x80 | (uni       & 0x3F));
}
//-----------------------------------------------------------------------------
ostream &
operator << (ostream & os, const UCS_string & ucs)
{
const int fill_len = os.width() - ucs.size();

   if (fill_len > 0)
      {
        os.width(0);
        loop(u, ucs.size())   os << ucs[u];
        loop(f, fill_len)     os << os.fill();
      }
   else
      {
        loop(u, ucs.size())   os << ucs[u];
      }

   return os;
}
//-----------------------------------------------------------------------------
bool
UCS_string::lexical_before(const UCS_string other) const
{
   loop(u, size())
      {
        if (u >= other.size())   return false;   // other is a prefix of this
        if (at(u) < other.at(u))   return true;
        if (at(u) > other.at(u))   return false;
      }

   // at this point the common part of this and other is equal, If other
   // is longer then this is a prefix of other (and this comes before other)
   return other.size() > size();
}
//-----------------------------------------------------------------------------
ostream &
UCS_string::dump(ostream & out) const
{
   out << right << hex << uppercase << setfill('0');
   loop(s, size())
      {
        out << " U+" << setw(4) << (int)at(s);
      }

   return out << left << dec << nouppercase << setfill(' ');
}
//-----------------------------------------------------------------------------
UCS_string
UCS_string::from_int(int64_t value)
{
   if (value >= 0)   return from_uint(value);

UCS_string ret(UNI_OVERBAR);
   return ret + from_uint(- value);
}
//-----------------------------------------------------------------------------
UCS_string
UCS_string::from_uint(uint64_t value)
{
   if (value == 0)   return UCS_string("0");

int digits[40];
int * d = digits;

   while (value)
      {
        const uint64_t v_10 = value / 10;
        *d++ = value - 10*v_10;
        value = v_10;
      }

UCS_string ret;
   while (d > digits)   ret.append(Unicode(UNI_ASCII_0 + *--d));
   return ret;
}
//-----------------------------------------------------------------------------
UCS_string
UCS_string::from_big(double & val)
{
   Assert(val >= 0);

long double value = val;
int digits[320];   // DBL_MAX is 1.79769313486231470E+308
int * d = digits;

const long double initial_fract = modf(value, &value);
long double fract;
   for (; value >= 1.0; ++d)
      {
         fract = modf(value / 10.0, &value);   // U.x -> .U
         *d = (int)((fract + .02) * 10.0);
         fract -= 0.1 * *d;
      }

   val = initial_fract;

UCS_string ret;
   if (d == digits)   ret.append(UNI_ASCII_0);   // 0.xxx

   while (d > digits)   ret.append(Unicode(UNI_ASCII_0 + *--d));
   return ret;
}
//-----------------------------------------------------------------------------
UCS_string
UCS_string::from_double_expo_prec(double v, int fract_digits)
{
UCS_string ret;

   if (v == 0.0)
      {
        ret.append(UNI_ASCII_0);
        if (fract_digits)   // unless integer only
           {
             ret.append(UNI_ASCII_FULLSTOP);
             loop(f, fract_digits)   ret.append(UNI_ASCII_0);
           }
        ret.append(UNI_ASCII_E);
        ret.append(UNI_ASCII_0);
        return ret;
      }

   if (v < 0)   { ret.append(UNI_OVERBAR);   v = - v; }

int expo = 0;
   while (v >= 1.0E1)
      {
        if (v >= 1.0E9)
           if (v >= 1.0E13)
              if (v >= 1.0E15)
                 if     (v >= 1.0E16)      { v *= 1.0E-16;   expo += 16; }
                 else /* v >= 1.0E15 */    { v *= 1.0E-15;   expo += 15; }
              else
                 if     (v >= 1.0E14)      { v *= 1.0E-14;   expo += 14; }
                 else /* v >= 1.0E13 */    { v *= 1.0E-13;   expo += 13; }
           else
              if (v >= 1.0E11)
                 if     (v >= 1.0E12)      { v *= 1.0E-12;   expo += 12; }
                 else /* v >= 1.0E11 */    { v *= 1.0E-11;   expo += 11; }
              else
                 if     (v >= 1.0E10)      { v *= 1.0E-10;   expo += 10; }
                 else /* v >= 1.0E9 */     { v *= 1.0E-9;    expo += 9;  }
        else
           if (v >= 1.0E5)
              if (v >= 1.0E7)
                 if     (v >= 1.0E8)       { v *= 1.0E-8;    expo += 8;  }
                 else /* v >= 1.0E7 */     { v *= 1.0E-7;    expo += 7;  }
              else
                 if     (v >= 1.0E6)       { v *= 1.0E-6;    expo += 6;  }
                 else /* v >= 1.0E5 */     { v *= 1.0E-5;    expo += 5;  }
           else
              if (v >= 1.0E3)
                 if     (v >= 1.0E4)       { v *= 1.0E-4;    expo += 4;  }
                 else /* v >= 1.0E3 */     { v *= 1.0E-3;    expo += 3;  }
              else
                 if     (v >= 1.0E2)       { v *= 1.0E-2;    expo += 2;  }
                 else /* v >= 1.0E1 */     { v *= 1.0E-1;    expo += 1;  }
      }

   while (v < 1.0E0)
      {
        if (v < 1.0E-8)
           if (v < 1.0E-12)
              if (v < 1.0E-14)
                 if     (v < 1.0E-15)      { v *= 1.0E-16;   expo += 16; }
                 else /* v < 1.0E-14 */    { v *= 1.0E-15;   expo += 15; }
              else
                 if     (v < 1.0E-13)      { v *= 1.0E-14;   expo += 14; }
                 else /* v < 1.0E-12 */    { v *= 1.0E-13;   expo += 13; }
           else
              if (v < 1.0E-10)
                 if     (v < 1.0E-11)      { v *= 1.0E12;   expo += -12; }
                 else /* v < 1.0E-10 */    { v *= 1.0E11;   expo += -11; }
              else
                 if     (v < 1.0E-9 )      { v *= 1.0E10;   expo += -10; }
                 else /* v < 1.0E-8 */     { v *= 1.0E9;    expo += -9;  }
        else
           if (v < 1.0E-4)
              if (v < 1.0E-6)
                 if     (v < 1.0E-7)       { v *= 1.0E8;    expo += -8;  }
                 else /* v < 1.0E-6 */     { v *= 1.0E7;    expo += -7;  }
              else
                 if     (v < 1.0E-5)       { v *= 1.0E6;    expo += -6;  }
                 else /* v < 1.0E-4 */     { v *= 1.0E5;    expo += -5;  }
           else
              if (v < 1.0E-2)
                 if     (v < 1.0E-3)       { v *= 1.0E4;    expo += -4;  }
                 else /* v < 1.0E-2 */     { v *= 1.0E3;    expo += -3;  }
              else
                 if     (v < 1.0E-1)       { v *= 1.0E2;    expo += -2;  }
                 else /* v < 1.0E0  */     { v *= 1.0E1;    expo += -1;  }
      }

   Assert(v >= 1.0);
   Assert(v < 10.0);

   // print mantissa in fixed format
   //
UCS_string mantissa = from_double_fixed_prec(v, fract_digits);
   if (mantissa.size() > 2 &&
       mantissa[0] == UNI_ASCII_1 &&
       mantissa[1] == UNI_ASCII_0 &&
       mantissa[2] == UNI_ASCII_FULLSTOP)   // 9.xxx rounded up to 10.xxx
      {
        mantissa[1] = UNI_ASCII_FULLSTOP;
        mantissa[2] = UNI_ASCII_0;
       ++expo;
      }
       
   ret.append(mantissa);
   ret.append(UNI_ASCII_E);
   ret.append(from_int(expo));

   return ret;
}
//-----------------------------------------------------------------------------
UCS_string
UCS_string::from_double_fixed_prec(double v, int fract_digits)
{
UCS_string ret;

   if (v < 0)   { ret.append(UNI_OVERBAR);   v = - v; }

   // in the loop below, there could be rounding errors when casting float
   // to int. We therefore increase v slighly (by 0.3 of the rounded digit)
   // to avoid that.
   //
   v += 0.03 * pow(10.0, -fract_digits);

   ret.append(from_big(v));   // leaves fractional part of v in v

   ret.append(UNI_ASCII_FULLSTOP);

   loop(f, fract_digits + 1)
      {
        v *= 10;
        const int vv = v;   // subject to rounding errors!
        ret.append(Unicode(UNI_ASCII_0 + vv));
        v -= vv;
      }

   ret.round_last_digit();
   return ret;
}
//-----------------------------------------------------------------------------
void
UCS_string::round_last_digit()
{
   Assert1(size() > 1);
   if (back() >= UNI_ASCII_5)   // round up
      {
        for (int q = size() - 2; q >= 0; --q)
            {
              const Unicode cc = at(q);
              if (cc < UNI_ASCII_0)   continue;   // not a digit
              if (cc > UNI_ASCII_9)   continue;   // not a digit

              at(q) = Unicode(cc + 1);   // round up
              if (cc != UNI_ASCII_9)   break;    // 0-8 rounded up: stop

              at(q) = UNI_ASCII_0;    // 9 rounded up: say 0 and repeat
              if (q)   continue;   // not first difit

              // something like 9.xxx has been rounded up to, say, 0.xxx
              // but should be 10.xxx Fix it.
              //
              for (int d = size() - 1; d > 0; --d)  at(d) = at(d - 1);
              at(0) = UNI_ASCII_1;
            }
      }

   shrink(size() - 1);
   if (items[size() - 1] == UNI_ASCII_FULLSTOP)   shrink(size() - 1);
}
//----------------------------------------------------------------------------
bool
UCS_string::contains(Unicode uni)
{
   loop(u, size())   if (at(u) == uni)   return true;
   return false;
}
//----------------------------------------------------------------------------
UCS_string
UCS_string::sort() const
{
UCS_string ret(*this);
   Heapsort<Unicode>::sort(&ret[0], ret.size(), 0, greater_uni);
   return ret;
}
//----------------------------------------------------------------------------
UCS_string
UCS_string::unique() const
{
   if (size() <= 1)   return UCS_string(*this);

UCS_string sorted = sort();
UCS_string ret;
   ret.reserve(sorted.size());

   ret.append(sorted[0]);
   for (ShapeItem j = 1; j < size(); ++j)
       {
         if (sorted[j] != ret.back())   ret.append(sorted[j]);
       }

   Heapsort<Unicode>::sort(&ret[0], ret.size(), 0, greater_uni);
   return ret;
}
//----------------------------------------------------------------------------
