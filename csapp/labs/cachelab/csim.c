#include "cachelab.h"
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>

int verbose = 0;
// current instruction hit, miss and eviction count
int hits = 0, misses = 0, evictions = 0;
int s = 0; // number of set index bits
int E = 0; // number of lines per set
int b = 0; // number of block bits
int t = 0; // number of tag bits

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
  int size;
} CacheSet;

typedef struct Cache {
  // metadata about the cache
  int S;          // number of sets
  int E;          // number of lines per set
  int B;          // block size
  CacheSet *sets; // array of set
} Cache;

void moveToHead(CacheSet *set, CacheLine *line) {
  // remove line from the linkedlist
  line->prev->next = line->next;
  line->next->prev = line->prev;

  // add to the head
  set->dummyHead->next->prev = line;
  line->next = set->dummyHead->next;
  set->dummyHead->next = line;
  line->prev = set->dummyHead;
}

void addToHead(CacheSet *set, CacheLine *line) {
  set->dummyHead->next->prev = line;
  line->next = set->dummyHead->next;
  line->prev = set->dummyHead;
  set->dummyHead->next = line;

  set->size++;
}

// remove the least recently used cache line which is at the end of the set
CacheLine *removeTail(CacheSet *set) {
  CacheLine *last = set->dummyTail->prev;
  set->dummyTail->prev = last->prev;
  last->prev->next = set->dummyTail;

  set->size--;
  return last;
}

void initCache() {}

void accessCache(Cache *cache, unsigned address) {
  // get the set id [b, s+b]
  int64_t set_id = (address >> b) & ((1 << s) - 1);
  int64_t tag = (address >> (s + b)) & ((1 << t) - 1);
  // printf("accessCache: set id = %x, tag = %x\n", set_id, tag);

  // loop over all cache line within the set to match tag and valid bit
  CacheSet *set = &cache->sets[set_id];

  // if find, move node to head and return
  CacheLine *cur = set->dummyHead->next;
  while (cur != set->dummyTail) {
    if (cur->valid && cur->tag == tag) {
      hits++;
      if (verbose) {
        printf("hit ");
      }
      moveToHead(set, cur);
      return;
    }
    cur = cur->next;
  }

  misses++;
  if (verbose) {
    printf("miss ");
  }
  // if not find, add this cache line or or (evict and write)
  if (set->size >= E) {
    // evict
    CacheLine *last = removeTail(set);
    free(last);
    evictions++;
    if (verbose) {
      printf("eviction ");
    }
  }

  if (set->size >= E) {
    printf(
        "set->size should < E after eviction but actual size is %d and E is %d",
        set->size, E);
  }

  // add new cache line to the set
  CacheLine *new_line = malloc(sizeof(CacheLine));
  new_line->tag = tag;
  new_line->valid = 1;
  addToHead(set, new_line);
}

int main(int argc, char **argv) {
  int opt;
  char *trace_file = NULL;
  // loop over arguments
  while (-1 != (opt = getopt(argc, argv, "hvs:E:b:t:"))) {
    switch (opt) {
    case 'h':
      printf("TODO: print usage info");
      break;
    case 'v':
      verbose = 1;
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

  // printf("reading s = %d, E = %d, b = %d\n", s, E, b);
  t = 64 - s - b;

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
    cache->sets[i].size = 0;
  }

  // parse the input trace file and simulate the process
  FILE *pFile;
  pFile = fopen(trace_file, "r");
  char op;
  int64_t address;
  int size;

  while (fscanf(pFile, " %c %lx,%d", &op, &address, &size) > 0) {
    if (op != 'I' && verbose) {
      printf("%c %lx,%d ", op, address, size);
    }
    switch (op) {
    case 'L':
      accessCache(cache, address);
      break;
    case 'S':
      if (address == 0x600a80) {
        printf("debug");
      }
      accessCache(cache, address);
      break;
    case 'M':
      accessCache(cache, address);
      accessCache(cache, address);
      break;
    }
    if (op != 'I' && verbose) {
      printf("\n");
    }
  }

  fclose(pFile);

  // TODO: free Cache struct
  printSummary(hits, misses, evictions);
  return 0;
}
