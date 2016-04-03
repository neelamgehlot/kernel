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

#include "types.h"
#include "globals.h"
#include "kernel.h"

#include "util/gdb.h"
#include "util/init.h"
#include "util/debug.h"
#include "util/string.h"
#include "util/printf.h"

#include "mm/mm.h"
#include "mm/page.h"
#include "mm/pagetable.h"
#include "mm/pframe.h"

#include "vm/vmmap.h"
#include "vm/shadowd.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "main/acpi.h"
#include "main/apic.h"
#include "main/interrupt.h"
#include "main/gdt.h"

#include "proc/sched.h"
#include "proc/proc.h"
#include "proc/kthread.h"

#include "drivers/dev.h"
#include "drivers/blockdev.h"
#include "drivers/disk/ata.h"
#include "drivers/tty/virtterm.h"
#include "drivers/pci.h"

#include "api/exec.h"
#include "api/syscall.h"

#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/fcntl.h"
#include "fs/stat.h"

#include "test/kshell/kshell.h"
#include "errno.h"

GDB_DEFINE_HOOK(boot)
GDB_DEFINE_HOOK(initialized)
GDB_DEFINE_HOOK(shutdown)

static void hard_shutdown(void);
static void *bootstrap(int arg1, void *arg2);
static void *idleproc_run(int arg1, void *arg2);
static kthread_t *initproc_create(void);
static void *initproc_run(int arg1, void *arg2);

extern void *sunghan_test(int, void*);
extern void *sunghan_deadlock_test(int, void*);
extern void *faber_thread_test(int, void*);

extern void *faber_fs_thread_test(int, void*);
extern void *faber_directory_test(int, void*);
extern void *vfs_test_main(int, void*);

static context_t bootstrap_context;
extern int gdb_wait;

/**
 * This is the first real C function ever called. It performs a lot of
 * hardware-specific initialization, then creates a pseudo-context to
 * execute the bootstrap function in.
 */
void
kmain()
{
        GDB_CALL_HOOK(boot);

        dbg_init();
        dbgq(DBG_CORE, "Kernel binary:\n");
        dbgq(DBG_CORE, "  text: 0x%p-0x%p\n", &kernel_start_text, &kernel_end_text);
        dbgq(DBG_CORE, "  data: 0x%p-0x%p\n", &kernel_start_data, &kernel_end_data);
        dbgq(DBG_CORE, "  bss:  0x%p-0x%p\n", &kernel_start_bss, &kernel_end_bss);

        page_init();

        pt_init();
        slab_init();
        pframe_init();

        acpi_init();
        apic_init();
        pci_init();
        intr_init();

        gdt_init();

        /* initialize slab allocators */
#ifdef __VM__
        anon_init();
        shadow_init();
#endif
        vmmap_init();
        proc_init();
        kthread_init();

#ifdef __DRIVERS__
        bytedev_init();
        blockdev_init();
#endif

        void *bstack = page_alloc();
        pagedir_t *bpdir = pt_get();
        KASSERT(NULL != bstack && "Ran out of memory while booting.");
        /* This little loop gives gdb a place to synch up with weenix.  In the
         * past the weenix command started qemu was started with -S which
         * allowed gdb to connect and start before the boot loader ran, but
         * since then a bug has appeared where breakpoints fail if gdb connects
         * before the boot loader runs.  See
         *
         * https://bugs.launchpad.net/qemu/+bug/526653
         *
         * This loop (along with an additional command in init.gdb setting
         * gdb_wait to 0) sticks weenix at a known place so gdb can join a
         * running weenix, set gdb_wait to zero  and catch the breakpoint in
         * bootstrap below.  See Config.mk for how to set GDBWAIT correctly.
         *
         * DANGER: if GDBWAIT != 0, and gdb is not running, this loop will never
         * exit and weenix will not run.  Make SURE the GDBWAIT is set the way
         * you expect.
         */
        while (gdb_wait) ;
        context_setup(&bootstrap_context, bootstrap, 0, NULL, bstack, PAGE_SIZE, bpdir);
        context_make_active(&bootstrap_context);

        panic("\nReturned to kmain()!!!\n");
}

/**
 * Clears all interrupts and halts, meaning that we will never run
 * again.
 */
static void
hard_shutdown()
{
#ifdef __DRIVERS__
        vt_print_shutdown();
#endif
        __asm__ volatile("cli; hlt");
}

/**
 * This function is called from kmain, however it is not running in a
 * thread context yet. It should create the idle process which will
 * start executing idleproc_run() in a real thread context.  To start
 * executing in the new process's context call context_make_active(),
 * passing in the appropriate context. This function should _NOT_
 * return.
 *
 * Note: Don't forget to set curproc and curthr appropriately.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
bootstrap(int arg1, void *arg2)
{
        /*NOT_YET_IMPLEMENTED("PROCS: bootstrap");*/
        /* If the next line is removed/altered in your submission, 20 points will be deducted. */
        dbgq(DBG_CORE, "SIGNATURE: 53616c7465645f5fc89e29cb09bf365345b72f6af8d5c10193b579d94b4ca6230ab7223f16292966a02323321516e800\n");
        /* necessary to finalize page table information */
        pt_template_init();
        proc_t *idle_proc = proc_create("idle_proc");
        curproc = idle_proc;
        KASSERT((NULL!= curproc) && "Unable to create idle process.\n");
        dbg(DBG_PRINT, "GRADING1A 1.a\n");
        KASSERT(PID_IDLE == curproc->p_pid && "The process created is not idle process.\n");
        dbg(DBG_PRINT, "GRADING1A 1.a\n");
        kthread_t *idle_thread = kthread_create(idle_proc, idleproc_run, arg1, arg2);
        curthr = idle_thread;
        KASSERT((NULL != curthr) && "Unable to create idle thread.\n");
        dbg(DBG_PRINT, "GRADING1A 1.a\n");
        context_make_active(&(idle_thread->kt_ctx));
        panic("weenix returned to bootstrap()!!! BAD!!!\n");
        return NULL;
}

/**
 * Once we're inside of idleproc_run(), we are executing in the context of the
 * first process-- a real context, so we can finally begin running
 * meaningful code.
 *
 * This is the body of process 0. It should initialize all that we didn't
 * already initialize in kmain(), launch the init process (initproc_run),
 * wait for the init process to exit, then halt the machine.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
idleproc_run(int arg1, void *arg2)
{
        int status;
        pid_t child;

        /* create init proc */
        kthread_t *initthr = initproc_create();

        init_call_all();
        GDB_CALL_HOOK(initialized);

        /* Create other kernel threads (in order) */

#ifdef __VFS__
        /* Once you have VFS remember to set the current working directory
         * of the idle and init processes */
        /*NOT_YET_IMPLEMENTED("VFS: idleproc_run");*/
        curproc->p_cwd = vfs_root_vn;
        vref(vfs_root_vn);
        initthr->kt_proc->p_cwd = vfs_root_vn;
        vref(vfs_root_vn);

        /* Here you need to make the null, zero, and tty devices using mknod */
        /* You can't do this until you have VFS, check the include/drivers/dev.h
         * file for macros with the device ID's you will need to pass to mknod */
        /*NOT_YET_IMPLEMENTED("VFS: idleproc_run");*/
        int x = do_mkdir("/dev");
        if( x == 0)
        {
            int y = do_mknod("/dev/tty0",S_IFCHR,MKDEVID(2,0));
            y = do_mknod("/dev/null",S_IFCHR,MEM_NULL_DEVID);
            

            y = do_mknod("/dev/zero",S_IFCHR,MEM_ZERO_DEVID);

            
            y = do_mknod("/dev/tty1",S_IFCHR,MKDEVID(2,1));
            y = do_mknod("/dev/tty2",S_IFCHR,MKDEVID(2,2));

        }
    
        x = do_mkdir("/tmp");
         dbg(DBG_PRINT,"PP END of VFS \n");

#endif

        /* Finally, enable interrupts (we want to make sure interrupts
         * are enabled AFTER all drivers are initialized) */
        intr_enable();

        /* Run initproc */
        sched_make_runnable(initthr);
        /* Now wait for it */
        child = do_waitpid(-1, 0, &status);
        KASSERT(PID_INIT == child);

#ifdef __MTP__
        kthread_reapd_shutdown();
#endif


#ifdef __SHADOWD__
        /* wait for shadowd to shutdown */
        shadowd_shutdown();
#endif

#ifdef __VFS__
        /* Shutdown the vfs: */
        dbg_print("weenix: vfs shutdown...\n");
        vput(curproc->p_cwd);
        if (vfs_shutdown())
                panic("vfs shutdown FAILED!!\n");

#endif

        /* Shutdown the pframe system */
#ifdef __S5FS__
        pframe_shutdown();
#endif

        dbg_print("\nweenix: halted cleanly!\n");
        GDB_CALL_HOOK(shutdown);
        hard_shutdown();
        return NULL;
}

/**
 * This function, called by the idle process (within 'idleproc_run'), creates the
 * process commonly refered to as the "init" process, which should have PID 1.
 *
 * The init process should contain a thread which begins execution in
 * initproc_run().
 *
 * @return a pointer to a newly created thread which will execute
 * initproc_run when it begins executing
 */
static kthread_t *
initproc_create(void)
{
        /*NOT_YET_IMPLEMENTED("PROCS: initproc_create");*/
        proc_t *init_proc = proc_create("init_proc");
        KASSERT((NULL != init_proc) && "Unable to create init process.\n");
        dbg(DBG_PRINT, "GRADING1A 1.b\n");
        KASSERT(PID_INIT == init_proc->p_pid && "The process created is not init process.\n");
        dbg(DBG_PRINT, "GRADING1A 1.b\n");
        kthread_t *init_thread = kthread_create(init_proc, initproc_run, 0, NULL);
        KASSERT((init_thread != NULL) && "Unable to create init thread.\n");
        dbg(DBG_PRINT, "GRADING1A 1.b\n");
        return init_thread;
}

/**
 * The init thread's function changes depending on how far along your Weenix is
 * developed. Before VM/FI, you'll probably just want to have this run whatever
 * tests you've written (possibly in a new process). After VM/FI, you'll just
 * exec "/sbin/init".
 *
 * Both arguments are unused.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */

 #ifdef __DRIVERS__

 /*Empty function*/
 static void *empty(int arg1,void *arg2)
 {
     return NULL;
 }

 /*Function which kills itself*/
 static void *self_cancel(int arg1,void *arg2)
 {
     kthread_cancel(curthr,0);
     return NULL;
 }

int
do_foo(kshell_t *kshell, int argc, char **argv)
{
    KASSERT(kshell != NULL);
    dbg(DBG_PRINT, "GRADING1B\n");
    int status = 0;
	proc_t *faber_test = proc_create("faber_test");
    kthread_t *faber_thread = kthread_create(faber_test, faber_thread_test, 0, 0);
 	sched_make_runnable(faber_thread);
 	do_waitpid(faber_test->p_pid , 0, &status);
    return 0;
}

int
sunghan_test_func(kshell_t *kshell, int argc, char **argv)
{
    KASSERT(kshell != NULL);
    dbg(DBG_PRINT, "GRADING1D 1\n");
    int status = 0;
	proc_t *sunghan_test_proc = proc_create("sunghan_test");
    kthread_t *sunghan_thread = kthread_create(sunghan_test_proc, sunghan_test, 0, 0);
 	sched_make_runnable(sunghan_thread);
 	do_waitpid(sunghan_test_proc->p_pid , 0, &status);
    return 0;
}

int
sunghan_deadlock_test_func(kshell_t *kshell, int argc, char **argv)
{
    KASSERT(kshell != NULL);
    dbg(DBG_PRINT, "GRADING1D 2\n");
    int status = 0;
	proc_t *sunghan_deadlock_test_proc = proc_create("sunghan_test");
    kthread_t *sunghan_deadlock_thread = kthread_create(sunghan_deadlock_test_proc, sunghan_deadlock_test, 0, 0);
 	sched_make_runnable(sunghan_deadlock_thread);
 	do_waitpid(sunghan_deadlock_test_proc->p_pid , 0, &status);
    return 0;
}

int
big_process_name(kshell_t *kshell, int argc, char **argv)
{
    KASSERT(kshell != NULL);
    dbg(DBG_PRINT, "GRADING1E 1\n");
    int status = 0,i;
    char a[300];
    for(i=0;i<300;++i)a[i]='a';
	proc_t *pt = proc_create(a);
    kthread_t *thr = kthread_create(pt, empty, 0, NULL);
 	sched_make_runnable(thr);
 	do_waitpid(pt->p_pid , 0, &status);
    return 0;
}

int
thread_self_cancel(kshell_t *kshell, int argc, char **argv)
{
    KASSERT(kshell != NULL);
    dbg(DBG_PRINT, "GRADING1E 2\n");
    int status = 0;
	proc_t *pt = proc_create("ProcE2");
    kthread_t *thr = kthread_create(pt, self_cancel, 0, NULL);
 	sched_make_runnable(thr);
 	do_waitpid(pt->p_pid , 0, &status);
    return 0;
}


int
init_clean(kshell_t *kshell, int argc, char **argv)
{
    KASSERT(kshell != NULL);
    dbg(DBG_PRINT, "GRADING1E 3\n");
    kthread_cancel(curthr,0);
    return 0;
}

int
badargs1(kshell_t *kshell, int argc, char **argv)
{
    KASSERT(kshell != NULL);
    dbg(DBG_PRINT, "GRADING1E 4\n");
    int x;
    if(do_waitpid(-2,1000,&x) != -ECHILD)
    {
        panic("Something very wrong!\n");
    }
    return 0;
}

int
faber_test_func(kshell_t *kshell, int argc, char **argv)
{
    KASSERT(kshell != NULL);
    dbg(DBG_PRINT, "GRADING1D 1\n");
    int status = 0;
	proc_t *faber_test_proc = proc_create("faber_fs_thread_test");
    	kthread_t *faber_thread = kthread_create(faber_test_proc, faber_fs_thread_test, 0, 0);
 	sched_make_runnable(faber_thread);
 	do_waitpid(faber_test_proc->p_pid , 0, &status);
    return 0;
}

int
faber_test_dir(kshell_t *kshell, int argc, char **argv)
{
    KASSERT(kshell != NULL);
    dbg(DBG_PRINT, "GRADING1D 1\n");
    int status = 0;
	proc_t *faber_test_dir = proc_create("faber_fs_dir");
    	kthread_t *faber_thread_dir = kthread_create(faber_test_dir, faber_directory_test, 0, 0);
 	sched_make_runnable(faber_thread_dir);
 	do_waitpid(faber_test_dir->p_pid , 0, &status);
    return 0;
}

int
vfs_test(kshell_t *kshell, int argc, char **argv)
{
    KASSERT(kshell != NULL);
    
    dbg(DBG_PRINT, "GRADING1E 2\n");
    int status = 0;
	proc_t *pt = proc_create("vfs");
   	kthread_t *thr = kthread_create(pt, vfs_test_main, 0, NULL);
 	sched_make_runnable(thr);
 	do_waitpid(pt->p_pid , 0, &status);
    return 0;
}


int
badargs2(kshell_t *kshell, int argc, char **argv)
{
    KASSERT(kshell != NULL);
    dbg(DBG_PRINT, "GRADING1E 5\n");
    int x;
    if(do_waitpid(-1,1000,&x) != -ECHILD)
    {
        panic("Something very wrong!\n");
    }
    return 0;
}


 #endif /* __DRIVERS__ */

 static void *
 initproc_run(int arg1, void *arg2)
 {
         /*NOT_YET_IMPLEMENTED("PROCS: initproc_run");*/

 #ifdef __DRIVERS__

         kshell_add_command("faberproc", do_foo, "Invoke faber tests");
         kshell_add_command("sunghan", sunghan_test_func, "Sunghan test");
 	 kshell_add_command("sunghan_dead", sunghan_deadlock_test_func, "Sunghan deadlock test");
         kshell_add_command("bigprocname", big_process_name, "Additional test for long process name");
         kshell_add_command("thrselfcancel", thread_self_cancel, "Cancelling the current thread");
         kshell_add_command("initclean", init_clean, "Proc clean called on init(Works similar to exit)");
         kshell_add_command("badargs1", badargs1, "Pid other than -1 and positive numbers");
         kshell_add_command("badargs2", badargs2, "Options other than 0");
         
         kshell_add_command("faber", faber_test_func, "faber fs thread test");
         kshell_add_command("faber_dir", faber_test_dir, "faber fs dir");
         kshell_add_command("vfs", vfs_test, "invoke vfs tests");
         
         kshell_t *kshell = kshell_create(0);
         dbg(DBG_PRINT,"QQ Before kShell\n");
        
         if (NULL == kshell) panic("init: Couldn't create kernel shell\n");
         dbg(DBG_PRINT,"QQ after if\n");
         while (kshell_execute_next(kshell));
         kshell_destroy(kshell);

 #endif /* __DRIVERS__ */
         return NULL;
 }
