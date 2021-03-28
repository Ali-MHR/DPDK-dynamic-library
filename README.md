# DPDK dynamic library
# Tested with DPDK v 1.8.0

Build Fast Packet Capture .so dynamic library for usage as a simaple stand alone wrapper for utilizing Intel DPDK packet capture feature


change 'CONFIG_RTE_BUILD_COMBINE_LIBS' in %DPDK-Source-Folder%/config/common_linuxapp to 'y'
export RTE_SDK=%DPDK-Source-Folder%
export RTE_TARGET=x86_64-native-linuxapp-gcc
make install T=x86_64-native-linuxapp-gcc
 
