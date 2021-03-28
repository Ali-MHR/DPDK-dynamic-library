#include <pcap.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_errno.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_log.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_atomic.h>
#include <rte_version.h>


/* Useful macro for error handling */
#define FATAL_ERROR(fmt, args...)       rte_exit(EXIT_FAILURE, fmt "\n", ##args)

//static int DPDK_pkts_rcv(__attribute__((unused)) void * arg);
static void init_port(int);

void print_stats (void);
int capture_dpdk(struct pcap_pkthdr*,char*);
void *start_dpdk(void *arg);
void free_dpdk(void);

int DPDK_pkts_rcv(void);

#define INTERMEDIATERING_NAME "intermedate_ring"
#define MEMPOOL_ELEM_SZ (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)			// Power of two greater than 1500
#define MEMPOOL_CACHE_SZ 32	// or 0			// Max is 512
#define MBUF_CACHE_SIZE  256
#define PKT_BURST_SZ 32   // The max size of batch of packets retreived when invoking the receive function. Use the RX_QUEUE_SZ for high speed
#define RX_QUEUE_SZ 128	  // The size of rx queue. Max is 4096 and is the one you'll have best performances with. Use lower if you want to use Burst Bulk Alloc.
#define TX_QUEUE_SZ 256			// Unused, you don't tx packets


static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};



/*
// RSS symmetrical 40 Byte seed, according to "Scalable TCP Session Monitoring with Symmetric Receive-side Scaling" (Shinae Woo, KyoungSoo Park from KAIST) 
uint8_t rss_seed [] = {	0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
			0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
			0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
			0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
			0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a
};

// This seed is to load balance only respect source IP, according to me (Martino Trevisan, from nowhere particular)
uint8_t rss_seed_src_ip [] = { 	
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// This seed is to load balance only destination source IP, according to me (Martino Trevisan, from nowhere particular) 
uint8_t rss_seed_dst_ip [] = { 	
			0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


// Struct for devices configuration for const defines see rte_ethdev.h 
static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode = ETH_MQ_RX_RSS,  	// Enable RSS 
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = rss_seed,				// Set the seed,
			.rss_key_len = 40,				// and the seed length.	
			.rss_hf = (ETH_RSS_IPV4_TCP | ETH_RSS_UDP) ,	// Set the mask of protocols RSS will be applied to 
		}	
	}
};

// Struct for configuring each rx queue. These are default values
static const struct rte_eth_rxconf rx_conf = {
	.rx_thresh = {
		.pthresh = 8,   // Ring prefetch threshold
		.hthresh = 8,   // Ring host threshold 
		.wthresh = 4,   // Ring writeback threshold 
	},
	.rx_free_thresh = 32,    // Immediately free RX descriptors
};

// Struct for configuring each tx queue. These are default values
static const struct rte_eth_txconf tx_conf = {
	.tx_thresh = {
		.pthresh = 36,  // Ring prefetch threshold 
		.hthresh = 0,   // Ring host threshold 
		.wthresh = 0,   // Ring writeback threshold 
	},
	.tx_free_thresh = 0,    // Use PMD default values
	.txq_flags = ETH_TXQ_FLAGS_NOOFFLOADS | ETH_TXQ_FLAGS_NOMULTSEGS,  // IMPORTANT for vmxnet3, otherwise it won't work
	.tx_rs_thresh = 0,      // Use PMD default values 
};
*/
