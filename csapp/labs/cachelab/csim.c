#include "cachelab.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>

typedef struct CacheLine {
  int valid;
  int tag;
  struct CacheLine *next;
  struct CacheLine *prev;
} CacheLine;

// CacheSet has a doubly linked list of each line within the set
typedef struct CacheSet {
  // pointer to the head of list
  CacheLine *dummyHead;
  // pointer to the tail of list
  CacheLine *dummyTail;
} CacheSet;

typedef struct Cache {
  // metadata about the cache
  int S;          // number of sets
  int E;          // number of lines per set
  int B;          // block size
  CacheSet *sets; // array of set
} Cache;

void initCache() {

}

int main(int argc, char **argv) {
  int opt;
  int s = 0;
  int E = 0;
  int b = 0;
  char *trace_file = NULL;
  // loop over arguments
  while (-1 != (opt = getopt(argc, argv, "h:v:s:E:b:t:"))) {
    switch (opt) {
    case 'h':
      printf("TODO: print usage info");
      break;
    case 'v':
      printf("TODO: print verbose flag that displays trace info");
      break;
    case 's':
      s = atoi(optarg);
      break;
    case 'E':
      E = atoi(optarg);
      break;
    case 'b':
      b = atoi(optarg);
      break;
    case 't':
      int len = strlen(optarg) + 1;
      trace_file = malloc(len);
      strncpy(trace_file, optarg, len);
      // strcpy(trace_file, optarg);
      break;
    default:
      printf("wrong argument\n");
      break;
    }
  }

  printf("reading s = %d, E = %d, b = %d\n", s, E, b);

  // initCache(cache, s, E, b);
  Cache *cache = malloc(sizeof(Cache));
  cache->B = pow(2, b);
  cache->E = E;
  cache->S = pow(2, s);
  cache->sets = malloc(sizeof(CacheSet) * cache->S);
  for (int i = 0; i < cache->S; ++i) {
    cache->sets[i].dummyHead = malloc(sizeof(CacheLine));
    cache->sets[i].dummyTail = malloc(sizeof(CacheLine));

    cache->sets[i].dummyHead->next = cache->sets[i].dummyTail;
    cache->sets[i].dummyHead->prev = NULL;
    cache->sets[i].dummyTail->next = NULL;
    cache->sets[i].dummyTail->prev = cache->sets[i].dummyHead;
  }

  int hits = 0, misses = 0, evictions = 0;
  // TODO: parse the input trace file and simulate the process
  FILE *pFile;
  pFile = fopen(trace_file, "r");
  char op;
  unsigned address;
  int size;

  while (fscanf(pFile, " %c %x,%d", &op, &address, &size) > 0) {
    printf("reading line: %c %x,%d\n", op, address, size);    
  }

  fclose(pFile);

  // TODO: free Cache struct
  printSummary(hits, misses, evictions);
  return 0;
}
