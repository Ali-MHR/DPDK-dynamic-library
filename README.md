# DPDK dynamic library
# Tested with DPDK v 1.8.0

Migrate PCAP packet capture part of your application into DPDK Fast Packet Capture by building .so dynamic library as a simaple stand-alone wrapper for utilizing Intel DPDK packet capture feature



# How to build your DPDK .so dynamic file

change 'CONFIG_RTE_BUILD_COMBINE_LIBS' in %DPDK-Source-Folder%/config/common_linuxapp to 'y'

export RTE_SDK=%DPDK-Source-Folder%

export RTE_TARGET=x86_64-native-linuxapp-gcc

export RTE_TARGET=x86_64-native-linuxapp-gcc

make install T=x86_64-native-linuxapp-gcc
 


 
# Usage in your application

To use the packet capture function just call capture_dpdk(struct pcap_pkthdr*,char*) in your application instead of pcap packet fetching

To be compatible with pcap library the packet receiving function is declared with PCAP packet structure

build your application with intel_dpdk .so library and enjoy  
