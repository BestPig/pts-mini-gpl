//
// printf.h: C++ typesafe version of the ANSI C printf() family of functions.
// by pts@fazekas.hu at Fri Jan 21 16:50:54 CET 2011
//
// This program is free software; you can redistribute it and/or modify 
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// By typesafe, we mean:
//
// * Automatic type conversion:
// ** %s accepts const char*, int, long long, double, string and any type T
//    which supports operator<<(std::ostream&, const T&).
// ** with %s, unsigned arguments are printed as unsigned, signed arguments
//    are printed as signed decimal
// ** %d accepts int, long long, double (and similar types of different size)
// ** %g accepts int, long long, double (and similar types of different size)
// * Automatic size detection: %x (etc.) is enough, there is no need to
//   distinguish %hx, %x, %lx, %llx etc.
// * Automatic type check: if a % format is used with an argument of an
//   unsupported type, a run-time error (abort()) is triggered (no
//   compile-time error or warning!).
//
// Implementation restrictions:
//
// * Needs GCC as a compiler because it uses macros with variable number of
//   arguments.
// * Needs GCC as a compiler because it uses blocks within expressions:
//   ({ ... }).
// * Uses the built-in vsnprintf etc. functions. Tested with glibc and uclibc.
// * The widest integer type supported is the long long, and the widest
//   floating point type is the long double.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <fstream>

#ifndef _PRINTF_H
#define _PRINTF_H

// The C++ typesafe version of printf() is implemented as:
//
//   int Printf(const char* fmt, ...);  // To stdout.
//   int Printf(FILE*, const char* fmt, ...);
//   int Printf(std::ostream&, const char* fmt, ...);
//   int Printf(std::ostringstream&, const char* fmt, ...);
//   int Printf(std::ofstream&, const char* fmt, ...);
//   int Printf(std::string*, const char* fmt, ...);  // Append to string.
//   std::string SPrintf(const char* fmt, ...);
//
// Please note that Printf() and SPrintf() are implemented using macros and
// overloading operator, (and templates). The implementation is black magic.
//
// Please note that there are run-time checks for the types of the arguments
// and the types in the format string, e.g. Printf("%d", "foo") is a
// run-time error (not a compile-time error or warning!).
//
// Please note that the implementation is not optimized for speed. Most
// probably it's not possible to implement a typesafe version of printf()
// which is also as fast.

// Just like fprintf(), but its first argument is an std::ostream& instead
// of a FILE*.
//
// Please note that StreamPrintf() is much less efficient than fprintf().
//
// Please also note that StreamPrintf() requires the same type consistency
// for the specified fmt and values (e.g. %ld for a long argument) as
// fprintf(). Please use Printf() if you'd like to get automatic type
// conversion (e.g. %d for short, int, long etc.).
int StreamPrintf(std::ostream* os, const char* fmt, ...);

// Just like sprintf(), but returns a C++ string.
// Please also note that StreamPrintf() requires the same type consistency
// for the specified fmt and values (e.g. %ld for a long argument) as
// fprintf(). Please use SPrintf() or Printf(&string_obj, ...) if you'd like
// to get automatic type conversion (e.g. %d for short, int, long etc.).
std::string StringPrintf(const char* fmt, ...);

// Equivalent to s->append(StringPrintf(fmt, ...)), but faster.
int StringAppendf(std::string* s, const char* fmt, ...);

// Everything below is black magic, users of printf.h don't have to
// understand it, and they are discouraged to use anything below directly.

// Helper class for a typesfe printf.
class PrintfHelper {
 public:
  class F {};

  class G {
   public:
    G(FILE* outf): outf_(outf) {}
    FILE* outf() const { return outf_; }
   private:
    FILE* outf_;
  };

  class H {
   public:
    H(std::ostream* outs): outs_(outs) {}
    std::ostream* outs() const { return outs_; }
   private:
    std::ostream* outs_;
  };

  class I {
   public:
    I(std::string* outx): outx_(outx) {}
    std::string* outx() const { return outx_; }
   private:
    std::string* outx_;
  };

  PrintfHelper(const char* fmt, FILE* outf):
      rest_(fmt), outf_(outf), outs_(NULL), outx_(NULL), size_(0) {}
  PrintfHelper(const char* fmt, std::ostream* outs):
      rest_(fmt), outf_(NULL), outs_(outs), outx_(NULL), size_(0) {}
  PrintfHelper(const char* fmt, std::string* outx):
      rest_(fmt), outf_(NULL), outs_(NULL), outx_(outx), size_(0) {}
  int Rest() const;
  void Print(char i) { PrintInteger(i, (unsigned char)-1); }
  void Print(unsigned char i) { PrintInteger(i, (unsigned char)-1); }
  void Print(short i) { PrintInteger(i, (unsigned short)-1); }
  void Print(unsigned short i) { PrintInteger(i, (unsigned short)-1); }
  void Print(int i) { PrintInteger(i, -1U); }
  void Print(unsigned i) { PrintInteger(i, -1U); }
  void Print(long i) { PrintInteger(i, -1UL); }
  void Print(unsigned long i) { PrintInteger(i, -1UL); }
  void Print(long long i) { PrintInteger(i, -2ULL); }
  void Print(unsigned long long i) { PrintInteger(i, -1ULL); }
  void Print(float f) { PrintFloat(f); }
  void Print(double f) { PrintFloat(f); }
  void Print(long double f) { PrintFloat(f); }
  void Print(const char* s);
  void Print(const std::string& s);
  void Print(int* n);
  // An explicit (void*) cast is necessary.
  // Example: Printf("%p", (void*)main);
  void Print(void* p);
  // Print anything which supports operator<<(std::ostream&, const T&),
  // using %s.
  template<typename T>void Print(const T& t) {
    if (*rest_ != '%')
      abort();
    if (rest_[1] != 's')
      abort();
    rest_ += 2;
    std::string s;
    {
      std::ostringstream oss;
      oss << t;
      // Unfortunately this involves a copy.
      std::string s1 = oss.str();
      s.swap(s1);
    }
    PrintBuf(s.data(), s.size());
  }
 private:
  void PrintInteger(long long i, long long unsigned_mask);
  void PrintFloat(long double f);
  void PrintString(const char* s);
  void PrintBuf(const char* s, size_t len);
  void PrintLiteral();
  // Return true on an unsigned format.
  char ParseNumberFormat(char* ifmt, int isize);
  void ParseStringFormat(char* ifmt, int isize);

  const char* rest_;
  FILE* outf_;
  std::ostream* outs_;
  std::string* outx_;
  int size_;
};

template<typename T>
const PrintfHelper& operator,(const PrintfHelper& c, T t) {
  (const_cast<PrintfHelper&>(c)).Print(t);
  return c;
}

class PrintfArg1 {};
inline void PrintfHelperallPrintfArg1(PrintfArg1) {}
// Make Printf(WhateverPrintfHelperlass()) fail.
template<typename T>void operator,(const PrintfHelper::F&, T t) {
  PrintfHelperallPrintfArg1(t);
}

inline PrintfHelper operator,(const PrintfHelper::F&, char const* fmt) {
  return PrintfHelper(fmt, stdout);
}

inline PrintfHelper::G operator,(const PrintfHelper::F&, FILE* outf) {
  return PrintfHelper::G(outf);
}

// SUXX: We have to specify all ostream subclasses by hand.
// SUXX: This doesn't work: Printf(std::ostringstream(), "X");
inline PrintfHelper::H operator,(const PrintfHelper::F&, std::ostream& outs) {
  return PrintfHelper::H(&outs);
}
inline PrintfHelper::H operator,(const PrintfHelper::F&,
                                 std::ostringstream& outs) {
  return PrintfHelper::H(&outs);
}
inline PrintfHelper::H operator,(const PrintfHelper::F&, std::ofstream& outs) {
  return PrintfHelper::H(&outs);
}

inline PrintfHelper::I operator,(const PrintfHelper::F&, std::string* outx) {
  return PrintfHelper::I(outx);
}

template<typename T>
const PrintfHelper operator,(const PrintfHelper::G& g, T fmt) {
  return PrintfHelper(fmt, g.outf());
}

template<typename T>
const PrintfHelper operator,(const PrintfHelper::H& h, T fmt) {
  return PrintfHelper(fmt, h.outs());
}

template<typename T>
const PrintfHelper operator,(const PrintfHelper::I& i, T fmt) {
  return PrintfHelper(fmt, i.outx());
}

// GCC-specific variable-length macro arguments.
#define Printf(args...) ((PrintfHelper::F(), ##args).Rest())

// GCC-specific block within the expression.
#define SPrintf(args...) ({ std::string SPrintf_tmp; (PrintfHelper::F(), \
    &SPrintf_tmp, ##args).Rest(); SPrintf_tmp; })

#endif  // _PRINTF_H
