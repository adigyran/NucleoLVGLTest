#include "msgq_demo.hpp"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <cstdint>
 
#define MSGQ_STACK_SIZE 1024
#define MSGQ_THREAD_PRIO 7
LOG_MODULE_REGISTER(msgq_demo, LOG_LEVEL_INF);
 
K_MSGQ_DEFINE(cpp_msgq, sizeof(uint32_t), 16, 4);
K_THREAD_STACK_DEFINE(msgq_prod_stack, MSGQ_STACK_SIZE);
K_THREAD_STACK_DEFINE(msgq_cons_stack, MSGQ_STACK_SIZE);
static struct k_thread msgq_prod_thread;
static struct k_thread msgq_cons_thread;
static bool msgq_started;
 
static void producer(void *p1, void *p2, void *p3)
{
	uint32_t seq = 0;
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
 
	while (true) {
		++seq;
		(void)k_msgq_put(&cpp_msgq, &seq, K_NO_WAIT);
		k_msleep(250);
	}
}
 
static void consumer(void *p1, void *p2, void *p3)
{
	uint32_t value;
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
 
	while (true) {
		(void)k_msgq_get(&cpp_msgq, &value, K_FOREVER);
	}
}
 
void start_msgq_demo()
{
	if (msgq_started) {
		return;
	}
 
	k_thread_create(&msgq_prod_thread, msgq_prod_stack, K_THREAD_STACK_SIZEOF(msgq_prod_stack),
			producer, NULL, NULL, NULL, MSGQ_THREAD_PRIO, 0, K_NO_WAIT);
	k_thread_name_set(&msgq_prod_thread, "msgq_prod");
 
	k_thread_create(&msgq_cons_thread, msgq_cons_stack, K_THREAD_STACK_SIZEOF(msgq_cons_stack),
			consumer, NULL, NULL, NULL, MSGQ_THREAD_PRIO, 0, K_NO_WAIT);
	k_thread_name_set(&msgq_cons_thread, "msgq_cons");
 
	msgq_started = true;
	LOG_INF("Message queue demo started");
}