
#include <vdp.h>
#include <system.h>
#include <string.h>

#define SCREEN_COLOR (COLOR_BLACK << 4) + COLOR_CYAN
#define ERROR_COLOR (COLOR_BLACK << 4) + COLOR_MEDRED
#define SUCCESS_COLOR (COLOR_BLACK << 4) + COLOR_LTGREEN

void writehex(unsigned int row, unsigned int col, const unsigned int value);
void printSummary(unsigned int ec);
unsigned int testBlock(const unsigned int row, unsigned char* addr, int blocksize);
void mapOn();
void mapOff();
void mapPage(int page, int location);
int detectBoard();
void main();
void test32k(int passcount);
void testSams(int pages, int passcount);

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

unsigned int testBlock(const unsigned int row, unsigned char* addr, int blocksize) {
  unsigned int ec = 0;
  unsigned int* end=(unsigned int*)(addr + blocksize);
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

void mapOn() {
  __asm__(
    "LI r12, >1E00\n\t"
    "SBO 1\n\t"
  );
}

void mapOff() {
  __asm__(
    "LI r12, >1E00\n\t"
    "SBZ 1\n\t"
  );
}

void mapPage(int page, int location) {
  int adjusted = page << 8;

  __asm__(
    "LI r12, >1E00\n\t"
    "SRL %0, 12\n\t"
    "SLA %0, 1\n\t"
    "SBO 0\n\t"
    "MOVB %1, @>4000(%0)\n\t"
    "SBZ 0\n\t"
     : : "r"(location), "r"(adjusted) : "r12"
  );
}

int detectBoard() {
  mapOn();

  volatile int* lower_exp = (volatile int*) 0x2000;
  volatile int tag = 0x55AA;

  *lower_exp = tag;
  if (*lower_exp != tag) {
    // no expansion ram at all.
    return -1;
  }

  mapPage(4, 0x2000);
  if (*lower_exp == tag) {
    return -2;
  }

  // sams seems to be there, so lets find the page count
  tag = 0x1234;

  int i = 0;
  while(i < 256) {
    mapPage(i, 0x2000);
    *lower_exp = tag;
    i++;
  }

  int j = 0;
  mapPage(j, 0x2000);
  while(*lower_exp == tag) {
    *lower_exp = 0xffff;
    j++;
    mapPage(j, 0x2000);
  }

  mapOff();

  return j;
}

void main()
{
  int passcount = *(((volatile int*)0x8300)+12);

  set_text();
  VDP_SET_REGISTER(VDP_REG_COL, SCREEN_COLOR);
  vdpmemset(0x0000,' ',nTextEnd);
  charsetlc();

  writestring(1, 0, "Expansion Memory Test ver 1.4");

  writestring(20, 0, "- https://jedimatt42.com -");

  if (passcount > 1) {
    writestring(14, 8, "Burnin");
  }

  int samsPages = detectBoard();
  switch(samsPages) {
    case -1:
      writestring(2,0, "No expansion ram");
      break;
    case -2:
      writestring(2,0, "Standard 32k");
      test32k(passcount);
      break;
    default:
      if (samsPages > 0) {
        writestring(2,0, "SAMS Detected");
        writestring(2,14, int2str(samsPages * 4));
        writestring(2,18, "K");
        testSams(samsPages, passcount);
      } else {
        writestring(2,0, "Unknown Memory Type");
      }
      break;
  }

  // Reset some state the vdp interrupt expects
  VDP_INT_CTRL=VDP_INT_CTRL_DISABLE_SPRITES|VDP_INT_CTRL_DISABLE_SOUND;
  VDP_SCREEN_TIMEOUT=1;
  halt();
}

void test32k(int passcount) {
  writestring(14, 15, "Pass >");

  unsigned int ec = 0;
  for( int i = 1; i <= passcount && ec == 0; i++ ) {
    writehex(14, 21, i);
    ec += testBlock(4, (unsigned char*)0x2000, 0x1FFF);
    ec += testBlock(6, (unsigned char*)0xA000, 0x1FFF);
    ec += testBlock(8, (unsigned char*)0xC000, 0x1FFF);
    ec += testBlock(10, (unsigned char*)0xE000, 0x1FFF);
  }
  printSummary(ec);
}

void testSams(int pages, int passcount) {
  writestring(4, 15, "Pass >");
  writestring(5, 15, "Page");

  mapOn();

  unsigned int ec = 0;
  for( int i = 1; i <= passcount && ec == 0; i++ ) {
    for( int j = 0; j < pages && ec == 0; j++) {
      writehex(4, 21, i);
      writestring(5, 21, "   ");
      writestring(5, 21, int2str(j));
      mapPage(j, 0x2000);
      ec += testBlock(7, (unsigned char*)0x2000, 0x0FFF);
    }
  }
  printSummary(ec);

  mapOff();
}