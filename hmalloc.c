
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <math.h>
#include "hmalloc.h"

typedef struct free_block {
  size_t size;
  struct free_block* next; //HERE
} free_block;

const size_t PAGE_SIZE = 4096;
static hm_stats stats; // This initializes the stats to 0.
static free_block* head; //This initialises the head to 0.

long
free_list_length()
{
    long l = 0;
    free_block* node = head;
    while(node!=0){
        node = node->next;
        ++l;
    }
    return l;    
}

hm_stats*
hgetstats()
{
    stats.free_length = free_list_length();
    return &stats;
}

void
hprintstats()
{
    stats.free_length = free_list_length();
    fprintf(stderr, "\n== husky malloc stats ==\n");
    fprintf(stderr, "Mapped:   %ld\n", stats.pages_mapped);
    fprintf(stderr, "Unmapped: %ld\n", stats.pages_unmapped);
    fprintf(stderr, "Allocs:   %ld\n", stats.chunks_allocated);
    fprintf(stderr, "Frees:    %ld\n", stats.chunks_freed);
    fprintf(stderr, "Freelen:  %ld\n", stats.free_length);
}

static
size_t
div_up(size_t xx, size_t yy)
{
    // This is useful to calculate # of pages
    // for large allocations.
    size_t zz = xx / yy;

    if (zz * yy == xx) {
        return zz;
    }
    else {
        return zz + 1;
    }
}

void helper(free_block* puttable){
    //Check if the free list is empty, if so, put as first block of memory. 
    if(head==0){
        head = puttable;
        return;
    }
    else{
        //Addresses of nodes are mapped by memory. We iterate till we find an address lesser. 
        free_block* temp = head;
        free_block* previous = 0;
        while(temp!=0){
            //Check if we reached the point where we have to insert
            if((void*) temp > (void*) puttable){ //Here is where we have to put it
                size_t previous_size = 0;
                if (previous != 0) {
                    previous_size = previous->size;
                }              
                //First we check if the node is addressed sequentially to two other nodes, in which case we coalesce. 
                if (((void*) previous + previous_size == (void*) puttable) && ((void*) puttable + puttable->size == (void*) temp)) {
                    previous->next = temp->next;
                    previous->size = previous->size + puttable->size + temp->size;
                }
                //If not the previous condition, we then check if something is available on the left. 
                else if (((void*) previous + previous_size == (void*) puttable))
                    previous->size+=puttable->size;
                //If not the previous condition, we then check if something is available on the right. 
                else if (((void*) puttable + puttable->size == (void*) temp)){
                    puttable->size+=temp->size;
                    if(previous != 0){
                        previous->next = temp;
                    }
                    puttable->next = temp->next;
                }
                else{ //Nothing is available at the moment so we do nothing, just add it to the free list. 
                    puttable->next = temp;  
                    if(previous!=0){
                        previous->next = puttable;
                    }
                } 
                if(previous == 0){
                    head = puttable;
                }
                break;
            }
            previous = temp;
            temp = temp->next;
        }
    }
}

void*
hmalloc(size_t size)
{
    stats.chunks_allocated += 1;
    size += sizeof(size_t);
    if (size < PAGE_SIZE) {
      free_block* previous = 0;
      free_block* used = 0;
      free_block* node = head;
      while (node != 0) {
        if (node->size >= size) {
          used = node; 
          if (previous == 0) {
            head = node->next;
          } else {
            previous->next = node->next;
          }
          break;
        }
        previous = node; 
        node = node->next; 
      }
      if (used == 0) {
        used = mmap(0, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        stats.pages_mapped += 1;
        used->size = PAGE_SIZE;
      }
      if ((used->size - size > sizeof(free_block))) {
        void* address = (void*) used + size;
        free_block* leftover = (free_block*) address;
        leftover->size = used->size - size;
        helper(leftover);
        used->size = size;
      }

      return (void*) used + sizeof(size_t);
    }

    else {
      long pages_needed = (size+4095)/PAGE_SIZE;
      size_t size = pages_needed * PAGE_SIZE;
      stats.pages_mapped += pages_needed;
      free_block* block = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
      block->size = size; 
      return (void*) block + sizeof(size_t);
    }
}

void
hfree(void* item)
{
    stats.chunks_freed += 1;
    //We need to free-
    //1) The item itself
    //2) Its size info
    free_block* freeable = (free_block*) (item - sizeof(size_t));
    //If we have less than page size to be freed
    if (freeable->size < PAGE_SIZE) 
      helper(freeable);
    else{
        long pages_needed = (long) ceil(freeable->size/PAGE_SIZE);
        munmap((void*) freeable, freeable->size);
        stats.pages_unmapped += pages_needed;
    }    
}