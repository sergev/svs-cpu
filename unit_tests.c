#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
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
    if (address >= 1024*1024)
        fail();

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
    if (address >= 1024*1024)
        fail();

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
        fail();
}

//
// UJ instruction (ПБ).
//
static void uj(void **unused)
{
    // Instantiate the processor.
    struct ElSvsProcessor *cpu = ElSvsAllocate(0);

    // Store a test code.
    ElSvsStoreInstruction(cpu, 1, 000, 0300, 000003, 000, 0220, 000000);    // 1: пб 3
    ElSvsStoreInstruction(cpu, 2, 002, 0330, 076543, 000, 0220, 000000);    // 2: стоп '76543'(2) -- fail
    ElSvsStoreInstruction(cpu, 3, 006, 0330, 012345, 000, 0220, 000000);    // 3: стоп '12345'(6) -- pass

    // Run the code, starting at address 1.
    ElSvsSetPC(cpu, 1);
    ElSvsStatus status = ElSvsSimulate(cpu);
    assert_int_equal(status, ESS_HALT);

    // Check the PC address.
    assert_int_equal(ElSvsGetPC(cpu), 3);
    free((void*)cpu);
}

//
// Run all tests.
//
int main()
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(uj),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
