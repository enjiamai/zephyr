/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>
#include <irq_offload.h>
#include <syscall_handler.h>
#include <ztest_error_hook.h>


static ZTEST_BMEM int case_type;

/* A semaphore using inside irq_offload */
extern struct k_sem offload_sem;

/* test case type */
enum {
	TEST_ZTEST_CATCH_ASSERT_FAIL,
	TEST_ZTEST_CATCH_FATAL_ERROR,
	TEST_ZTEST_CATCH_ASSERT_IN_ISR,
	TEST_ZTEST_CATCH_FATAL_IN_ISR,
	TEST_ZTEST_CATCH_Z_OOPS,
	NOT_DEFINE
} error_case;


static void func_assert_foo(void *a)
{
	__ASSERT(a != NULL, "parameter a should not be NULL!");
}

static void func_fault_foo(void *a)
{
	/* execute a function by it's pointer */
	((void(*)(void))&a)();
}

static void release_offload_sem(void)
{
	/* Semaphore used inside irq_offload need to be
	 * released after assert or fault happened.
	 */
	k_sem_give(&offload_sem);
}

/* This is the fatal error hook that allow you to do actions after
 * fatal error happened. This is optional, you can choose to define
 * this yourself. If not, it will use the default one.
 */
void ztest_post_fatal_error_hook(unsigned int reason,
		const z_arch_esf_t *pEsf)
{
	switch (case_type) {
	case TEST_ZTEST_CATCH_FATAL_ERROR:
	case TEST_ZTEST_CATCH_Z_OOPS:
		zassert_true(true, NULL);
		break;

	/* Unfortunately, the case of trigger a fatal error
	 * inside ISR context still cannot be dealed with,
	 * So please don't use it this way.
	 */
	case TEST_ZTEST_CATCH_FATAL_IN_ISR:
		zassert_true(false, NULL);
		break;
	default:
		zassert_true(false, NULL);
		break;
	}
}

/* This is the assert fail post hook that allow you to do actions after
 * assert fail happened. This is optional, you can choose to define
 * this yourself. If not, it will use the default one.
 */
void ztest_post_assert_fail_hook(void)
{
	switch (case_type) {
	case TEST_ZTEST_CATCH_ASSERT_FAIL:
		ztest_test_pass();
		break;
	case TEST_ZTEST_CATCH_ASSERT_IN_ISR:
		release_offload_sem();
		ztest_test_pass();
		break;

	default:
		ztest_test_fail();
		break;
	}
}
/**
 * @brief Test if an assert works
 *
 * @details Valid the assert in thread context works. If the assert fail
 * happened and the program enter assert_post_handler, that means
 * assert works as expected.
 */
void test_catch_assert_fail(void)
{
	case_type = TEST_ZTEST_CATCH_ASSERT_FAIL;

	ztest_set_assert_valid(false);
	ztest_set_assert_valid(true);
	func_assert_foo(NULL);
}

/**
 * @brief Test if a fatal error can be catched
 *
 * @details Valid a fatal error we triggered in thread context works.
 * If the fatal error happened and the program enter assert_post_handler,
 * that means fatal error triggered as expected.
 */
void test_catch_fatal_error(void)
{
	case_type = TEST_ZTEST_CATCH_FATAL_ERROR;

	ztest_set_fault_valid(false);
	ztest_set_fault_valid(true);
	func_fault_foo(NULL);
}

/* a handler using by irq_offload  */
static void tIsr_assert(const void *p)
{
	ztest_set_assert_valid(true);
	func_assert_foo(NULL);
}

/**
 * @brief Test if an assert fail works in ISR context
 *
 * @details Valid the assert in ISR context works. If the assert fail
 * happened and the program enter assert_post_handler, that means
 * assert works as expected.
 */
void test_catch_assert_in_isr(void)
{
	case_type = TEST_ZTEST_CATCH_ASSERT_IN_ISR;
	irq_offload(tIsr_assert, NULL);
}


#if defined(CONFIG_USERSPACE)
static void func_z_oops_foo(void *a)
{
	Z_OOPS(*((bool *)a));
}

/**
 * @brief Test if a z_oops can be catched
 *
 * @details Valid a z_oops we triggered in thread context works.
 * If the z_oops happened and the program enter our handler,
 * that means z_oops triggered as expected. This test only for
 * userspace.
 */
void test_catch_z_oops(void)
{
	case_type = TEST_ZTEST_CATCH_Z_OOPS;

	ztest_set_fault_valid(true);
	func_z_oops_foo((void *)false);
}
#endif


void test_main(void)
{

#if defined(CONFIG_USERSPACE)
	ztest_test_suite(error_hook_tests,
			 ztest_user_unit_test(test_catch_assert_fail),
			 ztest_user_unit_test(test_catch_fatal_error),
			 ztest_unit_test(test_catch_assert_in_isr),
			 ztest_user_unit_test(test_catch_z_oops)
			 );
	ztest_run_test_suite(error_hook_tests);
#else
	ztest_test_suite(error_hook_tests,
			 ztest_unit_test(test_catch_assert_fail),
			 ztest_unit_test(test_catch_fatal_error),
			 ztest_unit_test(test_catch_assert_in_isr)
			 );
	ztest_run_test_suite(error_hook_tests);
#endif
}
