#include "DPDK.h"

static struct rte_ring    * intermediate_ring;
uint64_t buffer_size ;
int nb_sys_ports;
static struct rte_mempool * pktmbuf_pool;

void *start_dpdk(void* arg)
{
	int ret,i;
	buffer_size = 65536;
/* Initialize DPDK enviroment with args, then shift argc and argv to get application parameters */

	char** argv = (char**)arg;
	int argc = 27;

	int arg_num = rte_eal_init(argc,argv);
	if (arg_num < 0) FATAL_ERROR("Cannot init EAL\n");

	/* Check if this application can use two cores*/
	ret = rte_lcore_count ();
//	if (ret < 2) FATAL_ERROR("This application needs exactly two (2) cores.");

	/* Get number of ethernet devices */
	nb_sys_ports = rte_eth_dev_count();
	if (nb_sys_ports <= 0) FATAL_ERROR("Cannot find ETH devices: %d ]\n", rte_errno);
	
	/* Create a mempool with per-core cache, initializing every element for be used as mbuf, and allocating on the current NUMA node */
	pktmbuf_pool = rte_mempool_create( "cluster_mem_pool", buffer_size-1, MEMPOOL_ELEM_SZ, MEMPOOL_CACHE_SZ, sizeof(struct rte_pktmbuf_pool_private), rte_pktmbuf_pool_init, NULL, rte_pktmbuf_init, NULL,rte_socket_id(), 0);
	if (pktmbuf_pool == NULL) FATAL_ERROR("Cannot create cluster_mem_pool.Errno: %d ]\n", rte_errno);//, ENOMEM, ENOSPC, E_RTE_NO_TAILQ, E_RTE_NO_CONFIG, E_RTE_SECONDARY, EINVAL, EEXIST  );
	//	rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
	
	/* Creates a new mempool in memory to hold the mbufs. */
//	pktmbuf_pool = rte_pktmbuf_pool_create("cluster_mem_pool", buffer_size-1,MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
//	if (pktmbuf_pool == NULL)
//		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
	
	/* Init intermediate queue data structures: the ring. */
	intermediate_ring = rte_ring_create (INTERMEDIATERING_NAME, buffer_size, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ );
 	if (intermediate_ring == NULL ) FATAL_ERROR("Cannot create ring");

	/* Operations needed for each ethernet device */
	FILE* err_log = fopen("dpdk.err","a");
	fprintf(err_log,"\n\nports: %d\n\n",nb_sys_ports);fflush(err_log);
	for (i=0;i<nb_sys_ports; i++)
	{
		init_port(i);
		rte_eth_stats_reset ( i );
	}


/* Start consumer and producer routine on 2 different cores: consumer launched first... 
	if ( rte_eal_remote_launch ((lcore_function_t *)DPDK_pkts_rcv, NULL, rte_get_next_lcore(-1, 1, 0) ) )
		FATAL_ERROR("Cannot start recv thread\n"); 
	
	ret = rte_eal_wait_lcore(0);
	if (ret < 0)
		FATAL_ERROR("Cannot quit recv thread\n"); 
      
	 return 0;
}

int DPDK_pkts_rcv(void)
{
	int ret,i;*/
	int no_packet=0;
	struct rte_mbuf * pkt;
	struct rte_mbuf * pkts_burst[PKT_BURST_SZ];
	struct timeval t_pack;
	int read_from_port = 0;
	int nb_rx;

	int timeout_count = nb_sys_ports*100;
	/* Infinite loop */
	while(1)
	{
		/* Read a burst for current port at queue 'nb_istance'*/
		nb_rx = rte_eth_rx_burst(read_from_port, 0, pkts_burst, PKT_BURST_SZ);
		
		/* Increasing reading port number in Round-Robin logic */
		read_from_port = (read_from_port + 1) % nb_sys_ports;
		
		if(nb_rx == 0)
		{
			no_packet++;
			if(no_packet == timeout_count)
			{
				usleep(1);
				no_packet = 0;
			}
			continue;
		}
		
		no_packet = 0;
		  
		/* For each received packet. */
		for (i = 0; likely( i < nb_rx ) ; i++)
		{
			/* Retreive packet from burst, increase the counter */
			pkt = pkts_burst[i];

			/* Timestamp the packet */
			ret = gettimeofday(&t_pack, NULL);
			if (ret != 0) FATAL_ERROR("Error: gettimeofday failed. Quitting...\n");


			/* Writing packet timestamping in unused mbuf fields. (wild approach ! ) */
			pkt->tx_offload = t_pack.tv_sec;
			pkt->udata64 =  t_pack.tv_usec;

			/*Enqueieing buffer */
			ret = rte_ring_enqueue (intermediate_ring, pkt);
			if(ret)
			{
				fprintf(err_log,"full ring error\n");fflush(err_log);
				rte_pktmbuf_free(pkt);
			}
		}
	}
		
	return 0;
}



int g=0;
int capture_dpdk(struct pcap_pkthdr* h,char* packet)
{
		struct rte_mbuf * m;
		struct timeval t_pack;

		/* Dequeue packet - Continue polling if no packet available */
		if( unlikely (rte_ring_dequeue(intermediate_ring, (void**)&m) != 0))
		{ 
			return 0;
		}
		rte_prefetch0(rte_pktmbuf_mtod(m, void *));
		/* Read timestamp of the packet */
		t_pack.tv_usec = m->udata64;
		t_pack.tv_sec = m->tx_offload;
		h->ts = t_pack;
		h->caplen = rte_pktmbuf_data_len(m); 
		h->len = rte_pktmbuf_data_len(m);
		memcpy(packet,rte_pktmbuf_mtod(m, u_char* ),h->len);
		rte_pktmbuf_free(m);
		m = NULL;
		
		g++;
		if(g==1000000)
		{
			printf("------   %u  --------\n", rte_ring_count (intermediate_ring) );
			g=0;
		}

		return 1;
}


void free_dpdk(void)
{
/*
	g++;
	if(g==1000000)
	{
		printf("------   %u  --------\n",rte_mempool_count (pktmbuf_pool) );
		g=0;
	}
	rte_pktmbuf_free(m);
	m = NULL;
*/
}



void print_stats (void)
{
	struct rte_eth_stats stat;
	u_int8_t i;
	uint64_t good_pkt = 0, miss_pkt = 0;

	/* Print per port stats */
	for (i = 0; i < nb_sys_ports; i++){	
		rte_eth_stats_get(i, &stat);
		good_pkt += stat.ipackets;
		miss_pkt += stat.imissed;
		printf("\nPORT: %2d Rx: %ld Drp: %ld Tot: %ld Perc: %.3f%%", i, stat.ipackets, stat.imissed, stat.ipackets+stat.imissed, (float)stat.imissed/(stat.ipackets+stat.imissed)*100 );
	}
	printf("\n-------------------------------------------------");
	printf("\nTOT:     Rx: %ld Drp: %ld Tot: %ld Perc: %.3f%%", good_pkt, miss_pkt, good_pkt+miss_pkt, (float)miss_pkt/(good_pkt+miss_pkt)*100 );
	printf("\n");

}


static void init_port(int i) {

		int ret;
		uint8_t rss_key [40];
		struct rte_eth_link link;
		struct rte_eth_dev_info dev_info;
		struct rte_eth_rss_conf rss_conf;


		/* Retreiving and printing device infos */
		rte_eth_dev_info_get(i, &dev_info);
	//	printf("Name:%s\n\tDriver name: %s\n\tMax rx queues: %d\n\tMax tx queues: %d\n", dev_info.pci_dev->driver->name,dev_info.driver_name, dev_info.max_rx_queues, dev_info.max_tx_queues);
		printf("\tPCI Adress: %04d:%02d:%02x:%01d\n", dev_info.pci_dev->addr.domain, dev_info.pci_dev->addr.bus, dev_info.pci_dev->addr.devid, dev_info.pci_dev->addr.function);

		/* Configure device with '1' rx queues and 1 tx queue */
		ret = rte_eth_dev_configure(i, 1, 1, &port_conf);
		if (ret < 0) rte_panic("Error configuring the port\n");

		/* For each RX queue in each NIC */
		/* Configure rx queue j of current device on current NUMA socket. It takes elements from the mempool */
		ret = rte_eth_rx_queue_setup(i, 0, RX_QUEUE_SZ, rte_socket_id(),NULL/* &rx_conf*/, pktmbuf_pool);
		if (ret < 0) FATAL_ERROR("Error configuring receiving queue\n");
		/* Configure mapping [queue] -> [element in stats array] */
// 		ret = rte_eth_dev_set_rx_queue_stats_mapping 	(i, 0, 0);
// 		if (ret < 0) FATAL_ERROR("Error configuring receiving queue stats\n");


		/* Configure tx queue of current device on current NUMA socket. Mandatory configuration even if you want only rx packet */
		ret = rte_eth_tx_queue_setup(i, 0, TX_QUEUE_SZ, rte_socket_id(),NULL/* &tx_conf*/);
		if (ret < 0) FATAL_ERROR("Error configuring transmitting queue. Errno: %d (%d bad arg, %d no mem)\n", -ret, EINVAL ,ENOMEM);

		/* Start device */		
		ret = rte_eth_dev_start(i);
		if (ret < 0) FATAL_ERROR("Cannot start port\n");

		/* Enable receipt in promiscuous mode for an Ethernet device */
		rte_eth_promiscuous_enable(i);

		/* Print link status */
		rte_eth_link_get_nowait(i, &link);
		if (link.link_status) 	printf("\tPort %d Link Up - speed %u Mbps - %s\n", (uint8_t)i, (unsigned)link.link_speed,(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?("full-duplex") : ("half-duplex\n"));
		else			printf("\tPort %d Link Down\n",(uint8_t)i);

		/* Print RSS support, not reliable because a NIC could support rss configuration just in rte_eth_dev_configure whithout supporting rte_eth_dev_rss_hash_conf_get*/
		rss_conf.rss_key = rss_key;
		ret = rte_eth_dev_rss_hash_conf_get (i,&rss_conf);
		if (ret == 0) printf("\tDevice supports RSS\n"); else printf("\tDevice DOES NOT support RSS\n");
		
		/* Print Flow director support *
		struct rte_eth_fdir fdir_conf;
		ret = rte_eth_dev_fdir_get_infos (i, &fdir_conf);
		if (ret == 0) printf("\tDevice supports Flow Director\n"); else printf("\tDevice DOES NOT support Flow Director\n");
		*/

	
}
