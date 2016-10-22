/*
 * headers.h
 *  
 * by Stylianos Rousoglou
 * and Alex Saiontz
 *
 * Provides function prototypes
 * and structure definitions
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/mman.h>

/*
	Hashtable API prototypes
*/

// size of hashtable
#define SIZE (100000)

//Queue for doing BFS and tracking nodes
struct elt {
    struct elt *next;
    uint64_t value;
};

typedef struct queue{
    struct elt *head;
    struct elt *tail;
} queue;


// edge node definition
typedef struct edge {
	uint64_t b;				// adjacent vertex
	struct edge* next;		// for chaining
} edge;

// vertex node definition
typedef struct vertex {
	uint64_t id;			// unique id of vertex
	edge* head; 			// linked list of edges
	struct vertex* next;	// for chaining
	int path;
} vertex;

// vertex hashtable definition
typedef struct vertex_map {
	vertex** table;
	size_t size;
} vertex_map;

// Returns hash value
int hash_vertex(uint64_t id);
// return true if vertices the same 
bool same_vertex(uint64_t a, uint64_t b);
// returns pointer to vertex, or NULL if it doesn't exist
vertex * ret_vertex(uint64_t id);
// adds vertex, returns false is vertex existed
bool add_vertex(uint64_t id);
// helper, returns false if vertex does not exist
bool delete_vertex(vertex** head, uint64_t id);
// removes vertex, returns false is vertex does not exist
bool remove_vertex(uint64_t id);
// checks if a vertex is in a graph
bool get_node(uint64_t id);
// checks if an edge is in a graph
bool get_edge(uint64_t a, uint64_t b);
// get array of neighbors
uint64_t *get_neighbors(uint64_t id, int* n); 
// finds shortest path between two nodes
int shortest_path(uint64_t id1, uint64_t id2);
// For testing, print all nodes
void all_nodes();

/*
	Linked-list API prototypes
*/

// Inserts node in given LL
void LL_insert(edge** head, uint64_t n);
// Returns true if n is in given linked list
bool LL_contains(edge** head, uint64_t n);
// Removes n from linked list
bool LL_delete(edge** head, uint64_t n);
// Adds edge, returns 400, 204 or 200
int add_edge(uint64_t a, uint64_t b);
// Removes edge, returns false if it didn't exist
bool remove_edge(uint64_t a, uint64_t b);

/*
	Queue prototypes
*/

// Initializes queue
queue * queueCreate(void);
// Enqueues element value to queue *q
void enqueue(queue **q, uint64_t value);
// Dequeues element value from queue *q
uint64_t dequeue(queue **q);
// Empties queue and frees allocated memory
void queue_destroy(queue **q);

/*
	Graph API
*/

// Assumes both nodes exist; returns value of shortest path
int shortest_path(uint64_t id1, uint64_t id2);

// Given a valid node_id, returns list of neighbors
uint64_t *get_neighbors(uint64_t id, int* n);

/*
	Log functionality API
*/

// Sizes in bytes
#define SUPERBLOCK (20)
#define LOG_ENTRY_BLOCK (4000)
#define REMAINING_BLOCK (3984)

// op-codes for log entries
#define ADD_NODE (0)
#define ADD_EDGE (1)
#define REMOVE_NODE (2)
#define REMOVE_EDGE (3)

// Definition of a 20B superblock
typedef struct superblock {
        uint64_t checksum;
        uint32_t generation;
        uint32_t log_start;
        uint32_t log_size;
} superblock;
// Definition of a 20B log entry
typedef struct log_entry {
        uint64_t node_a_id;
        uint64_t node_b_id;
        uint32_t opcode;
} log_entry;
// Definition of a 4KB log entry block
typedef struct log_entry_block {
        uint64_t checksum;
        uint32_t generation;
        uint32_t n_entries;
        // essentially a sequence of log_entry structs
        log_entry log_entries[199]; // In every log entry block, (4000 - 16) / 20 = 199.2
} log_entry_block;

// Returns malloced superblock read from disk
superblock* get_superblock(int fd);

// Calculates and returns the checksum of the superblock
uint64_t checksum_superblock(void *bytes);

// Calculates and returns the checksum of the log entry block
uint64_t checksum_log_entry_block(void *bytes);

// Writes superblock sup to disk
size_t write_superblock(int fd, superblock* sup);

// Returns true if checksum is equal to the XOR of all 8-byte words in superblock
bool valid_superblock(superblock *block, uint64_t checksum);

// Implements -f (fomrat) functionality
bool format_superblock(int fd);

// Reads the superblock, checks if it is valid, and if so returns generation number (otherwise 0)
uint64_t normal_startup(int fd);

