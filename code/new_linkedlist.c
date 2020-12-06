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
#include <linux/atomic.h>

#define NUM_OF_ITERS 10000
#define NUM_OF_THREADS 1
#define BILLION 1000000
#define KEY_RANGE 1000

// node structure with mutex lock
struct my_node {
	struct my_node *next;
	int data;
	struct mutex nlock;
};

// head and tail
struct my_node *head;
struct my_node *tail;

// freelist 
struct my_node *freelist;
struct my_node freetail;
struct mutex freelock;

int finishthread;

bool validate(struct my_node *pred, struct my_node *curr){
	struct my_node *m_node = head;
	while (m_node->data <= pred->data){
		if (m_node == pred){
			return pred->next == curr;
		}
		m_node = m_node->next;
	}
	return false;
}


void start(void){
	head = kmalloc(sizeof(struct my_node), GFP_KERNEL);
	tail = kmalloc(sizeof(struct my_node), GFP_KERNEL);
	head->data = 0x80000000;
	tail->data = 0x7FFFFFFF;
	head->next = tail;
	mutex_init(&head->nlock);
	mutex_init(&tail->nlock);
	freetail.data = 0x7FFFFFFF;
	freelist = &freetail;
	mutex_init(&freelock);
}

void init(void){
	struct my_node *ptr;
	while (head->next != tail){
		ptr = head->next;
		head->next = head->next->next;
		kfree(ptr);
	}
}

void recycle_freelist(void){
	struct my_node *ptr = freelist;
	while (ptr != &freetail){
		struct my_node *n = ptr->next;
		kfree(ptr);
		ptr = n;
	}
	freelist = &freetail;
}

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
		temp_n = 1000000000 + spclock[1].tv_nsec - spclock[0].tv_nsec;
		timedelay = BILLION * temp + temp_n;
	}
	
	return timedelay;
}

int addlist(int key, void *data){
	int *arg = (int*)data;

	struct my_node *pred;
	struct my_node *curr;
	pred = head;
	
	curr = pred->next;
	while(curr->data < key){
		pred = curr;
		if(curr->data == 0x7FFFFFFF) break;
		curr = curr->next;
	}
	mutex_lock(&pred->nlock);
	mutex_lock(&curr->nlock);

	if (validate(pred, curr)){
		if(key == curr->data){
			mutex_unlock(&curr->nlock);
			mutex_unlock(&pred->nlock);
			//printk("key already exists: %d, thread %d", key, *arg);
			return 0;
		} else {
			struct my_node *new = kmalloc(sizeof(struct my_node), GFP_KERNEL);
			new->data = key;
			new->next = curr;
			pred->next = new;
			mutex_init(&new->nlock);
			mutex_unlock(&curr->nlock);
			mutex_unlock(&pred->nlock);
			//printk("Add list, key: %d, thread %d", key, *arg);
			return 1;
		}
	} 
	mutex_unlock(&curr->nlock);
	mutex_unlock(&pred->nlock);
	//printk("cannot insert key: %d, thread %d", key, *arg);
	return 0;
}


int removelist(int key, void *data){
	int *arg = (int*)data;
	struct my_node *pred;
	struct my_node *curr;
	pred = head;
	curr = pred->next;
	while(curr->data < key){
		pred = curr;
		if(curr->data == 0x7FFFFFFF) break;
		curr = curr->next;
	}
	mutex_lock(&pred->nlock);
	mutex_lock(&curr->nlock);
	
	if (validate(pred, curr)){
		if(key == curr->data){
			pred->next = curr->next;
			mutex_lock(&freelock);
			freelist = curr;
			mutex_unlock(&freelock);
			mutex_unlock(&curr->nlock);
			mutex_unlock(&pred->nlock);
			//printk("delete list, key: %d, thread %d", key, *arg);		
			return 1;
		} else {
			mutex_unlock(&curr->nlock);
			mutex_unlock(&pred->nlock);
			//printk("not existed key: %d, thread %d", key, *arg);
			return 0;
		}
	} 
	mutex_unlock(&curr->nlock);
	mutex_unlock(&pred->nlock);
	//printk("cannot delete key: %d, thread %d", key, *arg);
	return 0;
}
	

int containlist(int key, void *data){
	int*arg = (int*)data;
	struct my_node *pred;
	struct my_node *curr;
	pred = head;
	curr = pred->next;
	while(curr->data < key){
		pred = curr;
		if(curr->data == 0x7FFFFFFF) break;
		curr = curr->next;
	}
	mutex_lock(&pred->nlock);
	mutex_lock(&curr->nlock);
	if (validate(pred, curr)){
		if (key == curr->data){
			mutex_unlock(&curr->nlock);
			mutex_unlock(&pred->nlock);
			//printk("list contains key: %d, thread %d", key, *arg);
			return 1;
		} else {
			mutex_unlock(&curr->nlock);
			mutex_unlock(&pred->nlock);
			//printk("not existed key: %d, thread %d", key, *arg);
			return 1;
		}
	} 
	mutex_unlock(&curr->nlock);
	mutex_unlock(&pred->nlock);
	//printk("cannot find key: %d, thread %d", key, *arg);
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
		//printk("key: %d", key);
		switch(key % 3){
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
	struct timespec spclock[2];
	
	start();
	int tot_thread;
	for(tot_thread = 1; tot_thread <= 32; tot_thread *= 2){
		init();
		finishthread = 0;

		getnstimeofday(&spclock[0]);

		int i;
		for(i = 0; i < tot_thread; i++){
			int* arg = (int*)kmalloc(sizeof(int), GFP_KERNEL);
			*arg = i;
			kthread_run(&ThreadFunc, (void*)arg, "ThreadFunc");
		}		
		while(true){
			if(finishthread >= tot_thread){
				getnstimeofday(&spclock[1]);
				break;
			}
			msleep(1);
		}
		
		getnstimeofday(&spclock[1]);
		recycle_freelist();
		printk("Thread[%2d], total_times: %15lluns\n", tot_thread, calclock(spclock));
		
	}
	
	return 0;
}


int __init hello_parkmodule_init(void){
	printk("Starting Find-grained Test... \n");	
	test();

	return 0;
}

void __exit hello_parkmodule_cleanup(void){
	printk("Exiting Test... \n");
}

module_init(hello_parkmodule_init);
module_exit(hello_parkmodule_cleanup);
MODULE_LICENSE("GPL");
