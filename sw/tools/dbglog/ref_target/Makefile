C_SRCS := 
OBJS := 

OUTPUT_FILE_PATH +=ref_target.elf

BUILDDIR := build

C_SRCS +=  \
     dbg_log.c \
     main.c

OBJS := $(addsuffix .o, $(addprefix $(BUILDDIR)/, $(basename $(C_SRCS))))

SCD := scd.txt

$(BUILDDIR)/%.o: %.c | $(BUILDDIR)
	@echo;  echo --- Building file: $< to $@
	gcc -x c -c -g2 -std=gnu99 -save-temps=obj -o "$@" "$<"      
	@echo Finished building: $<

diag: 
	@echo OUTPUT_FILE_PATH = $(OUTPUT_FILE_PATH)
	@echo BUILDDIR = $(BUILDDIR)
	@echo C_SRCS = $(C_SRCS)
	@echo OBJS   = $(OBJS)

all: $(SCD) $(OUTPUT_FILE_PATH)

scd: $(SCD)

$(SCD): local_fids.h $(C_SRCS)
	@echo;  echo ======= Building SCD file 
#rem Gen ELF ID
	@echo 'E   1   C:\\MyWorks\\BTCar\\sw\\tools\\dbglog\\ref_target\\'$(OUTPUT_FILE_PATH) >eid_list.txt
#Gen FID list
	grep -r "#define LOCAL_FILE_ID" * | ../gen_fids.exe local_fids.h 2>/dev/null 1>fid_list.txt
#Gen LID list
	grep -n -r -A5 "DBG_LOG_D" *.c | ../gen_lids.exe fid_list.txt 2>/dev/null 1>lid_list.txt
#Combine all IDs together
	cat eid_list.txt fid_list.txt  lid_list.txt > scd.txt
#Cleanup temp files
	rm eid_list.txt fid_list.txt lid_list.txt


$(OUTPUT_FILE_PATH): $(OBJS) 
	@echo;  echo ======= Building binary $@ from $(OBJS)
	gcc -o $(OUTPUT_FILE_PATH)  $(OBJS)  
	@echo;  echo ======= Execute reference target
	./$(OUTPUT_FILE_PATH)
	

$(BUILDDIR):
	@echo;  echo --- Create directory "$<"
	mkdir $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)/*
	rm -f $(OUTPUT_FILE_PATH)
	rm -f eid_list.txt fid_list.txt lid_list.txt
	rm -f $(SCD)
