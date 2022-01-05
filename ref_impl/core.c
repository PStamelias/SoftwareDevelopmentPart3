#include "../include/core.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#define NUM_THREADS 10
#define SMALLEST_STACKSZ PTHREAD_STACK_MIN
#define SMALL_STACK (24*1024)
//Global Variables
struct HammingDistanceStruct* HammingDistanceStructNode;
Index*  BKTreeIndexEdit;
int bucket_sizeofHashTableExact;
JobScheduler* JobSchedulerNode;
struct Exact_Root* HashTableExact;
struct Stack_result* StackArray;
//used in EditDistance below
int min(int a, int b, int c){
    int m = a;
    if(b<m){
        m = b;
    }
    if(c<m){
        m = c;
    }
    return m;
}


int EditDistance(char* a, int na, char* b, int nb)
{
	int table[na+1][nb+1];
    int i,j;
    for(i=0; i<=na; i++){
        for(j=0; j<=nb; j++){
            if(i == 0){
                table[i][j] = j;
            }
            else if(j == 0){
                table[i][j] = i;
            }
            else if(a[i-1] == b[j-1]){
                table[i][j] = table[i-1][j-1];
            }
            else{
                table[i][j] = 1 + min(table[i-1][j], table[i][j-1], table[i-1][j-1]);
            }
        }
    }
    return table[na][nb];
}

unsigned int HammingDistance(char* a, int na, char* b, int nb)
{
	int i;
	int distance = 0;
	for(i=0; i<na; i++){
		if(a[i] != b[i]){
			distance++;
		}
	}
	return distance;
}

ErrorCode InitializeIndex(){
	printf("InitializeIndex\n");
	initialize_scheduler(NUM_THREADS);
	BKTreeIndexEdit=malloc(sizeof(Index));
	BKTreeIndexEdit->root=NULL;
	HashTableExact=NULL;
	int HammingIndexSize=(MAX_WORD_LENGTH-MIN_WORD_LENGTH)+1;
	HammingDistanceStructNode=NULL;
	HammingDistanceStructNode=malloc(sizeof(struct HammingDistanceStruct));
	HammingDistanceStructNode->word_RootPtrArray=malloc(HammingIndexSize*sizeof(struct word_RootPtr));
	for(int i=0;i<HammingIndexSize;i++){
		HammingDistanceStructNode->word_RootPtrArray[i].HammingPtr=NULL;
		HammingDistanceStructNode->word_RootPtrArray[i].word_length=4+i;
	}
	bucket_sizeofHashTableExact=5;/*starting bucket size of hash array*/
	HashTableExact=malloc(sizeof(struct Exact_Root));
	HashTableExact->array=malloc(bucket_sizeofHashTableExact*sizeof(struct Exact_Node*));
	for(int i=0;i<bucket_sizeofHashTableExact;i++)
		HashTableExact->array[i]=NULL;
	if(BKTreeIndexEdit==NULL) return EC_FAIL;
	if(HashTableExact==NULL) return EC_FAIL;
	if(HammingDistanceStructNode==NULL) return EC_FAIL;
	return EC_SUCCESS;
}

ErrorCode DestroyIndex(){
	destroy_scheduler(JobSchedulerNode);
	printf("DestroyIndex\n");
	int HammingIndexSize=(MAX_WORD_LENGTH-MIN_WORD_LENGTH)+1;
	for(int i=0; i<bucket_sizeofHashTableExact; i++){
		if(HashTableExact->array[i] == NULL) continue;
		struct Exact_Node* start = HashTableExact->array[i];
		struct Exact_Node* start_next = start->next;
		while(1){
			struct payload_node* p_start=start->beg;
			struct payload_node* p_start_next=p_start->next;
			while(1){
				free(p_start);
				p_start=p_start_next;
				if(p_start==NULL)
					break;
				p_start_next=p_start_next->next;
			}
			free(start->wd);
			free(start);
			start = start_next;
			if(start == NULL) break;
			start_next = start_next->next;
		}
	}
	free(HashTableExact->array);
	free(HashTableExact);
	destroy_Edit_index(BKTreeIndexEdit);
	for(int i=0;i<HammingIndexSize;i++)
		destroy_hamming_entry_index(HammingDistanceStructNode->word_RootPtrArray[i].HammingPtr);
	free(HammingDistanceStructNode->word_RootPtrArray);
	free(HammingDistanceStructNode);	
	return EC_SUCCESS;
}

ErrorCode StartQuery(QueryID query_id, const char* query_str, MatchType match_type, unsigned int match_dist)
{
	JobSchedulerNode->stage=0;
	printf("StartQuery\n");
	Job* JobNode=malloc(sizeof(Job));
	strcpy(JobNode->Job_Type,"StartQuery");
	JobNode->query_id=query_id;
	JobNode->doc_id=-1;
	JobNode->match_type=match_type;
	strcpy(JobNode->arg,query_str);
	JobNode->match_dist=match_dist;
	JobNode->next=NULL;
	JobNode->prev=NULL;
	submit_job(JobSchedulerNode,JobNode);
	return EC_SUCCESS;
}

ErrorCode EndQuery(QueryID query_id)
{
	printf("EndQuery\n");
	Job* JobNode=malloc(sizeof(Job));
	strcpy(JobNode->Job_Type,"EndQuery");
	JobNode->query_id=query_id;
	JobNode->doc_id=-1;
	JobNode->match_type=-1;
	strcpy(JobNode->arg,"");
	JobNode->match_dist=-1;
	JobNode->next=NULL;
	JobNode->prev=NULL;
	submit_job(JobSchedulerNode,JobNode);
	return EC_SUCCESS;
}

ErrorCode MatchDocument(DocID doc_id, const char* doc_str)
{
	printf("MatchDocument\n");
	JobSchedulerNode->stage=1;
	Job* JobNode=malloc(sizeof(Job));
	strcpy(JobNode->Job_Type,"MatchDocument");
	JobNode->query_id=-1;
	JobNode->doc_id=doc_id;
	JobNode->match_type=-1;
	strcpy(JobNode->arg,doc_str);
	JobNode->match_dist=-1;
	JobNode->next=NULL;
	JobNode->prev=NULL;
	submit_job(JobSchedulerNode,JobNode);
	return EC_SUCCESS;
}


ErrorCode GetNextAvailRes(DocID* p_doc_id, unsigned int* p_num_res, QueryID** p_query_ids)
{
	/*Get From top node its info and put it on input variables*/
	DocID doc=StackArray->top->doc_id;
	*p_doc_id=doc;
	unsigned int counter=StackArray->top->result_counter;
	*p_num_res=counter;
	QueryID* curr=malloc(StackArray->top->result_counter*sizeof(QueryID));
	for(int i=0;i<StackArray->top->result_counter;i++)
		curr[i]=StackArray->top->query_id[i];
	*p_query_ids=curr;
	/*Delete the top node from stack */
	Delete_From_Stack();
	return EC_SUCCESS;
}




/*Function that destroys all Nodes of Edit Index*/
ErrorCode destroy_Edit_index(Index* ix){
	if(ix==NULL) return EC_SUCCESS;
	destroy_Edit_nodes(ix->root);
	free(ix);
	ix=NULL;
	if(ix!=NULL) return EC_FAIL;	
	return EC_SUCCESS;
}


/*Destroy edit node and its children*/
void destroy_Edit_nodes(struct EditNode* node){
	if(node==NULL)
		return ;
	for(struct EditNode* first=node->firstChild;first!=NULL;first=first->next)
		destroy_Edit_nodes(first);
	struct Info* start=node->start_info;
	if(start==NULL){
		free(node->wd);
		free(node);
		return ;
	}
	struct Info* start_next=start->next;
	while(1){
		if(start==NULL)
			break;
		free(start);
		start=start_next;
		if(start==NULL)
			break;
		start_next=start_next->next;
	}
	free(node->wd);
	free(node);
}


/*Function that destroys all Nodes of Hamming Indexes*/
ErrorCode destroy_hamming_entry_index(struct HammingIndex* ix){
	if(ix==NULL) return EC_SUCCESS;
	destroy_hamming_nodes(ix->root);
	free(ix);
	ix=NULL;
	if(ix!=NULL) return EC_FAIL;	
	return EC_SUCCESS;
}

/*Destroy hamming node and its children*/
void destroy_hamming_nodes(struct HammingNode* node){
	for(struct HammingNode* first=node->firstChild;first!=NULL;first=first->next)
		destroy_hamming_nodes(first);
	struct Info* start=node->start_info;
	if(start==NULL){
		free(node->wd);
		free(node);
		return ;
	}
	struct Info* start_next=start->next;
	while(1){
		if(start==NULL)
			break;
		free(start);
		start=start_next;
		if(start==NULL)
			break;
		start_next=start_next->next;
	}
	free(node->wd);
	free(node);
}


/*For each query return the words of it to an array*/
char** words_ofquery(const char* query_str,int* num){
	char curr_words[MAX_QUERY_WORDS][MAX_WORD_LENGTH];
	int coun=0;
	char word[MAX_WORD_LENGTH];
	int len=0;
	for(int i=0;i<strlen(query_str);i++){
		if(query_str[i]==' '){
			word[len]='\0';
			len=0;
			strcpy(curr_words[coun],word);
			coun++;
			memset(word,0,MAX_WORD_LENGTH);
			continue;
		}
		if(i==strlen(query_str)-1){
			word[len++]=query_str[i];
			word[len]='\0';
			len=0;
			strcpy(curr_words[coun],word);
			coun++;
			memset(word,0,MAX_WORD_LENGTH);
			continue;
		}
		word[len++]=query_str[i];
	}
	char** returning_array=malloc(coun*sizeof(char*));
	for(int i=0;i<coun;i++)
		returning_array[i]=malloc(MAX_WORD_LENGTH*sizeof(char));
	for(int i=0;i<coun;i++)
		strcpy(returning_array[i],curr_words[i]);
	*num=coun;
	return returning_array;
}

void initialize_scheduler(int execution_threads){
	JobSchedulerNode=malloc(sizeof(JobScheduler));
	JobSchedulerNode->execution_threads=execution_threads;
	JobSchedulerNode->q=NULL;
	JobSchedulerNode->q=malloc(sizeof(Queue));
	JobSchedulerNode->work_finish=false;
	JobSchedulerNode->q->First=NULL;
	JobSchedulerNode->q->Last=NULL;
	JobSchedulerNode->tids=NULL;
	pthread_barrier_init(&JobSchedulerNode->barrier, NULL, JobSchedulerNode->execution_threads);
	JobSchedulerNode->tids=malloc(NUM_THREADS*sizeof(pthread_t));
	if(pthread_mutex_init(&JobSchedulerNode->lock1,NULL)!= 0)
        printf("\n mutex init has failed\n");
    JobSchedulerNode->stage=0;
    if(pthread_mutex_init(&JobSchedulerNode->mutex1,NULL)!= 0)
        printf("\n mutex init has failed\n");
    pthread_cond_init(&JobSchedulerNode->con1,NULL);
	for(int i=0;i<NUM_THREADS;i++)
		pthread_create(&JobSchedulerNode->tids[i], NULL, &threadFunc, NULL);
}

int submit_job(JobScheduler* sch,Job* j){
	pthread_mutex_lock(&JobSchedulerNode->lock1);
	if(sch->q->First==NULL){
		sch->q->First=j;
		sch->q->Last=j;
	}
	else{
		j->next=sch->q->Last;
		sch->q->Last->prev=j;
		sch->q->Last=j;
	}
    pthread_mutex_unlock(&JobSchedulerNode->lock1);
	return 0; 
}

int execute_all_jobs(JobScheduler* sch){
	Job* current_Job=NULL;
	if(sch->q->First==NULL)
		return 0;
	pthread_mutex_lock(&JobSchedulerNode->lock1);
	current_Job=sch->q->First;
	sch->q->First=sch->q->First->prev;
	pthread_mutex_unlock(&JobSchedulerNode->lock1);
	if(!strcmp(current_Job->Job_Type,"MatchDocument")){
		if(JobSchedulerNode->stage==1){
			pthread_barrier_wait(&JobSchedulerNode->barrier);
			JobSchedulerNode->stage=0;
		}
	}
	else if(!strcmp(current_Job->Job_Type,"StartQuery")){
		
	}
	else if(!strcmp(current_Job->Job_Type,"EndQuery")){
		
	}
	free(current_Job);
	return 0;
}

int wait_all_tasks_finish(JobScheduler* sch){
	if(sch->q->First==NULL&&sch->q->Last==NULL&&sch->work_finish==true)
		return 1;
	return 0;
}

int destroy_scheduler(JobScheduler* sch){
	JobSchedulerNode->work_finish=true;
	for(int i=0;i<NUM_THREADS;i++)
		pthread_join(JobSchedulerNode->tids[i], NULL);
	pthread_mutex_destroy(&JobSchedulerNode->lock1);
	pthread_mutex_destroy(&JobSchedulerNode->mutex1);
	pthread_cond_destroy(&JobSchedulerNode->con1);
	pthread_barrier_destroy(&JobSchedulerNode->barrier);
	free(sch->q);
	free(sch->tids);
	free(sch);
	return 0;
}

/*Delete the topp node of StackArray*/
void Delete_From_Stack(){
	struct result* node=StackArray->top;
	StackArray->top=StackArray->top->next;
	if(StackArray->top==NULL)
		StackArray->first=NULL;
	StackArray->counter--;
	free(node->query_id);
	free(node);
}


void* threadFunc(void * arg){
    printf("Thread\n");
	while(1){
		if(wait_all_tasks_finish(JobSchedulerNode)==1)
			break;
		execute_all_jobs(JobSchedulerNode);
	}
	return NULL;
}