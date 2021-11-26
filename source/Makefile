HDF5_DIR=../../hdf5

ALL=hdf5_discover1 hdf5_discover2 hdf5_discover3 hdf5_readDataset1

.PHONY: all clean
all: $(ALL)
clean:
	rm -rf $(ALL)

hdf5_%: hdf5_%.c
	clang -DH5_USE_110_API -I$(HDF5_DIR)/src -L$(HDF5_DIR)/src/.libs -lhdf5 -lz -o $@ $@.c
