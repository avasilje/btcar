C_SRCS := 
OBJS := 

C_SRCS +=  \
dbg_log.c \
main.c

OBJS +=  \
dbg_log.o \
main.o

OUTPUT_FILE_PATH +=ref_target.elf

./%.o: ./%.c
	@echo Building file: $<
	gcc -x c -c -g2 -Wall -std=gnu99 -save-temps -o "$@" "$<"      
	@echo Finished building: $<
	

all: $(OUTPUT_FILE_PATH)

$(OUTPUT_FILE_PATH): $(OBJS)
	gcc -o $(OUTPUT_FILE_PATH)  $(OBJS)  
clean:
	rm -rf $(OBJS) $(OUTPUT_FILE_PATH) *.s *.i
