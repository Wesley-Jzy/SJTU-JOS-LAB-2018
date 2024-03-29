#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>


// Choose a user environment to run and run it.
void
sched_yield(void)
{
	struct Env *idle;
	int i;

	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING) and never choose an
	// idle environment (env_type == ENV_TYPE_IDLE).  If there are
	// no runnable environments, simply drop through to the code
	// below to switch to this CPU's idle environment.

	// LAB 4: Your code here.
	/* Switch to the first such environment found */
	envid_t env_id;

	/* Lab4 Challenge */
	envid_t highest_priority_env_id = -1;

	if (curenv) {
		env_id = ENVX(curenv->env_id);
		for (i = (env_id + 1) % NENV; i != env_id; i = (i + 1) % NENV) {
			if (envs[i].env_type != ENV_TYPE_IDLE &&
				envs[i].env_status == ENV_RUNNABLE) {
				/* Lab4 Challenge */
				if (highest_priority_env_id == -1 || 
					envs[i].env_priority > envs[highest_priority_env_id].env_priority) {
					highest_priority_env_id = i;
				}
			}
		}

		/* Lab4 Challenge */
		if (highest_priority_env_id != -1) {
			if (curenv->env_type != ENV_TYPE_IDLE && curenv->env_status == ENV_RUNNING) {
	 			if (curenv->env_priority > envs[highest_priority_env_id].env_priority) {
	 				env_run(curenv);
	 			}
	 		}
			env_run(&envs[highest_priority_env_id]);
		}

		/* If no envs are runnable,  running on this CPU is still ENV_RUNNING, 
	 	* it's okay to choose */
	 	if (curenv->env_type != ENV_TYPE_IDLE && curenv->env_status == ENV_RUNNING) {
	 		env_run(curenv);
	 	}

	} else {
		for (i = 0; i < NENV; i++) {
			if (envs[i].env_type != ENV_TYPE_IDLE &&
				envs[i].env_status == ENV_RUNNABLE) {
				/* Lab4 Challenge */
				if (highest_priority_env_id == -1 || 
					envs[i].env_priority > envs[highest_priority_env_id].env_priority) {
					highest_priority_env_id = i;
				}
			}
		}

		/* Lab4 Challenge */
		if (highest_priority_env_id != -1) {
			env_run(&envs[highest_priority_env_id]);
		}
	}

	// For debugging and testing purposes, if there are no
	// runnable environments other than the idle environments,
	// drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if (envs[i].env_type != ENV_TYPE_IDLE &&
		    (envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING))
			break;
	}
	if (i == NENV) {
		cprintf("No more runnable environments!\n");
		while (1)
			monitor(NULL);
	}

	// Run this CPU's idle environment when nothing else is runnable.
	idle = &envs[cpunum()];
	if (!(idle->env_status == ENV_RUNNABLE || idle->env_status == ENV_RUNNING))
		panic("CPU %d: No idle environment!", cpunum());
	env_run(idle);
}
