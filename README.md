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
For example, for VTM/VZM/V1M test:
```
cpu0       Write M21 = 02017
cpu0       Write M27 = 02007
cpu0       Write RUU = 044
cpu0 00010 0000010 L: 02 24 00000  уиа (2)
cpu0 00010 0000010 R: 02 34 00012  пио 12(2)
cpu0 00012 0000012 L: 02 35 00015  пино 15(2)
cpu0 00012 0000012 R: 02 35 00015  пино 15(2)
cpu0 00013 0000013 L: 02 24 77777  уиа -1(2)
cpu0       Write M2 = 77777
cpu0 00013 0000013 R: 02 34 00015  пио 15(2)
cpu0 00014 0000014 L: 02 34 00015  пио 15(2)
cpu0 00014 0000014 R: 02 35 00016  пино 16(2)
cpu0 00016 0000016 L: 06 33 12345  стоп 12345(6)
cpu0 --- Останов
```
