

// System Includes
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <pthread.h>
#include <sched.h>
// Local Includes
#include "op_sched.h"
#include "vm_support.h"
#include "vm_process.h"

// Feel free to use these in your code, if you like! 
#define CRITICAL_FLAG   (1 << 31) 
#define LOW_FLAG        (1 << 30) 
#define READY_FLAG      (1 << 29)
#define DEFUNCT_FLAG    (1 << 28)

/* Feel free to create any helper functions you like! */

/* Initializes the Op_schedule_s Struct and all of the Op_queue_s Structs
 * Follow the project documentation for this function.
 * Returns a pointer to the new Op_schedule_s or NULL on any error.
 */
Op_schedule_s *op_create() {
       
	/** malloc schedule **/
	Op_schedule_s *schedule = malloc(sizeof(Op_schedule_s));
       	if(!schedule){
	       return NULL;
       	}
       
       	/** malloc queues **/
       	schedule->ready_queue_high = malloc(sizeof(Op_queue_s));  
       	schedule->ready_queue_low = malloc(sizeof(Op_queue_s));
       	schedule->defunct_queue = malloc(sizeof(Op_queue_s));
       
       	if(!schedule->ready_queue_high || !schedule->ready_queue_low || !schedule->defunct_queue){

	       return NULL;
       	}
       
	/** set data members of queues **/
       	schedule->ready_queue_high->count = 0;
       	schedule->ready_queue_high->head = NULL;


       	schedule->ready_queue_low->count = 0;
       	schedule->ready_queue_low->head = NULL;

       	schedule->defunct_queue->count = 0;
       	schedule->defunct_queue->head = NULL;

       	return schedule;

}

/* Create a new Op_process_s with the given information.
 * - Malloc and copy the command string, don't just assign it!
 * Follow the project documentation for this function.
 * Returns the Op_process_s on success or a NULL on any error.
 */
Op_process_s *op_new_process(char *command, pid_t pid, int is_low, int is_critical) {
  
	/** malloc process **/
	Op_process_s *process = malloc(sizeof(Op_process_s));

	if(!process){
		return NULL;
	}

	/** set data of process ***/
	process->pid = pid;
	process->cmd = (char*) malloc(sizeof(char)*strlen(command));

	if(!process->cmd){

		return NULL;
	}

	strncpy(process->cmd,command, strlen(command));

	process->next = NULL;

        /** set process state **/
	process->state = 0;
        
	process->state = process->state | READY_FLAG;
        process->state = process->state &  ~DEFUNCT_FLAG;
        	
	/** Can't be low and critical! **/
	if((is_low != 0) && (is_critical != 0)){

		return NULL;
	}

        if(is_low != 0){

		process->state = process->state | LOW_FLAG;
	}	
	else if(is_low == 0){

		 process->state = process->state & ~LOW_FLAG;
        }

        if(is_critical != 0){

                 process->state = process->state | CRITICAL_FLAG;       
     	}
        else if(is_critical == 0){

	         process->state = process->state & ~CRITICAL_FLAG;
       	}

	return process; 


}

/** HELPER METHOD: adds process to end of given queue **/
void add_p( Op_queue_s *queue, Op_process_s *process){

        Op_process_s *cur = queue->head;

	/** if list is empty **/
        if(queue->count == 0 || queue->head == NULL){

                queue->head = process;
                process->next = NULL;

                queue->count += 1;
                return;

        }

	/** traverses queue to reach last node **/
        while(cur->next!= NULL){

                cur = cur->next;
        }

        cur->next = process;
        process->next = NULL;

        queue->count += 1;

        return;

}

/** HELPER METHOD: removes and returns process from given queue **/
Op_process_s *remove_p( Op_queue_s *queue, Op_process_s *process){

        Op_process_s *cur;
        Op_process_s *prev;
        Op_process_s *p;


        /** if process is the head **/
	if(process->pid == queue->head->pid){

                cur = queue->head;
                queue->head = queue->head->next;
                queue->count -= 1;


                return cur;
        }

	/** if process is not the head **/
        prev = queue->head;
        cur = prev->next;
        
        while(cur != NULL){
                if(process->pid == cur->pid){

                        p = process;
                        prev->next = cur->next;
                        queue->count -= 1;
                        return p;
                }

                prev = cur;
                cur = cur->next;
        }
	
	return NULL;
}


/* Adds a process into the appropriate singly linked list queue.
 * Follow the project documentation for this function.
 * Returns a 0 on success or a -1 on any error.
 */
int op_add(Op_schedule_s *schedule, Op_process_s *process) {
        
	if(!schedule || !process){
		return -1;
	}	
	
	/** sets flags **/
	process->state = process->state | READY_FLAG;
	process->state = process->state &  ~DEFUNCT_FLAG;
       
	/** picks a queue **/
	if((process->state & LOW_FLAG) !=0){
		
		add_p(schedule->ready_queue_low, process);

	}
	else{
		add_p(schedule->ready_queue_high, process);
        } 

	return 0;
}



/* Returns the number of items in a given Op_queue_s
 * Follow the project documentation for this function.
 * Returns the number of processes in the list or -1 on any errors.
 */
int op_get_count(Op_queue_s *queue) {
        if(!queue){
		
		return -1;

	}
	if(queue->count < 0){
                return -1;
	}
      	return queue->count;
}

/* Selects the next process to run from the High Ready Queue.
 * Follow the project documentation for this function.
 * Returns the process selected or NULL if none available or on any errors.
 */
Op_process_s *op_select_high(Op_schedule_s *schedule) {
  
	
	if(!schedule){
		return NULL;
	}

	if(!schedule->ready_queue_high){
		return NULL;
	}

	
        Op_process_s *pointer;
        Op_process_s *cur;	
	Op_process_s *prev;
        Op_queue_s *queue = schedule->ready_queue_high;
		

	if(queue->count == 0){
		return NULL;
	}

	pointer = queue->head;
	cur = pointer;
        
	/** if the head is critical **/
        if(queue->count == 1 || (queue->head->state & CRITICAL_FLAG) !=0 ){
		
		queue->head = queue->head->next;
		queue->count -= 1 ;
		pointer->age = 0;
		pointer->next = NULL;

		return pointer;

	}
        
	prev = cur;
        
	/** if the middle or last node is critical **/
	while(cur != NULL){

		if((cur->state & CRITICAL_FLAG) !=0){            
			
			prev->next = cur->next;
			queue->count -= 1;
                        pointer->age = 0;
		        pointer->next = NULL;

			return pointer;
		}
		prev= cur;
		cur = cur->next;
		pointer = cur;
	}
	
	/** there are no cirtical process so remove head **/
	pointer = queue->head;
        queue->head = queue->head->next;
	queue->count -= 1;
	pointer->age = 0;
        pointer->next = NULL;

        return pointer;

}

/* Schedule the next process to run from the Low Ready Queue.
 * Follow the project documentation for this function.
 * Returns the process selected or NULL if none available or on any errors.
 */
Op_process_s *op_select_low(Op_schedule_s *schedule) {

	if(!schedule){

		return NULL;
	}	

	if(!schedule->ready_queue_high){
		               
		return NULL;
	}

	if(!schedule->ready_queue_low || schedule->ready_queue_low->count == 0){

		return NULL;
	}

	Op_queue_s *queue = schedule->ready_queue_low;
        Op_process_s *pointer = queue->head;
	
	/** remove head **/
	queue->head = queue->head->next;
	queue->count -= 1;
	pointer->age = 0;
	pointer->next = NULL;
	
	return pointer;
	
}

/* Add age to all processes in the Ready - Low Priority Queue, then
 *  promote all processes that are >= MAX_AGE.
 * Follow the project documentation for this function.
 * Returns a 0 on success or -1 on any errors.
 */
int op_promote_processes(Op_schedule_s *schedule) {
  
	if(!schedule){
		return -1;
	}

	if(!schedule->ready_queue_low || !schedule->ready_queue_high){

		return -1;
	}

        if(schedule->ready_queue_low->count == 0){
		return 0;
	}
	
	Op_queue_s *low_queue = schedule->ready_queue_low;
        Op_process_s *pointer = low_queue->head;
        Op_process_s *prev;
        Op_process_s *cur = pointer;
        
	/** update age **/
        while(cur != NULL){

		cur->age += 1;
	        cur = cur->next;
	}        
        
	prev = pointer;
	cur = prev;
        
	/** check if max age reached **/
	while(cur != NULL){

        	if(cur->age >= MAX_AGE){
			
			/** remove starved process **/
			pointer = remove_p(schedule->ready_queue_low, cur);
			pointer->age = 0;
			pointer->next = NULL;

			/** add to high queue **/
			add_p(schedule->ready_queue_high, pointer);
		}	

		 prev = cur;
		 cur = cur->next;
		 
	}
	return 0;
}



/* This is called when a process exits normally.
 * Put the given node into the Defunct Queue and set the Exit Code 
 * Follow the project documentation for this function.
 * Returns a 0 on success or a -1 on any error.
 */
int op_exited(Op_schedule_s *schedule, Op_process_s *process, int exit_code) {
  
	if(!schedule || !process){

		return -1;
	}
        if(schedule->defunct_queue == NULL){

		return -1;
	}
        
	
	unsigned int x = 0x0FFFFFFF;
        
        /** set flags **/
        process->state = process->state & ~READY_FLAG;
	process->state = process->state |  DEFUNCT_FLAG;
	
	process->state = process->state & ~x;  /** resets exit code bits to 0 **/
	process->state = process->state | exit_code; /** updates new exit code **/

	/** add to defunct queue **/
	add_p(schedule->defunct_queue, process); 
	
	return -1;
}

/* This is called when the OS terminates a process early.
 * Remove the process with matching pid from Ready High or Ready Low and add the Exit Code to it.
 * Follow the project documentation for this function.
 * Returns a 0 on success or a -1 on any error.
 */
int op_terminated(Op_schedule_s *schedule, pid_t pid, int exit_code) {
        
	if(!schedule || !schedule->ready_queue_low || !schedule->ready_queue_high){

		return -1;
	}
        
	Op_queue_s *low_queue = schedule->ready_queue_low;
        Op_queue_s *high_queue = schedule->ready_queue_high;
	Op_process_s *pointer = low_queue->head;
	Op_process_s *cur = pointer;
	
       

        int check = -1; 
        
	/** search low queue **/
	if(low_queue->count != 0){

		while(cur != NULL && check != 0){

                	if(cur->pid == pid){
                        
				pointer = remove_p(schedule->ready_queue_low, cur);
				check = 0;
				
			}
                	cur = cur->next;
		}	
	}
	
	/** search high queue **/
	if( check != 0 && high_queue->count !=0){

		while( cur != NULL && check != 0){

			if(cur->pid == pid){

			        check  = 0;
				pointer = remove_p(high_queue, cur);
			}
			cur = cur->next;
		}
	}	
        
        /** if process was found and removed **/	
        if( check == 0){

		pointer->next = NULL;
	        return	op_exited(schedule, pointer, exit_code);
	}
	
	return -1;
}

/** HELPER METHOD: frees processes in a queue **/
void free_q(Op_process_s *process){
        
	Op_process_s *p;

        if(process == NULL){
                return;
        }

        while(process != NULL){

                p = process;
                process = process->next;
		free(p->cmd);
                free(p);
        }

}

/* Frees all allocated memory in the Op_schedule_s, all of the Queues, and all of their Nodes.
 * Follow the project documentation for this function.
 */
void op_deallocate(Op_schedule_s *schedule) {
  
	if(!schedule){
             return;
	}

	if(schedule->ready_queue_low != NULL){
 	
		free_q(schedule->ready_queue_low->head);
		free(schedule->ready_queue_low);
	}

	if(schedule->ready_queue_high != NULL){

		free_q(schedule->ready_queue_high->head); 
		free(schedule->ready_queue_high);
	}
	

	if(schedule->defunct_queue != NULL){

		free_q(schedule->defunct_queue->head);
                free(schedule->defunct_queue);

	}
        
	free(schedule);
	
	return;
}
