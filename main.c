
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
    writestring(18, 13, "All 32K Passed");
  } else {
    VDP_SET_REGISTER(VDP_REG_COL, ERROR_COLOR);
    writestring(18, 14, "Found errors");
  }
}

unsigned int test4k(const unsigned int row, unsigned char* addr) {
  unsigned int ec = 0;
  unsigned int* end=(unsigned int*)(addr + 0x1FFF);
  writestring(row, 3, "Testing");
  writehex(row, 11, (int)addr);
  writestring(row, 16, "->");
  writehex(row, 19, (int)end);
  writestring(row, 24, ":");
  writestring(row, 26, "      ");

  const char spinner[4] = { '-', '\\', '|', '/' };

  for( int tries=0; tries<10; tries++) {
    vdpchar(VDP_SCREEN_TEXT(row, 26), spinner[tries % 4]);

    // Test with incrementing values
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

void main()
{
  int unblank = set_text();
  VDP_SET_REGISTER(VDP_REG_COL, SCREEN_COLOR);
  vdpmemset(0x0000,' ',nTextEnd);
  VDP_SET_REGISTER(VDP_REG_MODE1, unblank);
  charsetlc();

  writestring(1, 0, "32K Expansion Memory Test ver 1.1");

  writestring(20, 0, "- Jedimatt42/Atariage : matt@cwfk.net -");
  writestring(22, 0, "   Crafted with gcc-tms9900 by insomnia");
  writestring(23, 0, "              & libti99 by Tursi");


  writestring(14, 15, "Pass #");

  unsigned int ec = 0;
  for( int i = 1; i <= 10 && ec == 0; i++ ) {
    writehex(14, 21, i);
    ec += test4k(4, (unsigned char*)0x2000);
    ec += test4k(6, (unsigned char*)0xA000);
    ec += test4k(8, (unsigned char*)0xC000);
    ec += test4k(10, (unsigned char*)0xE000);
  }
  printSummary(ec);
  
  // Reset some state the vdp interrupt expects
  VDP_INT_CTRL=VDP_INT_CTRL_DISABLE_SPRITES|VDP_INT_CTRL_DISABLE_SOUND;
  VDP_SCREEN_TIMEOUT=1;
  halt();
}
