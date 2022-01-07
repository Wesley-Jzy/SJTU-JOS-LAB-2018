/* Lab4 Challenge */
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	struct Env *e;
	envid_t who;

	int parent_priority = 0;
	int child_priority = 1;

	/* set priority to parent, 0 = LOW, 1 = HIGH */
	sys_env_set_priority(0, parent_priority);
	cprintf("l'm parent %x\n", sys_getenvid());

	if ((who = fork()) != 0) {
		/* set priority to child */
		sys_env_set_priority(who, child_priority);
		cprintf("l'm child %x\n", who);

		if ((who = fork()) != 0) {
			sys_env_set_priority(who, child_priority);
			cprintf("l'm child %x\n", who);
		}
	}

	/* check the schedule order */
	e = (struct Env *)envs + ENVX(sys_getenvid()); 
	int i;
	for (i = 0; i < 5; i++) {
		cprintf("l'm %x\n", sys_getenvid());
		cprintf("my priority is %d\n", e->env_priority);
		sys_yield();
	}
}
