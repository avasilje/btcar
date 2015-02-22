rem Gen ELF ID
@echo E   1   C:\MyWorks\BTCar\sw\btcar-avr\Debug\btcar-avr.elf >eid_list.txt
rem Gen FID list
grep -r "#define LOCAL_FILE_ID" * | ..\tools\dbglog\gen_fids.exe local_fids.h 2>nul 1>fid_list.txt

rem Gen LID list
grep -n -r -A5 "DBG_LOG_D" *.c unit_tests | ..\tools\dbglog\gen_lids.exe fid_list.txt 2>nul 1>lid_list.txt

rem Combine all IDs together
copy eid_list.txt + fid_list.txt + lid_list.txt scd.txt

rem Cleanup temp files
del eid_list.txt fid_list.txt lid_list.txt