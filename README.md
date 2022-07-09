The goal of this effort is to develop an isolated model of SVS processor
for the project of the full scale SVS simulator.

The CPU implementation was borrowed from the SIMH project: https://github.com/besm6/simh

For unit tests, the CinyTest framework is used.

# Build

Compile the CPU sources and the unit tests.

```
$ make
```

Run tests:
```
$ make test
```
Expected output:
```
$ make test
./unit_tests
Test suite 'main' started at 2022-07-08T21:00:28-0700
[ ✓ ] - 'uj' success
[ ✓ ] - 'vtm_vzm_v1m' success
[ ✓ ] - 'jam_utm' success
[ ✓ ] - 'vlm' success
[ ✓ ] - 'utc_wtc' success
[ ✓ ] - 'vjm' success
[ ✓ ] - 'mtj' success
[ ✓ ] - 'xta_uza_u1a' success
[ ✓ ] - 'atx' success
[ ✓ ] - 'ati_ita' success
[ ✓ ] - 'addr0' success
[ ✓ ] - 'aax_aox_aex' success
[ ✓ ] - 'arx' success
[ ✓ ] - 'its' success
[ ✓ ] - 'sti' success
[ ✓ ] - 'xts' success
[ ✓ ] - 'stx' success
[ ✓ ] - 'asn_asx' success
[ ✓ ] - 'acx_anx' success
[ ✓ ] - 'apx_aux' success
[ ✓ ] - 'stack' success
[ ✓ ] - 'ntr_rte' success
[ ✓ ] - 'yta' success
[ ✓ ] - 'ean_esn_eax_esx' success
[ ✓ ] - 'aax_asx_xsa' success
[ ✓ ] - 'amx' success
[ ✓ ] - 'avx' success
[ ✓ ] - 'multiply' success
[ ✓ ] - 'divide' success
Ran 29 tests (0.132 seconds): 29 passed.
[ SUCCESS ] - Ran 29 tests (0.132 seconds): 29 passed.
```

# Trace log

See the trace log in file `test.output`.
For example, for UJ test:
```
cpu0       Write M21 = 02017
cpu0       Write M27 = 02007
cpu0       Write RUU = 044
cpu0       Fetch [00010 0000010] = 35:00 30 00012 00 22 00000
cpu0 00010 0000010 L: 00 30 00012  пб 12
cpu0       Fetch [00012 0000012] = 35:06 33 12345 00 22 00000
cpu0 00012 0000012 L: 06 33 12345  стоп 12345(6)
cpu0 --- Останов
```
