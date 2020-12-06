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

#define NUM_OF_ITERS 20
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
	struct my_node *next;
	int data;
	struct mutex nlock;
	//int (*lock)(void);
	//int (*unlock)(void);
};


struct my_node *current_node;
struct my_node *head;
struct my_node *tail;


int finishthread;
int marked;



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
}

void init(void){
	struct my_node *ptr;
	while (head->next != tail){
		ptr = head->next;
		head->next = head->next->next;
		kfree(ptr);
	}
}

unsigned long long calclock(struct timespec *spclock){
	long temp, temp_n;
	unsigned long long timedelay = 0;
	if(spclock[1].tv_nsec >= spclock[0].tv_nsec){
		temp = spclock[1].tv_sec - spclock[0].tv_sec;
		temp_n = spclock[1].tv_nsec - spclock[0].tv_nsec;
		timedelay = BILLION/1000 * temp + temp_n/1000;
	}
	else {
		temp = spclock[1].tv_sec - spclock[0].tv_sec - 1;
		temp_n = 1000000000 + spclock[1].tv_nsec - spclock[0].tv_nsec;
		timedelay = BILLION/1000 * temp + temp_n/1000;
	}
	
	return timedelay;
}

int addlist(int key, void *data){
	int*arg = (int*)data;
	printk("8");
	struct my_node *pred;
	struct my_node *curr;
	printk("9");
	pred = head;
	
	curr = pred->next;
	while(curr->data < key){
		if(curr->data == 0x7FFFFFFF) break;
		pred = curr;
		curr = curr->next;
	}
	mutex_lock(&pred->nlock);
	mutex_lock(&curr->nlock);
	printk("10");
	if (validate(pred, curr)){
		printk("11")
		if(key == curr->data){
			printk("12");
			mutex_unlock(&curr->nlock);
			mutex_unlock(&pred->nlock);
			return 0;
		}
	} else {
		printk("13");
		struct my_node *new = kmalloc(sizeof(struct my_node), GFP_KERNEL);
		new->data = key;
		new->next = curr;
		pred->next = new;
		mutex_init(&new->nlock);
		printk("13");
		mutex_unlock(&curr->nlock);
		mutex_unlock(&pred->nlock);
		printk("Add list, key: %d, thread %d", key, *arg);
		return 0;
	}
}

/*
int removelist(int key, void *data){
	int*arg = (int*)data;

	struct my_node *pred;
	struct my_node *curr;
	printk("6");
	printk("7");
	pred = head;
	mutex_lock(&my_mutex);
	
	curr = pred->next;
	while(curr->data < key){
		pred = curr;
		if(curr->data == 0x7FFFFFFF) break;
		curr = curr->next;
	}
	if(key == curr->data){
		pred->next = curr->next;
		kfree(curr);
		mutex_unlock(&my_mutex);
		return 0;
	}
	else{
		mutex_unlock(&my_mutex);
		return 0;
	}
}
	

int containlist(int key, void *data){
	int*arg = (int*)data;
	struct my_node *pred;
	struct my_node *curr;
	printk("4");
	printk("5");
	pred = head;
	mutex_lock(&my_mutex);

	curr = pred->next;
	while(curr->data < key){
		pred = curr;
		if(curr->data == 0x7FFFFFFF) break;
		curr = curr->next;
	}
	if(key == curr->data){
		mutex_unlock(&my_mutex);
		return 0;
	}
	else{
		mutex_unlock(&my_mutex);
		return 0;
	}

}*/

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
		printk("3");
		addlist(key, (void*)arg);
		/*switch(key % 3){
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
		
		}*/
	}
	finishthread++;
	kfree(arg);
	return 0;
}




int test(void){
	int thr_id;
	struct timespec spclock[2];
	
	printk("0");

	printk("1");
	start();
	int tot_thread;
	for(tot_thread = 1; tot_thread <= 32; tot_thread *= 2){
		init();
		finishthread = 0;
		
		printk("2");
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
		printk("Thread[%d], total_times: %15llums\n", tot_thread, calclock(spclock));
		
	}
	
	/*struct my_node *p = head -> next;
	while(p != &tail){
		printk("%d\n", p->data);
		if(p->data == 0x7FFFFFFF) break;
		p = p->next;
	}*/
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
