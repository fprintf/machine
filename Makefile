TARGETS := 
CLEAN :=

all: targets

# Include all the subdirs Rules.mk files
d  :=  src
include $(d)/Rules.mk

.PHONY: clean
targets: $(TARGETS)

clean: 
	$(RM) -f $(CLEAN)
