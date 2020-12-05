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
#define NUM_OF_THREADS 1
#define BILLION 1000000
#define KEY_RANGE 1000

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

struct mutex my_mutex;
int finishthread;

struct task_struct *k_thread[NUM_OF_THREADS];

unsigned long long calclock(struct timespec *spclock){
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

int addlist(int key, void *data){
	int*arg = (int*)data;
	
	struct my_node *new = kmalloc(sizeof(struct my_node), GFP_KERNEL);
	new->data = key;
	
	mutex_lock(&my_mutex);
	list_for_each_entry(current_node, &my_list, list){
		if(current_node == key) {
			mutex_unlock(&my_mutex);
			return 0;
		}
	}
	list_add_tail(&new->list, &my_list);
	printk("Add list, key: %d, thread %d", key, *arg);
	mutex_unlock(&my_mutex);
	
	return 0;
}

int removelist(int key, void *data){
	int*arg = (int*)data;

	struct my_node *tmp;
	
	mutex_lock(&my_mutex);
	list_for_each_entry_safe(current_node, tmp, &my_list, list){
		if(current_node->data == key){
			printk("Delete list, key: %d, thread %d", key, *arg);
			list_del(&current_node->list);
			kfree(current_node);
			mutex_unlock(&my_mutex);
			return 0;
		}
	}
	mutex_unlock(&my_mutex);
	
	return 0;
}

int containlist(int key, void *data){
	int*arg = (int*)data;
	
	mutex_lock(&my_mutex);
	list_for_each_entry(current_node, &my_list, list){
		if(current_node == key) {
			printk("Contain list, key: %d, thread %d", key, *arg);
			mutex_unlock(&my_mutex);
			return 0;
		}
	}
	
	mutex_lock(&my_mutex);
	return 0;
}



int ThreadFunc(void *data){
	int*arg = (int*)data;
	
	int i;
	int n;
	int key;
	for(i = 0; i < NUM_OF_ITERS; i++){
		get_random_bytes(&n, sizeof(int));
		key = n % KEY_RANGE;
		if(key < 0) key = -key;
		printk("key: %d", key);
		
		switch(n % 3){
		case 0: 
			addlist(key, (void*)arg);
			break;
		case 1: 
			removelist(key, (void*)arg);
			break;
		case 2: 
			containlist(key, (void*)arg);
			break;
		default:
			printk("Error");
			return -1;
		
		}
	}
	finishthread++;
	kfree(arg);
	return 0;
}




int test(void){
	INIT_LIST_HEAD(&my_list);
	int thr_id;
	struct timespec spclock[2];
	
	//for(int tot_thread = 1; tot_thread <= 16; tot_thread *= 2){
	sum = 0;
	mutex_init(&my_mutex);
	
	getnstimeofday(&spclock[0]);
	 
	 int i;
	 for(i = 0; i < NUM_OF_THREADS; i++){
	 	int* arg = (int*)kmalloc(sizeof(int), GFP_KERNEL);
		*arg = i;
		kthread_run(&ThreadFunc, (void*)arg, "ThreadFunc");
	 }
	
	
	
	while(true){
		if(finishthread == NUM_OF_THREADS){
			getnstimeofday(&spclock[1]);
			break;
		}
		msleep(1);
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
