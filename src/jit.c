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

static xt_t x_lit, x_branch, x_0branch, x_exit, x_squote, x_fetch;
static xt_t x_execute, x_quit, x_abort, x_does, x_lshift, x_rshift;

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

static int
fits32 (int64_t v)
{
  return v == (int64_t)(int32_t)v;
}

/* register numbers as x86 encodes them; r8+ set a REX bit */
enum
{
  RAX = 0,
  RCX = 1,
  RDX = 2,
  RBX = 3,
  RSI = 6,
  R8 = 8,
  R9 = 9,
  R10 = 10,
  R11 = 11
};

/* REX.W plus the extension bits for a reg field and an rm/base field */
static void
rex (int reg, int rm)
{
  e8 ((uint8_t)(0x48 | ((reg >> 3) << 2) | (rm >> 3)));
}

/* op reg, rm  (or op rm, reg: the opcode decides the direction) */
static void
op_rr (uint8_t op, int reg, int rm)
{
  rex (reg, rm);
  e8 (op);
  e8 ((uint8_t)(0xC0 | ((reg & 7) << 3) | (rm & 7)));
}

/* op reg, [base+disp32] -- or op [base+disp32], reg */
static void
op_rm (uint8_t op, int reg, int base, int32_t disp)
{
  rex (reg, base);
  e8 (op);
  e8 ((uint8_t)(0x80 | ((reg & 7) << 3) | (base & 7)));
  e32 ((uint32_t)disp);
}

static void
ld (int reg, int base, long cells)
{
  op_rm (0x8B, reg, base, (int32_t)(cells * 8));
}

static void
st (int reg, int base, long cells)
{
  op_rm (0x89, reg, base, (int32_t)(cells * 8));
}

static void
mov_rr (int dst, int src)
{
  op_rr (0x89, src, dst);
}

static void
mov_imm (int reg, int64_t v)
{
  if (fits32 (v))
    {
      rex (0, reg);
      e8 (0xC7);
      e8 ((uint8_t)(0xC0 | (reg & 7)));
      e32 ((uint32_t)(int32_t)v);
    }
  else
    {
      rex (0, reg);
      e8 ((uint8_t)(0xB8 | (reg & 7)));
      e64 ((uint64_t)v);
    }
}

/* 81 /digit: add 0, or 1, and 4, sub 5, xor 6, cmp 7 */
static void
alu_ri (int digit, int reg, int32_t imm)
{
  rex (0, reg);
  e8 (0x81);
  e8 ((uint8_t)(0xC0 | (digit << 3) | (reg & 7)));
  e32 ((uint32_t)imm);
}

static void
alu_mi (int digit, int base, int32_t disp, int32_t imm)
{
  rex (0, base);
  e8 (0x81);
  e8 ((uint8_t)(0x80 | (digit << 3) | (base & 7)));
  e32 ((uint32_t)disp);
  e32 ((uint32_t)imm);
}

/* mov rcx, [rbx+off] / mov [rbx+off], rcx */
static void
ld_rcx (uint32_t off)
{
  op_rm (0x8B, RCX, RBX, (int32_t)off);
}

static void
st_rcx (uint32_t off)
{
  op_rm (0x89, RCX, RBX, (int32_t)off);
}

/* ---- segment caches ----
   Within a straight-line segment, rcx holds vm->dsp and rsi holds
   vm->rsp, each with a compile-time delta that tracks pushes and
   pops, and the top few cells of the data stack live in a small
   register file -- or are compile-time constants, or, for the top
   cell only, the condition codes of the last compare. Stack
   shuffles are then renames, literals become immediates, and a
   compare feeds its branch directly. sync_caches() stores everything
   and kills every cache at every control-flow boundary, so the
   contract's "stacks fully materialized where control may leave"
   holds by construction. */

static int dsp_live, rsp_live;
static long dsp_delta,
    rsp_delta; /* logical top slots, cached cells included */

enum
{
  NC = 4, /* cached cells: cs[i] is the cell at [rcx + (dsp_delta+i)*8] */
  NREG = 6
};

enum
{
  S_REG,
  S_CONST,
  S_FLAGS /* only ever cs[0] */
};

typedef struct
{
  int kind;
  int reg;
  int64_t val;
} slot_t;

static slot_t cs[NC];
static int ncs; /* cs[0] is the top; cells beyond ncs are in memory */
static const int pool[NREG] = { RAX, RDX, R8, R9, R10, R11 };
static int held[16];
static uint8_t cc; /* x86 condition code of the S_FLAGS cell */

enum
{
  CC_E = 0x4,
  CC_NE = 0x5,
  CC_L = 0xC,
  CC_GE = 0xD,
  CC_LE = 0xE,
  CC_G = 0xF
};

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
ensure_rsp (void)
{
  if (!rsp_live)
    {
      op_rm (0x8B, RSI, RBX, (int32_t)OFF_RSP);
      rsp_live = 1;
      rsp_delta = 0;
    }
}

/* At most NC cells hold registers and NREG > NC, so a register is
   always free; the pool never needs to spill to allocate. */
static int
alloc_reg (void)
{
  int k;

  for (k = 0; k < NREG; k++)
    if (!held[pool[k]])
      {
        held[pool[k]] = 1;
        return pool[k];
      }
  abort ();
}

static void
free_reg (int r)
{
  held[r] = 0;
}

/* reg = -1 if cc else 0: setcc; movzx; neg */
static void
flags_to_reg (int r)
{
  if (r >= 8)
    e8 (0x41);
  e8 (0x0F);
  e8 (0x90 | cc);
  e8 ((uint8_t)(0xC0 | (r & 7)));
  rex (r, r);
  e8 (0x0F);
  e8 (0xB6);
  e8 ((uint8_t)(0xC0 | ((r & 7) << 3) | (r & 7)));
  rex (0, r);
  e8 (0xF7);
  e8 ((uint8_t)(0xD8 | (r & 7)));
}

/* the top is anything but condition codes: before an instruction
   that writes flags, and before pushing over it */
static void
unflag (void)
{
  if (ncs && cs[0].kind == S_FLAGS)
    {
      int r = alloc_reg ();

      flags_to_reg (r);
      cs[0].kind = S_REG;
      cs[0].reg = r;
    }
}

/* mov qword [rcx+cells*8], imm -- through a free register if needed */
static void
st_imm (long cells, int64_t v)
{
  if (fits32 (v))
    {
      e8 (0x48);
      e8 (0xC7);
      e8 (0x81);
      e32 ((uint32_t)(int32_t)(cells * 8));
      e32 ((uint32_t)(int32_t)v);
    }
  else
    {
      int r = alloc_reg ();

      mov_imm (r, v);
      st (r, RCX, cells);
      free_reg (r);
    }
}

static void
spill_deepest (void)
{
  slot_t *s = &cs[ncs - 1];
  long cell = dsp_delta + ncs - 1;

  if (s->kind == S_REG)
    {
      st (s->reg, RCX, cell);
      free_reg (s->reg);
    }
  else if (s->kind == S_CONST)
    st_imm (cell, s->val);
  else
    {
      int r = alloc_reg ();

      flags_to_reg (r);
      st (r, RCX, cell);
      free_reg (r);
    }
  ncs--;
}

/* every cached cell into its slot; the pointer caches stay. The flag
   cell, if any, is the top and goes last: nothing before it writes
   flags. */
static void
materialize (void)
{
  while (ncs)
    spill_deepest ();
}

static void
push_slot (slot_t s)
{
  unflag ();
  ensure_dsp ();
  if (ncs == NC)
    spill_deepest ();
  memmove (cs + 1, cs, (size_t)ncs * sizeof (slot_t));
  cs[0] = s;
  ncs++;
  dsp_delta -= 1;
}

static void
push_reg (int r)
{
  slot_t s = { S_REG, r, 0 };

  push_slot (s);
}

static void
push_const (int64_t v)
{
  slot_t s = { S_CONST, 0, v };

  push_slot (s);
}

static void
push_flags (uint8_t c)
{
  slot_t s = { S_FLAGS, 0, 0 };

  push_slot (s);
  cc = c;
}

static void
drop_top (void)
{
  if (ncs)
    {
      if (cs[0].kind == S_REG)
        free_reg (cs[0].reg);
      memmove (cs, cs + 1, (size_t)(ncs - 1) * sizeof (slot_t));
      ncs--;
    }
  ensure_dsp ();
  dsp_delta += 1;
}

/* cells 0..i cached, loading from memory as needed (i < NC) */
static void
pull (int i)
{
  ensure_dsp ();
  while (ncs <= i)
    {
      int r = alloc_reg ();

      ld (r, RCX, dsp_delta + ncs);
      cs[ncs].kind = S_REG;
      cs[ncs].reg = r;
      ncs++;
    }
}

/* cell i in a register, whatever it was */
static int
slot_reg (int i)
{
  pull (i);
  if (cs[i].kind == S_CONST)
    {
      int r = alloc_reg ();

      mov_imm (r, cs[i].val);
      cs[i].kind = S_REG;
      cs[i].reg = r;
    }
  else if (cs[i].kind == S_FLAGS)
    unflag ();
  return cs[i].reg;
}

static void
sync_caches (void)
{
  materialize ();
  if (dsp_live && dsp_delta != 0)
    {
      op_rm (0x8D, RCX, RCX, (int32_t)(dsp_delta * 8)); /* lea rcx,[rcx+d] */
      st_rcx (OFF_DSP);
    }
  if (rsp_live && rsp_delta != 0)
    {
      op_rm (0x8D, RSI, RSI, (int32_t)(rsp_delta * 8)); /* lea rsi,[rsi+d] */
      op_rm (0x89, RSI, RBX, (int32_t)OFF_RSP);
    }
  dsp_live = rsp_live = 0;
  dsp_delta = rsp_delta = 0;
}

static void
reset_caches (void)
{
  dsp_live = rsp_live = 0;
  dsp_delta = rsp_delta = 0;
  ncs = 0;
  memset (held, 0, sizeof held);
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
  op_rm (0x8B, RAX, RBX, (int32_t)OFF_IP); /* mov rax, [rbx+ip] */
  ld_rcx (OFF_RSP);
  e8 (0x48);
  e8 (0x8D);
  e8 (0x49);
  e8 (0xF8); /* lea rcx, [rcx-8] */
  e8 (0x48);
  e8 (0x89);
  e8 (0x01); /* mov [rcx], rax */
  st_rcx (OFF_RSP);
}

/* ip = rpop; pop rbx; ret -- back to the one dispatch loop. A direct
   jump into a native continuation was tried and measured slower: the
   ret/call pair is predicted by the return stack buffer, an indirect
   jmp is not. */
static void
emit_exit (void)
{
  sync_caches ();
  ld_rcx (OFF_RSP);
  e8 (0x48);
  e8 (0x8B);
  e8 (0x01); /* mov rax, [rcx] */
  e8 (0x48);
  e8 (0x8D);
  e8 (0x49);
  e8 (0x08); /* lea rcx, [rcx+8] */
  st_rcx (OFF_RSP);
  op_rm (0x89, RAX, RBX, (int32_t)OFF_IP); /* mov [rbx+ip], rax */
  e8 (0x5B);
  e8 (0xC3);
}

/* a safe primitive: call code(vm, w) and continue. The code field is
   loaded at run time so the decision stays honest if it changes. */
static void
emit_call (xt_t w)
{
  sync_caches ();
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
   all through the segment caches ---- */

static void
i_dup (void)
{
  unflag ();
  pull (0);
  if (cs[0].kind == S_CONST)
    push_const (cs[0].val);
  else
    {
      int r = alloc_reg ();

      mov_rr (r, cs[0].reg);
      push_reg (r);
    }
}

static void
i_drop (void)
{
  drop_top ();
}

static void
i_swap (void)
{
  slot_t t;

  unflag ();
  pull (1);
  t = cs[0];
  cs[0] = cs[1];
  cs[1] = t;
}

static void
i_over (void)
{
  unflag ();
  pull (1);
  if (cs[1].kind == S_CONST)
    push_const (cs[1].val);
  else
    {
      int r = alloc_reg ();

      mov_rr (r, cs[1].reg);
      push_reg (r);
    }
}

static void
i_rot (void)
{
  slot_t t;

  unflag ();
  pull (2);
  t = cs[2];
  cs[2] = cs[1];
  cs[1] = cs[0];
  cs[0] = t;
}

/* second OP= top. op_mr is "op r/m, reg", op_rm "op reg, r/m",
   digit the 81 /digit immediate form. */
typedef struct
{
  uint8_t op_mr, op_rm, digit;
  int commutes;
} binop_t;

static const binop_t B_ADD = { 0x01, 0x03, 0, 1 };
static const binop_t B_SUB = { 0x29, 0x2B, 5, 0 };
static const binop_t B_AND = { 0x21, 0x23, 4, 1 };
static const binop_t B_OR = { 0x09, 0x0B, 1, 1 };
static const binop_t B_XOR = { 0x31, 0x33, 6, 1 };

static int64_t
fold (const binop_t *b, int64_t a, int64_t c)
{
  uint64_t x = (uint64_t)a, y = (uint64_t)c;

  switch (b->digit)
    {
    case 0:
      return (int64_t)(x + y);
    case 5:
      return (int64_t)(x - y);
    case 4:
      return a & c;
    case 1:
      return a | c;
    default:
      return a ^ c;
    }
}

static void
binop (const binop_t *b)
{
  unflag ();
  pull (0);
  if (ncs == 1 && cs[0].kind == S_REG && b->commutes)
    {
      /* op reg, [second]: the result takes the second's slot */
      op_rm (b->op_rm, cs[0].reg, RCX, (int32_t)((dsp_delta + 1) * 8));
      dsp_delta += 1;
      return;
    }
  pull (1);
  if (cs[0].kind == S_CONST && cs[1].kind == S_CONST)
    {
      cs[1].val = fold (b, cs[1].val, cs[0].val);
      drop_top ();
      return;
    }
  if (cs[0].kind == S_CONST && fits32 (cs[0].val))
    {
      if (cs[0].val != 0 || b->digit == 4) /* x+0, x-0, x|0, x^0: nothing */
        alu_ri (b->digit, slot_reg (1), (int32_t)cs[0].val);
      drop_top ();
      return;
    }
  if (cs[1].kind == S_CONST && fits32 (cs[1].val))
    {
      int r = slot_reg (0);

      if (!b->commutes)
        {
          rex (0, r);
          e8 (0xF7);
          e8 ((uint8_t)(0xD8 | (r & 7))); /* neg r: a-b = -b+a */
        }
      if (cs[1].val != 0 || b->digit == 4)
        alu_ri (b->commutes ? b->digit : 0, r, (int32_t)cs[1].val);
      cs[1] = cs[0];
      cs[0].kind = S_CONST; /* a dead placeholder, dropped next */
      drop_top ();
      return;
    }
  op_rr (b->op_mr, slot_reg (0), slot_reg (1)); /* op r1, r0 */
  drop_top ();
}

static void
i_add (void)
{
  binop (&B_ADD);
}

static void
i_sub (void)
{
  binop (&B_SUB);
}

static void
i_and (void)
{
  binop (&B_AND);
}

static void
i_or (void)
{
  binop (&B_OR);
}

static void
i_xor (void)
{
  binop (&B_XOR);
}

static void
i_mul (void)
{
  int r0, r1;

  unflag ();
  pull (1);
  if (cs[0].kind == S_CONST && cs[1].kind == S_CONST)
    {
      cs[1].val = (int64_t)((uint64_t)cs[1].val * (uint64_t)cs[0].val);
      drop_top ();
      return;
    }
  if (cs[0].kind == S_CONST || cs[1].kind == S_CONST)
    {
      int k = cs[0].kind == S_CONST ? 0 : 1;
      int r = slot_reg (1 - k);

      if (fits32 (cs[k].val))
        {
          rex (r, r);
          e8 (0x69);
          e8 ((uint8_t)(0xC0 | ((r & 7) << 3) | (r & 7)));
          e32 ((uint32_t)(int32_t)cs[k].val); /* imul r, r, imm */
          cs[1] = cs[1 - k];
          cs[0].kind = S_CONST;
          drop_top ();
          return;
        }
    }
  r0 = slot_reg (0);
  r1 = slot_reg (1);
  rex (r1, r0);
  e8 (0x0F);
  e8 (0xAF);
  e8 ((uint8_t)(0xC0 | ((r1 & 7) << 3) | (r0 & 7))); /* imul r1, r0 */
  drop_top ();
}

static void
i_invert (void)
{
  int r;

  if (ncs && cs[0].kind == S_CONST)
    {
      cs[0].val = ~cs[0].val;
      return;
    }
  if (ncs && cs[0].kind == S_FLAGS)
    {
      cc ^= 1;
      return;
    }
  r = slot_reg (0);
  rex (0, r);
  e8 (0xF7);
  e8 ((uint8_t)(0xD0 | (r & 7))); /* not r */
}

/* compare second with top: the flag lives in the condition codes
   until something consumes it -- a branch, ideally */
static void
cmpop (uint8_t c)
{
  unflag ();
  if (ncs == 1 && cs[0].kind == S_CONST && fits32 (cs[0].val))
    alu_mi (7, RCX, (int32_t)((dsp_delta + 1) * 8), (int32_t)cs[0].val);
  else if (ncs == 1 && cs[0].kind == S_REG)
    op_rm (0x39, cs[0].reg, RCX, (int32_t)((dsp_delta + 1) * 8));
  else
    {
      pull (1);
      if (cs[0].kind == S_CONST && cs[1].kind == S_CONST)
        {
          int64_t a = cs[1].val, b = cs[0].val;
          int t = c == CC_E ? a == b : c == CC_L ? a < b : a > b;

          drop_top ();
          drop_top ();
          push_const (t ? -1 : 0);
          return;
        }
      if (cs[0].kind == S_CONST && fits32 (cs[0].val))
        alu_ri (7, slot_reg (1), (int32_t)cs[0].val);
      else
        op_rr (0x39, slot_reg (0), slot_reg (1)); /* cmp r1, r0 */
    }
  drop_top ();
  drop_top ();
  push_flags (c);
}

static void
i_lt (void)
{
  cmpop (CC_L);
}

static void
i_gt (void)
{
  cmpop (CC_G);
}

static void
i_eq (void)
{
  cmpop (CC_E);
}

static void
i_zeq (void)
{
  if (ncs == 0)
    {
      ensure_dsp ();
      alu_mi (7, RCX, (int32_t)(dsp_delta * 8), 0); /* cmp qword [top], 0 */
      cs[0].kind = S_FLAGS;
      ncs = 1;
      cc = CC_E;
      return;
    }
  switch (cs[0].kind)
    {
    case S_CONST:
      cs[0].val = cs[0].val == 0 ? -1 : 0;
      return;
    case S_FLAGS:
      cc ^= 1;
      return;
    default:
      op_rr (0x85, cs[0].reg, cs[0].reg); /* test r, r */
      free_reg (cs[0].reg);
      cs[0].kind = S_FLAGS;
      cc = CC_E;
    }
}

static void
i_fetch (void)
{
  int r;

  unflag ();
  pull (0);
  if (cs[0].kind == S_CONST && !held[RAX])
    {
      held[RAX] = 1;
      e8 (0x48);
      e8 (0xA1);
      e64 ((uint64_t)cs[0].val); /* movabs rax, [addr] */
      cs[0].kind = S_REG;
      cs[0].reg = RAX;
      return;
    }
  r = slot_reg (0);
  op_rm (0x8B, r, r, 0); /* mov r, [r] */
}

static void
i_store (void)
{
  unflag ();
  pull (1);
  if (cs[0].kind == S_CONST && cs[1].kind == S_REG && cs[1].reg == RAX)
    {
      e8 (0x48);
      e8 (0xA3);
      e64 ((uint64_t)cs[0].val); /* movabs [addr], rax */
    }
  else
    {
      int ra = slot_reg (0);

      if (cs[1].kind == S_CONST && fits32 (cs[1].val))
        {
          rex (0, ra);
          e8 (0xC7);
          e8 ((uint8_t)(0x80 | (ra & 7)));
          e32 (0);
          e32 ((uint32_t)(int32_t)cs[1].val); /* mov qword [ra], imm */
        }
      else
        op_rm (0x89, slot_reg (1), ra, 0); /* mov [ra], rv */
    }
  drop_top ();
  drop_top ();
}

static void
i_cfetch (void)
{
  int r;

  unflag ();
  r = slot_reg (0);
  rex (r, r);
  e8 (0x0F);
  e8 (0xB6);
  e8 ((uint8_t)(0x80 | ((r & 7) << 3) | (r & 7)));
  e32 (0); /* movzx r, byte [r] */
}

static void
i_cstore (void)
{
  int ra;

  unflag ();
  pull (1);
  ra = slot_reg (0);
  if (cs[1].kind == S_CONST)
    {
      if (ra >= 8)
        e8 (0x41);
      e8 (0xC6);
      e8 ((uint8_t)(0x80 | (ra & 7)));
      e32 (0);
      e8 ((uint8_t)cs[1].val); /* mov byte [ra], imm8 */
    }
  else
    {
      int rv = slot_reg (1);
      uint8_t x = (uint8_t)(0x40 | ((rv >> 3) << 2) | (ra >> 3));

      if (x != 0x40)
        e8 (x);
      e8 (0x88);
      e8 ((uint8_t)(0x80 | ((rv & 7) << 3) | (ra & 7)));
      e32 (0); /* mov [ra], rv8 */
    }
  drop_top ();
  drop_top ();
}

static void
i_tor (void)
{
  int r = slot_reg (0);

  ensure_rsp ();
  rsp_delta -= 1;
  st (r, RSI, rsp_delta);
  drop_top ();
}

static void
i_fromr (void)
{
  int r;

  unflag ();
  ensure_rsp ();
  r = alloc_reg ();
  ld (r, RSI, rsp_delta);
  rsp_delta += 1;
  push_reg (r);
}

static void
i_rfetch (void)
{
  int r;

  unflag ();
  ensure_rsp ();
  r = alloc_reg ();
  ld (r, RSI, rsp_delta);
  push_reg (r);
}

/* shifts by a literal count become immediate shifts; a run-time
   count needs cl, which is the data-stack cache, so it stays a call */
static void
shift (int digit, xt_t fallback)
{
  int r;

  if (ncs && cs[0].kind == S_CONST && cs[0].val >= 0 && cs[0].val < 64)
    {
      r = slot_reg (1);
      rex (0, r);
      e8 (0xC1);
      e8 ((uint8_t)(0xC0 | (digit << 3) | (r & 7)));
      e8 ((uint8_t)cs[0].val);
      drop_top ();
      return;
    }
  emit_call (fallback);
}

static void
i_lshift (void)
{
  shift (4, x_lshift);
}

static void
i_rshift (void)
{
  shift (5, x_rshift);
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

/* A word whose value is fixed at compile time: a create word pushes
   its body, a constant (dodoes over the thread "@ exit") fetches one
   cell. Only (does>) can change either, and it patches only the
   latest definition -- so the latest word stays a transfer. */
static int
const_like (vm_t *vm, cell_t w, int64_t *val, int *fetch)
{
  code_t code = *(code_t *)w;
  cell_t *d = (cell_t *)((xt_t)w)[-1];

  if ((xt_t)w == entry_xt (vm->latest))
    return 0;
  if (code == docreate)
    {
      *val = (int64_t)((xt_t)w + 1);
      *fetch = 0;
      return 1;
    }
  if (code == dodoes && d && (xt_t)d[0] == x_fetch && (xt_t)d[1] == x_exit)
    {
      *val = (int64_t)((xt_t)w + 1);
      *fetch = 1;
      return 1;
    }
  return 0;
}

static void
emit_const_like (int64_t val, int fetch)
{
  push_const (val);
  if (fetch)
    i_fetch ();
}

/* a colon word short and pure enough to inline whole: literals,
   constants, and inlinable primitives only, exit at the end, and no
   return-stack ops -- inlined, those would see the caller's frame. */
static int
inlinable_colon (vm_t *vm, cell_t *b, size_t *n)
{
  size_t i = 0;

  while (i < 12)
    {
      cell_t w = b[i];
      inline_emit_t f;
      int64_t v;
      int fetch;

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
      if (f == i_tor || f == i_fromr || f == i_rfetch)
        return 0;
      if (!f && !const_like (vm, w, &v, &fetch))
        return 0;
      i += 1;
    }
  return 0;
}

static void
emit_inline_colon (vm_t *vm, cell_t *b, size_t n)
{
  size_t i = 0;

  while (i < n)
    {
      inline_emit_t f = find_inliner (b[i]);
      int64_t v;
      int fetch;

      if ((xt_t)b[i] == x_lit)
        {
          push_const (b[i + 1]);
          i += 2;
        }
      else
        {
          if (f)
            f ();
          else if (const_like (vm, b[i], &v, &fetch))
            emit_const_like (v, fetch);
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

  sync_caches ();
  e8 (0x48);
  e8 (0xB8);
  fix_c1 = apos;
  e64 (0);                                 /* mov rax, &C1 (patched below) */
  op_rm (0x89, RAX, RBX, (int32_t)OFF_IP); /* mov [rbx+ip], rax */
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

/* The extent of a thread in cells: up to and including the first exit
   or branch that no branch jumps over. 0 when the thread must stay
   threaded -- (does>) patches ip from inside it -- or is unbounded. */
static size_t
thread_end (cell_t *body)
{
  size_t i, maxt = 0;

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
            return 0;
          if (t > maxt)
            maxt = t;
          sz = 2;
        }
      else if ((xt_t)w == x_does)
        return 0;
      if (((xt_t)w == x_exit || (xt_t)w == x_branch) && i + sz > maxt)
        return i + sz;
      i += sz;
      if (i > 65536)
        return 0;
    }
}

/* Compile one colon word. With osr < end, also emit an entry stub that
   resumes the word natively at body cell osr -- a branch target, so
   the stack cache is dead there by construction -- for a threaded
   activation already inside the word; the stub is returned. The
   continuation is already on the return stack, so the stub does not
   push one. */
static uint8_t *
compile (vm_t *vm, xt_t xt, size_t osr)
{
  cell_t *body;
  size_t end, i;
  size_t *off;
  patch_t *patches;
  uint8_t *is_target;
  uint8_t *stub = NULL;
  size_t npatch = 0;
  size_t fn;

  if (!arena || (code_t)*xt != docol)
    return NULL;
  body = xt + 1;

  /* pass 1: find the extent, bail on what must stay threaded */
  end = thread_end (body);
  if (end == 0)
    return NULL;

  if (apos + 512 * end + 512 > ARENA_BYTES)
    return NULL; /* arena full: stay threaded */

  off = calloc (end + 1, sizeof (size_t));
  patches = calloc (end + 1, sizeof (patch_t));
  is_target = calloc (end + 1, 1);
  if (!off || !patches || !is_target)
    {
      free (off);
      free (patches);
      free (is_target);
      return NULL;
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
  reset_caches ();

  for (i = 0; i < end;)
    {
      cell_t w = body[i];

      if (is_target[i])
        sync_caches ();
      off[i] = apos;
      if ((xt_t)w == x_lit)
        {
          push_const (body[i + 1]);
          i += 2;
        }
      else if ((xt_t)w == x_squote)
        {
          cell_t len = body[i + 1];

          push_const ((int64_t)&body[i + 2]);
          push_const (len);
          i += 2 + ((size_t)len + 7) / 8;
        }
      else if ((xt_t)w == x_0branch)
        {
          size_t tcell = (size_t)((cell_t *)body[i + 1] - body);

          if (ncs && cs[0].kind == S_CONST)
            {
              /* a known flag: the branch is a jump or nothing */
              int taken = cs[0].val == 0;

              drop_top ();
              sync_caches ();
              if (taken)
                {
                  e8 (0xE9);
                  patches[npatch].at = apos;
                  patches[npatch].tcell = tcell;
                  npatch++;
                  e32 (0);
                }
              i += 2;
              continue;
            }
          if (ncs && cs[0].kind == S_FLAGS)
            {
              /* the compare feeds the branch: jump when cc is false */
              uint8_t c = cc;

              cs[0].kind = S_CONST; /* nothing to free or store */
              drop_top ();
              sync_caches (); /* lea and mov only: flags survive */
              e8 (0x0F);
              e8 (0x80 | (c ^ 1));
            }
          else
            {
              int r = slot_reg (0);

              op_rr (0x85, r, r); /* test r, r */
              drop_top ();
              sync_caches ();
              e8 (0x0F);
              e8 (0x84); /* jz rel32 */
            }
          patches[npatch].at = apos;
          patches[npatch].tcell = tcell;
          npatch++;
          e32 (0);
          i += 2;
        }
      else if ((xt_t)w == x_branch)
        {
          sync_caches ();
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
             immutable C primitives; the hottest of those inline. A
             colon word already burned keeps its thread, so it inlines
             like one still threaded. */
          inline_emit_t inl = find_inliner (w);
          size_t ncells;
          int64_t v;
          int fetch;

          if (inl)
            inl ();
          else if (const_like (vm, w, &v, &fetch))
            emit_const_like (v, fetch);
          else if ((code == docol || in_arena (code))
                   && inlinable_colon (vm, (cell_t *)w + 1, &ncells))
            emit_inline_colon (vm, (cell_t *)w + 1, ncells);
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

  if (osr < end && is_target[osr])
    {
      int32_t rel;

      stub = arena + apos;
      prologue ();
      e8 (0xE9); /* jmp rel32 -> off[osr] */
      rel = (int32_t)((int64_t)off[osr] - (int64_t)(apos + 4));
      e32 ((uint32_t)rel);
    }

  mprotect (arena, ARENA_BYTES, PROT_READ | PROT_EXEC);

  *xt = (cell_t)(arena + fn); /* the one-cell strategy swap */

  free (off);
  free (patches);
  free (is_target);
  return stub;
}

void
jit_xt (vm_t *vm, xt_t xt)
{
  compile (vm, xt, SIZE_MAX);
}

/* ---- words ---- */

/* hotness: optimization policy, outside the dictionary entry. A
   two-way set-associative table counts events per key -- docol
   entries keyed by xt, taken backward branches keyed by their target
   -- and the word burns when a key reaches the threshold. Two keys
   that share a set keep separate counters; a third evicts the colder
   one, never resets a hot one. A compiled word never reaches docol
   again, so its counter stops by itself. */

enum
{
  HOT_SETS = 1024,
  HOT_WAYS = 2,
  HOT_THRESHOLD = 512
};

typedef struct
{
  const void *key;
  uint32_t n;
} hot_t;

static hot_t hot[HOT_SETS][HOT_WAYS];

/* true exactly once per key: the tick that reaches the threshold */
static int
hot_tick (const void *key)
{
  size_t s = (size_t)((((uintptr_t)key >> 3) * 0x9E3779B97F4A7C15ull) >> 54)
             & (HOT_SETS - 1);
  hot_t *set = hot[s], *victim = &set[0];
  int k;

  for (k = 0; k < HOT_WAYS; k++)
    {
      if (set[k].key == key)
        {
          if (++set[k].n != HOT_THRESHOLD)
            return 0;
          set[k].key = NULL;
          set[k].n = 0;
          return 1;
        }
      if (set[k].n < victim->n)
        victim = &set[k];
    }
  victim->key = key;
  victim->n = 1;
  return 0;
}

void
jit_count (vm_t *vm, xt_t xt)
{
  if (vm->jit_on < 0 || !arena)
    return;
  if (hot_tick (xt))
    jit_xt (vm, xt);
}

/* The word whose thread contains target. The dictionary gives the
   last headed entry below it; from there forward, every code field
   that is docol (or native with its thread still present) behind a
   zero does cell owns the cells of its thread -- the shape :noname
   lays down without a header, so headerless words are found too.
   NULL when the target sits in no compilable thread. */
static xt_t
enclosing_xt (vm_t *vm, cell_t *target)
{
  dict_entry_t *w;
  cell_t *p = NULL;

  for (w = vm->latest; w; w = w->link)
    if (entry_xt (w) < target)
      {
        p = entry_xt (w);
        break;
      }
  while (p && p < target)
    {
      code_t code = *(code_t *)p;

      if ((code == docol || in_arena (code)) && p[-1] == 0)
        {
          size_t end = thread_end (p + 1);

          if (end && target < p + 1 + end)
            return p;
          if (end)
            {
              p += 1 + end;
              continue;
            }
        }
      p++;
    }
  return NULL;
}

/* A taken backward branch in threaded code: the loop is the hot
   thing, not the call. At the threshold the enclosing word burns and
   this activation enters the native code at the branch target
   through the OSR stub, which returns -- like any primitive -- once
   the word exits or transfers. Returns 0 to let the caller branch in
   the thread as usual. */
int
jit_backedge (vm_t *vm, xt_t *target)
{
  xt_t xt;
  uint8_t *stub;

  if (vm->jit_on < 0 || !arena)
    return 0;
  if (!hot_tick (target))
    return 0;
  xt = enclosing_xt (vm, (cell_t *)target);
  if (!xt || (code_t)*xt != docol)
    return 0;
  stub = compile (vm, xt, (size_t)((cell_t *)target - (xt + 1)));
  if (!stub)
    return 0;
  (*(code_t)(cell_t)stub) (vm, xt); /* through a cell, like a code field */
  return 1;
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
  x_fetch = xt_of (vm, "@");
  x_lshift = xt_of (vm, "lshift");
  x_rshift = xt_of (vm, "rshift");
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
  reg_inline (vm, "c@", i_cfetch);
  reg_inline (vm, "c!", i_cstore);
  reg_inline (vm, "lshift", i_lshift);
  reg_inline (vm, "rshift", i_rshift);
  reg_inline (vm, ">r", i_tor);
  reg_inline (vm, "r>", i_fromr);
  reg_inline (vm, "r@", i_rfetch);

  defprim (vm, "jit", prim_jit);
  defprim (vm, "jit-on", prim_jit_on);
  defprim (vm, "jit-off", prim_jit_off);
}
