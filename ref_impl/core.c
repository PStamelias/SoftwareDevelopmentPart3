#include "../include/core.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


//Global Variables
struct HammingDistanceStruct* HammingDistanceStructNode;
Index*  BKTreeIndexEdit;
int bucket_sizeofHashTableExact;
struct Exact_Root* HashTableExact;

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
	int words_num=0;
	char** query_words=words_ofquery(query_str,&words_num);
	for(int i=0;i<words_num;i++)
		free(query_words[i]);
	free(query_words);
	query_words=NULL;
	if(query_words!=NULL)
		return EC_FAIL;
	return EC_SUCCESS;
}

ErrorCode EndQuery(QueryID query_id)
{
	return EC_SUCCESS;
}

ErrorCode MatchDocument(DocID doc_id, const char* doc_str)
{
	return EC_SUCCESS;
}


ErrorCode GetNextAvailRes(DocID* p_doc_id, unsigned int* p_num_res, QueryID** p_query_ids)
{
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
