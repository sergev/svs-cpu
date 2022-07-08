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

    // Check the registers.
    ct_assertequal(ElSvsGetPC(cpu), 016u);
    ct_assertequal(ElSvsGetM(cpu, 2), 077777u);
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
    };
    const struct ct_testsuite suite = ct_makesuite_setup_teardown(tests, setup, teardown);

    setenv("CINYTEST_SUPPRESS_OUTPUT", "no", 0);
    const size_t results = ct_runsuite_withargs(&suite, argc, argv);

    return results != 0;
}
