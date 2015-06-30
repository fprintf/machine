TARGETS := 
CLEAN :=

all: targets

# Include all the subdirs Rules.mk files
d  :=  cbot
include $(d)/Rules.mk

.PHONY: targets clean
targets: $(TARGETS)

clean: 
	$(RM) -f $(CLEAN)
