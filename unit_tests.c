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

    // Store a test code.
    ElSvsStoreInstruction(cpu, 1, 000, 0300, 000003, 000, 0220, 000000);    // 1: пб 3
    ElSvsStoreInstruction(cpu, 2, 002, 0330, 076543, 000, 0220, 000000);    // 2: стоп '76543'(2) -- fail
    ElSvsStoreInstruction(cpu, 3, 006, 0330, 012345, 000, 0220, 000000);    // 3: стоп '12345'(6) -- pass

    // Run the code, starting at address 1.
    ElSvsSetPC(cpu, 1);
    int status = ElSvsSimulate(cpu);
    ct_assertequal(status, ESS_HALT);

    // Check the PC address.
    ct_assertequal(ElSvsGetPC(cpu), 3u);
}

//
// Run all tests.
//
int main(int argc, char *argv[])
{
    // List of all tests.
    const struct ct_testcase tests[] = {
        ct_maketest(uj),
    };
    const struct ct_testsuite suite = ct_makesuite_setup_teardown(tests, setup, teardown);

    const size_t results = ct_runsuite_withargs(&suite, argc, argv);

    return results != 0;
}
