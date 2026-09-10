# Comprehensive dd0fa57 deployment

b6b4c2c merged as dd0fa579c731f657368c5383384d98aa98caab1e.
Mode3 entry preserves middle+outer line feedback; mode4 policy unchanged.
Full sign suite, mode1/2 andmode5 checks plus ARM build passed.
BIN120236 SHA256 CE7C7C71F18E72C03C661D2BBCB785C653B484460ADC4722F4CBB6F2B70E02C9.
HEX SHA256 E3079E28C51823792BF5695B055D543E1B28516679DE4CC46982AE979E7FE417.
Fresh CH340K COM11; programmer57600 PreserveLastPage; session70163 exit0:
```
ERASE OK: 59 firmware pages; calibration page preserved
WRITE OK: 120236 bytes
VERIFY OK: 120236 bytes
GO OK: 0x08000000
```
Reserved audio/calibration pages excluded. No post-GO query, physical test,
push or K210 change. Prior boardb55ab50 superseded.
Rollback: rollback/2026-09-10-before-b6b4c2c.
