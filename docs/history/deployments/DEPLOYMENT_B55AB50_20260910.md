# Comprehensive b55ab50 deployment

249927b merged as b55ab5097ead95f9103a7345cd43b6dbf490d526.
Mode3 observation minimum22%, mode4 stays26%. Sign suite, mode1/2 andmode5
checks plus ARM build passed. BIN120216 SHA256
7A8C542AD4616761784EFE6EA2E0B5FE229DCC27B7813AC71F7D4C8E5D1BA79A.
HEX SHA256 D67DEDD40E61CC88AA7B11A84361F2478442D1534A0E956FC0E46DEB3462C405.
Fresh CH340K COM11; programmer57600 PreserveLastPage; session9606 exit0:
```
ERASE OK: 59 firmware pages; calibration page preserved
WRITE OK: 120216 bytes
VERIFY OK: 120216 bytes
GO OK: 0x08000000
```
Reserved audio/calibration pages excluded. No post-GO query, physical test,
push or K210 change. Previous board020e7dd superseded.
Rollback: rollback/2026-09-10-before-249927b.
