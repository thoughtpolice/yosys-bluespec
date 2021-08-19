PLUGIN=bluespec
YOSYS_CONFIG?=yosys-config

CXX=$(shell $(YOSYS_CONFIG) --cxx)
CXXFLAGS=$(shell $(YOSYS_CONFIG) --cxxflags)
LDFLAGS=$(shell $(YOSYS_CONFIG) --ldflags)
LDLIBS=$(shell $(YOSYS_CONFIG) --ldlibs)

PREFIX?=$(shell $(YOSYS_CONFIG) --datdir)/plugins

SRC=$(PLUGIN).cc
OBJ=$(PLUGIN).o
SO=$(PLUGIN).so

FINALDEST=$(DESTDIR)$(PREFIX)

ifneq ($(STATIC_BSC_PATH),)
CXXFLAGS+=-DSTATIC_BSC_PATH=\"$(STATIC_BSC_PATH)\"
endif

ifneq ($(STATIC_BSC_LIBDIR),)
CXXFLAGS+=-DSTATIC_BSC_LIBDIR=\"$(STATIC_BSC_LIBDIR)\"
endif

$(SO): $(OBJ)
	$(CXX) $(LDFLAGS) -shared -o $@ $^ $(LDLIBS)
$(OBJ): $(SRC)
	$(CXX) $(CXXFLAGS) -std=c++17 -fPIC -c -o $@ $^

install: $(SO)
	mkdir -p $(FINALDEST)
	install -m 0775 $(SO) $(FINALDEST)/$(SO)

uninstall:
	rm -f $(FINALDEST)/$(SO)

check: $(SO)
	cd t && yosys -s test.yosys

clean:
	rm -f *.d *.o *.so *~

.PHONY: clean install uninstall
