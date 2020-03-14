#include <cstdio>
#include <cstdarg>


#define MAX_LAPS_NUM 20
#define MAX_NUM_PILOTS 2

class FakeSerial {
  public:
    void printf(const char* format, ...) {
      va_list args;
      va_start(args, format);
      vprintf(format, args);
      va_end(args);
    }
};

extern FakeSerial Serial;
