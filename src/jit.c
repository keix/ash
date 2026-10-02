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

/* pop the data stack into rax */
static void
pop_rax (void)
{
  ld_rcx (OFF_DSP);
  e8 (0x48);
  e8 (0x8B);
  e8 (0x01); /* mov rax, [rcx] */
  e8 (0x48);
  e8 (0x8D);
  e8 (0x49);
  e8 (0x08); /* lea rcx, [rcx+8] */
  st_rcx (OFF_DSP);
}

/* push a constant onto the data stack */
static void
push_const (uint64_t n)
{
  e8 (0x48);
  e8 (0xB8);
  e64 (n); /* mov rax, n */
  ld_rcx (OFF_DSP);
  e8 (0x48);
  e8 (0x8D);
  e8 (0x49);
  e8 (0xF8); /* lea rcx, [rcx-8] */
  e8 (0x48);
  e8 (0x89);
  e8 (0x01); /* mov [rcx], rax */
  st_rcx (OFF_DSP);
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

/* ip = rpop; pop rbx; ret -- back to the one dispatch loop */
static void
emit_exit (void)
{
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

/* ---- inlined primitives: immutable C prims expanded to native ---- */

/* rcx = dsp; rax = top */
static void
top_rax (void)
{
  ld_rcx (OFF_DSP);
  e8 (0x48);
  e8 (0x8B);
  e8 (0x01);
}

static void
i_dup (void)
{
  top_rax ();
  e8 (0x48);
  e8 (0x8D);
  e8 (0x49);
  e8 (0xF8); /* lea rcx, [rcx-8] */
  e8 (0x48);
  e8 (0x89);
  e8 (0x01); /* mov [rcx], rax */
  st_rcx (OFF_DSP);
}

static void
i_drop (void)
{
  ld_rcx (OFF_DSP);
  e8 (0x48);
  e8 (0x8D);
  e8 (0x49);
  e8 (0x08);
  st_rcx (OFF_DSP);
}

static void
i_swap (void)
{
  top_rax ();
  e8 (0x48);
  e8 (0x8B);
  e8 (0x51);
  e8 (0x08); /* mov rdx, [rcx+8] */
  e8 (0x48);
  e8 (0x89);
  e8 (0x11); /* mov [rcx], rdx */
  e8 (0x48);
  e8 (0x89);
  e8 (0x41);
  e8 (0x08); /* mov [rcx+8], rax */
}

static void
i_over (void)
{
  ld_rcx (OFF_DSP);
  e8 (0x48);
  e8 (0x8B);
  e8 (0x41);
  e8 (0x08); /* mov rax, [rcx+8] */
  e8 (0x48);
  e8 (0x8D);
  e8 (0x49);
  e8 (0xF8);
  e8 (0x48);
  e8 (0x89);
  e8 (0x01);
  st_rcx (OFF_DSP);
}

static void
i_rot (void)
{
  top_rax (); /* rax = c */
  e8 (0x48);
  e8 (0x8B);
  e8 (0x51);
  e8 (0x08); /* rdx = b */
  e8 (0x48);
  e8 (0x8B);
  e8 (0x71);
  e8 (0x10); /* rsi = a */
  e8 (0x48);
  e8 (0x89);
  e8 (0x31); /* [rcx]   = a */
  e8 (0x48);
  e8 (0x89);
  e8 (0x41);
  e8 (0x08); /* [rcx+8] = c */
  e8 (0x48);
  e8 (0x89);
  e8 (0x51);
  e8 (0x10); /* [rcx+16]= b */
}

/* pop top into rax, leave rcx on the new top, dsp stored */
static void
pop_adjust (void)
{
  top_rax ();
  e8 (0x48);
  e8 (0x8D);
  e8 (0x49);
  e8 (0x08);
  st_rcx (OFF_DSP);
}

/* second OP= top, for add/sub/and/or/xor */
static void
binop (uint8_t op)
{
  pop_adjust ();
  e8 (0x48);
  e8 (op);
  e8 (0x01); /* op [rcx], rax */
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
  pop_adjust ();
  e8 (0x48);
  e8 (0x0F);
  e8 (0xAF);
  e8 (0x01); /* imul rax, [rcx] */
  e8 (0x48);
  e8 (0x89);
  e8 (0x01); /* mov [rcx], rax */
}

static void
i_invert (void)
{
  ld_rcx (OFF_DSP);
  e8 (0x48);
  e8 (0xF7);
  e8 (0x11); /* not qword [rcx] */
}

/* compare second with top, leave a full flag: setcc al; -al */
static void
cmpop (uint8_t setcc)
{
  pop_adjust ();
  e8 (0x48);
  e8 (0x39);
  e8 (0x01); /* cmp [rcx], rax */
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
  e8 (0x48);
  e8 (0x89);
  e8 (0x01); /* mov [rcx], rax */
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
  ld_rcx (OFF_DSP);
  e8 (0x48);
  e8 (0x83);
  e8 (0x39);
  e8 (0x00); /* cmp qword [rcx], 0 */
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
  e8 (0x48);
  e8 (0x89);
  e8 (0x01);
}

static void
i_fetch (void)
{
  top_rax ();
  e8 (0x48);
  e8 (0x8B);
  e8 (0x00); /* mov rax, [rax] */
  e8 (0x48);
  e8 (0x89);
  e8 (0x01); /* mov [rcx], rax */
}

static void
i_store (void)
{
  top_rax (); /* rax = addr */
  e8 (0x48);
  e8 (0x8B);
  e8 (0x51);
  e8 (0x08); /* rdx = value */
  e8 (0x48);
  e8 (0x89);
  e8 (0x10); /* mov [rax], rdx */
  e8 (0x48);
  e8 (0x8D);
  e8 (0x49);
  e8 (0x10); /* rcx += 16 */
  st_rcx (OFF_DSP);
}

static void
i_tor (void)
{
  pop_adjust ();
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

static void
i_fromr (void)
{
  ld_rcx (OFF_RSP);
  e8 (0x48);
  e8 (0x8B);
  e8 (0x01);
  e8 (0x48);
  e8 (0x8D);
  e8 (0x49);
  e8 (0x08);
  st_rcx (OFF_RSP);
  ld_rcx (OFF_DSP);
  e8 (0x48);
  e8 (0x8D);
  e8 (0x49);
  e8 (0xF8);
  e8 (0x48);
  e8 (0x89);
  e8 (0x01);
  st_rcx (OFF_DSP);
}

static void
i_rfetch (void)
{
  ld_rcx (OFF_RSP);
  e8 (0x48);
  e8 (0x8B);
  e8 (0x01);
  ld_rcx (OFF_DSP);
  e8 (0x48);
  e8 (0x8D);
  e8 (0x49);
  e8 (0xF8);
  e8 (0x48);
  e8 (0x89);
  e8 (0x01);
  st_rcx (OFF_DSP);
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

  if (apos + 64 * end + 256 > ARENA_BYTES)
    return; /* arena full: stay threaded */

  off = calloc (end + 1, sizeof (size_t));
  patches = calloc (end + 1, sizeof (patch_t));
  if (!off || !patches)
    {
      free (off);
      free (patches);
      return;
    }

  mprotect (arena, ARENA_BYTES, PROT_READ | PROT_WRITE);

  while (apos & 15)
    e8 (0x90);
  fn = apos;
  prologue ();
  rpush_ip ();

  for (i = 0; i < end;)
    {
      cell_t w = body[i];

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

          if (inl)
            inl ();
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
}

/* ---- words ---- */

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
  vm->jit_on = -1;
}

static void
prim_jit_off (vm_t *vm, xt_t xt)
{
  (void)xt;
  vm->jit_on = 0;
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
