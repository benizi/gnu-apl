/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2015  Dr. Jürgen Sauermann

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

#include "CharCell.hh"
#include "ComplexCell.hh"
#include "Error.hh"
#include "FloatCell.hh"
#include "IntCell.hh"
#include "LvalCell.hh"
#include "Output.hh"
#include "PointerCell.hh"
#include "PrintOperator.hh"
#include "Value.icc"
#include "SystemLimits.hh"
#include "Workspace.hh"

//-----------------------------------------------------------------------------
void *
Cell::operator new(std::size_t s, void * pos)
{
   return pos;
}
//-----------------------------------------------------------------------------
void
Cell::init_from_value(Value_P value, Value & cell_owner, const char * loc)
{
   if (value->is_scalar())
      {
        init(value->get_ravel(0), cell_owner, loc);
      }
   else
      {
        new (this) PointerCell(value, cell_owner);
      }
}
//-----------------------------------------------------------------------------
Value_P
Cell::to_value(const char * loc) const
{
Value_P ret;
   if (is_pointer_cell())
      {
        ret = get_pointer_value(); // ->clone(LOC);
      }
   else
      {
        ret = Value_P(loc);
        ret->get_ravel(0).init(*this, ret.getref(), loc);
        ret->check_value(LOC);
      }

   return ret;
}
//-----------------------------------------------------------------------------
void
Cell::init_type(const Cell & other, Value & cell_owner, const char * loc)
{
   if (other.is_pointer_cell())
      {
        new (this) PointerCell(other.get_pointer_value()->clone(loc),
                               cell_owner);
        get_pointer_value()->to_proto();
      }
   else if (other.is_lval_cell())
      {
        new (this) LvalCell(other.get_lval_value(),
                            ((const LvalCell *)&other)->get_cell_owner());
      }
   else if (other.is_character_cell())
      {
        new (this) CharCell(UNI_ASCII_SPACE);
      }
   else // numeric
      {
        new (this) IntCell(0);
      }
}
//-----------------------------------------------------------------------------
void
Cell::copy(Value & val, const Cell * & src, ShapeItem count)
{
   loop(c, count)
      {
        Assert1(val.more());
        val.next_ravel()->init(*src++, val, LOC);
      }
}
//-----------------------------------------------------------------------------
bool
Cell::greater(const Cell & other) const
{
   MORE_ERROR() << "Cell::greater() : Objects of class " << get_classname()
                << " cannot be compared";
   DOMAIN_ERROR;
}
//-----------------------------------------------------------------------------
bool
Cell::equal(const Cell & other, APL_Float qct) const
{
   MORE_ERROR() << "Cell::equal() : Objects of class " << get_classname()
                << " cannot be compared";
   DOMAIN_ERROR;
}
//-----------------------------------------------------------------------------
bool
Cell::tolerantly_equal(APL_Complex A, APL_Complex B, APL_Float C)
{
   // if A equals B, return true
   //
   if (A == B) return true;

   // if A and B are not in the same half-plane, return false.
   //
   // Implementation: If A and B are in the same real half-plane then
   //                 the product of their real parts is ≥ 0,
   //
   //                 If A and B are in the same imag half-plane then
   //                 the product of their imag parts is ≥ 0,
   //
   //                 Otherwise: they are not in the same half-plane
   //                 and we return false;
   //
   if (A.real() * B.real() < 0.0 &&
       A.imag() * B.imag() < 0.0)   return false;

   // If the distance-between A and B is ≤ C times the larger-magnitude
   // of A and B, return true
   //
   // Implementation: Instead of mag(A-B)  ≤ C × max(mag(A),   mag(B))
   // we compute                 mag²(A-B) ≤ C² × max(mag²(A), mag²(B))
   //
   // 1. compute max(mag²A, mag²B)
   //
const APL_Float mag2_A   = A.real() * A.real() + A.imag() * A.imag();
const APL_Float mag2_B   = B.real() * B.real() + B.imag() * B.imag();
const APL_Float mag2_max = mag2_A > mag2_B ? mag2_A : mag2_B;

   // 2. compute mag²(A-B)
   //
const APL_Complex A_B = A - B;
const APL_Float dist2_A_B = A_B.real() * A_B.real()
                          + A_B.imag() * A_B.imag();
   // compare
   //

   return dist2_A_B <= C*C*mag2_max;
}
//-----------------------------------------------------------------------------
bool
Cell::tolerantly_equal(APL_Float A, APL_Float B, APL_Float C)
{
   // if the signs of A and B differ then they are unequal (ISO standard
   // page 19). We treat exact 0.0 as having both signs
   //
   if (A == B)               return true;
   if (A < 0.0 && B > 0.0)   return false;
   if (A > 0.0 && B < 0.0)   return false;

APL_Float mag_A = A < 0 ? -A : A;
APL_Float mag_B = B < 0 ? -B : B;
APL_Float mag_max = (mag_A > mag_B) ? mag_A : mag_B;

const APL_Float dist_A_B = (A > B) ? (A - B) : (B - A);

   return (dist_A_B < C*mag_max);
}
//-----------------------------------------------------------------------------
bool
Cell::is_near_int(APL_Float value)
{
   if (value > LARGE_INT)   return true;
   if (value < SMALL_INT)   return true;

const double result = nearbyint(value);
const double diff = value - result;
   if (diff >= INTEGER_TOLERANCE)    return false;
   if (diff <= -INTEGER_TOLERANCE)   return false;

   return true;
}
//-----------------------------------------------------------------------------
APL_Integer
Cell::near_int(APL_Float value)
{
   if (value >= LARGE_INT)   DOMAIN_ERROR;
   if (value <= SMALL_INT)   DOMAIN_ERROR;

const double result = nearbyint(value);
const double diff = value - result;
   if (diff > INTEGER_TOLERANCE)    DOMAIN_ERROR;
   if (diff < -INTEGER_TOLERANCE)   DOMAIN_ERROR;

   if (result > 0)   return   APL_Integer(0.3 + result);
   else              return - APL_Integer(0.3 - result);
}
//-----------------------------------------------------------------------------
bool
Cell::greater_vec(const IntCell & Za, const IntCell & Zb, const void * comp_arg)
{
struct _ctx { const Cell * base;   ShapeItem comp_len; };
const _ctx * ctx = (const _ctx *)comp_arg;
const Cell * ca = ctx->base + ctx->comp_len * Za.get_int_value();
const Cell * cb = ctx->base + ctx->comp_len * Zb.get_int_value();

const APL_Float qct = Workspace::get_CT();

   // most frequently comp_len is 1, so we optimize for this case.
   //
   if (ctx->comp_len == 1)
      {
        const bool equal = ca[0].equal(cb[0], qct);
        if (equal)   return Za.get_int_value() > Zb.get_int_value();
        const bool result = ca[0].greater(cb[0]);
        return result;
      }

   loop(c, ctx->comp_len)
      {
        const bool equal = ca[c].equal(cb[c], qct);
        if (equal)   continue;
        const bool result = ca[c].greater(cb[c]);
        return result;
      }

   return Za.get_int_value() > Zb.get_int_value();   // a and b are equal: sort by position
}
//-----------------------------------------------------------------------------
bool
Cell::smaller_vec(const IntCell & Za, const IntCell & Zb, const void * comp_arg)
{
struct _ctx { const Cell * base;   ShapeItem comp_len; };
const _ctx * ctx = (const _ctx *)comp_arg;
const Cell * ca = ctx->base + ctx->comp_len * Za.get_int_value();
const Cell * cb = ctx->base + ctx->comp_len * Zb.get_int_value();

const APL_Float qct = Workspace::get_CT();

   // most frequently comp_len is 1, so we optimize for this case.
   //
   if (ctx->comp_len == 1)
      {
        const bool equal = ca[0].equal(cb[0], qct);
        if (equal)   return Za.get_int_value() > Zb.get_int_value();
        const bool result = ca[0].greater(cb[0]);
        return !result;
      }

   loop(c, ctx->comp_len)
      {
        const bool equal = ca[c].equal(cb[c], qct);
        if (equal)   continue;
        const bool result = ca[c].greater(cb[c]);
        return !result;
      }

   return Za.get_int_value() > Zb.get_int_value();   // a and b are equal: sort by position
}
//-----------------------------------------------------------------------------
ostream & 
operator <<(ostream & out, const Cell & cell)
{
PrintBuffer pb = cell.character_representation(PR_BOXED_GRAPHIC);
UCS_string ucs(pb, 0, Workspace::get_PW());
   return out << ucs << " ";
}
//-----------------------------------------------------------------------------
ErrorCode
Cell::bif_equal(Cell * Z, const Cell * A) const
{
   // incompatible types ?
   //
   if (is_character_cell() != A->is_character_cell())   return IntCell::z0(Z);

   return IntCell::zv(Z, equal(*A, Workspace::get_CT()));
}
//-----------------------------------------------------------------------------
ErrorCode
Cell::bif_not_equal(Cell * Z, const Cell * A) const
{
   // incompatible types ?
   //
   if (is_character_cell() != A->is_character_cell())   return IntCell::z1(Z);

   return IntCell::zv(Z, !equal(*A, Workspace::get_CT()));
}
//-----------------------------------------------------------------------------
ErrorCode
Cell::bif_greater_than(Cell * Z, const Cell * A) const
{
   return IntCell::zv(Z, (A->compare(*this) == COMP_GT) ? 1 : 0);
}
//-----------------------------------------------------------------------------
ErrorCode
Cell::bif_less_eq(Cell * Z, const Cell * A) const
{
   return IntCell::zv(Z, (A->compare(*this) != COMP_GT) ? 1 : 0);
}
//-----------------------------------------------------------------------------
ErrorCode
Cell::bif_less_than(Cell * Z, const Cell * A) const
{
   return IntCell::zv(Z, (A->compare(*this) == COMP_LT) ? 1 : 0);
}
//-----------------------------------------------------------------------------
ErrorCode
Cell::bif_greater_eq(Cell * Z, const Cell * A) const
{
   return IntCell::zv(Z, (A->compare(*this) != COMP_LT) ? 1 : 0);
}
//-----------------------------------------------------------------------------
