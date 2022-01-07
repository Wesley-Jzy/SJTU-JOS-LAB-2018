// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Backtrace the stack", mon_backtrace },
	{ "time", "Show the cycles", mon_time },
	{ "x", "Show address", mon_x },
	{ "si", "step execute", mon_si },
	{ "c", "continue running", mon_c }
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

unsigned read_eip();

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		(end-entry+1023)/1024);
	return 0;
}

// Lab1 only
// read the pointer to the retaddr on the stack
static uint32_t
read_pretaddr() {
    uint32_t pretaddr;
    __asm __volatile("leal 4(%%ebp), %0" : "=r" (pretaddr)); 
    return pretaddr;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	cprintf("Stack backtrace:\n");
	uint32_t* cur_ebp;
	uint32_t* old_ebp;
	uint32_t eip;

	int i;

	struct Eipdebuginfo info;

	cur_ebp = (uint32_t *) read_ebp();
	/* ebp = 0x0 when init */
	while (cur_ebp) {
		old_ebp = (uint32_t *) *cur_ebp;
		eip = *(cur_ebp + 1); 
		/* stack info */
		cprintf("  eip %08x ebp %08x args", eip, cur_ebp);
		for (i = 0; i < 5; i++) {
			cprintf(" %08x", *(cur_ebp + 2 + i));
		}
		cprintf("\n");

		/* eip info */
		debuginfo_eip(eip, &info);
		cprintf("     %s:%d ", info.eip_file, info.eip_line);
		for (i = 0; i < info.eip_fn_namelen; i++) {
			cprintf("%c", info.eip_fn_name[i]);
		}
		cprintf("+%d\n", eip - (uint32_t) info.eip_fn_addr);

		cur_ebp = old_ebp;
	}

    cprintf("Backtrace success\n");
	return 0;
}


int
mon_time(int argc, char **argv, struct Trapframe *tf) {
	int start = 0, end = 0;

	int i;
 	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[1], commands[i].name) == 0) {
			start = read_tsc();
			commands[i].func(argc - 1, argv + 1, tf);
			end = read_tsc();
			break;
		}
	}

	cprintf("%s cycles: %ld\n", argv[1], end - start);

	return 0;
}

int c2i(char c) {
	int res = 0;
	if (c >= '0' && c <= '9') {
		res = c - '0';
	} else if (c >= 'a' && c <= 'f') {
		res = c - 'a' + 10;
	} else if (c >= 'A' && c <= 'F') {
		res = c - 'A' + 10;
	} else {
		res = -1;
	}

	return res;
}

uint32_t hex2dec(char *hex) {  
	uint32_t res = 0;
	if (hex[0] != '0' || hex[1] != 'x') {
		return res;
	}

	int i = 2;
	int temp = 0;
	while (hex[i] != '\0') {
		res *= 16;
		temp = c2i(hex[i]);
		if (temp < 0) {
			return 0;
		}

		res += (uint32_t)temp;
		i++;
	}

	return res;
}  

int 
mon_x(int argc, char **argv, struct Trapframe *tf) {
	if (argc != 2) {
		cprintf("x v_address\n");
		return 0;
	}

	uint32_t addr = hex2dec(argv[1]);
	//cprintf("s--%s\n", argv[1]);
	//cprintf("x--%x\n", addr);

	uint32_t val = *(uint32_t *)addr; 
	cprintf("%u\n", val);

	return 0;
}

int 
mon_si(int argc, char **argv, struct Trapframe *tf) {
	tf->tf_eflags |= FL_TF;

	uint32_t eip = tf->tf_eip;
	cprintf("tf_eip=%08x\n", eip);

	struct Eipdebuginfo info;

	debuginfo_eip(eip, &info);
	cprintf("%s:%d: ", info.eip_file, info.eip_line);
	int i;
	for (i = 0; i < info.eip_fn_namelen; i++) {
		cprintf("%c", info.eip_fn_name[i]);
	}
	cprintf("+%d\n", eip - (uint32_t) info.eip_fn_addr);

	/* it must a val < 0 so that cmd mode can quit */
	return -1;
}

int 
mon_c(int argc, char **argv, struct Trapframe *tf) {
	tf->tf_eflags &= ~FL_TF;
	return -1;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}

// return EIP of caller.
// does not work if inlined.
// putting at the end of the file seems to prevent inlining.
unsigned
read_eip()
{
	uint32_t callerpc;
	__asm __volatile("movl 4(%%ebp), %0" : "=r" (callerpc));
	return callerpc;
}
