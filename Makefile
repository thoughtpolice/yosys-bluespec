YOSYS_CONFIG?=yosys-config

CXX=$(shell $(YOSYS_CONFIG) --cxx)
CXXFLAGS=$(shell $(YOSYS_CONFIG) --cxxflags)
LDFLAGS=$(shell $(YOSYS_CONFIG) --ldflags)
LDLIBS=$(shell $(YOSYS_CONFIG) --ldlibs)

PREFIX?=$(shell $(YOSYS_CONFIG) --datdir)/plugins

SRC=bsv.cc
OBJ=bsv.o
SO=bsv.so

FINALDEST=$(DESTDIR)$(PREFIX)

$(SO): $(OBJ)
	$(CXX) $(LDFLAGS) -shared -o $@ $^ $(LDLIBS)
$(OBJ): $(SRC)
	$(CXX) $(CXXFLAGS) -fPIC -c -o $@ $^

install: $(SO)
	mkdir -p $(FINALDEST)
	install -m 0775 $(SO) $(FINALDEST)/$(SO)

uninstall:
	rm -f $(FINALDEST)/$(SO)

clean:
	rm -f *.d *.o *.so *~

.PHONY: clean install uninstall
