/******************************************************************************/
/* Important Spring 2016 CSCI 402 usage information:                          */
/*                                                                            */
/* This fils is part of CSCI 402 kernel programming assignments at USC.       */
/*         53616c7465645f5f2e8d450c0c5851acd538befe33744efca0f1c4f9fb5f       */
/*         3c8feabc561a99e53d4d21951738da923cd1c7bbd11b30a1afb11172f80b       */
/*         984b1acfbbf8fae6ea57e0583d2610a618379293cb1de8e1e9d07e6287e8       */
/*         de7e82f3d48866aa2009b599e92c852f7dbf7a6e573f1c7228ca34b9f368       */
/*         faaef0c0fcf294cb                                                   */
/* Please understand that you are NOT permitted to distribute or publically   */
/*         display a copy of this file (or ANY PART of it) for any reason.    */
/* If anyone (including your prospective employer) asks you to post the code, */
/*         you must inform them that you do NOT have permissions to do so.    */
/* You are also NOT permitted to remove or alter this comment block.          */
/* If this comment block is removed or altered in a submitted file, 20 points */
/*         will be deducted.                                                  */
/******************************************************************************/

#include "config.h"
#include "globals.h"

#include "errno.h"

#include "util/init.h"
#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"

#include "proc/kthread.h"
#include "proc/proc.h"
#include "proc/sched.h"

#include "mm/slab.h"
#include "mm/page.h"

kthread_t *curthr; /* global */
static slab_allocator_t *kthread_allocator = NULL;

#ifdef __MTP__
/* Stuff for the reaper daemon, which cleans up dead detached threads */
static proc_t *reapd = NULL;
static kthread_t *reapd_thr = NULL;
static ktqueue_t reapd_waitq;
static list_t kthread_reapd_deadlist; /* Threads to be cleaned */

static void *kthread_reapd_run(int arg1, void *arg2);
#endif

void
kthread_init()
{
        kthread_allocator = slab_allocator_create("kthread", sizeof(kthread_t));
        KASSERT(NULL != kthread_allocator);
}

/**
 * Allocates a new kernel stack.
 *
 * @return a newly allocated stack, or NULL if there is not enough
 * memory available
 */
static char *
alloc_stack(void)
{
        /* extra page for "magic" data */
        char *kstack;
        int npages = 1 + (DEFAULT_STACK_SIZE >> PAGE_SHIFT);
        kstack = (char *)page_alloc_n(npages);
        return kstack;
}

/**
 * Frees a stack allocated with alloc_stack.
 *
 * @param stack the stack to free
 */
static void
free_stack(char *stack)
{
        page_free_n(stack, 1 + (DEFAULT_STACK_SIZE >> PAGE_SHIFT));
}

void
kthread_destroy(kthread_t *t)
{
        KASSERT(t && t->kt_kstack);
        free_stack(t->kt_kstack);
        if (list_link_is_linked(&t->kt_plink))
        {
        	list_remove(&t->kt_plink);
		}
        slab_obj_free(kthread_allocator, t);
}

/*
 * Allocate a new stack with the alloc_stack function. The size of the
 * stack is DEFAULT_STACK_SIZE.
 *
 * Don't forget to initialize the thread context with the
 * context_setup function. The context should have the same pagetable
 * pointer as the process.
 */
kthread_t *
kthread_create(struct proc *p, kthread_func_t func, long arg1, void *arg2)
{
        /*NOT_YET_IMPLEMENTED("PROCS: kthread_create");*/
        KASSERT(NULL != p);
        dbg(DBG_PRINT, "(GRADING1A 3.a)\n");
        kthread_t *kthr = (kthread_t *)slab_obj_alloc(kthread_allocator);
        KASSERT(kthr && "Unable to allocate memory for thread.\n");
        dbg(DBG_PRINT, "In kthread_create : Allocated memory for thread.\n");
		kthr->kt_kstack = alloc_stack();
		KASSERT(kthr->kt_kstack != NULL && "Unable to allocate thread stack.\n");
        dbg(DBG_PRINT, "In kthread_create : Allocated memory for thread stack.\n");
        kthr->kt_proc = p;
		kthr->kt_state = KT_NO_STATE;
        kthr->kt_wchan = NULL;
        kthr->kt_cancelled = 0;
		context_setup(&(kthr->kt_ctx), func, (int)arg1, arg2, kthr->kt_kstack, DEFAULT_STACK_SIZE, p->p_pagedir);
        list_link_init(&(kthr->kt_plink));
		list_insert_tail(&(p->p_threads), &(kthr->kt_plink));
        return kthr;
}

/*
 * If the thread to be cancelled is the current thread, this is
 * equivalent to calling kthread_exit. Otherwise, the thread is
 * sleeping and we need to set the cancelled and retval fields of the
 * thread.
 *
 * If the thread's sleep is cancellable, cancelling the thread should
 * wake it up from sleep.
 *
 * If the thread's sleep is not cancellable, we do nothing else here.
 */
void
kthread_cancel(kthread_t *kthr, void *retval)
{
    /*NOT_YET_IMPLEMENTED("PROCS: kthread_cancel");*/
	KASSERT(NULL != kthr && "Trying to cancel NULL thread!\n");
	dbg(DBG_PRINT, "(GRADING1A 3.b)\n");
    dbg(DBG_PRINT, "(GRADING1C 8)\n");
    if(kthr == curthr)
    {
        dbg(DBG_PRINT, "(GRADING1E 2)\n");
    	kthread_exit(retval);
    }
    else
    {
        dbg(DBG_PRINT, "(GRADING1C 8)\n");
    	kthr->kt_cancelled = 1;
    	kthr->kt_retval = retval;
    }
    if(kthr->kt_state == KT_SLEEP_CANCELLABLE)
    {
        dbg(DBG_PRINT, "(GRADING1C 8)\n");
    	sched_wakeup_on(kthr->kt_wchan);
    }
    dbg(DBG_PRINT, "(GRADING1C 8)\n");
}

/*
 * You need to set the thread's retval field, set its state to
 * KT_EXITED, and alert the current process that a thread is exiting
 * via proc_thread_exited.
 *
 * It may seem unneccessary to push the work of cleaning up the thread
 * over to the process. However, if you implement MTP, a thread
 * exiting does not necessarily mean that the process needs to be
 * cleaned up.
 */
void
kthread_exit(void *retval)
{
        /*NOT_YET_IMPLEMENTED("PROCS: kthread_exit");*/
        KASSERT(curthr->kt_wchan == NULL && "Current thread is in some blocking queue!\n");
        dbg(DBG_PRINT, "(GRADING1A 3.c)\n");
        KASSERT(curthr->kt_qlink.l_next == NULL && curthr->kt_qlink.l_prev==NULL && "Current thread queue is not empty!\n");
        dbg(DBG_PRINT, "(GRADING1A 3.c)\n");
        KASSERT(curthr->kt_proc == curproc && "Current thread is magically executing without curproc!\n");
        dbg(DBG_PRINT, "(GRADING1A 3.c)\n");
        curthr->kt_retval = retval;
        curthr->kt_state = KT_EXITED;
        proc_thread_exited(retval);
}

/*
 * The new thread will need its own context and stack. Think carefully
 * about which fields should be copied and which fields should be
 * freshly initialized.
 *
 * You do not need to worry about this until VM.
 */
kthread_t *
kthread_clone(kthread_t *thr)
{
        /*NOT_YET_IMPLEMENTED("VM: kthread_clone");*/
        kthread_t *temp = slab_obj_alloc(kthread_allocator);
        if(temp==NULL)
        {
            return temp;
        }
        char *stack = alloc_stack();
        if(stack==NULL)
        {
            slab_obj_free(kthread_allocator,temp);
            return NULL;
        }
        temp->kt_kstack=stack;
        temp->kt_retval=thr->kt_retval;
        temp->kt_errno=thr->kt_errno;
        /*DONT KNOW WHAT TO SET PROC TO, IF I SET IT TO SAME, THEN IT'LL BE MTP*/
        temp->kt_proc=NULL;
        temp->kt_cancelled=thr->kt_canclled;
        temp->kt_wchan=thr->kt_wchan;
        list_link_init(&(temp->kt_qlink));
        list_link_init(&(temp->kt_plink));
        return temp;
}

/*
 * The following functions will be useful if you choose to implement
 * multiple kernel threads per process. This is strongly discouraged
 * unless your weenix is perfect.
 */
#ifdef __MTP__
int
kthread_detach(kthread_t *kthr)
{
        NOT_YET_IMPLEMENTED("MTP: kthread_detach");
        return 0;
}

int
kthread_join(kthread_t *kthr, void **retval)
{
        NOT_YET_IMPLEMENTED("MTP: kthread_join");
        return 0;
}

/* ------------------------------------------------------------------ */
/* -------------------------- REAPER DAEMON ------------------------- */
/* ------------------------------------------------------------------ */
static __attribute__((unused)) void
kthread_reapd_init()
{
        NOT_YET_IMPLEMENTED("MTP: kthread_reapd_init");
}
init_func(kthread_reapd_init);
init_depends(sched_init);

void
kthread_reapd_shutdown()
{
        NOT_YET_IMPLEMENTED("MTP: kthread_reapd_shutdown");
}

static void *
kthread_reapd_run(int arg1, void *arg2)
{
        NOT_YET_IMPLEMENTED("MTP: kthread_reapd_run");
        return (void *) 0;
}
#endif
