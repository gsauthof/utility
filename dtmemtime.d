/*

   Measure high-water memory usage of a process and its descendants.

   To be called from the shell script wrapper dtmemtime
   which basically invokes it as:
   
   /usr/sbin/dtrace -C -s dtmemtime.d -c 'cmd arg1 ... argn'

   (where -C => run the script through cpp)

   2018, Georg Sauthoff <mail@gms.tf>, GPLv3+

 */

#pragma D option quiet

#include <sys/mman.h>

/* Global variables */
uint64_t global_highwater;
uint64_t global_mem;
uint64_t global_start;
/* uint64_t global_end; */

/* Thread local variables */
self uint64_t highwater;
self uint64_t mem;

self uint64_t first_brk;
self uint64_t brk_mem;

self uint64_t proc_start;
self uint64_t proc_end;

/* Associative array */
self int mapping[void*];

BEGIN
{
    global_start = timestamp;
}

END
{
    /* in DTrace format strings, the length modifier is optional,
       and when omitted, DTrace automatically recognizes the
       right length of a variable for printing */
    printf("Global runtime: %d ms, highwater memory: %d bytes\n",
            (timestamp-global_start)/1000000, global_highwater);
}

proc:::lwp-exit
/progenyof($target)/
{
    self->proc_end    = timestamp;
    self->proc_start  = self->proc_start ? self->proc_start : global_start;

    global_mem       -= self->mem;

    printf("pid %d (#%d) (%s) runtime: %d ms, highwater memory: %d bytes\n",
            pid, tid, execname,
            (self->proc_end - self->proc_start)/1000000,
            self->highwater);
}


syscall::exec*:return
/progenyof($target)/
{
    /* otherwise we may easily get very wrong numbers */
    self->first_brk = 0;
}

syscall::fork*:return
/progenyof($target) && arg0 == 0/
{
    /* due to copy-on-write, this is probably for the best */
    self->first_brk  = 0;
}

proc:::lwp-start
/progenyof($target)/
{
    self->proc_start = timestamp;
}

syscall::brk:entry
/progenyof($target) && !self->first_brk/
{
    self->first_brk  = (uint64_t)arg0;
}

syscall::brk:entry
/progenyof($target)/
{
    self->mem       -= self->brk_mem;
    global_mem      -= self->brk_mem;
    self->brk_mem    = ((uint64_t)arg0) - self->first_brk;
    self->mem       += self->brk_mem;
    global_mem      += self->brk_mem;

    self->highwater  = self->mem > self->highwater ?
                       self->mem : self->highwater;
    global_highwater = global_mem > global_highwater ?
                       global_mem : global_highwater;
}

syscall::mmap*:entry
/progenyof($target) && ((arg3 & MAP_ANON) == MAP_ANON) && ((arg2 & PROT_WRITE)==PROT_WRITE)/
{
    self->mem        += (uint64_t)arg1;
    self->highwater   = self->mem > self->highwater ?
                        self->mem : self->highwater;

    global_mem       += (uint64_t)arg1;
    global_highwater  = global_mem > global_highwater ?
                        global_mem : global_highwater;

    self->mapping[(void*)arg0] = 1;
}

    /* gross simplification because:
     * 1. it's ok to unmap non-mapped paged
     * 2. it's possible to specify the length in non page-multiples
     *    but still the complete pages are unmapped */
syscall::munmap*:entry
/progenyof($target) && self->mapping[(void*)arg0]/
{
    self->mem  -= (uint64_t)arg1;
    global_mem -= (uint64_t)arg1;

    /* zeroing it implicitly deletes the entry in the associative array */
    self->mapping[(void*)arg0] = 0;
}


