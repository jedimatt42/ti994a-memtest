
#include <vdp.h>
#include <system.h>
#include <string.h>

#define PROG_VERSION "2.2"

extern char mcolor;
extern char scolor;
extern char fcolor;

#define SCREEN_COLOR mcolor
#define ERROR_COLOR fcolor
#define SUCCESS_COLOR scolor

int try_limit;
int corcompCru;

void __attribute__ ((noinline)) writehex(unsigned int row, unsigned int col, const unsigned int value) {
  unsigned char buf[3] = { 0, 0, 0 };
  *((unsigned int*)buf) = byte2hex[value >> 8];
  writestring(row, col, buf);
  *((unsigned int*)buf) = byte2hex[0xFF & value];
  writestring(row, col + 2, buf);
}

void __attribute__ ((noinline)) printSummary(int ec) {
  if (ec == 0) {
    VDP_SET_REGISTER(VDP_REG_COL, SUCCESS_COLOR);
    writestring(21, 11, "All Memory Passed");
  } else {
    VDP_SET_REGISTER(VDP_REG_COL, ERROR_COLOR);
    writestring(21, 14, "Found errors");
  }
}

int __attribute__ ((noinline)) testBlock(const unsigned int row, unsigned char* addr) {
  int ec = 0;
  unsigned int* end=(unsigned int*)(addr + 0x1FFF);
  writestring(row, 3, "Testing");
  writehex(row, 11, (int)addr);
  writestring(row, 16, "->");
  writehex(row, 19, (int)end);
  writestring(row, 24, ":");
  writestring(row, 26, "      ");

  const char spinner[4] = { '-', '\\', '|', '/' };
  int spinneridx = 0;

  for( int tries=0; tries<try_limit; tries++) {

    // Test with incrementing values
    vdpchar(VDP_SCREEN_TEXT(row, 26), spinner[spinneridx++ % 4]);
    unsigned int val = 0x0000;

    for( volatile unsigned int* a=(unsigned int*)addr; a<=end; a++ ) {
      *a = val;
      val++;
    }

    val = 0x0000;
    for (volatile unsigned int *a = (unsigned int *)addr; a <= end; a++) {
      if (*a != val) {
        ec++;
      }
      val++;
    }

    if (try_limit > 1) {
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
  }

  if (ec == 0) {
    writestring(row, 26, "PASSED");
  } else {
    writestring(row, 26, "FAILED");
  }

  return ec;
}

int __attribute__ ((noinline)) hasRam() {
  volatile int* lower_exp = (volatile int*) 0x2000;
  *lower_exp = 0;
  *lower_exp = 0x1234;
  return (*lower_exp == 0x1234);
}

#define CRU_FOUNDATION 0x1E00
#define CRU_MYARC 0x1000

void __attribute__ ((noinline)) foundationBank(int page, int crubase) {
  int adjusted = page << 8;

  __asm__(
    "MOV %1, r12\n\t"
    "LDCR %0,4\n\t"
    : : "r"(adjusted), "r"(crubase+2) : "r12"
  );
}

int __attribute__ ((noinline)) hasFoundation(int crubase) {
  volatile int* lower_exp = (volatile int*) 0x2000;
  foundationBank(0, crubase);
  *lower_exp = 0x1234;
  foundationBank(1, crubase);
  *lower_exp = 0;
  foundationBank(0, crubase);
  return (*lower_exp == 0x1234);
}

int __attribute__ ((noinline)) foundationPagecount(int crubase) {
  volatile int* lower_exp = (volatile int*) 0x2000;
  for(int i = 0; i < 16; i++) {
    foundationBank(i, crubase);
    *lower_exp = 0x1234;
  }
  foundationBank(0, crubase);
  int pages = 0;
  while(pages < 16 && *lower_exp != 0xFFFF) {
    *lower_exp = 0xFFFF;
    foundationBank(++pages, crubase);
  }
  foundationBank(0, crubase);
  return pages;
}

void crubit(int cru, int onoff) {
  __asm__(
    "MOV %0,r12\n\t"
    : : "r" (cru) : "r12"
  );

  if (onoff) {
    __asm__(
      "SBO 0\n\t"
    );
  } else {
    __asm__(
      "SBZ 0\n\t"
    );
  }
}

/*
Corcomp bank switching

CRU Bit         Purpose
>1000           DSR Rom Enable
>1002           Ram Disable (0 is disable)
>1004           Bank 0
>1006           Second 256K
>1008           Rom Page
>100C           Bank 1 \
>1014           Bank 2  \
>101C           Bank 3    \  Pages
>1024           Bank 4    /
>102C           Bank 5   /
>1034           Bank 6  /
>103C           Bank 7 /
>1040           Reset=1
*/
int __attribute__((noinline)) bank2cru(int page, int crubase) {
  unsigned char crus[8] = { 0x04, 0x0C, 0x14, 0x1C, 0x24, 0x2C, 0x34, 0x3C };
  return crubase + crus[page%8];
}

void __attribute__((noinline)) corcompBank(int page, int crubase) {
  int bank = page % 8;

  int bit = bank2cru(bank, crubase);

  crubit(crubase + 6, bank == page);

  int oldbank = bank == 0 ? 7 : bank - 1;
  int oldbit = bank2cru(oldbank, crubase);

  crubit(oldbit, 0);
  crubit(bit, 1);
}

int __attribute__((noinline)) hasCorcomp(int crubase) {
  // can be at crubase >1000 or >1400
  volatile int* lower_exp = (volatile int*)0x2000;

  // there may be clash with myarc... we can distinguish by
  // turning all banks off.
  crubit(crubase + 2, 0);
  // ram should be disabled now.
  *lower_exp = 0xCCCC;
  if (*lower_exp == 0xCCCC) {
    return 0;
  }

  // re-enable ram
  crubit(crubase + 2, 1);

  corcompBank(0, crubase);
  *lower_exp = 0x1234;
  corcompBank(1, crubase);
  *lower_exp = 0;
  corcompBank(0, crubase);
  if (*lower_exp == 0x1234) {
    corcompCru = crubase;
    return 1;
  } else {
    corcompCru = 0;
    return 0;
  }
}

int __attribute__((noinline)) corcompPagecount() {
  volatile int* lower_exp = (volatile int*)0x2000;
  for (int i = 0; i < 16; i++) {
    corcompBank(i, corcompCru);
    *lower_exp = 0x1234;
  }
  corcompBank(0, corcompCru);
  int pages = 0;
  while (pages < 16 && *lower_exp != 0xFFFF) {
    *lower_exp = 0xFFFF;
    corcompBank(++pages, corcompCru);
  }
  corcompBank(0, corcompCru);
  return pages;
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
  __asm__(
      "LI r12, >1E00\n\t"
      "SRL %0, 12\n\t"
      "SLA %0, 1\n\t"
      "SWPB %1\n\t"
      "SBO 0\n\t"
      "MOV %1, @>4000(%0)\n\t"
      "SBZ 0\n\t"
      "SWPB %1\n\t"
      :
      : "r"(location), "r"(page)
      : "r12");
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

int __attribute__ ((noinline)) samsPagecount() {
  samsMapOn();
  volatile int* lower_exp = (volatile int*) 0x2000;

  // set initial state of all pages
  for(int i = 0; i < 4096; i++) {
    samsMapPage(i, 0x2000);
    *lower_exp = 0x1234;
  }
  // now mark pages and stop when they repeat
  samsMapPage(0, 0x2000);
  int pages = 0;
  while(pages < 4096 && *lower_exp != 0xFFFF) {
    *lower_exp = 0xFFFF;
    samsMapPage(++pages, 0x2000);
  }
  samsMapOff();
  return pages;
}

int __attribute__ ((noinline)) test32k() {
  int ec = testBlock(6, (unsigned char*)0x2000);
  ec += testBlock(7, (unsigned char*)0xA000);
  ec += testBlock(8, (unsigned char*)0xC000);
  ec += testBlock(9, (unsigned char*)0xE000);
  return ec;
}

int __attribute__ ((noinline)) testFoundation(int pagecount, int crubase) {
  writestring(2, 20, int2str(32*pagecount));
  writestring(2, 23, "K");
  int ec = 0;
  for (int j = 0; j < pagecount && ec == 0; j++) {
    foundationBank(j, crubase);
    writestring(4, 16, "page ");
    writestring(4,21, int2str(j));
    ec += testBlock(6, (unsigned char*)0x2000);
    ec += testBlock(7, (unsigned char*)0xA000);
    ec += testBlock(8, (unsigned char*)0xC000);
    ec += testBlock(9, (unsigned char*)0xE000);
  }
  return ec;
}

int __attribute__((noinline)) testCorcomp(int pagecount) {
  writestring(2, 20, int2str(32 * pagecount));
  writestring(2, 23, "K");
  int ec = 0;
  for (int j = 0; j < pagecount && ec == 0; j++) {
    corcompBank(j, corcompCru);
    writestring(4, 16, "page ");
    writestring(4, 21, int2str(j));
    ec += testBlock(6, (unsigned char*)0x2000);
    ec += testBlock(7, (unsigned char*)0xA000);
    ec += testBlock(8, (unsigned char*)0xC000);
    ec += testBlock(9, (unsigned char*)0xE000);
  }
  return ec;
}

int __attribute__ ((noinline)) testSams(int pagecount) {
  writestring(2, 16, int2str(4*pagecount));
  writestring(2, 21, "K");
  int ec = 0;
  for (int j = 0; j < pagecount && ec == 0; j += 8 ) {
    samsMapOn();
    samsMapPage(j, 0x2000);
    samsMapPage(j+1, 0x3000);
    samsMapPage(j+2, 0xA000);
    samsMapPage(j+3, 0xB000);
    samsMapPage(j+4, 0xC000);
    samsMapPage(j+5, 0xD000);
    samsMapPage(j+6, 0xE000);
    samsMapPage(j+7, 0xF000);
    writestring(4, 16, "pages            ");
    writestring(4,22, int2str(j));
    writestring(4,26, "->");
    writestring(4,29, int2str(j+7));
    ec += testBlock(6, (unsigned char*)0x2000);
    ec += testBlock(7, (unsigned char*)0xA000);
    ec += testBlock(8, (unsigned char*)0xC000);
    ec += testBlock(9, (unsigned char*)0xE000);
    samsMapOff();
  }
  return ec;
}

#define NOMEM 0
#define FOUNDATION 1
#define SAMS 2
#define MYARC 3
#define BASE32K 4
#define CORCOMP 5

void main(int passcount)
{
  set_text();
  VDP_SET_REGISTER(VDP_REG_COL, SCREEN_COLOR);
  vdpmemset(0x0000,' ',nTextEnd);
  charsetlc();

  char* ver = PROG_VERSION;

  try_limit = 12;
  if (passcount == 0)
  {
    try_limit = 1;
    passcount = 1; // perform 1 pass, but abreviate the test using try_limit
    writestring(0, 0, "Memory Quick Check ver ");
    writestring(0, 23, ver);
  } else {
    writestring(0, 0, "Expansion Memory Test ver ");
    writestring(0, 26, ver);
  }

  writestring(23, 0, "- Jedimatt42/Atariage : matt@cwfk.net -");

  int memtype = NOMEM;

  if (!hasRam()) {
    writestring(2, 0, "No RAM detected");
    while(1) { }
  } else if (hasSams() && samsPagecount() > (128 / 4)) {
    writestring(2, 0, "SAMS detected");
    memtype = SAMS;
  } else if (hasCorcomp(0x1000) || hasCorcomp(0x1400)) {
    writestring(2, 0, "Corcomp 256k/512k");
    memtype = CORCOMP;
  } else if (hasFoundation(CRU_MYARC)) {
    writestring(2, 0, "Myarc detected");
    memtype = MYARC;
  } else if (hasFoundation(CRU_FOUNDATION)) { // sams testing can look like foundation :(
    writestring(2, 0, "Foundation detected");
    memtype = FOUNDATION;
  } else {
    writestring(2, 0, "Standard 32K detected");
    memtype = BASE32K;
  }

  if (passcount > 1) {
    writestring(4, 0, "Burnin");
    writestring(4, 7, "Pass ");
  }

  int pagecount = 0;

  if (memtype == SAMS) {
    pagecount = samsPagecount();
  } else if (memtype == MYARC) {
    pagecount = foundationPagecount(CRU_MYARC);
  } else if (memtype == FOUNDATION) {
    pagecount = foundationPagecount(CRU_FOUNDATION);
  } else if (memtype == CORCOMP) {
    pagecount = corcompPagecount();
  }

  int ec = 0;
  for( int i = 1; i <= passcount && ec == 0; i++ ) {
    if (passcount > 1) {
      writestring(4, 12, int2str(i));
    }
    switch(memtype) {
      case BASE32K:
        ec += test32k();
        break;
      case SAMS:
        ec += testSams(pagecount);
        break;
      case MYARC:
        ec += testFoundation(pagecount, CRU_MYARC);
        break;
      case FOUNDATION:
        ec += testFoundation(pagecount, CRU_FOUNDATION);
        break;
      case CORCOMP:
        ec += testCorcomp(pagecount);
        break;
    }
  }
  printSummary(ec);

  while(1) { }
}
