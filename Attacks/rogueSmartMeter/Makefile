LIBIEC_HOME=libiec61850
LIBPUG_HOME=libpugixml

PROJECT_BINARY_NAME = rogueSmartMeter
PROJECT_SOURCES = rogueSmartMeter.cpp
PROJECT_SOURCES += static_model.c

PROJECT_ICD_FILE = rogueSmartMeter.cid

ICD_GENERATOR = modelGenerator
ICD_GENERATOR_SOURCES = modelGenerator.cpp

include $(LIBIEC_HOME)/make/target_system.mk
include $(LIBIEC_HOME)/make/stack_includes.mk

LIB_PUG_NAME = $(LIBPUG_HOME)/libpugixml.a

all:	$(PROJECT_BINARY_NAME)

include $(LIBIEC_HOME)/make/common_targets.mk

LDLIBS += -lm

CP = cp

CC = g++

all:	$(PROJECT_BINARY_NAME)

libs:	$(LIB_NAME) $(LIB_PUG_NAME)

modelGenerator: $(LIB_PUG_NAME)
	$(CC) $(CFLAGS) -o $(ICD_GENERATOR) $(ICD_GENERATOR_SOURCES) -I$(LIBPUG_HOME) $(LIB_PUG_NAME) $(LDLIBS)

model:	$(PROJECT_ICD_FILE)
	java -jar $(LIBIEC_HOME)/tools/model_generator/genmodel.jar $(PROJECT_ICD_FILE)

$(PROJECT_BINARY_NAME):	$(PROJECT_SOURCES) $(LIB_NAME) $(LIB_SIM_NAME)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(PROJECT_BINARY_NAME) $(PROJECT_SOURCES) $(INCLUDES) $(LIB_NAME) $(LDLIBS)
	mkdir -p vmd-filestore
	$(CP) $(PROJECT_BINARY_NAME) vmd-filestore/IEDSERVER.BIN

$(LIB_PUG_NAME):
	cd $(LIBPUG_HOME); $(MAKE) -f Makefile

clean:
	rm -f $(PROJECT_BINARY_NAME)
	rm -rf vmd-filestore
	rm -rf $(ICD_GENERATOR)
	cd $(LIBPUG_HOME) && $(MAKE) -f Makefile clean; cd ..
	cd $(LIBIEC_HOME) && $(MAKE) -f Makefile clean; cd ..
