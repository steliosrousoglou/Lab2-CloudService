#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/*
	Note: sscanf reads from a char*, so it doesn't consume the input
*/

// Sizes in bytes
#define SUPERBLOCK (20)
#define LOG_ENTRY_BLOCK (4000)
#define REMAINING_BLOCK (3984)

// Definition of a 20B superblock
typedef struct superblock {
	uint32_t generation;
	uint64_t checksum;
	uint32_t log_start;
	uint32_t log_size;
} superblock;

// Definition of a 4KB log entry block
typedef struct log_entry_block {
	uint32_t generation;
	uint32_t n_entries;
	uint64_t checksum;
	// essentially a sequence of log_entry structs
	char entries[REMAINING_BLOCK];
} log_entry_block;

// Definition of a 20B log entry
typedef struct log_entry {
	uint32_t opcode;
	uint64_t node_a_id;
	uint64_t node_b_id;
} log_entry;

// Calculates and returns the checkwsum
uint64_t checksum (char* sum) {
	return 0;
}