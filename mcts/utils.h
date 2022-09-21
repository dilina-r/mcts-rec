#pragma once

#include <stdlib.h>

//#define DEBUG_DL 1
//#define DEBUG_MEM 1 // extra array bound checks

/*#ifdef DEBUG_DL
#define DEBUG_PRINT(...) printf(__VA_ARGS__ )
#else
#define DEBUG_PRINT(...) do {} while (0)
#endif*/
bool debug = false; // debugging output
#define DEBUG_PRINT(args ...) if (debug) printf(args)


void print_items(int* items, int num_items) {
  for (int i=0; i<num_items; i++) {
    DEBUG_PRINT("%d ",items[i]);
  }
  DEBUG_PRINT("\n");
}
