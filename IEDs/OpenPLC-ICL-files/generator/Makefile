LIBPUG_HOME=libpugixml

ICD_GENERATORCB = modelGeneratorCB
ICD_GENERATORCB_SOURCES = modelGeneratorCB.cpp

ICD_GENERATORSM = modelGeneratorSM
ICD_GENERATORSM_SOURCES = modelGeneratorSM.cpp

LIB_PUG_NAME = $(LIBPUG_HOME)/libpugixml.a

LDLIBS += -lm

CP = cp

CC = g++

libs:	$(LIB_PUG_NAME)

modelGeneratorCB: $(LIB_PUG_NAME)
	$(CC) $(CFLAGS) -o $(ICD_GENERATORCB) $(ICD_GENERATORCB_SOURCES) -I$(LIBPUG_HOME) $(LIB_PUG_NAME) $(LDLIBS)

modelGeneratorSM: $(LIB_PUG_NAME)
	$(CC) $(CFLAGS) -o $(ICD_GENERATORSM) $(ICD_GENERATORSM_SOURCES) -I$(LIBPUG_HOME) $(LIB_PUG_NAME) $(LDLIBS)

$(LIB_PUG_NAME):
	cd $(LIBPUG_HOME); $(MAKE) -f Makefile

clean:
	rm -rf $(ICD_GENERATORCB)
	rm -rf $(ICD_GENERATORSM)
	cd $(LIBPUG_HOME) && $(MAKE) -f Makefile clean; cd ..
