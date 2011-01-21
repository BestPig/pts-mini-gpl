#include "printf.h"

class Whatever {};

std::ostream& operator<<(std::ostream& os, const Whatever&) {
  return os << "Whatever";
}

void FX(std::ostream&) {
}

int main() {
  // Nice long missing method error: (PrintfHelper(), 5, 66, "Foo", Whatever(), 7);
  Printf(std::cout, "Welcome %d!\n", 42);
  Printf("format(% 4d)(0x%x)(%s)(%d)(%d)(%.5f)(%p)(%c)(%c)(%s)(%s)(%s)(%s)%m\n", 66, 66, "Foo", 7, 3.9, 3.9, (void*)main, 42, 42.9, -42, (unsigned short)-42, -42ULL, -42LL);
  Printf(stderr, "To stderr (%5s).\n", "hi");
  Printf("Int(%d)(%d)(%d)(%d)\n", int(-5), (unsigned long long)(-6), (long long)(-7), unsigned(-8));
  Printf("Int(%X)(%x)(%x)(%x)\n", short(-5), (unsigned long long)(-6), (long long)(-7), unsigned(-8));
  Printf("%s\n", Whatever());
#if 0
  Printf(Whatever());
#endif
  printf("size=%d\n", Printf("Foo%%%xBar\n", 65535));
  int n;
  if (Printf("Foo%%%x%nBar\n", 65535, &n) != 12)
    abort();
  if (n != 8)
    abort();
  if ("Hello, 42!" != StringPrintf("Hello, %d!", 42))
    abort();
  if ("Hello, 42!" != SPrintf("Hello, %s!", 42))
    abort();
  std::ostringstream oss;
  //std::ofstream of("foo.txt");
  //FX(of);
  std::string s;
  if (3 != Printf(&s, "Foo"))
    abort();
  std::string bar("B\0r", 3);
  if (5 != Printf(&s, "%s%d", bar, 42))  // Append.
    abort();
  if (s != std::string("FooB\0r42", 8))
    abort();
  Printf("All OK.\n");
  return 0;
}
