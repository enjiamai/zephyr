/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <ztest.h>
#include <irq_offload.h>
#include <ztest_error_hook.h>

#define STACK_SIZE (512 + CONFIG_TEST_EXTRA_STACKSIZE)
#define THREAD_TEST_PRIORITY 5

/* use to pass case type to threads */
static ZTEST_BMEM int case_type;

static struct k_mutex mutex;

static K_THREAD_STACK_DEFINE(tstack, STACK_SIZE);
static struct k_thread tdata;

/* enumerate our negative case scenario */
enum {
	MUTEX_INIT_NULL,
	MUTEX_LOCK_NULL,
	MUTEX_UNLOCK_NULL,
	MUTEX_LOCK_IN_ISR,
	MUTEX_UNLOCK_IN_ISR,
	NOT_DEFINE
} neg_case;

/* This is a semaphore using inside irq_offload */
extern struct k_sem offload_sem;

/* A call back function which is hooked in default assert handler. */
void ztest_post_assert_fail_hook(void)
{
	int choice = case_type;

	switch (choice) {
	case MUTEX_LOCK_IN_ISR:
	case MUTEX_UNLOCK_IN_ISR:
		/* Semaphore used inside irq_offload need to be
		 * released after assert or fault happened.
		 */
		k_sem_give(&offload_sem);
		ztest_test_pass();
		break;

	default:
		break;
	}
}

static void tIsr_entry_lock(const void *p)
{
	k_mutex_lock((struct k_mutex *)p, K_NO_WAIT);
}

static void tIsr_entry_unlock(const void *p)
{
	k_mutex_unlock((struct k_mutex *)p);
}

static void tThread_entry_negative(void *p1, void *p2, void *p3)
{
	int choice = case_type;

	TC_PRINT("case(%d) runs\n", case_type);

	/* Set up the fault or assert are expected before we call
	 * the target tested funciton.
	 */
	switch (choice) {
	case MUTEX_INIT_NULL:
		ztest_set_fault_valid(true);
		k_mutex_init(NULL);
		break;
	case MUTEX_LOCK_NULL:
		ztest_set_fault_valid(true);
		k_mutex_lock(NULL, K_NO_WAIT);
		break;
	case MUTEX_UNLOCK_NULL:
		ztest_set_fault_valid(true);
		k_mutex_unlock(NULL);
		break;
	case MUTEX_LOCK_IN_ISR:
		k_mutex_init(&mutex);
		ztest_set_assert_valid(true);
		irq_offload(tIsr_entry_lock, (void *)p1);
		break;
	case MUTEX_UNLOCK_IN_ISR:
		k_mutex_init(&mutex);
		ztest_set_assert_valid(true);
		irq_offload(tIsr_entry_unlock, (void *)p1);
	default:
		TC_PRINT("should not be here!\n");
		break;
	}

	/* If negative comes here, it means error condition not been
	 * detected.
	 */
	ztest_test_fail();
}

static int create_negative_test_thread(int choice)
{
	int ret;
	uint32_t perm = K_INHERIT_PERMS;

	if (_is_user_context()) {
		perm = perm | K_USER;
	}
	case_type = choice;

	k_tid_t tid = k_thread_create(&tdata, tstack, STACK_SIZE,
			(k_thread_entry_t)tThread_entry_negative,
			&mutex, NULL, NULL,
			K_PRIO_PREEMPT(THREAD_TEST_PRIORITY),
			perm, K_NO_WAIT);

	ret = k_thread_join(tid, K_FOREVER);

	return ret;
}

/* TESTPOINT: Pass a null pointer into the API k_mutex_init */
void test_mutex_init_null(void)
{
	create_negative_test_thread(MUTEX_INIT_NULL);
}
/* TESTPOINT: Pass a null pointer into the API k_mutex_lock */
void test_mutex_lock_null(void)
{
	create_negative_test_thread(MUTEX_LOCK_NULL);
}

/* TESTPOINT: Pass a null pointer into the API k_mutex_unlock */
void test_mutex_unlock_null(void)
{
	create_negative_test_thread(MUTEX_UNLOCK_NULL);
}

/* TESTPOINT: Try to lock mutex in isr context */
void test_mutex_lock_in_isr(void)
{
	create_negative_test_thread(MUTEX_LOCK_IN_ISR);
}

/* TESTPOINT: Try to unlock mutex in isr context */
void test_mutex_unlock_in_isr(void)
{
	create_negative_test_thread(MUTEX_UNLOCK_IN_ISR);
}

void test_mutex_unlock_count_unmet(void)
{
	struct k_mutex tmutex;

	k_mutex_init(&tmutex);
	zassert_true(k_mutex_lock(&tmutex, K_FOREVER) == 0,
			"current thread failed to lock the mutex");

	/* Verify an assertion will be trigger if lock_count is zero while
	 * the lock owner try to unlock mutex.
	 */
	tmutex.lock_count = 0;
	ztest_set_assert_valid(true);
	k_mutex_unlock(&tmutex);
}


/*test case main entry*/
void test_main(void)
{
	k_thread_access_grant(k_current_get(), &tdata, &tstack, &mutex);

	ztest_test_suite(mutex_api,
		 ztest_unit_test(test_mutex_lock_in_isr),
		 ztest_unit_test(test_mutex_unlock_in_isr),
		 ztest_user_unit_test(test_mutex_init_null),
		 ztest_user_unit_test(test_mutex_lock_null),
		 ztest_user_unit_test(test_mutex_unlock_null),
		 ztest_unit_test(test_mutex_unlock_count_unmet)
		 );
	ztest_run_test_suite(mutex_api);
}
