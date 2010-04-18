This directory demonstrates a Stackless Python segmentation fault bug.

Situation:

* Rhe main tasklet is not in the runnables list, and there is a C
  extension tasklet in the runnables list, and the current Python tasklet
  calls sys.exit().

Expected behavior:

* The process exits with code 0.

Actual, buggy behavior:

* The process segfaults in slp_kill_tasks_with_stacks (stacklesseval.c:349).

Segfault verified on:

  Python 2.6.5 Stackless 3.1b3 060516 (release26-maint, Mar 21 2010, 11:27:25) 
  [GCC 4.4.1] on linux2

How to reproduce:

* On a Linux system, run try.py using Stackless Python 2.6.5.

* It should print Segmentation fault as the last line. Example output:

  running build
  running build_ext
  building 'fooo' extension
  creating build
  creating build/temp.linux-i686-2.6
  gcc -pthread -fno-strict-aliasing -DSTACKLESS_FRHACK=0 -DNDEBUG -g -fwrapv -O3 -Wall -Wstrict-prototypes -fPIC -I/usr/local/include/python2.6 -c fooo.c -o build/temp.linux-i686-2.6/fooo.o
  creating build/lib.linux-i686-2.6
  gcc -pthread -shared build/temp.linux-i686-2.6/fooo.o -o build/lib.linux-i686-2.6/fooo.so
  last breath
  exiting
  Segmentation fault

Relevant output at the segfault from Valgrind, the memory checker:

  ==16458== Invalid write of size 4
  ==16458==    at 0x80DF749: slp_kill_tasks_with_stacks (stacklesseval.c:349)
  ==16458==    by 0x81055A1: Py_Finalize (pythonrun.c:416)
  ==16458==    by 0x8104FCE: handle_system_exit (pythonrun.c:1785)
  ==16458==    by 0x80E22D3: slp_run_tasklet (scheduling.c:1087)
  ==16458==    by 0x80DFD1B: slp_eval_frame (stacklesseval.c:299)
  ==16458==    by 0x80DFD6C: climb_stack_and_eval_frame (stacklesseval.c:266)
  ==16458==    by 0x80DFCC3: slp_eval_frame (stacklesseval.c:294)
  ==16458==    by 0x80D84F6: PyEval_EvalCode (ceval.c:543)
  ==16458==    by 0x8105D6B: PyRun_FileExFlags (pythonrun.c:1379)
  ==16458==    by 0x8105F31: PyRun_SimpleFileExFlags (pythonrun.c:952)
  ==16458==    by 0x805C7FA: Py_Main (main.c:572)
  ==16458==    by 0x805BAAA: main (python.c:23)
  ==16458==  Address 0x8 is not stack'd, malloc'd or (recently) free'd

See full valgrind output in valgrind.log.

Code snippet of slk_kill_tasks_with_stacks (grep for LINE_349):

  void slp_kill_tasks_with_stacks(PyThreadState *ts)
  {
	int count = 0;

	while (1) {
		PyCStackObject *csfirst = slp_cstack_chain, *cs;
		PyTaskletObject *t, *task;
		PyTaskletObject **chain;

		if (csfirst == NULL)
			break;
		for (cs = csfirst; ; cs = cs->next) {
			if (count && cs == csfirst) {
				/* nothing found */
				return;
			}
			++count;
			if (cs->task == NULL)
				continue;
			if (ts != NULL && cs->tstate != ts)
				continue;
			break;
		} 
		count = 0;
		t = cs->task;
		Py_INCREF(t);

		/* We need to ensure that the tasklet 't' is in the scheduler
		 * tasklet chain before this one (our main).  This ensures
		 * that this one is directly switched back to after 't' is
		 * killed.  The reason we do this this is because if another
		 * tasklet is switched to, this is of course it being scheduled
		 * and run.  Why we do not need to do this for tasklets blocked
		 * on channels is that when they are scheduled to be run and
		 * killed, they will be implicitly placed before this one,
		 * leaving it to run next.
		 */
		if (!t->flags.blocked && t != cs->tstate->st.main) {
			if (t->next && t->prev) { /* it may have been removed() */
				chain = &t;
				SLP_CHAIN_REMOVE(PyTaskletObject, chain, task, next, prev)
			}
			chain = &cs->tstate->st.main;
			task = cs->task;
			LINE_349: SLP_CHAIN_INSERT(PyTaskletObject, chain, task, next, prev);
			cs->tstate->st.current = cs->tstate->st.main;
			t = cs->task;
		}

		Py_INCREF(t); /* because the following steals a reference */
		PyTasklet_Kill(t);
		PyErr_Clear();

		if (t->f.frame == 0) {
			/* ensure a valid tstate */
			t->cstate->tstate = slp_initial_tstate;
		}
		Py_DECREF(t);
	}
  }

Bug found by Peter Szabo <ptspts@gmail.com> at
Sun Apr 18 18:45:40 CEST 2010

__END__
