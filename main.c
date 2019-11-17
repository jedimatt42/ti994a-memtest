
#include <vdp.h>
#include <system.h>

#define SCREEN_COLOR (COLOR_BLACK << 4) + COLOR_CYAN
#define ERROR_COLOR (COLOR_BLACK << 4) + COLOR_MEDRED
#define SUCCESS_COLOR (COLOR_BLACK << 4) + COLOR_LTGREEN

void writehex(unsigned int row, unsigned int col, const unsigned int value) {
  unsigned char buf[3] = { 0, 0, 0 };
  *((unsigned int*)buf) = byte2hex[value >> 8];
  writestring(row, col, buf);
  *((unsigned int*)buf) = byte2hex[0xFF & value];
  writestring(row, col + 2, buf);
}

void printSummary(unsigned int ec) {
  if (ec == 0) {
    VDP_SET_REGISTER(VDP_REG_COL, SUCCESS_COLOR);
    writestring(21, 11, "All Memory Passed");
  } else {
    VDP_SET_REGISTER(VDP_REG_COL, ERROR_COLOR);
    writestring(21, 14, "Found errors");
  }
}

unsigned int testBlock(const unsigned int row, unsigned char* addr) {
  unsigned int ec = 0;
  unsigned int* end=(unsigned int*)(addr + 0x1FFF);
  writestring(row, 3, "Testing");
  writehex(row, 11, (int)addr);
  writestring(row, 16, "->");
  writehex(row, 19, (int)end);
  writestring(row, 24, ":");
  writestring(row, 26, "      ");

  const char spinner[4] = { '-', '\\', '|', '/' };
  int spinneridx = 0;

  for( int tries=0; tries<12; tries++) {

    // Test with incrementing values
    vdpchar(VDP_SCREEN_TEXT(row, 26), spinner[spinneridx++ % 4]);
    unsigned int val = 0x0000;

    for( volatile unsigned int* a=(unsigned int*)addr; a<=end; a++ ) {
      *a = val;
      val++;
    }

    val = 0x0000;
    for( volatile unsigned int* a=(unsigned int*)addr; a<=end; a++ ) {
      if ( *a != val ) {
	    ec++;
      }
      val++;
    }

    // Test with bitwise not incrementing values
    vdpchar(VDP_SCREEN_TEXT(row, 26), spinner[spinneridx++ % 4]);
    val = 0x0000;

    for( volatile unsigned int* a=(unsigned int*)addr; a<=end; a++ ) {
      *a = ~val;
      val++;
    }

    val = 0x0000;
    for( volatile unsigned int* a=(unsigned int*)addr; a<=end; a++ ) {
      if ( *a != ~val ) {
	    ec++;
      }
      val++;
    }

    // Test data lines
    vdpchar(VDP_SCREEN_TEXT(row, 26), spinner[spinneridx++ % 4]);
    val = 0x8000;

    for( volatile unsigned int* a=(unsigned int*)addr; a<=end; a++ ) {
      for( unsigned char i = 0; i<<8; i++) {
        *a = val;
        if (*a != val) {
          ec++;
        }
        val = val << i;
      }
    }

    vdpchar(VDP_SCREEN_TEXT(row, 26), spinner[spinneridx++ % 4]);
    val = 0xA5A5;
    for( volatile unsigned int* a=(unsigned int*)addr; a<=end; a++ ) {
      *a = val;
    }

    val = 0xA5A5;
    for( volatile unsigned int* a=(unsigned int*)addr; a<=end; a++ ) {
      if ( *a != val ) {
	    ec++;
      }
    }

    vdpchar(VDP_SCREEN_TEXT(row, 26), spinner[spinneridx++ % 4]);
    val = ~0xA5A5;
    for( volatile unsigned int* a=(unsigned int*)addr; a<=end; a++ ) {
      *a = val;
    }

    val = ~0xA5A5;
    for( volatile unsigned int* a=(unsigned int*)addr; a<=end; a++ ) {
      if ( *a != val ) {
	    ec++;
      }
    }
  }

  if (ec == 0) {
    writestring(row, 26, "PASSED");
  } else {
    writestring(row, 26, "FAILED");
  }

  return ec;
}

int test32k() {
    int ec = testBlock(5, (unsigned char*)0x2000);
    ec += testBlock(6, (unsigned char*)0xA000);
    ec += testBlock(7, (unsigned char*)0xC000);
    ec += testBlock(8, (unsigned char*)0xE000);
    return ec;
}

void main()
{
  set_text();
  VDP_SET_REGISTER(VDP_REG_COL, SCREEN_COLOR);
  vdpmemset(0x0000,' ',nTextEnd);
  charsetlc();

  writestring(0, 0, "Expansion Memory Test ver 1.4");

  writestring(23, 0, "- Jedimatt42/Atariage : matt@cwfk.net -");

  int passcount = *(((volatile int*)0x8300)+12);

  if (passcount > 1) {
    writestring(3, 8, "Burnin");
  }
  writestring(3, 15, "Pass >");

  unsigned int ec = 0;
  for( int i = 1; i <= passcount && ec == 0; i++ ) {
    writehex(3, 21, i);
    ec = test32k();
  }
  printSummary(ec);
  
  while(1) { }
}
