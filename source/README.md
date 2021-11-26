# Build
## HDF5
brew install autogen
brew install automake
brew install libtool
git clone https://github.com/HDFGroup/hdf5
cd hdf5
./autogen.sh
./configure --enable-shared=no --disable-hl --enable-threadsafe CFLAGS="-arch x86_64 -arch arm64"
make

Libs are in hdf5/src/.libs
