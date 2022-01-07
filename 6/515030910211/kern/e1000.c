#include <kern/e1000.h>
#include <inc/error.h>
#include <inc/assert.h>

// LAB 6: Your driver code here

/* tx queue & buf */
/* 16 aligned */
struct e1000_tx_desc tx_descs[MAXTXDESC] __attribute__((aligned(16)));
char tx_buf[MAXTXDESC][TX_PACK_SZ];

/* rx queue & buf */
struct e1000_rx_desc rx_descs[MAXRXDESC] __attribute__((aligned(16)));
char rx_buf[MAXRXDESC][RX_PACK_SZ];

int E1000_attach(struct pci_func *f) {
	pci_func_enable(f);
	boot_map_region_pci(kern_pgdir, E1000_STACKBASE, f->reg_size[0], f->reg_base[0], PTE_W | PTE_PCD | PTE_PWT);
	E1000 = (uint32_t *)E1000_STACKBASE;
	//cprintf("E1000 STATUS = %08x\n", E1000[E1000_STATUS >> 2]);

	/* init */
	memset(tx_descs, 0, sizeof(tx_descs));
	memset(tx_buf, 0, sizeof(tx_buf));

	/* set phy addr */
	int i;
	for (i = 0; i < MAXTXDESC; i++) {
		tx_descs[i].buffer_addr = PADDR(tx_buf[i]);
		tx_descs[i].upper.data |= E1000_TXD_STAT_DD;
	}

	/* 
	 * Program the Transmit Descriptor Base Address 
	 * (TDBAL/TDBAH) register(s) with the address of the region. 
	 * TDBAL is used for 32-bit addresses 
     * and both TDBAL and TDBAH are used for 64-bit addresses. 
     */
	E1000[E1000_TDBAL >> 2] = PADDR(tx_descs);
	E1000[E1000_TDBAH >> 2] = 0;

	/*
	 * Set the Transmit Descriptor Length (TDLEN) register 
	 * to the size (in bytes) of the descriptor ring. 
	 * This register must be 128-byte aligned.
	 */
	E1000[E1000_TDLEN >> 2] = sizeof(tx_descs);


	/*
	 * The Transmit Descriptor Head 
	 * and Tail (TDH/TDT) registers are 
	 * initialized (by hardware) to 0b 
	 * after a power-on or a software initiated Ethernet 
	 * controller reset. Software should write 0b to both 
	 * these registers to ensure this.
	 */
	E1000[E1000_TDH >> 2] = 0;
	E1000[E1000_TDT >> 2] = 0;

	/*
	 * Initialize the Transmit Control Register (TCTL) for desired operation to include the following
	 * -Set the Enable (TCTL.EN) bit to 1b for normal operation.
	 *
	 * -Set the Pad Short Packets (TCTL.PSP) bit to 1b.
	 *
	 * -Configure the Collision Threshold (TCTL.CT) to the desired value. Ethernet standard is 10h. 
	 *	This setting only has meaning in half duplex mode.
	 *
	 * -Configure the Collision Distance (TCTL.COLD) to its expected value. 
	 *  For full duplex operation, this value should be set to 40h. 
	 *  For gigabit half duplex, this value should be set to 200h. 
	 *  For 10/100 half duplex, this value should be set to 40h.
	 */

	E1000[E1000_TCTL >> 2] |= E1000_TCTL_EN;
	E1000[E1000_TCTL >> 2] |= E1000_TCTL_PSP;
	E1000[E1000_TCTL >> 2] &= ~E1000_TCTL_CT;
	E1000[E1000_TCTL >> 2] |= (0x10) << 4;
	E1000[E1000_TCTL >> 2] &= ~E1000_TCTL_COLD;
	E1000[E1000_TCTL >> 2] |= (0x40) << 12;

	/* 
	 * From 13.4.34
	 * 10 for IPGT
	 * IPGR1 should be 2/3 of IPGR2
	 * 6 for IPGR2
	 * IPGT 9:0 | IPGR1 19:10 | IPGR2 29:20
	 */
	E1000[E1000_TIPG >> 2] = 10 | (4 << 10) | (6 << 20);

	/* rx */
	memset(rx_descs, 0, sizeof(rx_descs));
	memset(rx_buf, 0, sizeof(rx_buf));
	for(i = 0; i < MAXRXDESC; i++) {
		rx_descs[i].buffer_addr = PADDR(rx_buf[i]);
	}

	E1000[E1000_RAL0  >> 2] = 0x12005452;
	E1000[E1000_RAH0  >> 2] = (0x00005634 | E1000_RAH_AV);

	E1000[E1000_RDBAL >> 2] = PADDR(rx_descs);
	E1000[E1000_RDBAH >> 2] = 0;
	E1000[E1000_RDLEN >> 2] = sizeof(rx_descs);
	E1000[E1000_RDH >> 2] = 1;
	E1000[E1000_RDT >> 2] = 0;

	E1000[E1000_RCTL >> 2] = E1000_RCTL_EN;
	E1000[E1000_RCTL >> 2] &= ~E1000_RCTL_LPE;
	E1000[E1000_RCTL >> 2] &= ~E1000_RCTL_LBM_MASK;
	E1000[E1000_RCTL >> 2] &= ~E1000_RCTL_RDMTS_MASK;
	E1000[E1000_RCTL >> 2] &= ~E1000_RCTL_MO_MASK;
	E1000[E1000_RCTL >> 2] |= E1000_RCTL_BAM;
	E1000[E1000_RCTL >> 2] &= ~E1000_RCTL_BSEX;
	E1000[E1000_RCTL >> 2] &= ~E1000_RCTL_SZ_MASK;
	E1000[E1000_RCTL >> 2] |= E1000_RCTL_SZ_2048;
	E1000[E1000_RCTL >> 2] |= E1000_RCTL_SECRC;

	return 0;
}

int E1000_send(char *data, int len) {
	if(data == NULL || len < 0 || len > TX_PACK_SZ) {
		return -E_INVAL;
	}

	uint32_t tdt = E1000[E1000_TDT >> 2];
	if(!(tx_descs[tdt].upper.data & E1000_TXD_STAT_DD)) {
		return -E_TX_FULL;
	}

	memset(tx_buf[tdt], 0 , sizeof(tx_buf[tdt]));
	memmove(tx_buf[tdt], data, len);

	tx_descs[tdt].lower.flags.length = len;
	tx_descs[tdt].lower.data |= E1000_TXD_CMD_RS;
	tx_descs[tdt].lower.data |= E1000_TXD_CMD_EOP;
	tx_descs[tdt].upper.data &= ~E1000_TXD_STAT_DD;

	E1000[E1000_TDT >> 2] = (tdt + 1) % MAXTXDESC;

	return 0;
}

int E1000_receive(char *buf) {
	if(buf == NULL) {
		return -E_INVAL;
	}

	uint32_t rdt = (E1000[E1000_RDT >> 2] + 1) % MAXRXDESC;

	if(!(rx_descs[rdt].status & E1000_RXD_STAT_DD)) {
		return -E_RX_EMPTY;
	}

	if(!(rx_descs[rdt].status & E1000_RXD_STAT_EOP)) {
		return -E_RX_LONG;
	}

	while(rdt == E1000[E1000_RDH >> 2]) {
		continue;
	}

	uint32_t len = rx_descs[rdt].length;
	memmove(buf, rx_buf[rdt], len);

	rx_descs[rdt].status &= ~E1000_RXD_STAT_DD;
	rx_descs[rdt].status &= ~E1000_RXD_STAT_EOP;

	E1000[E1000_RDT >> 2] = rdt;

	return len;
}