#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/random.h>

#define NUM_OF_ITERS 100
#define NUM_OF_THREAD 4
#define BILLION 1000000
#define KEY_RANGE = 1000;

#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos -> next)
	
#define list_for_each_entry(pos, head, member) \
	for (pos = list_first_entry(head, typeof(*pos), member); \
	&pos->member != (head); \
	pos = list_next_entry(pos, member))
	
struct my_node {
	struct list_head list;
	int data;
};

volatile int sum;
struct list_head my_list;
struct my_node *current_node;
struct list_head *p;


unsigned long long calclock3(struct timespec *spclock){
	long temp, temp_n;
	unsigned long long timedelay = 0;
	if(spclock[1].tv_nsec >= spclock[0].tv_nsec){
		temp = spclock[1].tv_sec - spclock[0].tv_sec;
		temp_n = spclock[1].tv_nsec - spclock[0].tv_nsec;
		timedelay = BILLION * temp + temp_n;
	}
	else {
		temp = spclock[1].tv_sec - spclock[0].tv_sec - 1;
		temp_n = BILLION + spclock[1].tv_nsec - spclock[0].tv_nsec;
		timedelay = BILLION * temp + temp_n;
	}
	
	return timedelay;
}









void ThreadFunc(void *data){
	int*arg = (int*)data;
	
	int i, n;
	int key;
	for(int i = 0; i < NUM__OF_ITERS; i++){
		n = get_random_bytes(&n, sizeof(int));
		key = n % KEY_RANGE;
		switch(n % 3){
		case 0: 
			addlist(key);
			break;
		case 1: 
			removelist(key);
			break;
		case 2: 
			containslist(key);
			break;
		default:
			printk("Error");
			return -1;
		
		}
	}
	
	kfree(arg);
}




int test(){
	INIT_LIST_HEAD(&my_list);
	pthread_t p_thread[16];
	struct timespec spclock[2];
	
	//for(int tot_thread = 1; tot_thread <= 16; tot_thread *= 2){
	sum = 0;
	
	getnstimeofday(&spclock[0]);
	 
	 int i;
	 for(i = 0; i < NUM_OF_THREAD; i++){
	 	int* arg = (int*)kmalloc(sizeof(int), GFP_KERNEL);
		*arg = i;
	 	thr_id = pthread_create(&p_thread[i], NULL, ThreadFunc, (void*)arg);
	 	
	 	if(thr_ic < 0){
	 		perror("thread create error: ");
	 		return -1;
	 	}
	 }
	
	for(i = 0; i < NUM_OF_THREAD; i++){
		pthread_join(p_thread[i], (void **)&status);
	}
	
	getnstimeofday(&spclock[1]);
	printk("Thread[%d}, total_times: %lldns\n", NUM_OF_THREADS, calclock(spclock));
	
	return 0;
}


int __init hello_parkmodule_init(void){
	test();
	
	printk("Time Checking Module\n");
	return 0;
}

void __exit hello_parkmodule_cleanup(void){
	printk("Bye\n");
}

module_init(hello_parkmodule_init);
module_exit(hello_parkmodule_cleanup);
MODULE_LICENSE("GPL");
