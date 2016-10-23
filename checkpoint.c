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
#include <inttypes.h>

#include "headers.h"


// Global in-memory variables
uint32_t generation;  // in-memory generation number
uint32_t tail;        // in-memory tail of the log

// Returns malloced superblock read from disk
superblock* get_superblock(int fd) {
	lseek(fd, 0, SEEK_SET);
	superblock* new = mmap(NULL, SUPERBLOCK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (read(fd, new, SUPERBLOCK) != SUPERBLOCK) return NULL;
	fprintf(stderr, "Reading: generation: %" PRIu32 ", start: %" PRIu32 ", size: %" PRIu32 "\n", new->generation, new->log_start, new->log_size);
	return new;
}

// Calculates and returns the checksum of the superblock
uint64_t checksum_superblock(void *bytes) {
	int i;
	uint64_t sum = 0;
	// skip checksum
	unsigned int *block = bytes + 8;

	// for all 8-byte words in block
	for (i = 0; SUPERBLOCK - 8 - i >= 8; i += 8) sum ^= *block++;
	// add constant to return value to avoid false positives
	return sum + 3;
}

// Calculates and returns the checksum of the log entry block
uint64_t checksum_log_entry_block(void *bytes) {
	int i;
	uint64_t sum = 0;
	// skip checksum
	unsigned int *block = bytes + 8;

	// for all 8-byte words in block
        for (i = 0; LOG_ENTRY_BLOCK - 8 - i > 0; i += 8) sum ^= *block++;
        // add constant to return value to avoid false positives
        return sum + 3;
}

// Writes superblock sup to disk
size_t write_superblock(int fd, superblock* sup) {
        lseek(fd, 0, SEEK_SET);
        sup->checksum = checksum_superblock(sup);

	// Debugging
	fprintf(stderr, "Writing: generation: %" PRIu32 ", start: %" PRIu32 ", size: %" PRIu32 "\n", sup->generation, sup->log_start, sup->log_size);

	return write(fd, sup, SUPERBLOCK);
}

// Returns true if checksum is equal to the XOR of all 8-byte words in superblock
bool valid_superblock(superblock *block, uint64_t checksum) {
	return checksum == checksum_superblock(block);
}

// Implements -f (fomrat) functionality, return true on success
bool format_superblock(int fd) {
	superblock* sup = get_superblock(fd);
	if (sup == NULL) return false;

	if (valid_superblock(sup, sup->checksum)) {
		sup->generation = sup->generation + 1;
		tail = get_tail(fd);
	} else {
		sup->generation = 0;
		sup->log_start = 1;
		sup->log_size = 2000000000; // unsure if this is meant to be 2GB always but that's what he said in class
		tail = 0;
	}
	generation = sup->generation;
	if (write_superblock(fd, sup) != SUPERBLOCK) return false;
	return true;
}

// Reads the superblock, checks if it is valid, and returns true upon success
bool normal_startup(int fd) {
	superblock* sup = get_superblock(fd);
	if (sup == NULL) return false;

	if (valid_superblock(sup, sup->checksum)) {
		generation = sup->generation;
		tail = get_tail(fd);
		return true;
	} else return false;
}

// Returns number of log entry block that should be written next
uint32_t get_tail(int fd) {
	//TODO
	return 0;
}

// Appends most recent mutating command to log, returns true on success
bool add_to_log(uint32_t opcode, uint64_t arg1, uint64_t arg2) {
	log_entry *new = mmap(NULL, LOG_ENTRY, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	new->opcode = opcode;
	new->node_a_id = arg1;
	new->node_b_id = arg2;

	// TODO
	return true;
}

checkpoint_area *get_checkpoint(int fd){
	lseek(fd, LOG_SIZE, SEEK_SET);
	checkpoint_area *new = mmap(NULL, CHECKPOINT_AREA, 
		PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (read(fd, &(new->nsize), 8) != 8) return NULL;
	if (read(fd, &(new->esize), 8) != 8) return NULL;
	if (read(fd, new->nodes, CHECKPOINT_NODE * new->nsize) 
		!= CHECKPOINT_NODE * new->nsize) {
		return NULL;
	}
	if (read(fd, new->edges, CHECKPOINT_EDGE * new->esize) 
		!= CHECKPOINT_EDGE * new->esize) {
		return NULL;
	}

	// introduce some check here
	return new;
}

bool write_cp(int fd, checkpoint_area *new){
	lseek(fd, LOG_SIZE, SEEK_SET);
	// add debug line
	int i;
	if (write(fd, &(new->nsize), 8) != 8) return false;
	if (write(fd, &(new->esize), 8) != 8) return false;
	if (write(fd, new->nodes, CHECKPOINT_NODE * new->nsize) 
		!= CHECKPOINT_NODE * new->nsize) {
		return false;
	}
	if (write(fd, new->edges, CHECKPOINT_EDGE * new->esize) 
		!= CHECKPOINT_EDGE * new->esize) {
		return false;
	}
	return true;
}

int docheckpoint(int fd, checkpoint_area *new){
	// make sure its not too big to checkpoint
	checkpoint_area* old = get_checkpoint(fd);
	if (old == NULL) return false;

	return write_cp(fd, new);


}
/*
// FOR TESTING PURPOSES ONLY
int main(int argc, char** argv) {
	int fd;

	fd = open("/dev/sdb", O_RDONLY);	

	struct superblock *new = mmap(NULL, SUPERBLOCK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

	printf("Read: %d\n", (int)read(fd, new, SUPERBLOCK));

	printf("Generation: %lu, checksum: %lu, logstart: %lu, logsize: %lu\n", (unsigned long) new->generation, (unsigned long) new->checksum, (unsigned long) new->log_start, (unsigned long) new->log_size);
	
	close(fd);
	if (valid_superblock(new, new->checksum)) {
		printf("Valid superblock!");
	} else {
		printf("Invalid");
	}
	
	if(sblock.generation == new.generation) printf("Generation\n");
	if(sblock.checksum == new.checksum) printf("Checksum\n");
	if(sblock.log_start == new.log_start) printf("Start\n");
	if(sblock.log_size == new.log_size) printf("Size\n");
	printf("%lu\n", sizeof(log_entry));

	fd = open("/dev/sdb", O_WRONLY);

	return 0;
}
*/

