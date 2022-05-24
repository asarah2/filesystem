////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_cache.c
//  Description    : This is the implementation of the cache for the 
//                   FS3 filesystem interface.
//
//  Author         : Patrick McDaniel / Sarah Babu (edited cache files for other functions)
//  Last Modified  : 11/19/2021
//

// Includes
#include <stdlib.h>
#include <cmpsc311_log.h>
#include <string.h>

// Project Includes
#include <fs3_cache.h>


//
// support macros/data
// cache structures
typedef struct cache_node
{
	uint8_t sector_data[FS3_SECTOR_SIZE]; //sector data
	uint32_t sector_id; // sector id
	struct cache_node *next; // next pointer
	struct cache_node *prev; // previous pointer
} cache_node;

//this link always point to first Link
cache_node *cache_head = NULL;

//this link always point to tail Link 
cache_node *cache_tail = NULL;

//initlializing log metrics
uint32_t fs3_put_cache_success = 0, fs3_put_cache_failure = 0, fs3_get_cache_success = 0, fs3_get_cache_failure = 0;

//
// Implementation

////////////////////////////////////////////////////////////////////////////////
//
// Function     : is_cache_empty
// Description  : checks if the cache is empty
//
// Inputs       : none
// Outputs      : True if empty, False otherwise

int is_cache_empty() 
{
   return (cache_head == NULL);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : insert_node_to_tail
// Description  : inserts a node to the tail of the cache
//
// Inputs       : pointer to the cache node
// Outputs      : none

void insert_node_to_tail(cache_node *node) 
{
    if (is_cache_empty()) {
        // Cache is empty head and tail point to the same node
        cache_head = node;
		cache_tail = node;
    } else {
        // add new code as next of last
        cache_tail->next = (struct cache_node*) node;     

        // new nodes previous pointer points to the old tail node
        node->prev = (struct cache_node*) cache_tail;
    }

    // new node is not the tail
    cache_tail = (struct cache_node*) node;
	cache_tail->next = NULL;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : move_node_to_tail
// Description  : moves a given node to the end of cache
//
// Inputs       : pointer to the cache node
// Outputs      : none

void move_node_to_tail(cache_node* node) {
   // Delete node from cache
    if (node == cache_head) {
        // change first to point to next node
        cache_head = cache_head->next;
    } else {
        // bypass the node
        node->prev->next = node->next;
    }    

    if (node == cache_tail) {
        // change last to point to prev node
        cache_tail = node->prev;
    } else {
        node->next->prev = node->prev;
    }

    // Insert node to the tail
    insert_node_to_tail(node);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_cache_size
// Description  : gives us the size of the cache
//
// Inputs       : none
// Outputs      : length of the given cache
int get_cache_size() {
   int length = 0;
   struct cache_node *current;
    // moves down the cache and increments lenght by 1 for every node 
   for(current = cache_head; current != NULL; current = current->next){
      length++;
   }
   return length;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_get_cache_node
// Description  : gives us the size of the cache
//
// Inputs       : track sector
// Outputs      : pointer to cache

struct cache_node* fs3_get_cache_node(FS3TrackIndex trk, FS3SectorIndex sct)  {
    // intitialize cache node structure.
    struct cache_node* current = cache_head;
	uint32_t sector_id = 0;
    //calculating sector index based on position.
	sector_id = ((trk)*1024) + sct;

    // if cache is empty
    if (is_cache_empty()) {
        return(NULL);
    }

    // navigate through cache
    while(current != NULL) {
        if (current->sector_id == sector_id) {
            return current;
        }
		current = current->next;
    }

    return(NULL);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_get_cache_node
// Description  : deletes the head of the cache
//
// Inputs       : track sector
// Outputs      : deleted the pointer from cache

struct cache_node* delete_head() {

    // save reference to first node
    struct cache_node *tempLink = cache_head;
	
    if (is_cache_empty()) {
        return NULL;
    }

    // if only one node
    if (cache_head->next == NULL){
        cache_tail = NULL;
    } else {
        cache_head->next->prev = NULL;
    }
	//points to next pointer
    cache_head = cache_head->next;

    // return the deleted node
    return tempLink;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : insert_tail
// Description  : inserts a tail to the cache
//
// Inputs       : track sector
// Outputs      : insert pointer tail to the cache node

int insert_tail(uint32_t sector_id, uint8_t *sector_data) {

    //create a node
    struct cache_node *node = (struct cache_node*) calloc(1,sizeof(struct cache_node));

    if (NULL == node) {
        return(1);
    }
    
    node->sector_id = sector_id;
    memcpy(node->sector_data, sector_data, FS3_SECTOR_SIZE);

    insert_node_to_tail(node);
    return(0);

}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_init_cache
// Description  : Initialize the cache with a fixed number of cache lines
//
// Inputs       : cachelines - the number of cache lines to include in cache
// Outputs      : 0 if successful, -1 if failure

int fs3_init_cache(uint16_t cachelines) {
	cache_head = NULL;
	cache_tail = NULL;
    return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_close_cache
// Description  : Close the cache, freeing any buffers held in it
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int fs3_close_cache(void)  {
    struct cache_node* node_to_free = NULL;
    struct cache_node* current = cache_head;

    // navigate through cache
    while(current != NULL) {
        node_to_free = current;
		current = current->next;
        free(node_to_free);
    }

	cache_head = NULL;
	cache_tail = NULL;
    return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_put_cache
// Description  : Put an element in the cache
//
// Inputs       : trk - the track number of the sector to put in cache
//                sct - the sector number of the sector to put in cache
// Outputs      : 0 if inserted, -1 if not inserted

int fs3_put_cache(FS3TrackIndex trk, FS3SectorIndex sct, void *buf) {
    struct cache_node* node = NULL;
    uint32_t sector_id = 0;
    //  as long there is a track/sector available, we can delete the head
    if (NULL == (node = fs3_get_cache_node(trk, sct)))  {
        if (get_cache_size() >= FS3_DEFAULT_CACHE_SIZE) {
            if ((node = delete_head()) != NULL) {
                free(node); 
            }
        }
        // calculating the sector index
	    sector_id = ((trk)*1024) + sct;
        // return -1 if we are able to insert cache to the end of the cache node
        if (1 == insert_tail(sector_id, buf)) {
            fs3_put_cache_failure++;
            return (-1);
        } // else return 0
        fs3_put_cache_success++;
        return (0);
    }
    // copy the data from the buffer to the cache pointer in use
    memcpy(node->sector_data ,buf, FS3_SECTOR_SIZE);
    // moves the cache pointer to the tail of the cache node
	move_node_to_tail(node);
    fs3_put_cache_success++;
    return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_get_cache
// Description  : Get an element from the cache (
//
// Inputs       : trk - the track number of the sector to find
//                sct - the sector number of the sector to find
// Outputs      : returns NULL if not found or failed, pointer to buffer if found

void * fs3_get_cache(FS3TrackIndex trk, FS3SectorIndex sct)  {
    struct cache_node* node = NULL;
    // as long as the cache isn't empty, the cache will be allowed to get current set of cache pointers
    if (NULL == (node = fs3_get_cache_node(trk, sct)))  {
        fs3_get_cache_failure++;
        return (NULL);
    }

	move_node_to_tail(node);
    fs3_get_cache_success++;
    return(node->sector_data);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_log_cache_metrics
// Description  : Log the metrics for the cache 
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int fs3_log_cache_metrics(void) {
    printf("fs3_get_cache hits count: %d \n",fs3_get_cache_success);
    printf("fs3_get_cache misses count: %d \n",fs3_get_cache_failure);
    return(0); // returns 0 if the metrics return is successful
}

