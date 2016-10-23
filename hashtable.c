/*
 * hashtable.c 
 *  
 * by Stylianos Rousoglou
 * and Alex Saiontz
 *
 * Provides hashtable, linked list, and
 * queue functionality, as well as a 
 * shortest path alogirthm
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "headers.h"


/*
	Hashtable API
*/

// global hashtable for vertices
vertex_map map;

// Returns hash value
int hash_vertex(uint64_t id) {
	return id % SIZE;
}

// Returns pointer to vertex id, or NULL if it doesn't exist
vertex *ret_vertex(uint64_t id) {
	int hash = hash_vertex(id);
	vertex** table = map.table;
	vertex* index = table[hash];

	if(index != NULL) {
		if(index->id == id) return index;
		while(index->next) {
			if(index->id == id) return index;
			index = index->next;
		}
	}
	return NULL;
}

// Adds vertex, returns false is vertex existed
bool add_vertex(uint64_t id) {
	int hash = hash_vertex(id);
	vertex** table = map.table;

	if(ret_vertex(id)) return false;

	vertex* new = malloc(sizeof(vertex));
	if(!new) exit(1); // TODO: free everything
	new->id = id;
	new->next = table[hash];
	new->head = NULL;
	new->path = -1;
	new->visited = 0;
	table[hash] = new;
	map.nsize += 1;
	return true;
}

// helper code 
void fix_edges(vertex *out){
	edge *head = out->head;
	uint64_t id = out->id;
	while(head){
		LL_delete(&((ret_vertex(head->b))->head), id);
		head=head->next;
	}
}


// Helper, returns false if vertex does not exist
bool delete_vertex(vertex** head, uint64_t id)
{
    if(*head == NULL) return false;
    else {
        vertex* tmp = *head;
        if(tmp->id == id) {
            fix_edges(tmp);
            tmp = tmp->next;
            free(*head);
            *head = tmp;
            return true;
        }

        while(tmp->next)
        {
            if(tmp->next->id == id) {
                vertex* tmp2 = tmp->next->next;
                fix_edges(tmp->next);
                free(tmp->next);
                tmp->next = tmp2;
                return true;
            }
            tmp = tmp->next;
        }
    }
    return false;
}

// Removes vertex, returns false is vertex does not exist
bool remove_vertex(uint64_t id) {
	int hash = hash_vertex(id);
	vertex** table = map.table;

	if(delete_vertex(&(table[hash]), id)) {
		map.nsize -= 1;
		return true;
	} else return false;
}

// Check if a vertex is in a graph. 
bool get_node(uint64_t id){
	if (ret_vertex(id) == NULL){
		return false;
	}
	return true;
}

// Check if an edge is in a graph 
bool get_edge(uint64_t a, uint64_t b){
	vertex *v1 = ret_vertex(a);
	vertex *v2 = ret_vertex(b);
	if (LL_contains(&(v1->head), b) && LL_contains(&(v2->head), a)){
		return true;
	}
	return false;
}

/*
	Linked-list API
*/

// Inserts n into linked list
void LL_insert(edge** head, uint64_t n){
    edge* newnode = malloc(sizeof(edge));
    newnode->b = n;
    newnode->next = NULL;

    if(*head==NULL)
    {
        *head = newnode;
    }
    else
    {
        edge* temp; 
        temp = *head;
        *head = newnode;
        newnode->next = temp;
    }
}

// Returns true if n is in the linked list
bool LL_contains(edge** head, uint64_t n) {
    edge* ptr = *head; 
    while(ptr) {
        if(ptr->b == n) return true;
        ptr = ptr->next;
    }
    return false;
}

// Removes n from linked list
bool LL_delete(edge** head, uint64_t n)
{
    if(*head == NULL) return false;
    else {
        edge* tmp = *head;
        if(tmp->b == n) {
            tmp = tmp->next;
            free(*head);
            *head = tmp;
            return true;
        }

        while(tmp->next)
        {
            if(tmp->next->b == n) {
                edge* tmp2 = tmp->next->next;
                free(tmp->next);
                tmp->next = tmp2;
                return true;
            }
            tmp = tmp->next;
        }
    }
    return false;
}

// Adds edge, returns 
int add_edge(uint64_t a, uint64_t b) {
	vertex** table = map.table;

	vertex* v1 = table[hash_vertex(a)];
	vertex* v2 = table[hash_vertex(b)];

	// find vertex a
	while(v1) {
		if (v1->id == a) break;
		v1 = v1->next;
	}
	// find vertex b
	while(v2) {
		if (v2->id == b) break;
		v2 = v2->next;
	}

	// code 400
	if(!v1 || !v2 || a == b) return 400;

	if(LL_contains(&(v1->head), b)) return 204;

	LL_insert(&(v1->head), b);
	LL_insert(&(v2->head), a);
	map.esize += 1;
	return 200;
}

// Removes edge, returns false if it didn't exist
bool remove_edge(uint64_t a, uint64_t b) {
	vertex** table = map.table;
	
	vertex* v1 = table[hash_vertex(a)];
	vertex* v2 = table[hash_vertex(b)];

	// find vertex a
	while(v1) {
		if (v1->id == a) break;
		v1 = v1->next;
	}
	// find vertex b
	while(v2) {
		if (v2->id == b) break;
		v2 = v2->next;
	}

	// can't remove edge
	if(!v1 || !v2) return false;
	map.esize -= 1;
	return (LL_delete(&(v1->head), b) && LL_delete(&(v2->head), a));
}

/*
	Queue API
*/

// Initializes queue
queue * queueCreate(void){
	queue *q;
	q = malloc(sizeof(queue));
	q->head = q->tail = 0;
	return q;

}

// Enqueues element value to queue *q
void enqueue(queue **q, uint64_t value){
	struct elt *e;
	e = malloc(sizeof(struct elt));
	assert(e);
	e->value = value;
	e->next = 0;
	if((*q)->head == 0) {
		(*q)->head = e;
	}
	else {
		(*q)->tail->next = e;
	}
	(*q)->tail = e;
}

// Dequeues element value from queue *q
uint64_t dequeue(queue **q){
	uint64_t ret;
	struct elt *e;
	assert(!((*q)->head == 0));

	ret= (*q)->head->value;

	e = (*q)->head;
	(*q)->head = e->next;
	free(e);
	return ret;

}

// Empties queue and frees allocated memory
void queue_destroy(queue **q){
	while(!((*q)->head == 0)){
		dequeue(q);

	}
	free(*q);
}

/*
	Other operations
*/

// Assumes both nodes exist; returns value of shortest path
int shortest_path(uint64_t id1, uint64_t id2){
	
	queue *resetqueue = queueCreate();
	queue *bfsQ = queueCreate();
	vertex *root=ret_vertex(id1);

	root->path=0;
	
	vertex *current;
	vertex *n;
	edge *runner;

	enqueue(&bfsQ, root->id);
	enqueue(&resetqueue, root->id);
	int shortest_path = -1;
	while (bfsQ->head != 0){
		current=ret_vertex(dequeue(&bfsQ));
		for (runner = current->head; 
			runner != NULL; 
			runner = runner->next){
			n = ret_vertex(runner->b);
			if (n->path == -1){
				n->path = current->path + 1;
				enqueue(&resetqueue, n->id);
				enqueue(&bfsQ, n->id);
			}
		}
	}
	shortest_path=(ret_vertex(id2))->path;
	vertex *reset;
	while(resetqueue->head !=0){
		reset = ret_vertex(dequeue(&resetqueue));
		reset->path = -1;
	}
	queue_destroy(&resetqueue);
	queue_destroy(&bfsQ);
	return shortest_path;

}

// Given a valid node_id, returns list of neighbors
uint64_t *get_neighbors(uint64_t id, int* n){
	vertex *v = ret_vertex(id);
	int size = 0;
	int i;
	
	edge *runner;
	runner = v->head;
	while(runner){
		size++;
		runner = runner->next;
	}	

	uint64_t *neighbors = malloc(sizeof(uint64_t)*size);

	runner = v->head;
	for (i=0; i<size; i++){
		neighbors[i] = runner->b;
		runner = runner->next;
	}
	*n = size;
	return neighbors;
}


bool is_unvisited(uint64_t id){
	if ((ret_vertex(id))->visited){
		return false;
	}
	return true;

}
int do_edge_mem(mem_edge *edges, vertex *index, int edgei){
	edge *head = index->head;
	uint64_t id = index->id;
	while(head){
		if (is_unvisited(head->b)){
			edges[edgei].a = id;
			edges[edgei].b = head->b;
			edgei++;
		}
		head = head->next;
	}
	return edgei;

}

int make_checkpoint(checkpoint_area * flat_graph){
	int nodei = 0;
	int edgei = 0;
	int bin;
	vertex** table = map.table;
	vertex *index;
	for (bin=0; bin < SIZE; bin++){
		index = table[bin];
		while(index !=NULL){
			edgei = do_edge_mem(flat_graph->edges, index, edgei);
			flat_graph->nodes[nodei]=index->id;
			nodei++;
			index->visited=1;
			index=index->next;	
		}
	}

	// might be faster just to correct the ones we know exist, idk
	for (bin=0; bin < SIZE; bin++){
		index = table[bin];
		while(index !=NULL){
			index->visited=0;
			index=index->next;	
		}
	}
	return 1;
}

int buildmap(struct checkpoint_area * loaded){
	int nodenum = loaded->nsize;
	int edgenum = loaded->esize;
	int i;
	for (i=0; i<nodenum; i++){
		add_vertex(loaded->nodes[i]);
	}
	for (i=0; i<edgenum; i++){
		add_edge(loaded->edges[i].a, loaded->edges[i].b);
	}
	if (map.nsize != nodenum || map.esize != edgenum){
		return 0;
	}
	return 1;
}
