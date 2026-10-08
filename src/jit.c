/* The template JIT: below the optimization boundary, per the contract
   in docs/DESIGN.md. A compiled word is a big primitive: it is
   entered through its code field like everything else, keeps every
   continuation on the Forth return stack, and transfers control to
   other words by replacing its own C frame with a tail jump -- so C
   frames never nest, and CATCH/THROW need not know the JIT exists. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "ash.h"

enum
{
  ARENA_BYTES = 1 << 20
};

static uint8_t *arena;
static size_t apos;

static xt_t x_lit, x_branch, x_0branch, x_exit, x_squote;
static xt_t x_execute, x_quit, x_abort, x_does;

#define OFF_DSP ((uint32_t)offsetof (vm_t, dsp))
#define OFF_RSP ((uint32_t)offsetof (vm_t, rsp))
#define OFF_IP ((uint32_t)offsetof (vm_t, ip))

/* ---- emitter ---- */

static void
e8 (uint8_t b)
{
  arena[apos++] = b;
}

static void
e32 (uint32_t v)
{
  memcpy (arena + apos, &v, 4);
  apos += 4;
}

static void
e64 (uint64_t v)
{
  memcpy (arena + apos, &v, 8);
  apos += 8;
}

/* mov rcx, [rbx+off] */
static void
ld_rcx (uint32_t off)
{
  e8 (0x48);
  e8 (0x8B);
  e8 (0x8B);
  e32 (off);
}

/* mov [rbx+off], rcx */
static void
st_rcx (uint32_t off)
{
  e8 (0x48);
  e8 (0x89);
  e8 (0x8B);
  e32 (off);
}

/* ---- segment stack cache ----
   Within a straight-line segment, rcx holds vm->dsp and a
   compile-time delta tracks pushes and pops; values always live in
   memory, only the pointer round-trips are elided. sync_dsp()
   materializes the pointer and kills the cache at every control-flow
   boundary, so the contract's "stacks fully materialized where
   control may leave" holds by construction. */

static int dsp_live;
static long dsp_delta;

/* mov reg, [rcx+cells*8] / mov [rcx+cells*8], reg
   reg modrm: rax 0x81, rdx 0x91, rsi 0xB1 */
static void
mem_ld (uint8_t reg, long cells)
{
  e8 (0x48);
  e8 (0x8B);
  e8 (reg);
  e32 ((uint32_t)(int32_t)(cells * 8));
}

static void
mem_st (uint8_t reg, long cells)
{
  e8 (0x48);
  e8 (0x89);
  e8 (reg);
  e32 ((uint32_t)(int32_t)(cells * 8));
}

static void
ensure_dsp (void)
{
  if (!dsp_live)
    {
      ld_rcx (OFF_DSP);
      dsp_live = 1;
      dsp_delta = 0;
    }
}

static void
sync_dsp (void)
{
  if (dsp_live && dsp_delta != 0)
    {
      e8 (0x48);
      e8 (0x8D);
      e8 (0x89);
      e32 ((uint32_t)(int32_t)(dsp_delta * 8)); /* lea rcx,[rcx+d] */
      st_rcx (OFF_DSP);
    }
  dsp_live = 0;
  dsp_delta = 0;
}

/* rax = top, consumed from the cache's view */
static void
pop_rax (void)
{
  ensure_dsp ();
  mem_ld (0x81, dsp_delta);
  dsp_delta += 1;
}

/* push a constant onto the data stack */
static void
push_const (uint64_t n)
{
  ensure_dsp ();
  e8 (0x48);
  e8 (0xB8);
  e64 (n); /* mov rax, n */
  dsp_delta -= 1;
  mem_st (0x81, dsp_delta);
}

/* push rbx; mov rbx, rdi -- entry and every resume point */
static void
prologue (void)
{
  e8 (0x53);
  e8 (0x48);
  e8 (0x89);
  e8 (0xFB);
}

/* rpush (vm->ip): the caller's continuation, exactly like docol */
static void
rpush_ip (void)
{
  e8 (0x48);
  e8 (0x8B);
  e8 (0x83);
  e32 (OFF_IP); /* mov rax, [rbx+ip] */
  ld_rcx (OFF_RSP);
  e8 (0x48);
  e8 (0x8D);
  e8 (0x49);
  e8 (0xF8);
  e8 (0x48);
  e8 (0x89);
  e8 (0x01);
  st_rcx (OFF_RSP);
}

/* ip = rpop; pop rbx; ret -- back to the one dispatch loop. A direct
   jump into a native continuation was tried and measured slower: the
   ret/call pair is predicted by the return stack buffer, an indirect
   jmp is not. */
static void
emit_exit (void)
{
  sync_dsp ();
  ld_rcx (OFF_RSP);
  e8 (0x48);
  e8 (0x8B);
  e8 (0x01);
  e8 (0x48);
  e8 (0x8D);
  e8 (0x49);
  e8 (0x08);
  st_rcx (OFF_RSP);
  e8 (0x48);
  e8 (0x89);
  e8 (0x83);
  e32 (OFF_IP);
  e8 (0x5B);
  e8 (0xC3);
}

/* a safe primitive: call code(vm, w) and continue. The code field is
   loaded at run time so the decision stays honest if it changes. */
static void
emit_call (xt_t w)
{
  sync_dsp ();
  e8 (0x48);
  e8 (0x89);
  e8 (0xDF); /* mov rdi, rbx */
  e8 (0x48);
  e8 (0xBE);
  e64 ((uint64_t)w); /* mov rsi, w */
  e8 (0x48);
  e8 (0x8B);
  e8 (0x06); /* mov rax, [rsi] */
  e8 (0xFF);
  e8 (0xD0); /* call rax */
}

static int
in_arena (code_t code)
{
  uintptr_t c = (uintptr_t)code;
  uintptr_t a = (uintptr_t)arena;

  return c >= a && c < a + ARENA_BYTES;
}

/* ---- inlined primitives: immutable C prims expanded to native,
   all through the segment stack cache ---- */

static void
i_dup (void)
{
  ensure_dsp ();
  mem_ld (0x81, dsp_delta);
  dsp_delta -= 1;
  mem_st (0x81, dsp_delta);
}

static void
i_drop (void)
{
  ensure_dsp ();
  dsp_delta += 1; /* bookkeeping only: no instruction */
}

static void
i_swap (void)
{
  ensure_dsp ();
  mem_ld (0x81, dsp_delta);
  mem_ld (0x91, dsp_delta + 1);
  mem_st (0x91, dsp_delta);
  mem_st (0x81, dsp_delta + 1);
}

static void
i_over (void)
{
  ensure_dsp ();
  mem_ld (0x81, dsp_delta + 1);
  dsp_delta -= 1;
  mem_st (0x81, dsp_delta);
}

static void
i_rot (void)
{
  ensure_dsp ();
  mem_ld (0x81, dsp_delta);     /* c */
  mem_ld (0x91, dsp_delta + 1); /* b */
  mem_ld (0xB1, dsp_delta + 2); /* a */
  mem_st (0xB1, dsp_delta);
  mem_st (0x81, dsp_delta + 1);
  mem_st (0x91, dsp_delta + 2);
}

/* second OP= top, for add/sub/and/or/xor */
static void
binop (uint8_t op)
{
  pop_rax ();
  e8 (0x48);
  e8 (op);
  e8 (0x81); /* op [rcx+delta*8], rax */
  e32 ((uint32_t)(int32_t)(dsp_delta * 8));
}

static void
i_add (void)
{
  binop (0x01);
}

static void
i_sub (void)
{
  binop (0x29);
}

static void
i_and (void)
{
  binop (0x21);
}

static void
i_or (void)
{
  binop (0x09);
}

static void
i_xor (void)
{
  binop (0x31);
}

static void
i_mul (void)
{
  pop_rax ();
  e8 (0x48);
  e8 (0x0F);
  e8 (0xAF);
  e8 (0x81); /* imul rax, [rcx+delta*8] */
  e32 ((uint32_t)(int32_t)(dsp_delta * 8));
  mem_st (0x81, dsp_delta);
}

static void
i_invert (void)
{
  ensure_dsp ();
  e8 (0x48);
  e8 (0xF7);
  e8 (0x91); /* not qword [rcx+delta*8] */
  e32 ((uint32_t)(int32_t)(dsp_delta * 8));
}

/* compare second with top, leave a full flag: setcc al; -al */
static void
cmpop (uint8_t setcc)
{
  pop_rax ();
  e8 (0x48);
  e8 (0x39);
  e8 (0x81); /* cmp [rcx+delta*8], rax */
  e32 ((uint32_t)(int32_t)(dsp_delta * 8));
  e8 (0x0F);
  e8 (setcc);
  e8 (0xC0); /* setcc al */
  e8 (0x48);
  e8 (0x0F);
  e8 (0xB6);
  e8 (0xC0); /* movzx rax, al */
  e8 (0x48);
  e8 (0xF7);
  e8 (0xD8); /* neg rax */
  mem_st (0x81, dsp_delta);
}

static void
i_lt (void)
{
  cmpop (0x9C);
}

static void
i_gt (void)
{
  cmpop (0x9F);
}

static void
i_eq (void)
{
  cmpop (0x94);
}

static void
i_zeq (void)
{
  ensure_dsp ();
  e8 (0x48);
  e8 (0x83);
  e8 (0xB9); /* cmp qword [rcx+delta*8], 0 */
  e32 ((uint32_t)(int32_t)(dsp_delta * 8));
  e8 (0x00);
  e8 (0x0F);
  e8 (0x94);
  e8 (0xC0); /* sete al */
  e8 (0x48);
  e8 (0x0F);
  e8 (0xB6);
  e8 (0xC0);
  e8 (0x48);
  e8 (0xF7);
  e8 (0xD8);
  mem_st (0x81, dsp_delta);
}

static void
i_fetch (void)
{
  ensure_dsp ();
  mem_ld (0x81, dsp_delta);
  e8 (0x48);
  e8 (0x8B);
  e8 (0x00); /* mov rax, [rax] */
  mem_st (0x81, dsp_delta);
}

static void
i_store (void)
{
  ensure_dsp ();
  mem_ld (0x81, dsp_delta);     /* rax = addr */
  mem_ld (0x91, dsp_delta + 1); /* rdx = value */
  e8 (0x48);
  e8 (0x89);
  e8 (0x10); /* mov [rax], rdx */
  dsp_delta += 2;
}

static void
i_tor (void)
{
  pop_rax ();
  e8 (0x48);
  e8 (0x8B);
  e8 (0x93);
  e32 (OFF_RSP); /* mov rdx, [rbx+rsp] */
  e8 (0x48);
  e8 (0x8D);
  e8 (0x52);
  e8 (0xF8); /* lea rdx, [rdx-8] */
  e8 (0x48);
  e8 (0x89);
  e8 (0x02); /* mov [rdx], rax */
  e8 (0x48);
  e8 (0x89);
  e8 (0x93);
  e32 (OFF_RSP);
}

static void
i_fromr (void)
{
  e8 (0x48);
  e8 (0x8B);
  e8 (0x93);
  e32 (OFF_RSP); /* mov rdx, [rbx+rsp] */
  e8 (0x48);
  e8 (0x8B);
  e8 (0x02); /* mov rax, [rdx] */
  e8 (0x48);
  e8 (0x8D);
  e8 (0x52);
  e8 (0x08); /* lea rdx, [rdx+8] */
  e8 (0x48);
  e8 (0x89);
  e8 (0x93);
  e32 (OFF_RSP);
  ensure_dsp ();
  dsp_delta -= 1;
  mem_st (0x81, dsp_delta);
}

static void
i_rfetch (void)
{
  e8 (0x48);
  e8 (0x8B);
  e8 (0x93);
  e32 (OFF_RSP); /* mov rdx, [rbx+rsp] */
  e8 (0x48);
  e8 (0x8B);
  e8 (0x02); /* mov rax, [rdx] */
  ensure_dsp ();
  dsp_delta -= 1;
  mem_st (0x81, dsp_delta);
}

typedef void (*inline_emit_t) (void);

typedef struct
{
  xt_t xt;
  inline_emit_t emit;
} inline_entry_t;

static inline_entry_t inliners[32];
static size_t ninline;

static inline_emit_t
find_inliner (cell_t w)
{
  size_t k;

  for (k = 0; k < ninline; k++)
    if ((cell_t)inliners[k].xt == w)
      return inliners[k].emit;
  return NULL;
}

/* a colon word short and pure enough to inline whole: literals and
   inlinable primitives only, exit at the end, and no return-stack
   ops -- inlined, those would see the caller's frame. */
static int
inlinable_colon (cell_t *b, size_t *n)
{
  size_t i = 0;

  while (i < 12)
    {
      cell_t w = b[i];
      inline_emit_t f;

      if ((xt_t)w == x_exit)
        {
          *n = i;
          return 1;
        }
      if ((xt_t)w == x_lit)
        {
          i += 2;
          continue;
        }
      f = find_inliner (w);
      if (!f || f == i_tor || f == i_fromr || f == i_rfetch)
        return 0;
      i += 1;
    }
  return 0;
}

static void
emit_inline_colon (cell_t *b, size_t n)
{
  size_t i = 0;

  while (i < n)
    {
      if ((xt_t)b[i] == x_lit)
        {
          push_const ((uint64_t)b[i + 1]);
          i += 2;
        }
      else
        {
          find_inliner (b[i]) ();
          i += 1;
        }
    }
}

/* transfer to any word: plant a one-cell trampoline thread as the
   continuation, restore the C stack, and tail-jump through the
   callee's code field (loaded at run time, so a callee JITted later
   is entered natively). The callee returns to the loop; the loop
   dispatches the trampoline; the trampoline's code field points at
   the resume code emitted right after it. */
static void
emit_transfer (xt_t w)
{
  size_t fix_c1;

  sync_dsp ();
  e8 (0x48);
  e8 (0xB8);
  fix_c1 = apos;
  e64 (0); /* mov rax, &C1 (patched below) */
  e8 (0x48);
  e8 (0x89);
  e8 (0x83);
  e32 (OFF_IP); /* mov [rbx+ip], rax */
  e8 (0x48);
  e8 (0x89);
  e8 (0xDF); /* mov rdi, rbx */
  e8 (0x48);
  e8 (0xBE);
  e64 ((uint64_t)w); /* mov rsi, w */
  e8 (0x48);
  e8 (0x8B);
  e8 (0x06); /* mov rax, [rsi] */
  e8 (0x5B); /* pop rbx */
  e8 (0xFF);
  e8 (0xE0); /* jmp rax */

  while (apos & 7)
    e8 (0x90);
  {
    size_t c1 = apos;
    e64 (0);
    {
      size_t c2 = apos;
      e64 (0);
      {
        uint64_t v;
        v = (uint64_t)(arena + c2);
        memcpy (arena + c1, &v, 8); /* C1: the trampoline xt */
        v = (uint64_t)(arena + apos);
        memcpy (arena + c2, &v, 8); /* C2: code field -> resume */
        v = (uint64_t)(arena + c1);
        memcpy (arena + fix_c1, &v, 8);
      }
    }
  }
  prologue (); /* resume re-establishes rbx */
}

/* ---- the compiler ---- */

typedef struct
{
  size_t at;    /* arena offset of a rel32 to patch */
  size_t tcell; /* target cell index in the body */
} patch_t;

void
jit_xt (vm_t *vm, xt_t xt)
{
  cell_t *body;
  size_t end = 0, maxt = 0, i;
  size_t *off;
  patch_t *patches;
  uint8_t *is_target;
  size_t npatch = 0;
  size_t fn;

  (void)vm;
  if (!arena || (code_t)*xt != docol)
    return;
  body = xt + 1;

  /* pass 1: find the extent, bail on what must stay threaded */
  for (i = 0;;)
    {
      cell_t w = body[i];
      size_t sz = 1;

      if ((xt_t)w == x_lit)
        sz = 2;
      else if ((xt_t)w == x_squote)
        sz = 2 + ((size_t)body[i + 1] + 7) / 8;
      else if ((xt_t)w == x_branch || (xt_t)w == x_0branch)
        {
          size_t t = (size_t)((cell_t *)body[i + 1] - body);

          if (t > 65536)
            return;
          if (t > maxt)
            maxt = t;
          sz = 2;
        }
      else if ((xt_t)w == x_does)
        return; /* DOES> patches ip from the thread: keep threaded */
      if (((xt_t)w == x_exit || (xt_t)w == x_branch) && i + sz > maxt)
        {
          end = i + sz;
          break;
        }
      i += sz;
      if (i > 65536)
        return;
    }

  if (apos + 512 * end + 512 > ARENA_BYTES)
    return; /* arena full: stay threaded */

  off = calloc (end + 1, sizeof (size_t));
  patches = calloc (end + 1, sizeof (patch_t));
  is_target = calloc (end + 1, 1);
  if (!off || !patches || !is_target)
    {
      free (off);
      free (patches);
      free (is_target);
      return;
    }

  /* every branch target begins a fresh segment */
  for (i = 0; i < end;)
    {
      cell_t w = body[i];

      if ((xt_t)w == x_lit)
        i += 2;
      else if ((xt_t)w == x_squote)
        i += 2 + ((size_t)body[i + 1] + 7) / 8;
      else if ((xt_t)w == x_branch || (xt_t)w == x_0branch)
        {
          size_t t = (size_t)((cell_t *)body[i + 1] - body);

          if (t <= end)
            is_target[t] = 1;
          i += 2;
        }
      else
        i += 1;
    }

  mprotect (arena, ARENA_BYTES, PROT_READ | PROT_WRITE);

  while (apos & 15)
    e8 (0x90);
  fn = apos;
  prologue ();
  rpush_ip ();
  dsp_live = 0;
  dsp_delta = 0;

  for (i = 0; i < end;)
    {
      cell_t w = body[i];

      if (is_target[i])
        sync_dsp ();
      off[i] = apos;
      if ((xt_t)w == x_lit)
        {
          push_const ((uint64_t)body[i + 1]);
          i += 2;
        }
      else if ((xt_t)w == x_squote)
        {
          cell_t len = body[i + 1];

          push_const ((uint64_t)&body[i + 2]);
          push_const ((uint64_t)len);
          i += 2 + ((size_t)len + 7) / 8;
        }
      else if ((xt_t)w == x_0branch)
        {
          pop_rax ();
          sync_dsp ();
          e8 (0x48);
          e8 (0x85);
          e8 (0xC0); /* test rax, rax */
          e8 (0x0F);
          e8 (0x84); /* jz rel32 */
          patches[npatch].at = apos;
          patches[npatch].tcell = (size_t)((cell_t *)body[i + 1] - body);
          npatch++;
          e32 (0);
          i += 2;
        }
      else if ((xt_t)w == x_branch)
        {
          sync_dsp ();
          e8 (0xE9); /* jmp rel32 */
          patches[npatch].at = apos;
          patches[npatch].tcell = (size_t)((cell_t *)body[i + 1] - body);
          npatch++;
          e32 (0);
          i += 2;
        }
      else if ((xt_t)w == x_exit)
        {
          emit_exit ();
          i += 1;
        }
      else
        {
          code_t code = *(code_t *)w;

          /* transfer to anything that owns control flow: colon and
             native words, create words (does> may repatch them), and
             the ip-manipulating primitives. Direct calls are only for
             immutable C primitives; the hottest of those inline. */
          inline_emit_t inl = find_inliner (w);
          size_t ncells;

          if (inl)
            inl ();
          else if (code == docol && inlinable_colon ((cell_t *)w + 1, &ncells))
            emit_inline_colon ((cell_t *)w + 1, ncells);
          else if (code == docol || code == dodoes || code == docreate
                   || in_arena (code) || (xt_t)w == x_execute
                   || (xt_t)w == x_quit || (xt_t)w == x_abort)
            emit_transfer ((xt_t)w);
          else
            emit_call ((xt_t)w);
          i += 1;
        }
    }
  off[end] = apos;

  for (i = 0; i < npatch; i++)
    {
      int32_t rel = (int32_t)((int64_t)off[patches[i].tcell]
                              - (int64_t)(patches[i].at + 4));
      memcpy (arena + patches[i].at, &rel, 4);
    }

  mprotect (arena, ARENA_BYTES, PROT_READ | PROT_EXEC);

  *xt = (cell_t)(arena + fn); /* the one-cell strategy swap */

  free (off);
  free (patches);
  free (is_target);
}

/* ---- words ---- */

/* hotness: optimization policy, outside the dictionary entry. A
   direct-mapped table counts docol entries per xt; at the threshold
   the word burns. A compiled word never reaches docol again, so its
   counter stops by itself. */

enum
{
  HOT_SLOTS = 1024,
  HOT_THRESHOLD = 512
};

static struct
{
  xt_t xt;
  uint32_t n;
} hot[HOT_SLOTS];

void
jit_count (vm_t *vm, xt_t xt)
{
  size_t i = ((uintptr_t)xt >> 3) & (HOT_SLOTS - 1);

  if (vm->jit_on < 0 || !arena)
    return;
  if (hot[i].xt != xt)
    {
      hot[i].xt = xt;
      hot[i].n = 1;
      return;
    }
  if (++hot[i].n == HOT_THRESHOLD)
    jit_xt (vm, xt);
}

static void
prim_jit (vm_t *vm, xt_t xt)
{
  (void)xt;
  jit_xt (vm, (xt_t)pop (vm));
}

static void
prim_jit_on (vm_t *vm, xt_t xt)
{
  (void)xt;
  vm->jit_on = 1;
}

static void
prim_jit_off (vm_t *vm, xt_t xt)
{
  (void)xt;
  vm->jit_on = -1;
}

static xt_t
defprim (vm_t *vm, const char *name, code_t code)
{
  xt_t xt = entry_xt (dict_header (vm, name, strlen (name)));

  *xt = (cell_t)code;
  return xt;
}

static xt_t
xt_of (vm_t *vm, const char *name)
{
  dict_entry_t *w = find_word (vm, name, strlen (name));

  return w ? entry_xt (w) : NULL;
}

static void
reg_inline (vm_t *vm, const char *name, inline_emit_t emit)
{
  xt_t x = xt_of (vm, name);

  if (x && ninline < sizeof (inliners) / sizeof (inliners[0]))
    {
      inliners[ninline].xt = x;
      inliners[ninline].emit = emit;
      ninline++;
    }
}

void
jit_register (vm_t *vm)
{
  arena = mmap (NULL, ARENA_BYTES, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (arena == MAP_FAILED)
    {
      arena = NULL;
      return;
    }

  x_lit = xt_of (vm, "lit");
  x_branch = xt_of (vm, "branch");
  x_0branch = xt_of (vm, "0branch");
  x_exit = xt_of (vm, "exit");
  x_squote = xt_of (vm, "(s\")");
  x_execute = xt_of (vm, "execute");
  x_quit = xt_of (vm, "quit");
  x_abort = xt_of (vm, "(abort)");
  x_does = xt_of (vm, "(does>)");

  reg_inline (vm, "dup", i_dup);
  reg_inline (vm, "drop", i_drop);
  reg_inline (vm, "swap", i_swap);
  reg_inline (vm, "over", i_over);
  reg_inline (vm, "rot", i_rot);
  reg_inline (vm, "+", i_add);
  reg_inline (vm, "-", i_sub);
  reg_inline (vm, "*", i_mul);
  reg_inline (vm, "and", i_and);
  reg_inline (vm, "or", i_or);
  reg_inline (vm, "xor", i_xor);
  reg_inline (vm, "invert", i_invert);
  reg_inline (vm, "=", i_eq);
  reg_inline (vm, "<", i_lt);
  reg_inline (vm, ">", i_gt);
  reg_inline (vm, "0=", i_zeq);
  reg_inline (vm, "@", i_fetch);
  reg_inline (vm, "!", i_store);
  reg_inline (vm, ">r", i_tor);
  reg_inline (vm, "r>", i_fromr);
  reg_inline (vm, "r@", i_rfetch);

  defprim (vm, "jit", prim_jit);
  defprim (vm, "jit-on", prim_jit_on);
  defprim (vm, "jit-off", prim_jit_off);
}
