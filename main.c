
#include <vdp.h>
#include <system.h>

#define SCREEN_COLOR (COLOR_BLACK << 4) + COLOR_CYAN
#define ERROR_COLOR (COLOR_BLACK << 4) + COLOR_MEDRED
#define SUCCESS_COLOR (COLOR_BLACK << 4) + COLOR_LTGREEN

void __attribute__ ((noinline)) writehex(unsigned int row, unsigned int col, const unsigned int value) {
  unsigned char buf[3] = { 0, 0, 0 };
  *((unsigned int*)buf) = byte2hex[value >> 8];
  writestring(row, col, buf);
  *((unsigned int*)buf) = byte2hex[0xFF & value];
  writestring(row, col + 2, buf);
}

void __attribute__ ((noinline)) printSummary(unsigned int ec) {
  if (ec == 0) {
    VDP_SET_REGISTER(VDP_REG_COL, SUCCESS_COLOR);
    writestring(21, 11, "All Memory Passed");
  } else {
    VDP_SET_REGISTER(VDP_REG_COL, ERROR_COLOR);
    writestring(21, 14, "Found errors");
  }
}

unsigned int __attribute__ ((noinline)) testBlock(const unsigned int row, unsigned char* addr) {
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

void __attribute__ ((noinline)) foundationBank(int page) {
  __asm__(
    "LI r12, >1E02\n\t"
    "LDCR %0,2\n\t"
    : : "r"(page) : "r12"
  );
}

void __attribute__ ((noinline)) samsMapOn() {
  __asm__(
    "LI r12, >1E00\n\t"
    "SBO 1\n\t"
  );
}

void __attribute__ ((noinline)) samsMapOff() {
  __asm__(
    "LI r12, >1E00\n\t"
    "SBZ 1\n\t"
  );
}

void __attribute__ ((noinline)) samsMapPage(int page, int location) {
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

int __attribute__ ((noinline)) hasRam() {
  volatile int* lower_exp = (volatile int*) 0x2000;
  *lower_exp = 0;
  *lower_exp = 0x1234;
  return (*lower_exp == 0x1234);
}

int __attribute__ ((noinline)) hasSams() {
  volatile int* lower_exp = (volatile int*) 0x2000;
  samsMapOn();
  samsMapPage(0, 0x2000);
  *lower_exp = 0x1234;
  samsMapPage(1, 0x2000);
  *lower_exp = 0;
  samsMapPage(0, 0x2000);
  int detected = (*lower_exp == 0x1234);
  samsMapOff();
  return detected;
}

int __attribute__ ((noinline)) test32k() {
    int ec = testBlock(6, (unsigned char*)0x2000);
    ec += testBlock(7, (unsigned char*)0xA000);
    ec += testBlock(8, (unsigned char*)0xC000);
    ec += testBlock(9, (unsigned char*)0xE000);
    return ec;
}

#define NOMEM 0
#define FOUNDATION 1
#define SAMS 2
#define MYARC 3
#define BASE32K 4

void main()
{
  int passcount = *(((volatile int*)0x8300)+12);

  set_text();
  VDP_SET_REGISTER(VDP_REG_COL, SCREEN_COLOR);
  vdpmemset(0x0000,' ',nTextEnd);
  charsetlc();

  writestring(0, 0, "Expansion Memory Test ver 1.4");

  writestring(23, 0, "- Jedimatt42/Atariage : matt@cwfk.net -");

  int memtype = NOMEM;

  if (!hasRam()) {
    writestring(2, 0, "No RAM detected");
    while(1) { }
  } else if (hasSams()) {
    writestring(2, 0, "SAMS detected");
    memtype = SAMS;
  } else {
    writestring(2, 0, "Standard 32K detected");
    memtype = BASE32K;
  }

  if (passcount > 1) {
    writestring(4, 0, "Burnin");
    writestring(4, 7, "Pass >");
  }

  unsigned int ec = 0;
  for( int i = 1; i <= passcount && ec == 0; i++ ) {
    if (passcount > 1) {
      writehex(4, 14, i);
    }
    ec = test32k();
  }
  printSummary(ec);
  
  while(1) { }
}
