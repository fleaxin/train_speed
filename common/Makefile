param_file=$(SCRIPTS_DIR)/mpp/sample/Makefile.param
include $(param_file)

.PHONY : clean all
all:$(COMM_OBJ)

clean:
	@rm $(COMM_OBJ) -f

%.o:%.c
	@$(CC) $(COMM_INC) $(CFLAGS) -c $< -o $@
