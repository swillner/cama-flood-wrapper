.PHONY: all clean

CAMADEFINES = -DUseCDF

CAMAFLAGS = -I /p/system/packages/netcdf-fortran/4.4.2/impi/include -I bin/src -I bin/mod -I bin/lib -L /p/system/packages/netcdf-fortran/4.4.4/intel/serial/lib
#CAMAFLAGS += -O0 -debug full -check all -g -warn all -stand f90 -lnetcdf -lnetcdff -convert little_endian -module bin -traceback -assume byterecl -heap-arrays
CAMAFLAGS += -O3
CAMAFLAGS += $(CAMADEFINES)

LIBRARIES = -lnetcdf -lnetcdff
FORTRANFLAGS = $(FFLAGS) -check none -stand f90 -convert little_endian -module bin -assume byterecl -heap-arrays

OBJECTFILES = $(patsubst mod/%.F,bin/mod/%.o,$(wildcard mod/*.F)) \
	$(patsubst lib/%.F,bin/lib/%.o,$(wildcard lib/*.F)) \
	$(patsubst src/%.F,bin/src/%.o,$(wildcard src/*.F))

.PRECIOUS: bin/%.f90

all: bin/camaflood bin/generate_inpmap

clean:
	@rm -rf bin

bin/%.f90: %.F
	@mkdir -p $(shell dirname $@)
	@gcc -E -P $(CAMADEFINES) $< > $@

bin/src/MAIN_day.o: bin/src/control0.o
bin/src/init_cond.o: bin/src/calc_fldstg.o
bin/src/control0.o: bin/src/init_map.o bin/src/init_cond.o bin/src/init_time.o bin/src/init_topo.o bin/src/init_inputnam.o bin/src/control_tstp.o
bin/src/control_tstp.o: bin/src/control_phy.o bin/src/control_out.o bin/src/control_rest.o
bin/src/control_phy.o: bin/src/calc_damout.o bin/src/calc_fldout.o bin/src/calc_outpre.o bin/src/calc_rivout.o bin/src/calc_rivout_kine.o bin/src/calc_stonxt.o
bin/src/control_out.o: bin/src/create_outbin.o bin/src/create_outcdf.o
bin/mod/mod_diag.o: bin/mod/parkind1.o
bin/%.o: bin/%.f90
	@$(F95) $(CAMAFLAGS) $(FORTRANFLAGS) -c -fopenmp $(FFLAGS) $< -o $@

bin/camaflood: $(OBJECTFILES)
	@$(F95) $(CAMAFLAGS) $(FORTRANFLAGS) -fopenmp $(FFLAGS) $^ $(LIBRARIES) -o $@

test.o: test.cpp
	@$(CXX) $(CXXFLAGS) -c $^ -o $@

bin/test: $(OBJECTFILES) test.o
	@$(CXX) $(CAMAFLAGS) -fopenmp $(FFLAGS) $^ $(LIBRARIES) -o $@ -lifport -lifcore -limf

bin/generate_inpmap: bin/map/generate_inpmat.o
	@$(F95) $(CAMAFLAGS) $(FORTRANFLAGS) -fopenmp $(FFLAGS) $^ $(LIBRARIES) -o $@


# in map/global_15min:
# ../../bin/generate_inpmap 0.5 -180 180 90 -90 StoN
# mv diminfo_tmp.txt inpmat-tmp.bin inpmat-tmp.txt ../..
