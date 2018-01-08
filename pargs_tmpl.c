// Word size (32/64) dependent ELF core file read functions.
// To be included from pargs.c
//
// 2017, Georg Sauthoff <mail@gms.tf>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PARGS_ELF_CLASS
#error "PARGS_ELF_CLASS must be set to 32 or 64 before including this file"
#endif

#define WIDTH PARGS_ELF_CLASS

#ifdef CAT
#error "symbol CAT already defined ..."
#endif

#define CAT_P(A, B) A ## B
// one redirection to expand A and B
#define CAT(A, B) CAT_P(A, B)
#define CAT3_P(A, B, C) A ## B ## C
#define CAT3(A, B, C) CAT3_P(A, B, C)

#include <assert.h> // static_assert()
#include <stddef.h> // offsetof()
#include <sys/procfs.h> // prpsinfo_t

// necessary e.g. when compiling with clang 3.4 under CentOS/RHEL 6
#ifndef static_assert
#define static_assert _Static_assert
#endif

// expands to parse_auxv_32() or parse_auxv_64()
//
// We look for the AT_EXECFN value because it's an address that points
// to the exec filename which is located on top of the envp/argv
// strings. Thus, we can use that address to identify the right
// section and search for the vectors.
static int CAT(parse_auxv_, WIDTH) (const char *filename, Landmarks *lm,
    const unsigned char *begin, const unsigned char *end)
{
  lm->auxv_note.begin = begin;
  lm->auxv_note.end   = end;
  lm->execfn_addr = 0;
  for (const unsigned char *p = begin; p < end; p += (WIDTH/8) * 2) {
    CAT3(uint, WIDTH, _t) key;
    memcpy(&key, p, sizeof key);
    if (lm->need_to_swap)
      key = CAT(__builtin_bswap, WIDTH) (key);
    if (!key || key == AT_EXECFN) {
      CAT3(uint, WIDTH, _t) value;
      memcpy(&value, p + WIDTH/8, sizeof value);
      if (lm->need_to_swap)
        value = CAT(__builtin_bswap, WIDTH) (value);
      switch (key) {
        case 0:
          if (value) {
            fprintf(stderr, "Unexpected auxv value for sentinel key.\n");
            return -1;
          }
          if (!lm->execfn_addr) {
            fprintf(stderr, "Didn't see AT_EXECFN in %s.\n", filename);
            return -1;
          }
          return 0;
        case AT_EXECFN:
          debug("Found AT_EXECFN value: 0x%" CAT(PRIx, WIDTH), value);
          lm->execfn_addr = value;
          break;
      }
    }
  }
  fprintf(stderr, "Sentinel missing in auxv note in %s\n", filename);
  return -1;
}

// expands to parse_prpsinfo_32() or parse_prpsinfo_64()
static int CAT(parse_prpsinfo_, WIDTH) (const char *filename, Landmarks *lm,
    const unsigned char *begin, const unsigned char *end)
{
  debug("prpsinfo note size: %zu", (size_t)(end - begin));
  uint32_t pid;
  ssize_t extra_off = 0;
  // __WORDSIZE is used in sys/procfs.h's definition of prpsinfo
  static_assert(__WORDSIZE == 32 || __WORDSIZE == 64,
      "unexpected __WORDSIZE");
  // why 12 bytes? On 64 bit (LP64),in contrast to 32 bit (ILP32):
  // there are 4 bytes padding after pr_nice
  // pr_flag is 8 bytes instead of 4
  // pr_uid + pr_gid use 8 bytes instead of 4
  if (__WORDSIZE < WIDTH)
    extra_off = 12;
  if (__WORDSIZE > WIDTH)
    extra_off = -12;
  if (sizeof(prpsinfo_t) + extra_off > end - begin) {
    fprintf(stderr, "prpsinfo section overflows in %s\n", filename);
    return -1;
  }

  // according to sys/procfs.h, pid should be 4 byte everwhere
  static_assert(sizeof(pid_t) == 4, "pid_t is not 4 byte large ...");
  memcpy(&pid, begin + offsetof(prpsinfo_t, pr_pid) + extra_off, sizeof pid);
  if (lm->need_to_swap)
    pid = __builtin_bswap32(pid);
  debug("found PID: %ju", (intmax_t)pid);
  lm->pid = pid;
  // we assume that it is always zero terminated
  lm->fname = (const char*)
    (begin + offsetof(prpsinfo_t, pr_fname) + extra_off);
  const unsigned char *t = memchr(lm->fname, 0, 16);
  if (!t) {
    fprintf(stderr, "prpsinfo::pr_fname is not zero terminated in %s.",
        filename);
  }
  // we assume that it is always zero terminated
  lm->args = (const char*)
    (begin + offsetof(prpsinfo_t, pr_psargs) + extra_off);
  t = memchr(lm->args, 0, ELF_PRARGSZ);
  if (!t) {
    fprintf(stderr, "prpsinfo::pr_psargs is not zero terminated in %s.",
        filename);
  }
  debug("found call: %s %s", lm->fname, lm->args);
  return 0;
}

// expands to parse_note_32() or parse_note_64()
static int CAT(parse_note_, WIDTH) (const char *filename,
    Landmarks *lm, uint32_t note_type,
    const unsigned char *name_begin, const unsigned char *name_end,
    const unsigned char *desc_begin, const unsigned char *desc_end)
{
  switch (note_type) {
    case NT_AUXV:
      debug("Reading auxv note");
      return CAT(parse_auxv_, WIDTH) (filename, lm, desc_begin, desc_end);
      break;
    case NT_PRPSINFO:
      debug("Reading prpsinfo note");
      return CAT(parse_prpsinfo_, WIDTH) (filename, lm, desc_begin, desc_end);
      break;
    default:
      debug("Skipping other note");
  }
  return 0;
}


// expands to parse_notes_32() or parse_notes_64()
static int CAT(parse_notes_, WIDTH) (
    const Range *range, const char *filename, Landmarks *lm,
    const Range *section_range)
{
  const unsigned char *x = section_range->begin;
  const unsigned char *y = section_range->end;
  size_t i = 0;
  for (const unsigned char *p = x; p < y; ++i) {
    uint32_t name_size;
    memcpy(&name_size, p, sizeof name_size);
    if (lm->need_to_swap)
      name_size = __builtin_bswap32(name_size);
    uint32_t aligned_name_size = align32_up_4(name_size);

    uint32_t desc_size;
    // Actually, the Nhdr has the same layout for 32/64 bit
    memcpy(&desc_size, p + offsetof(CAT3(Elf, WIDTH,_Nhdr), n_descsz),
        sizeof desc_size);
    if (lm->need_to_swap)
      desc_size = __builtin_bswap32(desc_size);
    uint32_t aligned_desc_size = align32_up_4(desc_size);

    static_assert(sizeof(CAT3(Elf, WIDTH, _Nhdr)) == 3*4,
        "Note header isn't 3*4");
    if (p + sizeof(CAT3(Elf, WIDTH, _Nhdr))
         + aligned_name_size + desc_size > y) {
      fprintf(stderr, "%zuth note overflows NOTE section in %s.\n",
          i, filename);
      return -1;
    }

    uint32_t note_type;
    memcpy(&note_type, p + offsetof(CAT3(Elf, WIDTH, _Nhdr), n_type),
        sizeof note_type);
    if (lm->need_to_swap)
      note_type = __builtin_bswap32(note_type);

    p += sizeof(CAT3(Elf, WIDTH, _Nhdr));
    int r = CAT(parse_note_, WIDTH) (filename, lm, note_type,
        p, p + name_size,
        p + aligned_name_size, p + aligned_name_size + desc_size);
    p += aligned_name_size + aligned_desc_size;
    if (r)
      return r;
  }
  return 0;
}

/*

The interetesting vectors like argv, envp, pieces like exec_fn
and the stack are located in the same memory region that is mapped
at the top of the address space. Basically it looks like this:

highest address   := end of mapping
      |           execfn[0] .. execfn[ex_l-1] \0  <-------------------+
      |       +-> envp[m-1][0] .. envp[m-1][envp_l[m-1]-1] \0         |
      |       |   ..                                                  |
      |       |   envp[0][0] .. envp[0][envp_l[0]-1] \0               |
      |       |   argv[n-1][0] .. argv[n-1][argv_l[n-1]-1] \0  <--------+
      |       |   ..                                                  | |
      |     +---> argv[0][0] .. argv[0][argv_l[0]-1] \0  <----------------+
      |     | |   ..                                                  | | |
      |     | |   platform[0] .. platform[p_l-1] \0   <-------------+ | | |
      |     | |   random[0] .. random[r_l-1]  <- 16 bytes  <------+ | | | |
      |     | |   ..                                              | | | | |
      |     | |   \0 .. \0 := 2 * $word_size/8 bytes, end-of-auxv | | | | |
      |     | |   ..                                              | | | | |
      |     | |   AT_RANDOM   addr_p -----------------------------+ | | | |
      |     | |   ..                                                | | | |
      |     | |   AT_PLATFORM addr_r -------------------------------+ | | |
      |     | |   ..                                                  | | |
      |     | |   AT_EXECFN   addr_e ---------------------------------+ | |
      |     | |   ..                                                    | |
      |     | |   .. <- first auxv pair (2 *$word_size/8 bytes)         | |
      |     | |   ..                                                    | |
      |     | |   envp[m] = \0 .. \0 := $word_size/8 bytes              | |
      |     | +-- envp[m-1]          := $word_size/8 bytes              | |
      |     |     ..                                                    | |
      |     +---- envp[0]                                               | |
      |           argv[n] = \0 .. \0  := $word_size/8 bytes  -----------+ |
      |           ..                                                      |
      |           argv[n-1]           := $word_size/8 bytes  -------------+
      |           ..
      |           argv[0]
      |           argc := $word_size/8 bytes
      |           ..
      |           stack_frame[2] start
      |           ..
      |           stack_frame[2] end
      |           ..
      |           .. := more stack frames ...
      |           ..
      |           .. := stack grows downwards
      |
      |
     \./
lowest address

Note that the auxillary vector (auvx) is also copied in the NOTE
section of core files - as separate NOTE. Thus, we don't have to
search for it here and can just use the copy. Addresses still have to
be resolved in this stack section, though.

See also:
How programs get run: ELF binaries (2015)
https://lwn.net/Articles/631631/

Readelf dump of AUXV values of an example core file (elf-utils):
eu-readelf -a core
Readelf hexdump output of an example core file:
readelf -x $sectionnr core

 */
static int CAT(find_vectors_, WIDTH) (const char *filename,
    Landmarks *lm, const Range *section_range,
    CAT3(uint, WIDTH, _t) section_virt_addr)
{
  lm->vector_section = *section_range;
  lm->vector_base_addr = section_virt_addr;
  const unsigned char *b = section_range->begin;
  const unsigned char *e = section_range->end;
  if (lm->execfn_addr < section_virt_addr) {
    fprintf(stderr, "Execfn address underflows section.\n");
    return -1;
  }
  CAT3(uint, WIDTH, _t) execfn_off = lm->execfn_addr - section_virt_addr;
  const unsigned char *p = memchr(b + execfn_off, 0, e - (b + execfn_off));
  if (!p) {
    fprintf(stderr, "exec filename is not null-terminated in %s.\n",
        filename);
    return -1;
  }
  p = b + execfn_off;
  debug("exec filename: %s", p);

  unsigned char envp_tail[4 * (WIDTH/8)] = {0};
  {
    unsigned j = sizeof envp_tail - (WIDTH/8);
    for (unsigned i = 0; i < sizeof envp_tail / (WIDTH/8);
        ++i, j -= (WIDTH/8)) {
      if (p-b <= 2) {
        fprintf(stderr, "Underflow in envp tail search.\n");
        return -1;
      }
      p = memrchr(b, 0, (p-b)-2);
      if (!p)
        return -1;
      ++p;
      CAT3(uint, WIDTH, _t) addr = section_virt_addr + (p - b);
      debug("Trailing environment string `%s' at offset %zd"
          " (virt address %" CAT(PRIx, WIDTH) ")", p, (p - b), addr);
      if (lm->need_to_swap)
        addr = CAT(__builtin_bswap, WIDTH) (addr);
      memcpy(envp_tail + j, &addr, WIDTH/8);
    }
  }
  p = aligned_naive_memmemr(b, execfn_off / (WIDTH/8) * (WIDTH/8),
      envp_tail, sizeof envp_tail, WIDTH/8);
  if (!p) {
    fprintf(stderr, "Can't find envp tail in %s.\n", filename);
    return -1;
  }
  debug("Found envp tail at offset %zd (virt address %" CAT(PRIx, WIDTH) ")",
      (p - b), section_virt_addr + (CAT3(uint, WIDTH, _t)) (p - b));
  lm->envp.end = p + sizeof envp_tail;
  CAT3(uint, WIDTH, _t) virt_begin =
    section_virt_addr + ((p + sizeof envp_tail) - b);
  CAT3(uint, WIDTH, _t) virt_end = section_virt_addr + (e - b);

  unsigned char argv_tail[WIDTH/8] = {0};
  p = aligned_naive_memmemr(b, p - b, argv_tail, sizeof argv_tail, WIDTH/8);
  if (!p) {
    fprintf(stderr, "Can't find argv tail in %s.\n", filename);
    return -1;
  }
  debug("Found argv tail at offset %zd (virt address %" CAT(PRIx, WIDTH) ")",
      (p - b), section_virt_addr + (CAT3(uint, WIDTH, _t)) (p - b));
  lm->envp.begin = p + sizeof argv_tail;
  lm->argv.end = p;

  p -= WIDTH/8;
  lm->argc = 0;
  for (; p > b; p -= WIDTH/8) {
    // amd64: argc is int, which is 4 byte, but argv[0] is 8 byte aligned
    // thus, we can read argc as uint64_t where the 4 most significant bytes
    // are zero
    CAT3(uint, WIDTH, _t) t;
    memcpy(&t, p, WIDTH/8);
    if (lm->need_to_swap)
      t = CAT(__builtin_bswap, WIDTH) (t);
    if (t < virt_begin || t >= virt_end) {
      lm->argc = t;
      debug("Found argc = %d at offset %zd (virt address %"
          CAT(PRIx, WIDTH) ")", lm->argc,
          (p - b), section_virt_addr + (CAT3(uint, WIDTH, _t)) (p - b));
      lm->argv.begin = p + WIDTH/8;
      break;
    }
  }
  if (!lm->argc) {
    fprintf(stderr, "Didn't find argc in %s.\n", filename);
    return -1;
  }
  return 0;
}

// expands to parse_segment_32() or parse_segment_64()
static int CAT(parse_segment_, PARGS_ELF_CLASS) (
    const Range *range, const char *filename, Landmarks *lm,
    const Range *segment_range, uint32_t segment_type,
    CAT3(uint, WIDTH, _t) segment_virt_addr)
{
  CAT3(uint, WIDTH, _t) segment_size =
    segment_range->end - segment_range->begin;
  switch (segment_type) {
    case PT_NOTE:
      debug("Reading NOTE segment");
      return CAT(parse_notes_, WIDTH) (range, filename, lm, segment_range);
    case PT_LOAD:
      if (lm->execfn_addr > segment_virt_addr
          && lm->execfn_addr < segment_virt_addr + segment_size) {
        debug("Found segment that includes the execfn address");
        return CAT(find_vectors_, WIDTH) (filename, lm, segment_range,
            segment_virt_addr);
      } else {
        debug("Skipping PROGBITS segment");
      }
      break;
    default: debug("Skipping other segment");
  }
  return 0;
}

/*

Note that the ELF header points to two tables:

- the program header table
- the section header table

The dumped memory content we are interested in is reachable via
both tables (cf. Wikipedia's [two-views figure][elfviews]). Core files
generated by `gcore` contain a section header table, while core
files generated by the kernel don't. Thus, we have to traverse
the program headers instead of the section headers.

The first traversal stop is the notes segement. It's
corresponding program header has type PT_NOTE where as the
referencing section has type SHT_NOTE. The vectors and other
interesting bits are in a segment referenced by a header of type
PT_LOAD or SHT_PROGBITS, respectively.

[elfviews]: https://commons.wikimedia.org/wiki/File:Elf-layout--en.svg

   */


// expands to parse_landmarks_32() or parse_landmarks_64()
static int CAT(parse_landmarks_, PARGS_ELF_CLASS) (
    const Range *range, const char *filename, Landmarks *lm)
{
  const unsigned char *b = range->begin;
  const unsigned char *e = range->end;
  if (e - b < sizeof(CAT3(Elf, WIDTH, _Ehdr))) {
    fprintf(stderr, "file too small for ELF%d header\n", WIDTH);
    return -1;
  }
  uint16_t type;
  // under strict-aliasing rules a Elf(32|64_Ehdr) pointer can't alias
  // a unsigned char pointer
  // the compiler should be able to optimize the memcpy() away
  // we could also copy the complete struct, but we just need a few fields
  // also, we need to byte-swap some integer fields when core file was
  // generated a machine with a different byte order ...
  memcpy(&type, b + offsetof(CAT3(Elf, WIDTH, _Ehdr), e_type), sizeof type);
  if (lm->need_to_swap)
    type = __builtin_bswap16(type);
  if (type != ET_CORE) {
    fprintf(stderr, "%s is not a core file.\n", filename);
    return -1;
  }
  uint16_t first_segment_count;
  memcpy(&first_segment_count, b + offsetof(CAT3(Elf, WIDTH, _Ehdr), e_phnum),
      sizeof first_segment_count);
  if (lm->need_to_swap)
    first_segment_count = __builtin_bswap16(first_segment_count);
  debug("%" PRIu16 " segments", first_segment_count);
  if (!first_segment_count) {
    fprintf(stderr, "File %s has no segments.\n", filename);
    return -1;
  }
  CAT3(uint, WIDTH, _t) program_header_off;
  memcpy(&program_header_off, b + offsetof(CAT3(Elf, WIDTH, _Ehdr), e_phoff),
      sizeof program_header_off);
  if (lm->need_to_swap)
    program_header_off = CAT(__builtin_bswap, WIDTH) (program_header_off);
  uint16_t program_header_size;
  memcpy(&program_header_size, b + offsetof(CAT3(Elf, WIDTH, _Ehdr),
        e_phentsize), sizeof program_header_size);
  if (lm->need_to_swap)
    program_header_size = __builtin_bswap16(program_header_size);
  if (program_header_off + program_header_size > e - b) {
    fprintf(stderr, "%s: program header table overflows\n", filename);
    return -1;
  }
  CAT3(uint, WIDTH, _t) segment_count = first_segment_count;
  // cf. the ELF standard, e.g. elf(5) and search for PN_XNUM
  if (segment_count >= PN_XNUM) {
    debug("file has more than 2**16-1 segments");
    const unsigned char *p = b + program_header_off;
    memcpy(&segment_count, p + offsetof(CAT3(Elf, WIDTH, _Shdr), sh_size),
        sizeof segment_count);
    if (lm->need_to_swap)
      segment_count = CAT(__builtin_bswap, WIDTH) (segment_count);
    debug("new segment count: %" CAT(PRIu, WIDTH), segment_count);
  }
  if (program_header_off + segment_count * program_header_size > e - b) {
    fprintf(stderr, "%s: program header table overflows\n", filename);
    return -1;
  }
  CAT3(uint, WIDTH, _t) i = 0;
  for (const unsigned char *p = b + program_header_off
        + (segment_count != first_segment_count) * program_header_size;
      p < b + program_header_off + segment_count * program_header_size;
      p += program_header_size, ++i) {
    debug("Reading %" CAT(PRIu, WIDTH) "th segment", i+1);
    CAT3(uint, WIDTH, _t) segment_off;
    memcpy(&segment_off, p + offsetof(CAT3(Elf, WIDTH, _Phdr), p_offset),
        sizeof segment_off);
    if (lm->need_to_swap)
      segment_off = CAT(__builtin_bswap, WIDTH) (segment_off);
    CAT3(uint, WIDTH, _t) segment_size;
    memcpy(&segment_size, p + offsetof(CAT3(Elf, WIDTH, _Phdr), p_filesz),
        sizeof segment_size);
    if (lm->need_to_swap)
      segment_size = CAT(__builtin_bswap, WIDTH) (segment_size);
    if (segment_off + segment_size > e - b) {
      fprintf(stderr, "segment %" CAT(PRIu, WIDTH) " overflows %s.\n",
          i, filename);
      return -1;
    }
    uint32_t segment_type;
    memcpy(&segment_type, p + offsetof(CAT3(Elf, WIDTH, _Phdr), p_type),
        sizeof segment_type);
    if (lm->need_to_swap)
      segment_type = __builtin_bswap32(segment_type);

    CAT3(uint, WIDTH, _t) segment_virt_addr;
    memcpy(&segment_virt_addr, p + offsetof(CAT3(Elf, WIDTH, _Phdr), p_vaddr),
        sizeof segment_virt_addr);
    if (lm->need_to_swap)
      segment_virt_addr = CAT(__builtin_bswap, WIDTH) (segment_virt_addr);

    Range segment_range = { b + segment_off, b + segment_off + segment_size };
    int r = CAT(parse_segment_, WIDTH) (range, filename, lm,
        &segment_range, segment_type, segment_virt_addr);
    if (r)
      return r;
  }
  if (!lm->execfn_addr) {
    fprintf(stderr, "Couldn't find any executable filename in %s.\n",
        filename);
    return -1;
  }
  return 0;
}

static int CAT(fput_core_vector_, WIDTH) (const Landmarks *lm,
    const Range *vec,
    const char *prefix, const char *delim, FILE *o)
{
  const unsigned char *b = lm->vector_section.begin;
  const unsigned char *e = lm->vector_section.end;
  size_t i = 0;
  for (const unsigned char *p = vec->begin; p < vec->end; p += WIDTH/8, ++i) {
    CAT3(uint, WIDTH, _t) addr;
    memcpy(&addr, p, sizeof addr);
    if (lm->need_to_swap)
      addr = CAT(__builtin_bswap, WIDTH) (addr);
    if (addr < lm->vector_base_addr) {
      fprintf(stderr, "Pointer underflows section.\n");
      return -1;
    }
    CAT3(uint, WIDTH, _t) off = addr - lm->vector_base_addr;
    if (off > e - b) {
      fprintf(stderr, "Pointer points outside section.\n");
      return -1;
    }
    const char *s = (const char*) (b + off);
    const char *t = memchr(s, 0, e - (b + off));
    if (!t) {
      fprintf(stderr, "Pointer is not null-terminated.\n");
      return -1;
    }
    if (i)
      fputs(delim, o);
    if (prefix)
      fprintf(o, "%s[%zu]: ", prefix, i);
    fputs(s, o);
  }
  return 0;
}

static const char *CAT(get_core_str_, WIDTH) (CAT3(uint, WIDTH, _t) addr,
    const Landmarks *lm)
{
  if (addr < lm->vector_base_addr) {
    fprintf(stderr, "Start of string underflows.\n");
    return 0;
  }
  CAT3(uint, WIDTH, _t) off = addr - lm->vector_base_addr;
  const unsigned char *b = lm->vector_section.begin;
  const unsigned char *e = lm->vector_section.end;
  if (off > e - b) {
    fprintf(stderr, "Start of string overflows.\n");
    return 0;
  }
  const char *t = memchr(b + off, 0, e - (b + off));
  if (!t) {
    fprintf(stderr, "String isn't null-terminated.");
    return 0;
  }
  return (const char*) (b + off);
}

static const unsigned char *CAT(
    get_core_mem_, WIDTH) (CAT3(uint, WIDTH, _t) addr, size_t n,
    const Landmarks *lm)
{
  if (addr < lm->vector_base_addr) {
    fprintf(stderr, "Start of string underflows.\n");
    return 0;
  }
  CAT3(uint, WIDTH, _t) off = addr - lm->vector_base_addr;
  const unsigned char *b = lm->vector_section.begin;
  const unsigned char *e = lm->vector_section.end;
  if (off > e - b) {
    fprintf(stderr, "Start of string overflows.\n");
    return 0;
  }
  if (off + n > e - b) {
    fprintf(stderr, "End of string overflows.\n");
    return 0;
  }
  return b + off;
}


static int CAT(pp_core_aux_ref_, WIDTH) (CAT3(uint, WIDTH, _t) key,
    CAT3(uint, WIDTH, _t) val, const Landmarks *lm, FILE *o)
{
  switch (key) {
    case AT_PLATFORM:
    case AT_EXECFN: {
      const char *s = CAT(get_core_str_, WIDTH) (val, lm);
      fprintf(o, " %s", s);
      break;
    }
    case AT_RANDOM:
    {
      static const size_t n = 16;
      const unsigned char *s = CAT(get_core_mem_, WIDTH) (val, n, lm);
      for (size_t i = 0; i < n; ++i)
        fprintf(o, " %.2x", (unsigned) s[i]);
      break;
    }
    default: break;
  }
  return 0;
}

static int CAT(fput_core_auxv_, WIDTH) (const Landmarks *lm, FILE *o,
    const Args *args)
{
  if (!lm->auxv_note.begin) {
    fprintf(stderr, "auxv not found.\n");
    return -1;
  }
  size_t i = 0;
  for (const unsigned char *p = lm->auxv_note.begin; ;
      p += 2 * (WIDTH/8), ++i) {
    CAT3(uint, WIDTH, _t) v[2] = {0};
    memcpy(&v[0], p, sizeof v[0]);
    if (lm->need_to_swap)
      v[0] = CAT(__builtin_bswap, WIDTH) (v[0]);
    memcpy(&v[1], p + sizeof v[0], sizeof v[1]);
    if (lm->need_to_swap)
      v[1] = CAT(__builtin_bswap, WIDTH) (v[1]);
    if (!v[0])
      return 0;
    if (i)
      fputc('\n', o);
    if (v[0] < sizeof auxv_type_map / sizeof auxv_type_map[0])
      fprintf(o, "%-16s", auxv_type_map[v[0]].key);
    else
      fprintf(o, "unk_%" CAT(PRIu, WIDTH), v[0]);
    fprintf(o, " 0x%.16" CAT(PRIx, WIDTH), v[1]);

    int r = pp_aux(v[0], v[1], o, args);
    if (r)
      return r;
    r = CAT(pp_core_aux_ref_, WIDTH) (v[0], v[1], lm, o);
    if (r)
      return r;
    r = pp_aux_v(v[0], v[1], o, args);
    if (r)
      return r;
  }
  return 0;
}

#undef WIDTH
#undef CAT
#undef CAT3
#undef CAT_P
#undef CAT3_P
