//
// printf.cc: C++ typesafe version of the ANSI C printf() family of functions.
// by pts@fazekas.hu at Fri Jan 21 16:50:54 CET 2011
//
// See the printf.h header file for documentation.
//

#include <errno.h>
#include <stdarg.h>

#include "printf.h"

int StreamPrintf(std::ostream* os, const char* fmt, ...) {
  int size, got;
  char buf[128], *p;
  va_list ap;
  va_start(ap, fmt);
  size = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (size < 0)
    return size;
  if (sizeof buf >= size + 1UL) {
    va_start(ap, fmt);
    got = vsprintf(buf, fmt, ap);
    va_end(ap);
    if (got > 0)
      os->write(buf, got);
  } else {
    if ((p = (char*)malloc(size + 1)) == NULL)
      return -1;
    va_start(ap, fmt);
    got = vsprintf(p, fmt, ap);
    va_end(ap);
    if (got > 0)
      os->write(p, got);
    free(p);
  }
  return got;
}

std::string StringPrintf(const char* fmt, ...) {
  std::string s;
  va_list ap;
  va_start(ap, fmt);
  int size = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (size > 0) {
    s.resize(size);
    va_start(ap, fmt);
    // Writes the trailing '\0' as well, but we don't care.
    vsprintf(const_cast<char*>(s.data()), fmt, ap);
    va_end(ap);
  }
  return s;
}

int StringAppendf(std::string* s, const char* fmt, ...) {
  int size, got;
  va_list ap;
  va_start(ap, fmt);
  size = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (size < 0)
    return size;
  s->resize(s->size() + size);
  va_start(ap, fmt);
  // Writes the trailing '\0' as well, but we don't care.
  got = vsprintf(const_cast<char*>(s->data()) + s->size() - size, fmt, ap);
  va_end(ap);
  return got;
}

int PrintfHelper::Rest() const {
  const_cast<PrintfHelper*>(this)->PrintLiteral();
  if (*rest_ != '\0')  // Too many '%'s in fmt.
    abort();
  return size_;
}
void PrintfHelper::Print(const char* s) {
  char ifmt[32];
  PrintLiteral();
  ParseStringFormat(ifmt, sizeof ifmt);
  if (ifmt[1] == 's') {  // Shortcut for "%s".
    PrintString(s);
  } else {
    size_ += (outx_ != NULL) ? StringAppendf(outx_, ifmt, s) :
             (outs_ != NULL) ? StreamPrintf(outs_, ifmt, s) :
             fprintf(outf_, ifmt, s);
  }
}
void PrintfHelper::Print(const std::string& s) {
  // TODO(pts): Support width (%42s etc.)
  PrintLiteral();
  if (*rest_ != '%')
    abort();
  if (rest_[1] != 's')
    abort();
  rest_ += 2;
  PrintBuf(s.data(), s.size());
}
void PrintfHelper::Print(int* n) {
  PrintLiteral();
  if (*rest_ != '%')
    abort();
  if (rest_[1] != 'n')
    abort();
  rest_ += 2;
  *n = size_;
}
void PrintfHelper::Print(void* p) {
  PrintLiteral();
  if (*rest_ != '%')
    abort();
  if (rest_[1] != 'p')
    abort();
  rest_ += 2;
  size_ += (outx_ != NULL) ? StringAppendf(outx_, "%p", p) :
           (outs_ != NULL) ? StreamPrintf(outs_, "%p", p) :
           fprintf(outf_, "%p", p);
}
void PrintfHelper::PrintInteger(long long i, long long unsigned_mask) {
  char ifmt[32];
  PrintLiteral();
  char mode = ParseNumberFormat(ifmt, sizeof ifmt);
  if (mode == 4) {  // String.
    if (unsigned_mask == -1LL) {
      strcpy(ifmt, "%llu");
      mode = 0 ;  // Unsigned integer.
    } else {
      strcpy(ifmt, "%lld");
      mode = 1;  // Signed integer.
    }
  }
  if (mode == 2) {  // Float.
    long double f = i;
    long long i2 = (long long)f;
    if (i != i2) {
      PrintString("(overflow)");
    } else {
      size_ += (outx_ != NULL) ? StringAppendf(outx_, ifmt, f) :
               (outs_ != NULL) ? StreamPrintf(outs_, ifmt, f) :
               fprintf(outf_, ifmt, f);
    }
  } else {  // This works %c, %d, %x etc.
    if (mode == 0)
      i &= unsigned_mask | 1;
    size_ += (outx_ != NULL) ? StringAppendf(outx_, ifmt, i) :
             (outs_ != NULL) ? StreamPrintf(outs_, ifmt, i) :
             fprintf(outf_, ifmt, i);
  }
}
void PrintfHelper::PrintFloat(long double f) {
  char ifmt[32];
  PrintLiteral();
  char mode = ParseNumberFormat(ifmt, sizeof ifmt);
  if (mode == 4) {  // String.
    // TOOD(pts): Use a more specific, but not too long format.
    strcpy(ifmt, "%g");
    mode = 2;
  }
  if (mode == 2) {  // Float.
    size_ += (outx_ != NULL) ? StringAppendf(outx_, ifmt, f) :
             (outs_ != NULL) ? StreamPrintf(outs_, ifmt, f) :
             fprintf(outf_, ifmt, f);
  } else if (mode == 0) {  // Unsigned integer.
    unsigned long long i = (unsigned long long)f;
    long double f2 = i;
    f2 -= f;
    if (f2 < 0)
      f2 = -f2;
    if (f2 >= 1) {
      PrintString("(overflow)");
    } else {
      size_ += (outx_ != NULL) ? StringAppendf(outx_, ifmt, i) :
               (outs_ != NULL) ? StreamPrintf(outs_, ifmt, i) :
               fprintf(outf_, ifmt, i);
    }
  } else {  // Signed integer. This works %c, %d, %x etc.
    long long i = (long long)f;
    long double f2 = i;
    f2 -= f;
    if (f2 < 0)
      f2 = -f2;
    if (f2 >= 1) {
      PrintString("(overflow)");
    } else {
      size_ += (outx_ != NULL) ? StringAppendf(outx_, ifmt, i) :
               (outs_ != NULL) ? StreamPrintf(outs_, ifmt, i) :
               fprintf(outf_, ifmt, i);
    }
  }
}
void PrintfHelper::PrintString(const char* s) {
  size_t len = strlen(s);
  if (outx_ != NULL) {
    outx_->append(s, len);
  } else if (outs_ != NULL) {
    outs_->write(s, len);
  } else {
    fputs(s, outf_);
  }
  size_ += len;
}
void PrintfHelper::PrintBuf(const char* s, size_t len) {
  if (outx_ != NULL) {
    outx_->append(s, len);
  } else if (outs_ != NULL) {
    outs_->write(s, len);
  } else {
    fwrite(s, 1, len, outf_);
  }
  size_ += len;
}
void PrintfHelper::PrintLiteral() {
  char c;
  const char* p;
  size_t len;
  for (;;) {
    p = rest_;
    while ((c = *p) != '%' && c != '\0') ++p;
    if ((len = p - rest_) > 0) {
      PrintBuf(rest_, len);
      rest_ = p;
    }
    if (c == '\0')
      break;
    if ((c = rest_[1]) == '\0')
      abort();  // Format string ends with '\0'.
    if (c == '%') {
      if (outx_ != NULL) {
        outx_->push_back(c);
      } else if (outs_ != NULL) {
        outs_->put(c);
      } else {
        putc(c, outf_);
      }
      ++size_;
      rest_ += 2;
    } else if (c == 'm') {
      rest_ += 2;
      PrintString(strerror(errno));
    } else {
      break;
    }
  }
}
char PrintfHelper::ParseNumberFormat(char* ifmt, int isize) {
  bool is_float_ok = true;
  bool is_char_ok = true;
  char c, mode;
  if (isize < 3)
    abort();
  if (*rest_ != '%')
    abort();
  *ifmt++ = *rest_++;
  --isize;
  if (*rest_ == 's') {
    ++rest_;
    *ifmt++ = 's';
    *ifmt = '\0';
    return 4;  // String.
  }
  for (;;) {
    c = *rest_;
    if (c == 'l') {
      is_char_ok = false;
    } else if (c == 'h' || c == 'L' || c == 'q' || c == 'j') {
      is_float_ok = is_char_ok = false;
    } else {
      if ((c >= '0' && c <= '9') || c == '#' || c == '0' || c == '-' || c == ' ' || c == '+' || c == '.' || c == '\'') {
        is_char_ok = false;
      } else if (c == 'I') {
        is_float_ok = is_char_ok = false;
      } else {
        break;
      }
      if (isize <= 0)
        abort();
      *ifmt++ = c;
      --isize;
    }
    ++rest_;
  }
  if (c == 'x' || c == 'X' || c == 'o' || c == 'u') {
    mode = 0;  // Unsigned integer.
  } else if (c == 'd' || c == 'i') {
    mode = 1;  // Signed integer.
  } else if (c == 'a' || c == 'A' || c == 'e' || c == 'E' || c == 'f' || c == 'F' || c == 'g' || c == 'G') {
    if (!is_float_ok)
      abort();
    mode = 2;  // Float.
  } else if (c == 'c') {
    if (!is_char_ok)
      abort();
    mode = 3;  // PrintfHelperhar.
  } else {
    abort();
  }
  ++rest_;
  if (mode == 2) {
    if (isize < 3)
      abort();
    *ifmt++ = 'L';  // Long double.
  } else {
    if (isize < 4)
      abort();
    *ifmt++ = 'l';
    *ifmt++ = 'l';
  }
  *ifmt++ = c;
  *ifmt = '\0';
  return mode;
}
void PrintfHelper::ParseStringFormat(char* ifmt, int isize) {
  char c;
  if (isize < 2)
    abort();
  if (*rest_ != '%')
    abort();
  *ifmt++ = *rest_++;
  --isize;
  while (((c = *rest_) >= '0' && c <= '9') ||
         c == ' ' || c == '-' || c == '.') {
    if (isize <= 0)
      abort();
    *ifmt++ = c;
    --isize;
    ++rest_;
  }
  if (c != 's')
    abort();
  ++rest_;
  if (isize < 2)
    abort();
  *ifmt++ = c;
  *ifmt = '\0';
}
