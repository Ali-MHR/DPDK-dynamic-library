# DPDK dynamic library

Tested with DPDK v 1.8.0

Migrate PCAP packet capture part of your application into DPDK Fast Packet Capture by building .so dynamic library as a simaple stand-alone wrapper for utilizing Intel DPDK packet capture feature



# How to build your DPDK .so dynamic file

1. Before building the .so library build the Intel DPDK dynamic library file:

change 'CONFIG_RTE_BUILD_COMBINE_LIBS' in %DPDK-Source-Folder%/config/common_linuxapp to 'y'

export RTE_SDK=%DPDK-Source-Folder%

export RTE_TARGET=x86_64-native-linuxapp-gcc

export RTE_TARGET=x86_64-native-linuxapp-gcc

make install T=x86_64-native-linuxapp-gcc
 
 
2. After building intel_dpdk.so file, use make command to build the .so file


 
# Usage in your application

To use the packet capture function just call capture_dpdk(struct pcap_pkthdr*,char*) in your application instead of pcap 'pcap_loop' function

To be compatible with pcap library the packet receiving function is declared with PCAP packet structure

build your application with pre-build libintel_dpdk.so library 

Load the DPDK driver and bind network interface to the DPDK driver(https://doc.dpdk.org/guides/index.html)

Run your program and enjoy fast packet capture
