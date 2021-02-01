export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig"
export LD_LIBRARY_PATH="/usr/local/lib"
echo $PKG_CONFIG_PATH
# gcc  lib_config.c -o lib_config `pkg-config --cflags libconfig --libs libconfig`
gcc  simulator.c -o simulator  -pthread `pkg-config --cflags libconfig --libs libconfig` && ./simulator