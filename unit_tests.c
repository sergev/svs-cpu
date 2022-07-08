//
// Unit tests for SVS processor.
// Using the CinyTest framework.
// For details, see: https://github.com/drmonkeysee/CinyTest
//
#include <stdlib.h>
#include "cinytest/ciny.h"
#include "el_master_api.h"
#include "el_svs_api.h"

//
// Mock implementation of the physical memory.
//
static ElMasterWord memory[1024*1024];
static ElMasterTag mem_tag[1024*1024];

//
// Read a word with tag from RAM.
// Mock implementation of the physical memory.
//
ElMasterStatus elMasterRamWordRead(
    ElMasterRamAddress address,
    ElMasterTag *pTag,
    ElMasterWord *pWord)
{
    ct_asserttrue(address < 1024*1024);

    *pWord = memory[address];
    *pTag = mem_tag[address];
    return EMS_OK;
}

//
// Write a word with tag to RAM.
// Mock implementation of the physical memory.
//
ElMasterStatus elMasterRamWordWrite(
    ElMasterRamAddress address,
    ElMasterTag tag,
    ElMasterWord word)
{
    ct_asserttrue(address < 1024*1024);

    memory[address] = word;
    mem_tag[address] = tag;
    return EMS_OK;
}

//
// Fail in case of premature exit.
//
void exit(int status)
{
    for (;;)
        ct_assertfail();
}

//
// Setup for every test: instantiate the processor.
//
static void setup(void **pcontext)
{
    *pcontext = ElSvsAllocate(0);
}

//
// Teardown for every test: deallocate the processor.
//
static void teardown(void **pcontext)
{
    free(*pcontext);
    *pcontext = NULL;
}

//
// Test: UJ instruction (ПБ).
//
static void uj(void *context)
{
    struct ElSvsProcessor *cpu = context;

    // Store the test code.
    //
    //  start   старт   '10'
    //          пб      pass
    //  fail    стоп    '76543'(2)
    //  pass    стоп    '12345'(6)
    //
    ElSvsStoreInstruction(cpu, 010, ElSvsAsm("00 30 00012, 00 22 00000"));
    ElSvsStoreInstruction(cpu, 011, ElSvsAsm("02 33 76543, 00 22 00000"));
    ElSvsStoreInstruction(cpu, 012, ElSvsAsm("06 33 12345, 00 22 00000"));

    // Run the code.
    ElSvsSetPC(cpu, 010);
    int status = ElSvsSimulate(cpu);
    ct_assertequal(status, ESS_HALT);

    // Check the PC address.
    ct_assertequal(ElSvsGetPC(cpu), 012u);
}

//
// Test: VTM, VZM, V1M instructions (УИА, ПИО, ПИНО).
//
static void vtm_vzm_v1m(void *context)
{
    struct ElSvsProcessor *cpu = context;

    // Store the test code.
    //
    //  start   старт   '10'
    //          уиа     0(2)
    //          пио     ok(2)
    //          пб      fail
    //  ok      пино    fail(2)
    //          пино    fail(2)
    //          уиа     -1(2)
    //          пио     fail(2)
    //          пио     fail(2)
    //          пино    pass(2)
    //  fail    стоп    '76543'(2)
    //  pass    стоп    '12345'(6)
    //
    ElSvsStoreInstruction(cpu, 010, ElSvsAsm("уиа (2), пио 12(2)"));
    ElSvsStoreInstruction(cpu, 011, ElSvsAsm("пб 15, мода"));
    ElSvsStoreInstruction(cpu, 012, ElSvsAsm("пино 15(2), пино 15(2)"));
    ElSvsStoreInstruction(cpu, 013, ElSvsAsm("уиа -1(2), пио 15(2)"));
    ElSvsStoreInstruction(cpu, 014, ElSvsAsm("пио 15(2), пино 16(2)"));
    ElSvsStoreInstruction(cpu, 015, ElSvsAsm("стоп 76543(2), мода")); // Magic opcode: Fail
    ElSvsStoreInstruction(cpu, 016, ElSvsAsm("стоп 12345(6), мода")); // Magic opcode: Pass

    // Run the code.
    ElSvsSetPC(cpu, 010);
    int status = ElSvsSimulate(cpu);
    ct_assertequal(status, ESS_HALT);

    // Check registers.
    ct_assertequal(ElSvsGetPC(cpu), 016u);
    ct_assertequal(ElSvsGetM(cpu, 2), 077777u);
}

//
// Test: J+M, UTM instructions (СЛИ, СЛИА).
//
static void jam_utm(void *context)
{
    struct ElSvsProcessor *cpu = context;

    // Store the test code.
    ElSvsStoreInstruction(cpu, 010, ElSvsAsm("уиа 1(2), уиа -17(3)"));
    ElSvsStoreInstruction(cpu, 011, ElSvsAsm("пб 13, мода"));
    ElSvsStoreInstruction(cpu, 012, ElSvsAsm("сли 2(2), слиа 1(3)"));
    ElSvsStoreInstruction(cpu, 013, ElSvsAsm("пино 12(2), пино 15(3)"));
    ElSvsStoreInstruction(cpu, 014, ElSvsAsm("стоп 12345(6), мода")); // Magic opcode: Pass
    ElSvsStoreInstruction(cpu, 015, ElSvsAsm("стоп 76543(2), мода")); // Magic opcode: Fail

    // Run the code.
    ElSvsSetPC(cpu, 010);
    int status = ElSvsSimulate(cpu);
    ct_assertequal(status, ESS_HALT);

    // Check registers.
    ct_assertequal(ElSvsGetPC(cpu), 014u);
    ct_assertequal(ElSvsGetM(cpu, 2), 0u);
    ct_assertequal(ElSvsGetM(cpu, 3), 0u);
}

//
// Test: VLM instruction (ЦИКЛ).
//
static void vlm(void *context)
{
    struct ElSvsProcessor *cpu = context;

    // Store the test code.
    ElSvsStoreInstruction(cpu, 0010, ElSvsAsm("уиа -11(2), уиа -12(3)"));
    ElSvsStoreInstruction(cpu, 0011, ElSvsAsm("слиа 1(3), цикл 11(2)"));
    ElSvsStoreInstruction(cpu, 0012, ElSvsAsm("пино 105(2), пино 105(3)"));
    ElSvsStoreInstruction(cpu, 0013, ElSvsAsm("цикл 105(2), пино 105(2)"));
    ElSvsStoreInstruction(cpu, 0014, ElSvsAsm("сч 2000, уиа 77401(16)"));
    ElSvsStoreInstruction(cpu, 0015, ElSvsAsm("зп 2400(16), цикл 15(16)"));
    ElSvsStoreInstruction(cpu, 0016, ElSvsAsm("сч, уиа 77401(17)"));
    ElSvsStoreInstruction(cpu, 0017, ElSvsAsm("слц 2400(17), цикл 17(17)"));
    ElSvsStoreInstruction(cpu, 0020, ElSvsAsm("нтж, по 105"));
    ElSvsStoreInstruction(cpu, 0021, ElSvsAsm("нтж 2000, пе 105"));
    ElSvsStoreInstruction(cpu, 0022, ElSvsAsm("уиа 77401(16), уиа 77401(15)"));
    ElSvsStoreInstruction(cpu, 0023, ElSvsAsm("слц 2400(16), цикл 23(16)"));
    ElSvsStoreInstruction(cpu, 0024, ElSvsAsm("нтж, по 105"));
    ElSvsStoreInstruction(cpu, 0025, ElSvsAsm("нтж 2000, пе 105"));
    ElSvsStoreInstruction(cpu, 0026, ElSvsAsm("слц 2400(15), цикл 26(15)"));
    ElSvsStoreInstruction(cpu, 0027, ElSvsAsm("нтж, по 105"));
    ElSvsStoreInstruction(cpu, 0030, ElSvsAsm("нтж 2000, пе 105"));
    ElSvsStoreInstruction(cpu, 0031, ElSvsAsm("уиа 77401(14), уиа 77401(13)"));
    ElSvsStoreInstruction(cpu, 0032, ElSvsAsm("слц 2400(14), цикл 32(14)"));
    ElSvsStoreInstruction(cpu, 0033, ElSvsAsm("нтж, по 105"));
    ElSvsStoreInstruction(cpu, 0034, ElSvsAsm("нтж 2000, пе 105"));
    ElSvsStoreInstruction(cpu, 0035, ElSvsAsm("слц 2400(13), цикл 35(13)"));
    ElSvsStoreInstruction(cpu, 0036, ElSvsAsm("нтж, по 105"));
    ElSvsStoreInstruction(cpu, 0037, ElSvsAsm("нтж 2000, пе 105"));
    ElSvsStoreInstruction(cpu, 0040, ElSvsAsm("уиа 77401(12), уиа 77401(11)"));
    ElSvsStoreInstruction(cpu, 0041, ElSvsAsm("слц 2400(12), цикл 41(12)"));
    ElSvsStoreInstruction(cpu, 0042, ElSvsAsm("нтж, по 105"));
    ElSvsStoreInstruction(cpu, 0043, ElSvsAsm("нтж 2000, пе 105"));
    ElSvsStoreInstruction(cpu, 0044, ElSvsAsm("слц 2400(11), цикл 44(11)"));
    ElSvsStoreInstruction(cpu, 0045, ElSvsAsm("нтж, по 105"));
    ElSvsStoreInstruction(cpu, 0046, ElSvsAsm("нтж 2000, пе 105"));
    ElSvsStoreInstruction(cpu, 0047, ElSvsAsm("уиа 77401(10), уиа 77401(7)"));
    ElSvsStoreInstruction(cpu, 0050, ElSvsAsm("слц 2400(10), цикл 50(10)"));
    ElSvsStoreInstruction(cpu, 0051, ElSvsAsm("нтж, по 105"));
    ElSvsStoreInstruction(cpu, 0052, ElSvsAsm("нтж 2000, пе 105"));
    ElSvsStoreInstruction(cpu, 0053, ElSvsAsm("слц 2400(7), цикл 53(7)"));
    ElSvsStoreInstruction(cpu, 0054, ElSvsAsm("нтж, по 105"));
    ElSvsStoreInstruction(cpu, 0055, ElSvsAsm("нтж 2000, пе 105"));
    ElSvsStoreInstruction(cpu, 0056, ElSvsAsm("уиа 77401(6), уиа 77401(5)"));
    ElSvsStoreInstruction(cpu, 0057, ElSvsAsm("слц 2400(6), цикл 57(6)"));
    ElSvsStoreInstruction(cpu, 0060, ElSvsAsm("нтж, по 105"));
    ElSvsStoreInstruction(cpu, 0061, ElSvsAsm("нтж 2000, пе 105"));
    ElSvsStoreInstruction(cpu, 0062, ElSvsAsm("слц 2400(5), цикл 62(5)"));
    ElSvsStoreInstruction(cpu, 0063, ElSvsAsm("нтж, по 105"));
    ElSvsStoreInstruction(cpu, 0064, ElSvsAsm("нтж 2000, пе 105"));
    ElSvsStoreInstruction(cpu, 0065, ElSvsAsm("уиа 77401(4), уиа 77401(3)"));
    ElSvsStoreInstruction(cpu, 0066, ElSvsAsm("слц 2400(4), цикл 66(4)"));
    ElSvsStoreInstruction(cpu, 0067, ElSvsAsm("нтж, по 105"));
    ElSvsStoreInstruction(cpu, 0070, ElSvsAsm("нтж 2000, пе 105"));
    ElSvsStoreInstruction(cpu, 0071, ElSvsAsm("слц 2400(3), цикл 71(3)"));
    ElSvsStoreInstruction(cpu, 0072, ElSvsAsm("нтж, по 105"));
    ElSvsStoreInstruction(cpu, 0073, ElSvsAsm("нтж 2000, пе 105"));
    ElSvsStoreInstruction(cpu, 0074, ElSvsAsm("уиа 77401(2), мода"));
    ElSvsStoreInstruction(cpu, 0075, ElSvsAsm("слц 2400(2), цикл 75(2)"));
    ElSvsStoreInstruction(cpu, 0076, ElSvsAsm("нтж, по 105"));
    ElSvsStoreInstruction(cpu, 0077, ElSvsAsm("нтж 2000, пе 105"));
    ElSvsStoreInstruction(cpu, 0100, ElSvsAsm("уиа 77401(1), мода"));
    ElSvsStoreInstruction(cpu, 0101, ElSvsAsm("слц 2400(1), цикл 101(1)"));
    ElSvsStoreInstruction(cpu, 0102, ElSvsAsm("нтж, по 105"));
    ElSvsStoreInstruction(cpu, 0103, ElSvsAsm("нтж 2000, пе 105"));
    ElSvsStoreInstruction(cpu, 0104, ElSvsAsm("стоп 12345(6), мода")); // Magic opcode: Pass
    ElSvsStoreInstruction(cpu, 0105, ElSvsAsm("стоп 76543(2), мода")); // Magic opcode: Fail
    ElSvsStoreData(cpu, 02000, 07777777777777777ul);

    // Run the code.
    ElSvsSetPC(cpu, 010);
    int status = ElSvsSimulate(cpu);
    ct_assertequal(status, ESS_HALT);

    // Check registers.
    ct_assertequal(ElSvsGetPC(cpu), 0104u);
    ct_assertequal(ElSvsGetM(cpu, 1), 0u);
    ct_assertequal(ElSvsGetM(cpu, 2), 0u);
    ct_assertequal(ElSvsGetM(cpu, 3), 0u);
    ct_assertequal(ElSvsGetM(cpu, 4), 0u);
    ct_assertequal(ElSvsGetM(cpu, 5), 0u);
    ct_assertequal(ElSvsGetM(cpu, 6), 0u);
    ct_assertequal(ElSvsGetM(cpu, 7), 0u);
    ct_assertequal(ElSvsGetM(cpu, 8), 0u);
    ct_assertequal(ElSvsGetM(cpu, 9), 0u);
    ct_assertequal(ElSvsGetM(cpu, 10), 0u);
    ct_assertequal(ElSvsGetM(cpu, 11), 0u);
    ct_assertequal(ElSvsGetM(cpu, 12), 0u);
    ct_assertequal(ElSvsGetM(cpu, 13), 0u);
    ct_assertequal(ElSvsGetM(cpu, 14), 0u);
    ct_assertequal(ElSvsGetM(cpu, 15), 0u);
}

//
// Test: UTC, WTC instructions (МОДА, МОД).
//
static void utc_wtc(void *context)
{
    struct ElSvsProcessor *cpu = context;

    // Store the test code.
    ElSvsStoreInstruction(cpu, 010, ElSvsAsm("мода -1, уиа (3)"));
    ElSvsStoreInstruction(cpu, 011, ElSvsAsm("пио 40(3), слиа 1(3)"));
    ElSvsStoreInstruction(cpu, 012, ElSvsAsm("пино 40(3), мода"));
    ElSvsStoreInstruction(cpu, 013, ElSvsAsm("мода -1, мода"));
    ElSvsStoreInstruction(cpu, 014, ElSvsAsm("уиа (3), пио 40(3)"));
    ElSvsStoreInstruction(cpu, 015, ElSvsAsm("слиа 1(3), пино 40(3)"));
    ElSvsStoreInstruction(cpu, 016, ElSvsAsm("мод 2000, уиа (3)"));
    ElSvsStoreInstruction(cpu, 017, ElSvsAsm("пио 40(3), слиа 1(3)"));
    ElSvsStoreInstruction(cpu, 020, ElSvsAsm("пино 40(3), мод 2000"));
    ElSvsStoreInstruction(cpu, 021, ElSvsAsm("уиа (3), пио 40(3)"));
    ElSvsStoreInstruction(cpu, 022, ElSvsAsm("слиа 1(3), пино 40(3)"));
    ElSvsStoreInstruction(cpu, 023, ElSvsAsm("мода -7, мода 10"));
    ElSvsStoreInstruction(cpu, 024, ElSvsAsm("уиа -2(3), пио 40(3)"));
    ElSvsStoreInstruction(cpu, 025, ElSvsAsm("слиа 1(3), пино 40(3)"));
    ElSvsStoreInstruction(cpu, 026, ElSvsAsm("мод 2000, мода 10"));
    ElSvsStoreInstruction(cpu, 027, ElSvsAsm("уиа -6(3), слиа -1(3)"));
    ElSvsStoreInstruction(cpu, 030, ElSvsAsm("пино 40(3), уиа -1(3)"));
    ElSvsStoreInstruction(cpu, 031, ElSvsAsm("мод 2002(3), уиа (4)"));
    ElSvsStoreInstruction(cpu, 032, ElSvsAsm("уии 5(4), слиа 52526(5)"));
    ElSvsStoreInstruction(cpu, 033, ElSvsAsm("пино 40(5), слиа 1(3)"));
    ElSvsStoreInstruction(cpu, 034, ElSvsAsm("мод 2002(3), уиа (4)"));
    ElSvsStoreInstruction(cpu, 035, ElSvsAsm("уии 5(4), слиа 25253(5)"));
    ElSvsStoreInstruction(cpu, 036, ElSvsAsm("пино 40(5), мода"));
    ElSvsStoreInstruction(cpu, 037, ElSvsAsm("стоп 12345(6), мода")); // Magic opcode: Pass
    ElSvsStoreInstruction(cpu, 040, ElSvsAsm("стоп 76543(2), мода")); // Magic opcode: Fail
    ElSvsStoreData(cpu, 02000, 00000000000077777ul);
    ElSvsStoreData(cpu, 02001, 05252525252525252ul);
    ElSvsStoreData(cpu, 02002, 02525252525252525ul);

    // Run the code.
    ElSvsSetPC(cpu, 010);
    int status = ElSvsSimulate(cpu);
    ct_assertequal(status, ESS_HALT);

    // Check registers.
    ct_assertequal(ElSvsGetPC(cpu), 037u);
    ct_assertequal(ElSvsGetM(cpu, 3), 0u);
    ct_assertequal(ElSvsGetM(cpu, 4), 052525u);
    ct_assertequal(ElSvsGetM(cpu, 5), 0u);
}

static void vjm(void *context)
{
}
static void mtj(void *context)
{
}
static void xta_uza_u1a(void *context)
{
}
static void atx(void *context)
{
}
static void ati_ita(void *context)
{
}
static void addr0(void *context)
{
}
static void aax_aox_aex(void *context)
{
}
static void arx(void *context)
{
}
static void its(void *context)
{
}
static void sti(void *context)
{
}
static void xts(void *context)
{
}
static void stx(void *context)
{
}
static void asn_asx(void *context)
{
}
static void acx_anx(void *context)
{
}
static void apx_aux(void *context)
{
}
static void stack(void *context)
{
}
static void ntr_rte(void *context)
{
}
static void yta(void *context)
{
}
static void ean_esn_eax_esx(void *context)
{
}
static void aax_asx_xsa(void *context)
{
}
static void amx(void *context)
{
}
static void avx(void *context)
{
}
static void multiply(void *context)
{
}
static void divide(void *context)
{
}

//
// Run all tests.
//
int main(int argc, char *argv[])
{
    // List of all tests.
    const struct ct_testcase tests[] = {
        ct_maketest(uj),
        ct_maketest(vtm_vzm_v1m),
        ct_maketest(jam_utm),
        ct_maketest(vlm),
        ct_maketest(utc_wtc),
        ct_maketest(vjm),
        ct_maketest(mtj),
        ct_maketest(xta_uza_u1a),
        ct_maketest(atx),
        ct_maketest(ati_ita),
        ct_maketest(addr0),
        ct_maketest(aax_aox_aex),
        ct_maketest(arx),
        ct_maketest(its),
        ct_maketest(sti),
        ct_maketest(xts),
        ct_maketest(stx),
        ct_maketest(asn_asx),
        ct_maketest(acx_anx),
        ct_maketest(apx_aux),
        ct_maketest(stack),
        ct_maketest(ntr_rte),
        ct_maketest(yta),
        ct_maketest(ean_esn_eax_esx),
        ct_maketest(aax_asx_xsa),
        ct_maketest(amx),
        ct_maketest(avx),
        ct_maketest(multiply),
        ct_maketest(divide),
    };
    const struct ct_testsuite suite = ct_makesuite_setup_teardown(tests, setup, teardown);

    setenv("CINYTEST_SUPPRESS_OUTPUT", "no", 0);
    const size_t results = ct_runsuite_withargs(&suite, argc, argv);

    return results != 0;
}
