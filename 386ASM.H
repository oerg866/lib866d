/* Microsoft C 6.x/7.x inline assembly helper for 386+ instructions */

#include "types.h"

/* Helper union to move 32-bit words in and out of inline assembly code */
#pragma pack(1)
typedef union {
    struct {
        u16 low;
        u16 high;
    } words;
    u16 array16[2];
    u32 dword;
} asm_DWORD;
#pragma pack()

#if defined(__BORLANDC__) || defined(__TURBOC__)
# define _asm asm
# define _DB(x) db (x);
# define _DL(x) dw (x) AND 0xFFFF, ((x) SHR 16) AND 0xFFFF;
# define _ASM_BRK_ ; /**/ ;
# define _ASM_LBL_(x) } x: _asm {
#elif defined(_MSC_VER) || defined(__WATCOMC__)
# define _DB(x) _asm _emit x
# define _DL(x) _DB(((x >> 0UL) & 0xFF)) _DB(((x >> 8UL) & 0xFF)) _DB(((x >> 16UL) & 0xFF)) _DB(((x >> 24UL) & 0xFF))
# define _ASM_BRK_ _asm
# define _ASM_LBL_(x) x:
#else
# error "Unknown Compiler"
#endif


#define _DPREFIX_ _DB(0x66)

#define _AX 0
#define _BX 3
#define _CX 1
#define _DX 2

#define _EAX _AX
#define _EBX _BX
#define _ECX _CX
#define _EDX _DX

#define NO_OPERATION _DB(0x90)

#define PUSH32(reg) _DPREFIX_ _DB(0x50+reg)
#define POP32(reg) _DPREFIX_ _DB(0x58+reg)

#define PUSHAD _DPREFIX_ _DB(0x60)
#define POPAD _DPREFIX_ _DB(0x61)

/* or <reg32>, <reg32> */
#define OR_REG_REG(reg1, reg2) _DPREFIX_ _DB(0x09) _DB(0xc0+(reg2 SHL 3)+(reg1))

/* or <reg32>, #imm */
#define OR_REG_IMM(reg, x) _DPREFIX_ _DB(0x81) _DB(0xc8+reg)  _DL(x)

/* and <reg32>, #imm */
#define AND_REG_IMM(reg, x) _DPREFIX_ _DB(0x81) _DB(0xe0+reg)  _DL(x)

/* shl <reg32>, #imm */
#define SHL_REG_IMM(reg, x) _DPREFIX_ _DB(0xc1) _DB(0xe0+reg) _DB(x)
/* shr <reg32>, #imm */
#define SHR_REG_IMM(reg, x) _DPREFIX_ _DB(0xc1) _DB(0xe8+reg) _DB(x)

/* mov <reg32>, #imm */
#define MOV_REG_IMM(reg, x) _DPREFIX_ _DB(0xb8+reg) _DL(x)

/* mov dword ptr es:[di+x], <reg32> */
#define MOV_DWORD_PTR_ESDI_OFFSET_REG(x, reg) _DB(0x26) _DPREFIX_ _DB(0x89) _DB(0x45+(reg SHL 3)) _DB(x)

/* mov <reg32>, dword ptr es:[di+x] */
#define MOV_REG_DWORD_PTR_ESDI_OFFSET(reg, x) _DB(0x26) _DPREFIX_ _DB(0x8b) _DB(0x45+(reg SHL 3)) _DB(x)

#define MOV_REG16_ES(reg) _DB(0x8c) _DB(0xc0+reg)
#define MOV_REG16_DI(reg) _DB(0x89) _DB(0xf8+reg)

/* move <reg32>, <C-var> WARNING: Really ugly hack :D */
#define MOV_REG_DWORDVAR(reg, var) \
              push es \
    _ASM_BRK_ push di \
    _ASM_BRK_ les di, var \
    _ASM_BRK_ MOV_REG16_ES(reg) \
    _ASM_BRK_ SHL_REG_IMM(reg, 16) \
    _ASM_BRK_ MOV_REG16_DI(reg) \
    _ASM_BRK_ pop di \
    _ASM_BRK_ pop es

/* Move 32 bit register to an asm_dword, reg16 is the 16 bit name for the register (e.g. ax) */
#define MOV_DWORDVAR_REG(var, reg, reg16) \
              PUSH32(reg) \
    _ASM_BRK_ mov var.words.low, reg16 \
    _ASM_BRK_ SHR_REG_IMM(reg, 16) \
    _ASM_BRK_ mov var.words.high, reg16 \
    _ASM_BRK_ POP32(reg)


#define CPUID _DB(0x0f) _DB(0xa2)
#define WBINVD _DB(0x0f) _DB(0x09)
#define RDMSR _DB(0x0f) _DB(0x32)
#define WRMSR _DB(0x0f) _DB(0x30)

#define CPUID_LEVEL(level) \
              MOV_REG_IMM(_EAX, level) \
    _ASM_BRK_ MOV_REG_IMM(_EBX, 0) \
    _ASM_BRK_ MOV_REG_IMM(_ECX, 0) \
    _ASM_BRK_ MOV_REG_IMM(_EDX, 0) \
    _ASM_BRK_ CPUID

#define MOV_EAX_CR(index) _DB(0x0f) _DB(0x20) _DB(0xc0+(index SHL 3))
#define MOV_CR_EAX(index) _DB(0x0f) _DB(0x22) _DB(0xc0+(index SHL 3))

#define MOV_DWORD_PTR_CR(index, pointer) \
    _ASM_BRK_ les di, dword ptr pointer \
    _ASM_BRK_ MOV_EAX_CR(index) \
    _ASM_BRK_ MOV_DWORD_PTR_ESDI_OFFSET_REG(0, _EAX) \

#define MOV_CR_DWORD_PTR(index, pointer) \
    _ASM_BRK_ les di, dword ptr pointer \
    _ASM_BRK_ MOV_REG_DWORD_PTR_ESDI_OFFSET(_EAX, 0) \
    _ASM_BRK_ pushf \
    _ASM_BRK_ cli \
    _ASM_BRK_ MOV_CR_EAX(index) \
    _ASM_BRK_ WBINVD \
    _ASM_BRK_ nop \
    _ASM_BRK_ popf

#define OUT_DX_EAX _DPREFIX_ _DB(0xef)
#define IN_EAX_DX _DPREFIX_ _DB(0xed)

