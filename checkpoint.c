#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#include "headers.h"

/*
	Note: sscanf reads from a char*, so it doesn't consume the input
*/

// Sizes in bytes
#define SUPERBLOCK (24) // 8-byte alligned memory, so 4 bytes of padding added to superblock
#define LOG_ENTRY_BLOCK (4000)
#define REMAINING_BLOCK (3984)

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
	log_entry log_entries[166]; // In every log entry block, (4000 - 16) / 24 = 166
} log_entry_block;

// Returns malloced superblock read from disk
superblock* get_superblock(int fd) {
	lseek(fd, 0, SEEK_SET);
	superblock* new = malloc(sizeof(superblock));
	read(fd, new, sizeof(superblock));
	return new;
}

// Calculates and returns the checksum of the superblock
uint64_t checksum_superblock(void *bytes) {
	int i;
	uint64_t sum = 0;
	// skip checksum
	unsigned int *block = bytes + 8;

	// for all 8-byte words in block
	for (i = 8; 16 - i >= 0; i += 8) sum ^= *block++;
	// add constant to return value to avoid false positives
	return sum + 3;
}

// Writes superblock sup to disk
size_t write_superblock(int fd, superblock* sup) {
        lseek(fd, 0, SEEK_SET);
        sup->checksum = checksum_superblock(sup);
        return write(fd, sup, sizeof(superblock));
}

// Returns true if checksum is equal to the XOR of all 8-byte words in superblock
bool valid_superblock(superblock *block, uint64_t checksum) {
	return checksum == checksum_superblock(block);
}

// Implements -f (fomrat) functionality
void format_superblock(int fd) {
	superblock* sup = get_superblock(fd);
	if (sup == NULL) {
		fprintf(stderr, "Failed to read superblock");
		exit(1);
	}

	if(valid_superblock(sup, sup->checksum)) {
		sup->generation = sup->generation + 1;
	} else {
		sup->generation = 0;
		sup->log_start = 1;
		sup->log_size = 2000000000;
	}
	write_superblock(fd, sup);
	free(sup);
}

int main(int argc, char** argv) {
	int fd;

	fd = open("/dev/sdb", O_RDONLY);	

	struct superblock new;

	printf("Read: %d\n", (int)read(fd, &new, sizeof(superblock)));

	printf("Generation: %lu, checksum: %lu, logstart: %lu, logsize: %lu\n", (unsigned long) new.generation, (unsigned long) new.checksum, (unsigned long) new.log_start, (unsigned long) new.log_size);
	
	close(fd);
	if (valid_superblock(&new, new.checksum)) {
		printf("Valid superblock!");
	}
/*	
	if(sblock.generation == new.generation) printf("Generation\n");
	if(sblock.checksum == new.checksum) printf("Checksum\n");
	if(sblock.log_start == new.log_start) printf("Start\n");
	if(sblock.log_size == new.log_size) printf("Size\n");
	printf("%lu\n", sizeof(log_entry));*/

/*	fd = open("/dev/sdb", O_WRONLY);
*/
	return 0;
}

