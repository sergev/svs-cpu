/*
 * SVS CPU simulator.
 *
 * Copyright (c) 2022 Leonid Broukhis, Serge Vakulenko
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "svs_defs.h"
#include <math.h>
#include <float.h>
#include <time.h>

t_value memory[MEMSIZE];                /* physical memory */
uint8 tag[MEMSIZE];                     /* tags */

CORE cpu_core[10];                      /* state of all processors */

int32 tmr_poll = INSN_PER_TICK;         /* pgm timer poll */

TRACEMODE svs_trace;                    /* trace mode */

extern const char *scp_errors[];

/* Wired (non-registered) bits of interrupt registers (RPR and GRVP)
 * cannot be cleared by writing to the RPR and must be cleared by clearing
 * the registers generating the corresponding interrupts.
 */
#define RPR_WIRED_BITS (0)

#define GRVP_WIRED_BITS (0)

t_stat cpu_examine(t_value *vptr, t_addr addr, UNIT *uptr, int32 sw);
t_stat cpu_deposit(t_value val, t_addr addr, UNIT *uptr, int32 sw);
t_stat cpu_reset(DEVICE *dev);
t_stat cpu_req(UNIT *u, int32 val, CONST char *cptr, void *desc);
t_stat cpu_set_pult(UNIT *u, int32 val, CONST char *cptr, void *desc);
t_stat cpu_show_pult(FILE *st, UNIT *up, int32 v, CONST void *dp);
t_stat cpu_set_trace(UNIT *u, int32 val, CONST char *cptr, void *desc);
t_stat cpu_set_itrace(UNIT *u, int32 val, CONST char *cptr, void *desc);
t_stat cpu_set_etrace(UNIT *u, int32 val, CONST char *cptr, void *desc);
t_stat cpu_show_trace(FILE *st, UNIT *up, int32 v, CONST void *dp);
t_stat cpu_clr_trace(UNIT *uptr, int32 val, CONST char *cptr, void *desc);

/*
 * CPU data structures
 *
 * cpu_dev[]    CPU device descriptors
 * cpu_unit[]   CPU unit descriptors
 * cpu0_reg     CPU register list
 * cpu_mod      CPU modifiers list
 */

UNIT cpu_unit[4] = {
    { UDATA(NULL, UNIT_FIX, MEMSIZE) },
    { UDATA(NULL, UNIT_FIX, MEMSIZE) },
    { UDATA(NULL, UNIT_FIX, MEMSIZE) },
    { UDATA(NULL, UNIT_FIX, MEMSIZE) },
};

#define ORDATAVM(nm,loc,wd) REGDATA(nm,(loc),8,wd,0,1,NULL,NULL,REG_VMIO,0,0)

REG cpu0_reg[] = {
    { ORDATA   (PC,     cpu_core[0].PC,         15) },  /* program counter */
    { ORDATA   (RK,     cpu_core[0].RK,         24) },  /* instruction register */
    { ORDATA   (Aex,    cpu_core[0].Aex,        15) },  /* effective address */
    { ORDATAVM (ACC,    cpu_core[0].ACC,        48) },  /* accumulator */
    { ORDATAVM (RMR,    cpu_core[0].RMR,        48) },  /* LSB register */
    { BINRDATA (RAU,    cpu_core[0].RAU,        6)  },  /* ALU modes */
    { ORDATA   (M1,     cpu_core[0].M[1],       15) },  /* index (modifier) registers */
    { ORDATA   (M2,     cpu_core[0].M[2],       15) },
    { ORDATA   (M3,     cpu_core[0].M[3],       15) },
    { ORDATA   (M4,     cpu_core[0].M[4],       15) },
    { ORDATA   (M5,     cpu_core[0].M[5],       15) },
    { ORDATA   (M6,     cpu_core[0].M[6],       15) },
    { ORDATA   (M7,     cpu_core[0].M[7],       15) },
    { ORDATA   (M10,    cpu_core[0].M[010],     15) },
    { ORDATA   (M11,    cpu_core[0].M[011],     15) },
    { ORDATA   (M12,    cpu_core[0].M[012],     15) },
    { ORDATA   (M13,    cpu_core[0].M[013],     15) },
    { ORDATA   (M14,    cpu_core[0].M[014],     15) },
    { ORDATA   (M15,    cpu_core[0].M[015],     15) },
    { ORDATA   (M16,    cpu_core[0].M[016],     15) },
    { ORDATA   (M17,    cpu_core[0].M[017],     15) },  /* also the stack pointer */
    { ORDATA   (M20,    cpu_core[0].M[020],     15) },  /* MOD - address modifier register */
    { ORDATA   (M21,    cpu_core[0].M[021],     15) },  /* PSW - CU modes */
    { ORDATA   (M27,    cpu_core[0].M[027],     15) },  /* SPSW - saved CU modes */
    { ORDATA   (M32,    cpu_core[0].M[032],     15) },  /* ERET - extracode return address */
    { ORDATA   (M33,    cpu_core[0].M[033],     15) },  /* IRET - interrupt return address */
    { ORDATA   (M34,    cpu_core[0].M[034],     16) },  /* IBP - instruction bkpt address */
    { ORDATA   (M35,    cpu_core[0].M[035],     16) },  /* DWP - watchpoint address */
    { BINRDATA (RUU,    cpu_core[0].RUU,        9)  },  /* execution modes */
    { ORDATAVM (RP0,    cpu_core[0].RP[0],      48) },  /* user page mapping */
    { ORDATAVM (RP1,    cpu_core[0].RP[1],      48) },
    { ORDATAVM (RP2,    cpu_core[0].RP[2],      48) },
    { ORDATAVM (RP3,    cpu_core[0].RP[3],      48) },
    { ORDATAVM (RP4,    cpu_core[0].RP[4],      48) },
    { ORDATAVM (RP5,    cpu_core[0].RP[5],      48) },
    { ORDATAVM (RP6,    cpu_core[0].RP[6],      48) },
    { ORDATAVM (RP7,    cpu_core[0].RP[7],      48) },
    { ORDATAVM (RPS0,   cpu_core[0].RPS[0],     48) },  /* kernel page mapping */
    { ORDATAVM (RPS1,   cpu_core[0].RPS[1],     48) },
    { ORDATAVM (RPS2,   cpu_core[0].RPS[2],     48) },
    { ORDATAVM (RPS3,   cpu_core[0].RPS[3],     48) },
    { ORDATAVM (RPS4,   cpu_core[0].RPS[4],     48) },
    { ORDATAVM (RPS5,   cpu_core[0].RPS[5],     48) },
    { ORDATAVM (RPS6,   cpu_core[0].RPS[6],     48) },
    { ORDATAVM (RPS7,   cpu_core[0].RPS[7],     48) },
    { ORDATA   (RZ,     cpu_core[0].RZ,         32) },  /* page protection */
    { ORDATAVM (RPR,    cpu_core[0].RPR,        48) },  /* internal interrupt reg */
    { ORDATA   (GRVP,   cpu_core[0].GRVP,       24) },  /* external interrupt reg */
    { ORDATA   (GRM,    cpu_core[0].GRM,        24) },  /* mask of the above */
    { ORDATAVM (PP,     cpu_core[0].PP,         48) },  /* requests to processors */
    { ORDATAVM (OPP,    cpu_core[0].OPP,        48) },  /* responds to processors */
    { ORDATAVM (POP,    cpu_core[0].POP,        48) },  /* interrupts from processors */
    { ORDATAVM (OPOP,   cpu_core[0].OPOP,       48) },  /* responds from processors */
    { ORDATAVM (RKP,    cpu_core[0].RKP,        48) },  /* configuration of processors */
    { 0 }
};

MTAB cpu_mod[] = {
    { MTAB_XTD|MTAB_VDV,
        0, "IDLE",  "IDLE",     &sim_set_idle,      &sim_show_idle,     NULL,
                                "Enables idle detection mode" },
    { MTAB_XTD|MTAB_VDV,
        0, NULL,    "NOIDLE",   &sim_clr_idle,      NULL,               NULL,
                                "Disables idle detection" },
    { MTAB_XTD|MTAB_VDV,
        0, NULL,    "REQ",      &cpu_req,           NULL,               NULL,
                                "Sends a request interrupt" },
    { MTAB_XTD|MTAB_VDV,
        0, "TRACE", "TRACE",    &cpu_set_trace,     &cpu_show_trace,    NULL,
                                "Enables full tracing of processor state" },
    { MTAB_XTD|MTAB_VDV,
        0, NULL,    "ITRACE",   &cpu_set_itrace,    NULL,               NULL,
                                "Enables instruction tracing" },
    { MTAB_XTD|MTAB_VDV,
        0, NULL,    "ETRACE",   &cpu_set_etrace,    NULL,               NULL,
                                "Enables extracode only tracing" },
    { MTAB_XTD|MTAB_VDV,
        0, NULL,    "NOTRACE",  &cpu_clr_trace,     NULL,               NULL,
                                "Disables tracing" },

    //TODO: Разрешить/запретить контроль числа.
    //{ 2, 0, "NOCHECK", "NOCHECK" },
    //{ 2, 2, "CHECK",   "CHECK" },
    { 0 }
};

DEVICE cpu_dev[4] = {
    { "CPU0", &cpu_unit[0], cpu0_reg, cpu_mod,
      1, 8, 17, 1, 8, 50,
      &cpu_examine, &cpu_deposit, &cpu_reset,
      NULL, NULL, NULL, (void*)&cpu_core[0], DEV_DEBUG },
    { "CPU1", &cpu_unit[1], NULL, cpu_mod,
      1, 8, 17, 1, 8, 50,
      &cpu_examine, &cpu_deposit, &cpu_reset,
      NULL, NULL, NULL, (void*)&cpu_core[1], DEV_DEBUG },
    { "CPU2", &cpu_unit[2], NULL, cpu_mod,
      1, 8, 17, 1, 8, 50,
      &cpu_examine, &cpu_deposit, &cpu_reset,
      NULL, NULL, NULL, (void*)&cpu_core[2], DEV_DEBUG },
    { "CPU3", &cpu_unit[3], NULL, cpu_mod,
      1, 8, 17, 1, 8, 50,
      &cpu_examine, &cpu_deposit, &cpu_reset,
      NULL, NULL, NULL, (void*)&cpu_core[3], DEV_DEBUG },
};

/*
 * SCP data structures and interface routines
 *
 * sim_name             simulator name string
 * sim_PC               pointer to saved PC register descriptor
 * sim_emax             maximum number of words for examine/deposit
 * sim_devices          array of pointers to simulated devices
 * sim_stop_messages    array of pointers to stop messages
 * sim_load             binary loader
 */

char sim_name[] = "СВС";

REG *sim_PC = &cpu0_reg[0];

int32 sim_emax = 1;     /* max number of addressable units per instruction */

DEVICE *sim_devices[] = {
    &cpu_dev[0],        /* процессоры */
#if NUM_CORES > 1
    &cpu_dev[1],
#endif
#if NUM_CORES > 2
    &cpu_dev[2],
#endif
#if NUM_CORES > 3
    &cpu_dev[3],
#endif
    &iom_dev[0],        /* ПВВ */
#if NUM_CORES > 1
    &iom_dev[1],
#endif
#if NUM_CORES > 2
    &iom_dev[2],
#endif
#if NUM_CORES > 3
    &iom_dev[3],
#endif

    &clock_dev,         /* таймер */

    &tty_dev,           /* терминалы */
    0
};

const char *sim_stop_messages[] = {
    "Неизвестная ошибка",                 /* Unknown error */
    "Останов",                            /* STOP */
    "Точка останова",                     /* Emulator breakpoint */
    "Точка останова по считыванию",       /* Emulator read watchpoint */
    "Точка останова по записи",           /* Emulator write watchpoint */
    "Выход за пределы памяти",            /* Run out end of memory */
    "Запрещенная команда",                /* Invalid instruction */
    "Контроль команды",                   /* A data-tagged word fetched */
    "Команда в чужом листе",              /* Paging error during fetch */
    "Число в чужом листе",                /* Paging error during load/store */
    "Контроль числа МОЗУ",                /* RAM parity error */
    "Контроль числа БРЗ",                 /* Write cache parity error */
    "Переполнение АУ",                    /* Arith. overflow */
    "Деление на нуль",                    /* Division by zero or denorm */
    "Двойное внутреннее прерывание",      /* SIMH: Double internal interrupt */
    "Чтение неформатированного барабана", /* Reading unformatted drum */
    "Чтение неформатированного диска",    /* Reading unformatted disk */
    "Останов по КРА",                     /* Hardware breakpoint */
    "Останов по считыванию",              /* Load watchpoint */
    "Останов по записи",                  /* Store watchpoint */
    "Не реализовано",                     /* Unimplemented I/O or special reg. access */
};

/*
 * Memory examine
 */
t_stat cpu_examine(t_value *vptr, t_addr addr, UNIT *uptr, int32 sw)
{
    DEVICE *dev = find_dev_from_unit(uptr);
    if (! dev)
        return SCPE_IERR;

    CORE *cpu = (CORE*) dev->ctxt;
    if (! cpu)
        return SCPE_IERR;

    if (addr >= MEMSIZE)
        return SCPE_NXM;
    if (vptr) {
        if (addr < 010) {
            /* from switch regs */
            *vptr = cpu->pult[addr];
        } else {
            *vptr = memory[addr] >> 16;
        }
    }
    return SCPE_OK;
}

/*
 * Memory deposit
 */
t_stat cpu_deposit(t_value val, t_addr addr, UNIT *uptr, int32 sw)
{
    DEVICE *dev = find_dev_from_unit(uptr);
    if (! dev)
        return SCPE_IERR;

    CORE *cpu = (CORE*) dev->ctxt;
    if (! cpu)
        return SCPE_IERR;

    if (addr >= MEMSIZE)
        return SCPE_NXM;
    if (addr < 010) {
        /* Deposited values for the switch register address range
         * always go to switch registers.
         */
        cpu->pult[addr] = val;
    } else {
        memory[addr] = val << 16;
        tag[addr] = TAG_INSN48;
    }
    return SCPE_OK;
}

/*
 * Reset routine
 */
t_stat cpu_reset(DEVICE *dev)
{
    CORE *cpu = (CORE*) dev->ctxt;
    if (! cpu)
        return SCPE_IERR;

    cpu->index = dev->name[3] - '0';
    cpu->ACC = 0;
    cpu->RMR = 0;
    cpu->RAU = 0;
    cpu->RUU = RUU_EXTRACODE | RUU_AVOST_DISABLE;

    int i;
    for (i=0; i<NREGS; ++i)
        cpu->M[i] = 0;

    /* Регистр 17: БлП, БлЗ, ПОП, ПОК, БлПр */
    cpu->M[PSW] = PSW_MMAP_DISABLE | PSW_PROT_DISABLE | PSW_INTR_HALT |
        PSW_CHECK_HALT | PSW_INTR_DISABLE;

    /* Регистр 23: БлП, БлЗ, РежЭ, БлПр */
    cpu->M[SPSW] = SPSW_MMAP_DISABLE | SPSW_PROT_DISABLE | SPSW_EXTRACODE |
        SPSW_INTR_DISABLE;

    cpu->RZ = 0;
    for (i = 0; i < 8; ++i) {
        cpu->RP[i] = 0;
        cpu->RPS[i] = 0;
    }

    cpu->RPR = 0;
    cpu->GRM = 0;
    cpu->PP = 0;
    cpu->OPP = 0;
    cpu->POP = 0;
    cpu->OPOP = 0;
    cpu->RKP = 0;

    // Disabled due to a conflict with loading
    // cpu->PC = 1;             /* "reset cpu; go" should start from 1  */

    sim_brk_types = SWMASK('E') | SWMASK('R') | SWMASK('W');
    sim_brk_dflt = SWMASK('E');

    // Setup register descriptors.
    if (! dev->registers) {
        REG *rptr, *r;

        rptr = (REG*) malloc(sizeof(cpu0_reg));
        if (! rptr) {
            return SCPE_MEM;
        }
        memcpy(rptr, &cpu0_reg, sizeof(cpu0_reg));
        dev->registers = rptr;
        for (r = rptr; r->name != NULL; r++) {
            /* Update register pointer. */
            r->loc = (char*)r->loc + sizeof(CORE)*cpu->index;
        }
    }

    if (svs_trace) {
        fprintf(sim_log, "cpu%d --- Reset\n", cpu->index);
    }
    mpd_reset(cpu);

    return SCPE_OK;
}

/*
 * Request routine
 */
t_stat cpu_req(UNIT *u, int32 val, CONST char *cptr, void *desc)
{
    DEVICE *dev = find_dev_from_unit(u);
    if (! dev)
        return SCPE_IERR;

    CORE *cpu = (CORE*) dev->ctxt;
    if (! cpu)
        return SCPE_IERR;

    if (svs_trace) {
        fprintf(sim_log, "cpu%d --- Request from control panel\n", cpu->index);
    }
    cpu->GRVP |= GRVP_PANEL_REQ;
    return SCPE_OK;
}

/*
 * Trace level selector
 */
t_stat cpu_set_trace(UNIT *u, int32 val, CONST char *cptr, void *desc)
{
    if (! sim_log) {
        sim_printf("Cannot enable tracing: please set console log first\n");
        return SCPE_INCOMP;
    }
    svs_trace = TRACE_ALL;
    sim_printf("Trace instructions, registers and memory access\n");
    return SCPE_OK;
}

t_stat cpu_set_etrace(UNIT *u, int32 val, CONST char *cptr, void *desc)
{
    if (! sim_log) {
        sim_printf("Cannot enable tracing: please set console log first\n");
        return SCPE_INCOMP;
    }
    svs_trace = TRACE_EXTRACODES;
    sim_printf("Trace extracodes (except e75)\n");
    return SCPE_OK;
}

t_stat cpu_set_itrace(UNIT *u, int32 val, CONST char *cptr, void *desc)
{
    if (! sim_log) {
        sim_printf("Cannot enable tracing: please set console log first\n");
        return SCPE_INCOMP;
    }
    svs_trace = TRACE_INSTRUCTIONS;
    sim_printf("Trace instructions only\n");
    return SCPE_OK;
}

t_stat cpu_clr_trace (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
    svs_trace = TRACE_NONE;
    return SCPE_OK;
}

t_stat cpu_show_trace(FILE *st, UNIT *up, int32 v, CONST void *dp)
{
    switch (svs_trace) {
    case TRACE_NONE:         break;
    case TRACE_EXTRACODES:   fprintf(st, "trace extracodes"); break;
    case TRACE_INSTRUCTIONS: fprintf(st, "trace instructions"); break;
    case TRACE_ALL:          fprintf(st, "trace all"); break;
    }
    return SCPE_OK;
}

/*
 * Write Unicode symbol to file.
 * Convert to UTF-8 encoding:
 * 00000000.0xxxxxxx -> 0xxxxxxx
 * 00000xxx.xxyyyyyy -> 110xxxxx, 10yyyyyy
 * xxxxyyyy.yyzzzzzz -> 1110xxxx, 10yyyyyy, 10zzzzzz
 */
void
utf8_putc(unsigned ch, FILE *fout)
{
    if (ch < 0x80) {
        putc(ch, fout);
        return;
    }
    if (ch < 0x800) {
        putc(ch >> 6 | 0xc0, fout);
        putc((ch & 0x3f) | 0x80, fout);
        return;
    }
    putc(ch >> 12 | 0xe0, fout);
    putc(((ch >> 6) & 0x3f) | 0x80, fout);
    putc((ch & 0x3f) | 0x80, fout);
}

/*
 * *call ОКНО - так называлась служебная подпрограмма в мониторной
 * системе "Дубна", которая печатала полное состояние всех регистров.
 */
void svs_okno(CORE *cpu, const char *message)
{
    svs_log_cont("_%%%%%% %s: ", message);
    if (sim_log)
        svs_fprint_cmd(sim_log, cpu->RK);
    svs_log("_");

    /* СчАС, системные индекс-регистры 020-035. */
    svs_log("_    СчАС:%05o  20:%05o  21:%05o  27:%05o  32:%05o  33:%05o  34:%05o  35:%05o",
        cpu->PC, cpu->M[020], cpu->M[021], cpu->M[027],
        cpu->M[032], cpu->M[033], cpu->M[034], cpu->M[035]);

    /* Индекс-регистры 1-7. */
    svs_log("_       1:%05o   2:%05o   3:%05o   4:%05o   5:%05o   6:%05o   7:%05o",
        cpu->M[1], cpu->M[2], cpu->M[3], cpu->M[4],
        cpu->M[5], cpu->M[6], cpu->M[7]);

    /* Индекс-регистры 010-017. */
    svs_log("_      10:%05o  11:%05o  12:%05o  13:%05o  14:%05o  15:%05o  16:%05o  17:%05o",
        cpu->M[010], cpu->M[011], cpu->M[012], cpu->M[013],
        cpu->M[014], cpu->M[015], cpu->M[016], cpu->M[017]);

    /* Сумматор, РМР, режимы АУ и УУ. */
    svs_log("_      СМ:%04o %04o %04o %04o  РМР:%04o %04o %04o %04o  РАУ:%02o    РУУ:%03o",
        (int) (cpu->ACC >> 36) & BITS(12), (int) (cpu->ACC >> 24) & BITS(12),
        (int) (cpu->ACC >> 12) & BITS(12), (int) cpu->ACC & BITS(12),
        (int) (cpu->RMR >> 36) & BITS(12), (int) (cpu->RMR >> 24) & BITS(12),
        (int) (cpu->RMR >> 12) & BITS(12), (int) cpu->RMR & BITS(12),
        cpu->RAU, cpu->RUU);
}

/*
 * Команда "рег"
 */
static void cmd_002(CORE *cpu)
{
    /*svs_debug("--- рег %03o", cpu->Aex & 0377);*/

    switch (cpu->Aex & 0377) {

    case 020: case 021: case 022: case 023:
    case 024: case 025: case 026: case 027:
        /* Запись в регистры приписки режима пользователя */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Установка приписки пользователя\n", cpu->index);
        mmu_set_rp(cpu, cpu->Aex & 7, cpu->ACC, 0);
        break;

    case 030: case 031: case 032: case 033:
        /* Запись в регистры защиты */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Запись в регистр защиты\n", cpu->index);
        mmu_set_protection(cpu, cpu->Aex & 3, cpu->ACC);
        break;

    case 034:
        /* Запись в регистр конфигурации оперативной памяти */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Запись конфигурации оперативной памяти\n", cpu->index);
        /* игнорируем */
        break;

    case 035:
        /* Запись в сигнал контроля оперативной памяти */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Запись в сигнал контроля оперативной памяти\n", cpu->index);
        /* игнорируем */
        break;

    case 0235:
        /* Чтение сигнала контроля от оперативной памяти */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Чтение сигнала контроля оперативной памяти\n", cpu->index);
        cpu->ACC = 0;
        break;

    case 0236:
        /* Считывание сигналов запрета запроса в МОП от коммутаторов памяти */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Чтение ЗЗ\n", cpu->index);
        cpu->ACC = 0; /* не используем */
        break;

    case 037:
        /* Гашение регистра внутренних прерываний */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Гашение РПР\n", cpu->index);
        cpu->RPR &= cpu->ACC | RPR_WIRED_BITS;
        break;

    case 0237:
        /* Чтение главного регистра прерываний */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Чтение ГРП\n", cpu->index);
        cpu->ACC = cpu->RPR;
        break;

    case 044:
        /* Запись в регистр тега */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Установка тега\n", cpu->index);
        cpu->TagR = cpu->ACC;
        break;

    case 0244:
        /* Чтение регистра тега */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Чтение регистра тега\n", cpu->index);
        cpu->ACC = cpu->TagR;
        break;

    case 0245:
        /* Чтение регистра ТЕГБРЧ */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Чтение ТЕГБРЧ\n", cpu->index);
        cpu->ACC = 0; //TODO
        break;

    case 046:
        /* Запись маски внешних прерываний */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Установка ГРМ\n", cpu->index);
        cpu->GRM = cpu->ACC;
        break;

    case 0246:
        /* Чтение маски внешних прерываний */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Чтение ГРМ\n", cpu->index);
        cpu->ACC = cpu->GRM;
        break;

    case 047:
        /* Clearing the external interrupt register: */
        /* it is impossible to clear wired (stateless) bits this way */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Гашение РВП\n", cpu->index);
        cpu->GRVP &= cpu->ACC | GRVP_WIRED_BITS;
        break;

    case 0247:
        /* Чтение регистра внешних прерываний */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Чтение РВП\n", cpu->index);
        cpu->ACC = cpu->GRVP;
        break;

    case 050:
        /* Запись в регистр прерываний процессорам */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Запись в ПП\n", cpu->index);
        cpu->PP = cpu->ACC & (CONF_IOM_MASK | CONF_CPU_MASK | CONF_DATA_MASK);
        if (cpu->ACC & CONF_MT) {
            /* Передача младшей половины байта. */
            mpd_send_nibble(cpu, CONF_GET_DATA(cpu->PP));
        }
        if (cpu->ACC & CONF_MR) {
            /* Подтверждение считывания принятого байта. */
            mpd_receive_update(cpu);
        }
        if (cpu->ACC & CONF_IOM1) {
            /* Запрос к ПВВ. */
            //TODO: ПВВ 2...4
            iom_request(cpu->index);
        }
        break;

    case 0250:
        /* Чтение номера процессора */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Чтение номера процессора\n", cpu->index);
        cpu->ACC = cpu->index;
        break;

    case 051:
        /* Запись в регистр ответов процессорам */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Запись в ОПП\n", cpu->index);
        cpu->OPP = cpu->ACC & (CONF_IOM_MASK | CONF_CPU_MASK | CONF_DATA_MASK);
        if (cpu->ACC & CONF_MT) {
            /* Передача старшей половины байта. */
            mpd_send_nibble(cpu, CONF_GET_DATA(cpu->OPP));
        }
        if (cpu->ACC & CONF_IOM_RESET) {
            /* Сброс ПВВ. */
            iom_reset(cpu->index);
        }
        break;

    case 052:
        /* Гашение регистра прерываний от процессоров */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Гашение ПОП\n", cpu->index);
        /* Оставляем бит передачи МПД. */
        cpu->POP &= cpu->ACC | CONF_MT;
        break;

    case 0252:
        /* Чтение регистра прерываний от процессоров */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Чтение ПОП\n", cpu->index);
        cpu->ACC = cpu->POP;
        break;

    case 053:
        /* Гашение регистра ответов от процессоров */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Гашение ОПОП\n", cpu->index);
        cpu->OPOP &= cpu->ACC;
        break;

    case 0253:
        /* Чтение регистра ответов от процессоров */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Чтение ОПОП\n", cpu->index);
        cpu->ACC = cpu->OPOP;
        break;

    case 054:
        /* Запись в регистр конфигурации процессора */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Установка конфигурации процессора\n", cpu->index);
        cpu->RKP = cpu->ACC & (CONF_IOM_MASK | CONF_CPU_MASK | CONF_MR | CONF_MT);
        break;

    case 0254:
        /* Чтение регистра конфигурации процессора */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Чтение регистра конфигурации процессора\n", cpu->index);
        cpu->ACC = cpu->RKP;
        break;

    case 055:
        /* Запись в регистр аварии процессоров */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Запись в регистр аварии процессоров\n", cpu->index);
        /* игнорируем */
        break;

    case 0255:
        /* Чтение регистра аварии процессоров */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Чтение регистра аварии процессоров\n", cpu->index);
        cpu->ACC = 0;
        break;

    case 056:
        /* Запись в регистр часов */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Установка часов\n", cpu->index);
        //TODO
        break;

    case 0256:
        /* Чтение регистра часов */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Чтение регистра часов\n", cpu->index);
        cpu->ACC = 0; //TODO
        break;

    case 057:
        /* Запись в регистр таймера */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Установка таймера\n", cpu->index);
        //TODO
        break;

    case 0257:
        /* Чтение регистра таймера */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Чтение регистра таймера\n", cpu->index);
        cpu->ACC = 0; //TODO
        break;

    case 060: case 061: case 062: case 063:
    case 064: case 065: case 066: case 067:
        /* Запись в регистры приписки супервизора */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Установка приписки супервизора\n", cpu->index);
        mmu_set_rp(cpu, cpu->Aex & 7, cpu->ACC, 1);
        break;

    case 0100: case 0101: case 0102: case 0103:
    case 0104: case 0105: case 0106: case 0107:
        /*
         * Бит 1: управление блокировкой режима останова БРО.
         * Биты 2 и 3 - признаки формирования контрольных
         * разрядов (ПКП и ПКЛ).
         */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Установка режимов УУ\n", cpu->index);

        if (cpu->Aex & 1) cpu->RUU |= RUU_AVOST_DISABLE;
        else              cpu->RUU &= ~RUU_AVOST_DISABLE;

        if (cpu->Aex & 2) cpu->RUU |= RUU_CHECK_RIGHT;
        else              cpu->RUU &= ~RUU_CHECK_RIGHT;

        if (cpu->Aex & 4) cpu->RUU |= RUU_CHECK_LEFT;
        else              cpu->RUU &= ~RUU_CHECK_LEFT;
        break;

    case 0140:
        /* Сброс контрольных признаков (СКП). */
        if (svs_trace >= TRACE_INSTRUCTIONS)
            fprintf(sim_log, "cpu%d --- Сброс контрольных признаков\n",
                cpu->index);
        //TODO
        break;

    default:
#if 0
        if ((cpu->Aex & 0340) == 0140) {
            /* TODO: watchdog reset mechanism */
            longjmp(cpu->exception, STOP_UNIMPLEMENTED);
        }
#endif
        /* Неиспользуемые адреса */
        svs_debug("--- %05o%s: РЕГ %o - неизвестный спец.регистр",
            cpu->PC, (cpu->RUU & RUU_RIGHT_INSTR) ? "п" : "л", cpu->Aex);
        break;
    }
}

static int is_extracode(int opcode)
{
    switch (opcode) {
    case 050: case 051: case 052: case 053: /* э50...э77 кроме э75 */
    case 054: case 055: case 056: case 057:
    case 060: case 061: case 062: case 063:
    case 064: case 065: case 066: case 067:
    case 070: case 071: case 072: case 073:
    case 074: case 076: case 077:
    case 0200:                              /* э20 */
    case 0210:                              /* э21 */
        return 1;
    }
    return 0;
}

/*
 * Execute one instruction, placed on address PC:RUU_RIGHT_INSTR.
 * When stopped, perform a longjmp to cpu->exception,
 * sending a stop code.
 */
void cpu_one_instr(CORE *cpu)
{
    int reg, opcode, addr, paddr, nextpc, next_mod;
    t_value word;

    /*
     * Instruction execution time in 100 ns ticks; not really used
     * as the amortized 1 MIPS instruction rate is assumed.
     * The assignments of MEAN_TIME(x,y) to the delay variable
     * are kept as a reference.
     */
    uint32 delay;

    cpu->corr_stack = 0;
    word = mmu_fetch(cpu, cpu->PC, &paddr);
    if (cpu->RUU & RUU_RIGHT_INSTR)
        cpu->RK = (uint32)word;         /* get right instruction */
    else
        cpu->RK = (uint32)(word >> 24); /* get left instruction */

    cpu->RK &= BITS(24);

    reg = cpu->RK >> 20;
    if (cpu->RK & BBIT(20)) {
        addr = cpu->RK & BITS(15);
        opcode = (cpu->RK >> 12) & 0370;
    } else {
        addr = cpu->RK & BITS(12);
        if (cpu->RK & BBIT(19))
            addr |= 070000;
        opcode = (cpu->RK >> 12) & 077;
    }

    /* Трассировка команды: адрес, код и мнемоника. */
    if (svs_trace >= TRACE_INSTRUCTIONS ||
        (svs_trace == TRACE_EXTRACODES && is_extracode(opcode))) {
        svs_trace_opcode(cpu, paddr);
    }

    nextpc = ADDR(cpu->PC + 1);
    if (cpu->RUU & RUU_RIGHT_INSTR) {
        cpu->PC += 1;                               /* increment PC */
        cpu->RUU &= ~RUU_RIGHT_INSTR;
    } else {
        cpu->RUU |= RUU_RIGHT_INSTR;
    }

    if (cpu->RUU & RUU_MOD_RK) {
        addr = ADDR(addr + cpu->M[MOD]);
    }
    next_mod = 0;
    delay = 0;

    switch (opcode) {
    case 000:                                       /* зп, atx */
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        mmu_store(cpu, cpu->Aex, cpu->ACC);
        if (! addr && reg == 017)
            cpu->M[017] = ADDR(cpu->M[017] + 1);
        delay = MEAN_TIME(3, 3);
        break;
    case 001:                                       /* зпм, stx */
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        mmu_store(cpu, cpu->Aex, cpu->ACC);
        cpu->M[017] = ADDR(cpu->M[017] - 1);
        cpu->corr_stack = 1;
        cpu->ACC = mmu_load(cpu, cpu->M[017]);
        cpu->RAU = SET_LOGICAL(cpu->RAU);
        delay = MEAN_TIME(6, 6);
        break;
    case 002:                                       /* рег, mod */
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        if (! IS_SUPERVISOR(cpu->RUU))
            longjmp(cpu->exception, STOP_BADCMD);
        cmd_002(cpu);
        /* Режим АУ - логический, если операция была "чтение" */
        if (cpu->Aex & 0200)
            cpu->RAU = SET_LOGICAL(cpu->RAU);
        delay = MEAN_TIME(3, 3);
        break;
    case 003:                                       /* счм, xts */
        mmu_store(cpu, cpu->M[017], cpu->ACC);
        cpu->M[017] = ADDR(cpu->M[017] + 1);
        cpu->corr_stack = -1;
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        cpu->ACC = mmu_load(cpu, cpu->Aex);
        cpu->RAU = SET_LOGICAL(cpu->RAU);
        delay = MEAN_TIME(6, 6);
        break;
    case 004:                                       /* сл, a+x */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        svs_add(cpu, mmu_load(cpu, cpu->Aex), 0, 0);
        cpu->RAU = SET_ADDITIVE(cpu->RAU);
        delay = MEAN_TIME(3, 11);
        break;
    case 005:                                       /* вч, a-x */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        svs_add(cpu, mmu_load(cpu, cpu->Aex), 0, 1);
        cpu->RAU = SET_ADDITIVE(cpu->RAU);
        delay = MEAN_TIME(3, 11);
        break;
    case 006:                                       /* вчоб, x-a */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        svs_add(cpu, mmu_load(cpu, cpu->Aex), 1, 0);
        cpu->RAU = SET_ADDITIVE(cpu->RAU);
        delay = MEAN_TIME(3, 11);
        break;
    case 007:                                       /* вчаб, amx */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        svs_add(cpu, mmu_load(cpu, cpu->Aex), 1, 1);
        cpu->RAU = SET_ADDITIVE(cpu->RAU);
        delay = MEAN_TIME(3, 11);
        break;
    case 010:                                       /* сч, xta */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        cpu->ACC = mmu_load(cpu, cpu->Aex);
        cpu->RAU = SET_LOGICAL(cpu->RAU);
        delay = MEAN_TIME(3, 3);
        break;
    case 011:                                       /* и, aax */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        cpu->ACC &= mmu_load(cpu, cpu->Aex);
        cpu->RMR = 0;
        cpu->RAU = SET_LOGICAL(cpu->RAU);
        delay = MEAN_TIME(3, 4);
        break;
    case 012:                                       /* нтж, aex */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        cpu->RMR = cpu->ACC;
        cpu->ACC ^= mmu_load(cpu, cpu->Aex);
        cpu->RAU = SET_LOGICAL(cpu->RAU);
        delay = MEAN_TIME(3, 3);
        break;
    case 013:                                       /* слц, arx */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        cpu->ACC += mmu_load(cpu, cpu->Aex);
        if (cpu->ACC & BIT49)
            cpu->ACC = (cpu->ACC + 1) & BITS48;
        cpu->RMR = 0;
        cpu->RAU = SET_MULTIPLICATIVE(cpu->RAU);
        delay = MEAN_TIME(3, 6);
        break;
    case 014:                                       /* знак, avx */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        svs_change_sign(cpu, mmu_load(cpu, cpu->Aex) >> 40 & 1);
        cpu->RAU = SET_ADDITIVE(cpu->RAU);
        delay = MEAN_TIME(3, 5);
        break;
    case 015:                                       /* или, aox */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        cpu->ACC |= mmu_load(cpu, cpu->Aex);
        cpu->RMR = 0;
        cpu->RAU = SET_LOGICAL(cpu->RAU);
        delay = MEAN_TIME(3, 4);
        break;
    case 016:                                       /* дел, a/x */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        svs_divide(cpu, mmu_load(cpu, cpu->Aex));
        cpu->RAU = SET_MULTIPLICATIVE(cpu->RAU);
        delay = MEAN_TIME(3, 50);
        break;
    case 017:                                       /* умн, a*x */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        svs_multiply(cpu, mmu_load(cpu, cpu->Aex));
        cpu->RAU = SET_MULTIPLICATIVE(cpu->RAU);
        delay = MEAN_TIME(3, 18);
        break;
    case 020:                                       /* сбр, apx */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        cpu->ACC = svs_pack(cpu->ACC, mmu_load(cpu, cpu->Aex));
        cpu->RMR = 0;
        cpu->RAU = SET_LOGICAL(cpu->RAU);
        delay = MEAN_TIME(3, 53);
        break;
    case 021:                                       /* рзб, aux */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        cpu->ACC = svs_unpack(cpu->ACC, mmu_load(cpu, cpu->Aex));
        cpu->RMR = 0;
        cpu->RAU = SET_LOGICAL(cpu->RAU);
        delay = MEAN_TIME(3, 53);
        break;
    case 022:                                       /* чед, acx */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        cpu->ACC = svs_count_ones(cpu->ACC) + mmu_load(cpu, cpu->Aex);
        if (cpu->ACC & BIT49)
            cpu->ACC = (cpu->ACC + 1) & BITS48;
        cpu->RAU = SET_LOGICAL(cpu->RAU);
        delay = MEAN_TIME(3, 56);
        break;
    case 023:                                       /* нед, anx */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        if (cpu->ACC) {
            int n = svs_highest_bit(cpu->ACC);

            /* "Остаток" сумматора, исключая бит,
             * номер которого определен, помещается в РМР,
             * начиная со старшего бита РМР. */
            svs_shift(cpu, 48 - n);

            /* Циклическое сложение номера со словом по Аисп. */
            cpu->ACC = n + mmu_load(cpu, cpu->Aex);
            if (cpu->ACC & BIT49)
                cpu->ACC = (cpu->ACC + 1) & BITS48;
        } else {
            cpu->RMR = 0;
            cpu->ACC = mmu_load(cpu, cpu->Aex);
        }
        cpu->RAU = SET_LOGICAL(cpu->RAU);
        delay = MEAN_TIME(3, 32);
        break;
    case 024:                                       /* слп, e+x */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        svs_add_exponent(cpu, (mmu_load(cpu, cpu->Aex) >> 41) - 64);
        cpu->RAU = SET_MULTIPLICATIVE(cpu->RAU);
        delay = MEAN_TIME(3, 5);
        break;
    case 025:                                       /* вчп, e-x */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        svs_add_exponent(cpu, 64 - (mmu_load(cpu, cpu->Aex) >> 41));
        cpu->RAU = SET_MULTIPLICATIVE(cpu->RAU);
        delay = MEAN_TIME(3, 5);
        break;
    case 026: {                                     /* сд, asx */
        int n;
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        n = (mmu_load(cpu, cpu->Aex) >> 41) - 64;
        svs_shift(cpu, n);
        cpu->RAU = SET_LOGICAL(cpu->RAU);
        delay = MEAN_TIME(3, 4 + abs(n));
        break;
    }
    case 027:                                       /* рж, xtr */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        cpu->RAU = (mmu_load(cpu, cpu->Aex) >> 41) & 077;
        delay = MEAN_TIME(3, 3);
        break;
    case 030:                                       /* счрж, rte */
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        cpu->ACC = (t_value) (cpu->RAU & cpu->Aex & 0177) << 41;
        cpu->RAU = SET_LOGICAL(cpu->RAU);
        delay = MEAN_TIME(3, 3);
        break;
    case 031:                                       /* счмр, yta */
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        if (IS_LOGICAL(cpu->RAU)) {
            cpu->ACC = cpu->RMR;
        } else {
            t_value x = cpu->RMR;
            cpu->ACC = (cpu->ACC & ~BITS41) | (cpu->RMR & BITS40);
            svs_add_exponent(cpu, (cpu->Aex & 0177) - 64);
            cpu->RMR = x;
        }
        delay = MEAN_TIME(3, 5);
        break;
    case 032:                                       /* зпп, запись полноразрядная */
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        if (! IS_SUPERVISOR(cpu->RUU))
            longjmp(cpu->exception, STOP_BADCMD);
        mmu_store64(cpu, cpu->Aex, (cpu->ACC << 16) |
            ((cpu->RMR >> 32) & BITS(16)));
        delay = MEAN_TIME(3, 8);
        break;
    case 033:                                       /* счп, считывание полноразрядное */
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        if (! IS_SUPERVISOR(cpu->RUU))
            longjmp(cpu->exception, STOP_BADCMD);
//svs_debug("--- счп %05o", cpu->Aex);
        cpu->ACC = mmu_load64(cpu, cpu->Aex, 1);
        cpu->RMR = (cpu->ACC & BITS(16)) << 32;
        cpu->ACC >>= 16;
        delay = MEAN_TIME(3, 8);
        break;
    case 034:                                       /* слпа, e+n */
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        svs_add_exponent(cpu, (cpu->Aex & 0177) - 64);
        cpu->RAU = SET_MULTIPLICATIVE(cpu->RAU);
        delay = MEAN_TIME(3, 5);
        break;
    case 035:                                       /* вчпа, e-n */
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        svs_add_exponent(cpu, 64 - (cpu->Aex & 0177));
        cpu->RAU = SET_MULTIPLICATIVE(cpu->RAU);
        delay = MEAN_TIME(3, 5);
        break;
    case 036: {                                     /* сда, asn */
        int n;
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        n = (cpu->Aex & 0177) - 64;
        svs_shift(cpu, n);
        cpu->RAU = SET_LOGICAL(cpu->RAU);
        delay = MEAN_TIME(3, 4 + abs(n));
        break;
    }
    case 037:                                       /* ржа, ntr */
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        cpu->RAU = cpu->Aex & 077;
        delay = MEAN_TIME(3, 3);
        break;
    case 040:                                       /* уи, ati */
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        if (IS_SUPERVISOR(cpu->RUU)) {
            int reg = cpu->Aex & 037;
            cpu->M[reg] = ADDR(cpu->ACC);
            /* breakpoint/watchpoint regs will match physical
             * or virtual addresses depending on the current
             * mapping mode.
             */
            if ((cpu->M[PSW] & PSW_MMAP_DISABLE) &&
                (reg == IBP || reg == DWP))
                cpu->M[reg] |= BBIT(16);

        } else
            cpu->M[cpu->Aex & 017] = ADDR(cpu->ACC);
        cpu->M[0] = 0;
        delay = MEAN_TIME(14, 3);
        break;
    case 041: {                                     /* уим, sti */
        unsigned rg, ad;

        cpu->Aex = ADDR(addr + cpu->M[reg]);
        rg = cpu->Aex & (IS_SUPERVISOR(cpu->RUU) ? 037 : 017);
        ad = ADDR(cpu->ACC);
        if (rg != 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->ACC = mmu_load(cpu, rg != 017 ? cpu->M[017] : ad);
        cpu->M[rg] = ad;
        if ((cpu->M[PSW] & PSW_MMAP_DISABLE) && (rg == IBP || rg == DWP))
            cpu->M[rg] |= BBIT(16);
        cpu->M[0] = 0;
        cpu->RAU = SET_LOGICAL(cpu->RAU);
        delay = MEAN_TIME(14, 3);
        break;
    }
    case 042:                                       /* счи, ita */
        delay = MEAN_TIME(6, 3);
load_modifier:
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        cpu->ACC = ADDR(cpu->M[cpu->Aex & (IS_SUPERVISOR(cpu->RUU) ? 037 : 017)]);
        cpu->RAU = SET_LOGICAL(cpu->RAU);
        break;
    case 043:                                       /* счим, its */
        mmu_store(cpu, cpu->M[017], cpu->ACC);
        cpu->M[017] = ADDR(cpu->M[017] + 1);
        delay = MEAN_TIME(9, 6);
        goto load_modifier;
    case 044:                                       /* уии, mtj */
        cpu->Aex = addr;
        if (IS_SUPERVISOR(cpu->RUU)) {
transfer_modifier:
            cpu->M[cpu->Aex & 037] = cpu->M[reg];
            if ((cpu->M[PSW] & PSW_MMAP_DISABLE) &&
                ((cpu->Aex & 037) == IBP || (cpu->Aex & 037) == DWP))
                cpu->M[cpu->Aex & 037] |= BBIT(16);

        } else
            cpu->M[cpu->Aex & 017] = cpu->M[reg];
        cpu->M[0] = 0;
        delay = 6;
        break;
    case 045:                                       /* сли, j+m */
        cpu->Aex = addr;
        if ((cpu->Aex & 020) && IS_SUPERVISOR(cpu->RUU))
            goto transfer_modifier;
        cpu->M[cpu->Aex & 017] = ADDR(cpu->M[cpu->Aex & 017] + cpu->M[reg]);
        cpu->M[0] = 0;
        delay = 6;
        break;
    case 046:                                       /* cоп, специальное обращение к памяти */
        cpu->Aex = addr;
        if (! IS_SUPERVISOR(cpu->RUU))
            longjmp(cpu->exception, STOP_BADCMD);
//svs_debug("--- соп %05o", cpu->Aex);
        cpu->ACC = mmu_load64(cpu, cpu->Aex, 0);
        cpu->RMR = (cpu->ACC & BITS(16)) << 32;
        cpu->ACC >>= 16;
        delay = 6;
        break;
    case 047:                                       /* э47, x47 */
        cpu->Aex = addr;
        if (! IS_SUPERVISOR(cpu->RUU))
            longjmp(cpu->exception, STOP_BADCMD);
        cpu->M[cpu->Aex & 017] = ADDR(cpu->M[cpu->Aex & 017] + cpu->Aex);
        cpu->M[0] = 0;
        delay = 6;
        break;
    case 050: case 051: case 052: case 053:
    case 054: case 055: case 056: case 057:
    case 060: case 061: case 062: case 063:
    case 064: case 065: case 066: case 067:
    case 070: case 071: case 072: case 073:
    case 074: case 075: case 076: case 077:         /* э50...э77 */
    case 0200:                                      /* э20 */
    case 0210:                                      /* э21 */
stop_as_extracode:
            cpu->Aex = ADDR(addr + cpu->M[reg]);
            /* Адрес возврата из экстракода. */
            cpu->M[ERET] = nextpc;
            /* Сохранённые режимы УУ. */
            cpu->M[SPSW] = (cpu->M[PSW] & (PSW_INTR_DISABLE | PSW_MMAP_DISABLE |
                                           PSW_PROT_DISABLE)) | IS_SUPERVISOR(cpu->RUU);
            /* Текущие режимы УУ. */
            cpu->M[PSW] = PSW_INTR_DISABLE | PSW_MMAP_DISABLE |
                          PSW_PROT_DISABLE | /*?*/ PSW_INTR_HALT;
            cpu->M[14] = cpu->Aex;
            cpu->RUU = SET_SUPERVISOR(cpu->RUU, SPSW_EXTRACODE);

            if (opcode <= 077)
                cpu->PC = 0500 + opcode;            /* э50-э77 */
            else
                cpu->PC = 0540 + (opcode >> 3);     /* э20, э21 */
            cpu->RUU &= ~RUU_RIGHT_INSTR;
            delay = 7;
            break;
    case 0220:                                      /* мода, utc */
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        next_mod = cpu->Aex;
        delay = 4;
        break;
    case 0230:                                      /* мод, wtc */
        if (! addr && reg == 017) {
            cpu->M[017] = ADDR(cpu->M[017] - 1);
            cpu->corr_stack = 1;
        }
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        next_mod = ADDR(mmu_load(cpu, cpu->Aex));
        delay = MEAN_TIME(13, 3);
        break;
    case 0240:                                      /* уиа, vtm */
        cpu->Aex = addr;
        cpu->M[reg] = addr;
        cpu->M[0] = 0;
        if (IS_SUPERVISOR(cpu->RUU) && reg == 0) {
            cpu->M[PSW] &= ~(PSW_INTR_DISABLE |
                             PSW_MMAP_DISABLE | PSW_PROT_DISABLE);
            cpu->M[PSW] |= addr & (PSW_INTR_DISABLE |
                                   PSW_MMAP_DISABLE | PSW_PROT_DISABLE);
        }
        delay = 4;
        break;
    case 0250:                                      /* слиа, utm */
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        cpu->M[reg] = cpu->Aex;
        cpu->M[0] = 0;
        if (IS_SUPERVISOR(cpu->RUU) && reg == 0) {
            cpu->M[PSW] &= ~(PSW_INTR_DISABLE |
                             PSW_MMAP_DISABLE | PSW_PROT_DISABLE);
            cpu->M[PSW] |= addr & (PSW_INTR_DISABLE |
                                   PSW_MMAP_DISABLE | PSW_PROT_DISABLE);
        }
        delay = 4;
        break;
    case 0260:                                      /* по, uza */
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        cpu->RMR = cpu->ACC;
        delay = MEAN_TIME(12, 3);
        if (IS_ADDITIVE(cpu->RAU)) {
            if (cpu->ACC & BIT41)
                break;
        } else if (IS_MULTIPLICATIVE(cpu->RAU)) {
            if (! (cpu->ACC & BIT48))
                break;
        } else if (IS_LOGICAL(cpu->RAU)) {
            if (cpu->ACC)
                break;
        } else
            break;
        cpu->PC = cpu->Aex;
        cpu->RUU &= ~RUU_RIGHT_INSTR;
        delay += 3;
        break;
    case 0270:                                      /* пе, u1a */
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        cpu->RMR = cpu->ACC;
        delay = MEAN_TIME(12, 3);
        if (IS_ADDITIVE(cpu->RAU)) {
            if (! (cpu->ACC & BIT41))
                break;
        } else if (IS_MULTIPLICATIVE(cpu->RAU)) {
            if (cpu->ACC & BIT48)
                break;
        } else if (IS_LOGICAL(cpu->RAU)) {
            if (! cpu->ACC)
                break;
        } else
            /* fall thru, i.e. branch */;
        cpu->PC = cpu->Aex;
        cpu->RUU &= ~RUU_RIGHT_INSTR;
        delay += 3;
        break;
    case 0300:                                      /* пб, uj */
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        cpu->PC = cpu->Aex;
        cpu->RUU &= ~RUU_RIGHT_INSTR;
        delay = 7;
        break;
    case 0310:                                      /* пв, vjm */
        cpu->Aex = addr;
        cpu->M[reg] = nextpc;
        cpu->M[0] = 0;
        cpu->PC = addr;
        cpu->RUU &= ~RUU_RIGHT_INSTR;
        delay = 7;
        break;
    case 0320:                                      /* выпр, iret */
        cpu->Aex = addr;
        if (! IS_SUPERVISOR(cpu->RUU)) {
            longjmp(cpu->exception, STOP_BADCMD);
        }
        cpu->M[PSW] = (cpu->M[PSW] & PSW_WRITE_WATCH) |
                      (cpu->M[SPSW] & (SPSW_INTR_DISABLE |
                                       SPSW_MMAP_DISABLE | SPSW_PROT_DISABLE));
        cpu->PC = cpu->M[(reg & 3) | 030];
        cpu->RUU &= ~RUU_RIGHT_INSTR;
        if (cpu->M[SPSW] & SPSW_RIGHT_INSTR)
            cpu->RUU |= RUU_RIGHT_INSTR;
        else
            cpu->RUU &= ~RUU_RIGHT_INSTR;
        cpu->RUU = SET_SUPERVISOR(cpu->RUU,
                                  cpu->M[SPSW] & (SPSW_EXTRACODE | SPSW_INTERRUPT));
        if (cpu->M[SPSW] & SPSW_MOD_RK)
            next_mod = cpu->M[MOD];
        /*svs_okno("Выход из прерывания");*/
        delay = 7;
        break;
    case 0330:                                      /* стоп, stop */
        cpu->Aex = ADDR(addr + cpu->M[reg]);
        delay = 7;
        if (! IS_SUPERVISOR(cpu->RUU)) {
            if (cpu->M[PSW] & PSW_CHECK_HALT)
                break;
            else {
                opcode = 063;
                goto stop_as_extracode;
            }
        }
        longjmp(cpu->exception, STOP_STOP);
        break;
    case 0340:                                      /* пио, vzm */
branch_zero:
        cpu->Aex = addr;
        delay = 4;
        if (! cpu->M[reg]) {
            cpu->PC = addr;
            cpu->RUU &= ~RUU_RIGHT_INSTR;
            delay += 3;
        }
        break;
    case 0350:                                      /* пино, v1m */
        cpu->Aex = addr;
        delay = 4;
        if (cpu->M[reg]) {
            cpu->PC = addr;
            cpu->RUU &= ~RUU_RIGHT_INSTR;
            delay += 3;
        }
        break;
    case 0360:                                      /* э36, *36 */
        /* Как ПИО, но с выталкиванием БРЗ. */
        goto branch_zero;
    case 0370:                                      /* цикл, vlm */
        cpu->Aex = addr;
        delay = 4;
        if (! cpu->M[reg])
            break;
        cpu->M[reg] = ADDR(cpu->M[reg] + 1);
        cpu->PC = addr;
        cpu->RUU &= ~RUU_RIGHT_INSTR;
        delay += 3;
        break;
    default:
        /* Unknown instruction - cannot happen. */
        longjmp(cpu->exception, STOP_STOP);
        break;
    }

    if (next_mod) {
        /* Модификация адреса следующей команды. */
        cpu->M[MOD] = next_mod;
        cpu->RUU |= RUU_MOD_RK;
    } else {
        cpu->RUU &= ~RUU_MOD_RK;
    }

    /* Обновляем регистр внешних прерываний РВП. */
    if (cpu->POP & cpu->RKP) {
        /* Есть внешние прерывания. */
        cpu->GRVP |= GRVP_REQUEST;
    } else {
        /* Внешние прерывания отсутствуют. */
        cpu->GRVP &= ~GRVP_REQUEST;
    }

    /* Трассировка изменённых регистров. */
    if (svs_trace == TRACE_ALL) {
        svs_trace_registers(cpu);
    }
#if 0
    //TODO: обнаружение цикла "ЖДУ" диспака
    /* Не находимся ли мы в цикле "ЖДУ" диспака? */
    if (cpu->RUU == 047 && cpu->PC == 04440 && cpu->RK == 067704440) {
        //check_initial_setup();
        sim_idle(0, TRUE);
    }
#endif
}

/*
 * Операция прерывания 1: внутреннее прерывание.
 * Описана в 9-м томе технического описания БЭСМ-6, страница 119.
 */
void op_int_1(CORE *cpu, const char *msg)
{
    /*svs_okno(msg);*/
    cpu->M[SPSW] = (cpu->M[PSW] & (PSW_INTR_DISABLE | PSW_MMAP_DISABLE |
                                   PSW_PROT_DISABLE)) | IS_SUPERVISOR(cpu->RUU);
    if (cpu->RUU & RUU_RIGHT_INSTR)
        cpu->M[SPSW] |= SPSW_RIGHT_INSTR;
    cpu->M[IRET] = cpu->PC;
    cpu->M[PSW] |= PSW_INTR_DISABLE | PSW_MMAP_DISABLE | PSW_PROT_DISABLE;
    if (cpu->RUU & RUU_MOD_RK) {
        cpu->M[SPSW] |= SPSW_MOD_RK;
        cpu->RUU &= ~RUU_MOD_RK;
    }
    cpu->PC = 0500;
    cpu->RUU &= ~RUU_RIGHT_INSTR;
    cpu->RUU = SET_SUPERVISOR(cpu->RUU, SPSW_INTERRUPT);
}

/*
 * Операция прерывания 2: внешнее прерывание.
 * Описана в 9-м томе технического описания БЭСМ-6, страница 129.
 */
void op_int_2(CORE *cpu)
{
    /*svs_okno("Внешнее прерывание");*/
    cpu->M[SPSW] = (cpu->M[PSW] & (PSW_INTR_DISABLE | PSW_MMAP_DISABLE |
                                   PSW_PROT_DISABLE)) | IS_SUPERVISOR(cpu->RUU);
    cpu->M[IRET] = cpu->PC;
    cpu->M[PSW] |= PSW_INTR_DISABLE | PSW_MMAP_DISABLE | PSW_PROT_DISABLE;
    if (cpu->RUU & RUU_MOD_RK) {
        cpu->M[SPSW] |= SPSW_MOD_RK;
        cpu->RUU &= ~RUU_MOD_RK;
    }
    cpu->PC = 0501;
    cpu->RUU &= ~RUU_RIGHT_INSTR;
    cpu->RUU = SET_SUPERVISOR(cpu->RUU, SPSW_INTERRUPT);
}

/*
 * Main instruction fetch/decode loop
 */
t_stat sim_instr(void)
{
    //TODO: выполнение инструкций на процессорах cpu1..9
    CORE *cpu = &cpu_core[0];
    t_stat r;
    int iintr = 0;

    /* Трассировка начального состояния. */
    if (svs_trace == TRACE_ALL) {
        svs_trace_registers(cpu);
    }

    /* Restore register state */
    cpu->PC &= BITS(15);                            /* mask PC */
    mmu_setup(cpu);                                 /* copy RP to TLB */

    /* An internal interrupt or user intervention */
    r = setjmp(cpu->exception);
    if (r) {
        const char *message = (r >= SCPE_BASE) ?
            scp_errors[r - SCPE_BASE] :
            sim_stop_messages[r];

        if (svs_trace >= TRACE_INSTRUCTIONS) {
            fprintf(sim_log, "cpu%d --- %s\n",
                cpu->index, message);
        }
        cpu->M[017] += cpu->corr_stack;

        /*
         * ПоП и ПоК вызывают останов при любом внутреннем прерывании
         * или прерывании по контролю, соответственно.
         * Если произошёл останов по ПоП или ПоК,
         * то продолжение выполнения начнётся с команды, следующей
         * за вызвавшей прерывание. Как если бы кнопка "ТП" (тип
         * перехода) была включена. Подробнее на странице 119 ТО9.
         */
        switch (r) {
        default:
ret:        return r;
        case STOP_RWATCH:
        case STOP_WWATCH:
            /* Step back one insn to reexecute it */
            if (! (cpu->RUU & RUU_RIGHT_INSTR)) {
                --cpu->PC;
            }
            cpu->RUU ^= RUU_RIGHT_INSTR;
            goto ret;
        case STOP_BADCMD:
            if (cpu->M[PSW] & PSW_INTR_HALT)        /* ПоП */
                goto ret;
            op_int_1(cpu, sim_stop_messages[r]);
            // SPSW_NEXT_RK is not important for this interrupt
            cpu->RPR |= RPR_ILL_INSN;
            break;
        case STOP_INSN_CHECK:
            if (cpu->M[PSW] & PSW_CHECK_HALT)       /* ПоК */
                goto ret;
            op_int_1(cpu, sim_stop_messages[r]);
            // SPSW_NEXT_RK must be 0 for this interrupt; it is already
            cpu->RPR |= RPR_INSN_CHECK;
            break;
        case STOP_INSN_PROT:
            if (cpu->M[PSW] & PSW_INTR_HALT)        /* ПоП */
                goto ret;
            if (cpu->RUU & RUU_RIGHT_INSTR) {
                ++cpu->PC;
            }
            cpu->RUU ^= RUU_RIGHT_INSTR;
            op_int_1(cpu, sim_stop_messages[r]);
            // SPSW_NEXT_RK must be 1 for this interrupt
            cpu->M[SPSW] |= SPSW_NEXT_RK;
            cpu->RPR |= RPR_INSN_PROT;
            break;
        case STOP_OPERAND_PROT:
#if 0
/* ДИСПАК держит признак ПоП установленным.
 * При запуске СЕРП возникает обращение к чужому листу. */
            if (cpu->M[PSW] & PSW_INTR_HALT)        /* ПоП */
                goto ret;
#endif
            if (cpu->RUU & RUU_RIGHT_INSTR) {
                ++cpu->PC;
            }
            cpu->RUU ^= RUU_RIGHT_INSTR;
            op_int_1(cpu, sim_stop_messages[r]);
            cpu->M[SPSW] |= SPSW_NEXT_RK;
            // The offending virtual page is in bits 5-9
            cpu->RPR |= RPR_OPRND_PROT;
            cpu->RPR = RPR_SET_PAGE(cpu->RPR, cpu->bad_addr);
            break;
        case STOP_RAM_CHECK:
            if (cpu->M[PSW] & PSW_CHECK_HALT)       /* ПоК */
                goto ret;
            op_int_1(cpu, sim_stop_messages[r]);
            // The offending interleaved block # is in bits 1-3.
            cpu->RPR |= RPR_CHECK | RPR_RAM_CHECK;
            cpu->RPR = RPR_SET_BLOCK(cpu->RPR, cpu->bad_addr);
            break;
        case STOP_CACHE_CHECK:
            if (cpu->M[PSW] & PSW_CHECK_HALT)       /* ПоК */
                goto ret;
            op_int_1(cpu, sim_stop_messages[r]);
            // The offending BRZ # is in bits 1-3.
            cpu->RPR |= RPR_CHECK;
            cpu->RPR &= ~RPR_RAM_CHECK;
            cpu->RPR = RPR_SET_BLOCK(cpu->RPR, cpu->bad_addr);
            break;
        case STOP_INSN_ADDR_MATCH:
            if (cpu->M[PSW] & PSW_INTR_HALT)        /* ПоП */
                goto ret;
            if (cpu->RUU & RUU_RIGHT_INSTR) {
                ++cpu->PC;
            }
            cpu->RUU ^= RUU_RIGHT_INSTR;
            op_int_1(cpu, sim_stop_messages[r]);
            cpu->M[SPSW] |= SPSW_NEXT_RK;
            cpu->RPR |= RPR_BREAKPOINT;
            break;
        case STOP_LOAD_ADDR_MATCH:
            if (cpu->M[PSW] & PSW_INTR_HALT)        /* ПоП */
                goto ret;
            if (cpu->RUU & RUU_RIGHT_INSTR) {
                ++cpu->PC;
            }
            cpu->RUU ^= RUU_RIGHT_INSTR;
            op_int_1(cpu, sim_stop_messages[r]);
            cpu->M[SPSW] |= SPSW_NEXT_RK;
            cpu->RPR |= RPR_WATCHPT_R;
            break;
        case STOP_STORE_ADDR_MATCH:
            if (cpu->M[PSW] & PSW_INTR_HALT)        /* ПоП */
                goto ret;
            if (cpu->RUU & RUU_RIGHT_INSTR) {
                ++cpu->PC;
            }
            cpu->RUU ^= RUU_RIGHT_INSTR;
            op_int_1(cpu, sim_stop_messages[r]);
            cpu->M[SPSW] |= SPSW_NEXT_RK;
            cpu->RPR |= RPR_WATCHPT_W;
            break;
        case STOP_OVFL:
            /* Прерывание по АУ вызывает останов, если БРО=0
             * и установлен ПоП или ПоК.
             * Страница 118 ТО9.*/
            if (! (cpu->RUU & RUU_AVOST_DISABLE) && /* ! БРО */
                ((cpu->M[PSW] & PSW_INTR_HALT) ||   /* ПоП */
                 (cpu->M[PSW] & PSW_CHECK_HALT)))   /* ПоК */
                goto ret;
            op_int_1(cpu, sim_stop_messages[r]);
            cpu->RPR |= RPR_OVERFLOW|RPR_RAM_CHECK;
            break;
        case STOP_DIVZERO:
            if (! (cpu->RUU & RUU_AVOST_DISABLE) && /* ! БРО */
                ((cpu->M[PSW] & PSW_INTR_HALT) ||   /* ПоП */
                 (cpu->M[PSW] & PSW_CHECK_HALT)))   /* ПоК */
                goto ret;
            op_int_1(cpu, sim_stop_messages[r]);
            cpu->RPR |= RPR_DIVZERO|RPR_RAM_CHECK;
            break;
        }
        ++iintr;
    }

    if (iintr > 1) {
        return STOP_DOUBLE_INTR;
    }

    /* Main instruction fetch/decode loop */
    for (;;) {
        if (sim_interval <= 0) {                /* check clock queue */
            r = sim_process_event();
            if (r) {
                return r;
            }
        }

        if (cpu->PC > BITS(15) && IS_SUPERVISOR(cpu->RUU)) {
            /*
             * Runaway instruction execution in supervisor mode
             * warrants attention.
             */
            return STOP_RUNOUT;                 /* stop simulation */
        }

        if ((sim_brk_summ & SWMASK('E')) &&     /* breakpoint? */
            sim_brk_test(cpu->PC, SWMASK('E')) &&
            ! (cpu->RUU & RUU_RIGHT_INSTR)) {
            return STOP_IBKPT;                  /* stop simulation */
        }

        if (! iintr && ! (cpu->RUU & RUU_RIGHT_INSTR) &&
            ! (cpu->M[PSW] & PSW_INTR_DISABLE))
        {
            if (cpu->RPR) {
                /* internal interrupt */
                if (svs_trace >= TRACE_INSTRUCTIONS) {
                    fprintf(sim_log, "cpu%d --- Внутреннее прерывание\n",
                        cpu->index);
                }
                op_int_2(cpu);
            }
            if (cpu->GRVP & cpu->GRM) {
                /* external interrupt */
                if (svs_trace >= TRACE_INSTRUCTIONS) {
                    fprintf(sim_log, "cpu%d --- Внешнее прерывание\n",
                        cpu->index);
                }
                op_int_2(cpu);
            }
        }
        cpu_one_instr(cpu);                     /* one instr */
        iintr = 0;

        sim_interval -= 1;                      /* count down instructions */
    }
}

/*
 * A 250 Hz clock as per the original documentation,
 * and matching the available software binaries.
 * Some installations used 50 Hz with a modified OS
 * for a better user time/system time ratio.
 */
t_stat fast_clk(UNIT *this)
{
    static unsigned counter;
    static unsigned tty_counter;

    if (svs_trace >= TRACE_INSTRUCTIONS) {
        fprintf(sim_log, "---- --- Timer\n");
    }

    ++counter;
    ++tty_counter;

    CORE *cpu;
    for (cpu = &cpu_core[0]; cpu < &cpu_core[NUM_CORES]; cpu++) {
        cpu->GRVP |= GRVP_TIMER;
    }

    /* Baudot TTYs are synchronised to the main timer rather than the
     * serial line clock. Their baud rate is 50.
     */
    if (tty_counter == TICKS_PER_SEC/50) {
        tt_print();
        tty_counter = 0;
    }

    tmr_poll = sim_rtcn_calb(TICKS_PER_SEC, 0);               /* calibrate clock */
    return sim_activate_after(this, 1000000/TICKS_PER_SEC);   /* reactivate unit */
}

UNIT clocks[] = {
    { UDATA(fast_clk, UNIT_IDLE, 0), INSN_PER_TICK },   /* Bit 40 of the RPR, 250 Hz */
};

t_stat clk_reset(DEVICE *dev)
{
    sim_register_clock_unit(&clocks[0]);

    /* Схема автозапуска включается по нереализованной кнопке "МР" */

    if (!sim_is_running) {                              /* RESET (not IORESET)? */
        tmr_poll = sim_rtcn_init(clocks[0].wait, 0);    /* init timer */
        sim_activate(&clocks[0], tmr_poll);             /* activate unit */
    }
    return SCPE_OK;
}

DEVICE clock_dev = {
    "CLK", clocks, NULL, NULL,
    1, 0, 0, 0, 0, 0,
    NULL, NULL, &clk_reset,
    NULL, NULL, NULL, NULL,
    DEV_DEBUG
};
