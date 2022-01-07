#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver

	int res;

	while(1) {
		res = sys_ipc_recv(&nsipcbuf);
		if (res < 0) {
			panic("output():%e\n", res);
		}

		if ((thisenv->env_ipc_from != ns_envid) || 
			(thisenv->env_ipc_value != NSREQ_OUTPUT)) {
			continue;
		}

		res = sys_e1000_send(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
		while (res < 0) {
			if (res != -E_TX_FULL) {
				panic("output(): %e\n", res);
			}
			res = sys_e1000_send(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
		}
	}
}
