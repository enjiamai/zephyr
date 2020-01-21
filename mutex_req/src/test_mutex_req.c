/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <ztest.h>
#include <kernel.h>
#include <kernel_structs.h>


#define TIMEOUT 500
#define STACK_SIZE (512 + CONFIG_TEST_EXTRA_STACKSIZE)


/**TESTPOINT: init via K_MUTEX_DEFINE*/
K_MUTEX_DEFINE(kmutex);
static struct k_mutex mutex;

static K_THREAD_STACK_DEFINE(tstack1, STACK_SIZE);
static K_THREAD_STACK_DEFINE(tstack2, STACK_SIZE);
static struct k_thread tdata1;
static struct k_thread tdata2;

__unused static void tThread_entry_lock_forever(void *p1, void *p2, void *p3)
{
        zassert_true(k_mutex_lock((struct k_mutex *)p1, K_FOREVER) == 0,
                      "access locked resource from spawn thread");

	while(1)
	{
		/*loop forever*/
		k_sleep(1000);

		TC_PRINT("thread T1 hold the mutex...\n");
	}
}

static void tThread_entry_lock_with_timeout(void *p1, void *p2, void *p3)
{
        zassert_true(k_mutex_lock((struct k_mutex *)p1, K_FOREVER) == 0,
                      "access locked resource from spawn thread");
        
	/*This thread will hold mutex for 2000 ms, then release it.*/
	k_sleep(2000);
	
	k_mutex_unlock((struct k_mutex *)p1);
}


static void tThread_entry_lock_priority_1(void *p1, void *p2, void *p3)
{	
	zassert_true(k_mutex_lock((struct k_mutex *)p1, K_FOREVER) == 0,
		      "access locked resource from spawn thread T1");

	int priority_origin = k_thread_priority_get((k_tid_t)p2);

	TC_PRINT("T1 is going to enter mutex: origin priority=%d\n", priority_origin);

	while (1)
	{
		k_sleep(TIMEOUT);

		int priority = k_thread_priority_get((k_tid_t)p2); 

		TC_PRINT("access resource from thread T1: priority=%d\n", priority);
		
		if (priority <= priority_origin)
		{
			k_mutex_unlock((struct k_mutex *)p1);
			break;
		}	
	}

	while (1)
	{
		k_sleep(TIMEOUT);

		int priority = k_thread_priority_get((k_tid_t)p2);

		TC_PRINT("after release thread T1: priority=%d\n", priority);

		if (priority == priority_origin)
                {
			break;
		}
	}
}

static void tThread_entry_lock_priority_2(void *p1, void *p2, void *p3)
{
	TC_PRINT("thread T2 priority=%d\n", k_thread_priority_get((k_tid_t)p2));

#if 1
	zassert_true(k_mutex_lock((struct k_mutex *)p1, K_FOREVER) == 0,
		      "access locked resource from spawn thread T2");
#else
	k_mutex_lock((struct k_mutex *)p1, K_FOREVER);
#endif
	TC_PRINT("thread T2 got the resource\n");

	k_mutex_unlock((struct k_mutex *)p1);
}

static void tThread_entry_lock_timeout_1(void *p1, void *p2, void *p3)
{
	int ret = 0;

        ret = k_mutex_lock((struct k_mutex *)p1, K_FOREVER);

        TC_PRINT("thread T2 ret = %d\n", ret);

        zassert_true(ret == 0, "fail to lock K_FOREVER");
}

static void tThread_entry_lock_timeout_2(void *p1, void *p2, void *p3)
{
        int ret = 0;

        ret = k_mutex_lock((struct k_mutex *)p1, 2000);

        TC_PRINT("thread T2 ret = %d\n", ret);

        zassert_true(ret == -EAGAIN, "fail to lock TIMEOUT");
}


static void tThread_entry_lock_timeout_3(void *p1, void *p2, void *p3)
{
        int ret = 0;

        ret = k_mutex_lock((struct k_mutex *)p1, K_NO_WAIT);

	TC_PRINT("thread T2 ret = %d\n", ret);
        
	zassert_true(ret == -EBUSY, "fail to lock K_NO_WAIT");
}

static void tThread_entry_lock_timeout_4(void *p1, void *p2, void *p3)
{
	int ret = 0;

        ret = k_mutex_lock((struct k_mutex *)p1, 500);
	
	TC_PRINT("thread T2 ret = %d\n", ret);
        
	zassert_true(ret == -EAGAIN, "fail to lock TIMEOUT");
}


static void tmutex_test_lock_t1(struct k_mutex *pmutex, int priority,
			     void (*entry_fn)(void *, void *, void *))
{
	k_thread_create(&tdata1, tstack1, STACK_SIZE,
			entry_fn, pmutex, &tdata1, NULL,
			K_PRIO_PREEMPT(priority),
			K_USER | K_INHERIT_PERMS, K_NO_WAIT);


	/* wait for spawn thread to take action */
	k_sleep(TIMEOUT);
}

static void tmutex_test_lock_t2(struct k_mutex *pmutex, int priority,
                             void (*entry_fn)(void *, void *, void *))
{
        k_thread_create(&tdata2, tstack2, STACK_SIZE,
                        entry_fn, pmutex, &tdata2, NULL,
                        K_PRIO_PREEMPT(priority),
                        K_USER | K_INHERIT_PERMS, K_NO_WAIT);


        /* wait for spawn thread to take action */
        k_sleep(TIMEOUT);
}

/*test cases*/


/**
 * @brief Test Mutex tests
 *
 * @defgroup kernel_mutex_tests Mutex Tests
 *
 * @ingroup all_tests
 *
 * @{
 */

/**
 * @brief Test Mutex's priority inheritance
 * @ingroup kernel_mutex_tests
 * @verify{@req{284}}
 */ 
void test_mutex_priority_inheritance_s1(void)
{
	/**TESTPOINT: test priority inheritance scenairo 1, given priority t1 < t2*/
	k_mutex_init(&mutex);
	tmutex_test_lock_t1(&mutex, 5, tThread_entry_lock_priority_1);

	tmutex_test_lock_t2(&mutex, 1, tThread_entry_lock_priority_2);
	
	k_thread_abort(&tdata1);
	//k_thread_abort(&tdata2);
}

void test_mutex_priority_inheritance_s2(void)
{
        /**TESTPOINT: test priority_inheritance scenairo 2, given priority t1 > t2*/
        k_mutex_init(&mutex);
        tmutex_test_lock_t1(&mutex, 2, tThread_entry_lock_priority_1);

        tmutex_test_lock_t2(&mutex, 3, tThread_entry_lock_priority_2);
	
	k_thread_abort(&tdata1);
	//k_thread_abort(&tdata2);
}


/**
 * @brief Test Mutex's timeout operations
 * @ingroup kernel_mutex_tests
 * @verify{@req{286}}
 */
void test_mutex_timeout_s1(void)
{
	/**TESTPOINT: test timeout operations scenairo 1, given paramter TK_FOREVER*/
        k_mutex_init(&mutex);

	tmutex_test_lock_t1(&mutex, 0, tThread_entry_lock_with_timeout);

	tmutex_test_lock_t2(&mutex, 0, tThread_entry_lock_timeout_1);
	
	k_thread_abort(&tdata1);
	k_thread_abort(&tdata2);
}

void test_mutex_timeout_s2(void)
{
        k_mutex_init(&mutex);
        /**TESTPOINT: test timeout operations scenairo 2, given paramter TIMEOUT 3000*/
        tmutex_test_lock_t1(&mutex, 0, tThread_entry_lock_with_timeout);

        tmutex_test_lock_t2(&mutex, 0, tThread_entry_lock_timeout_2);
	
	k_thread_abort(&tdata1);
	k_thread_abort(&tdata2);
}


void test_mutex_timeout_s3(void)
{
        /**TESTPOINT: test timeout operations scenairo 3, given paramter  K_NO_WAIT*/
        k_mutex_init(&mutex);

        tmutex_test_lock_t1(&mutex, 0, tThread_entry_lock_with_timeout);

        tmutex_test_lock_t2(&mutex, 0, tThread_entry_lock_timeout_3);

	k_thread_abort(&tdata1);
	//k_thread_abort(&tdata2);
}

void test_mutex_timeout_s4(void)
{
        k_mutex_init(&mutex);
	/**TESTPOINT: test timeout operations scenairo 4, given paramter TIMEOUT 500*/
        tmutex_test_lock_t1(&mutex, 0, tThread_entry_lock_with_timeout);

        tmutex_test_lock_t2(&mutex, 0, tThread_entry_lock_timeout_4);
	
	k_thread_abort(&tdata1);
	k_thread_abort(&tdata2);
}


/*test case main entry*/
void test_main(void)
{	
	k_thread_access_grant(k_current_get(), &tdata1, &tdata2, &tstack1,
			&tstack2, &kmutex, &mutex);

	ztest_test_suite(mutex_req,
                         ztest_user_unit_test(test_mutex_timeout_s1),
                         ztest_user_unit_test(test_mutex_timeout_s2),
                         ztest_user_unit_test(test_mutex_timeout_s3),
                         ztest_user_unit_test(test_mutex_timeout_s4),
                         ztest_user_unit_test(test_mutex_priority_inheritance_s2),
			 ztest_user_unit_test(test_mutex_priority_inheritance_s1)
                         );
	ztest_run_test_suite(mutex_req);
}
