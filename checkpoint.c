#include "headers.h"

// Global in-memory variables
uint32_t generation;  // in-memory generation number
uint32_t tail;        // in-memory tail of the log

extern int fd;

// Returns malloced superblock read from disk
superblock* get_superblock() {
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
	unsigned int *block = bytes;
	block++;	// skip checksum (first 8 bytes)

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
	unsigned int *block = bytes;
	block++;	// skip checksum (first 8 bytes)

	// for all 8-byte words in block
        for (i = 0; LOG_ENTRY_BLOCK - 8 - i >= 8; i += 8) sum ^= *block++;

        // add constant to return value to avoid false positives
        return sum + 3;
}

// Writes superblock sup to disk
size_t write_superblock(superblock* sup) {
        lseek(fd, 0, SEEK_SET);
        sup->checksum = checksum_superblock(sup);

	// Debugging
	fprintf(stderr, "Writing Superblock: generation: %" PRIu32 ", start: %" PRIu32 ", size: %" PRIu32 "\n", sup->generation, sup->log_start, sup->log_size);

	return write(fd, sup, SUPERBLOCK);
}

// Returns true if checksum is equal to the XOR of all 8-byte words in superblock
bool valid_superblock(superblock *block, uint64_t checksum) {
	return checksum == checksum_superblock(block);
}

// Returns true if checksum is equal to the XOR of all 8-byte words in log entry block
bool valid_log_entry_block(void *block, uint64_t checksum) {
        return checksum == checksum_log_entry_block(block);
}

// Implements -f (fomrat) functionality, return true on success
bool format_superblock() {
	superblock* sup = get_superblock();
	if (sup == NULL) return false;

	if (valid_superblock(sup, sup->checksum)) {
		sup->generation = sup->generation + 1;
		fprintf(stderr, "Superblock was valid. Incremented to %d\n", (int) sup->generation);
	} else {
		sup->generation = 0;
		sup->log_start = 1;
		sup->log_size = LOG_SIZE; // unsure if this is meant to be 2GB always but that's what he said in class
		fprintf(stderr, "Superblock was invalid. Initialized to 0\n");
	}
	generation = sup->generation;
	tail = 0;
	if (write_superblock(sup) != SUPERBLOCK) return false;
	return true;
}

// Writes superblock with incremented generation number upon chckpoint
bool update_superblock() {
	superblock* sup = get_superblock();
        if (sup == NULL) return false;

	sup->generation = sup->generation + 1;
	fprintf(stderr, "Generation incremented to %d\n", (int) sup->generation);
	tail = 0;
	if (write_superblock(sup) != SUPERBLOCK) return false;
	return true;
}

// Reads the superblock, checks if it is valid, and returns true upon success
bool normal_startup() {
	superblock* sup = get_superblock();
	if (sup == NULL) return false;

	if (valid_superblock(sup, sup->checksum)) {
		generation = sup->generation;
		fprintf(stderr, "Superblock was valid. Normal startup\n");
		return true;
	} else {
		fprintf(stderr, "Superblock was invalid. Abort\n");
		return false;
	}
}

// Returns number of log entry block that should be written next
uint32_t get_tail() {
	uint32_t runner = -1;
	log_entry_block_header *new = mmap(NULL, LOG_ENTRY_HEADER, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	char *block = mmap(NULL, LOG_ENTRY_BLOCK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

	bool valid, full, gen, end;
	// move to end of superblock
	lseek(fd, SUPERBLOCK, SEEK_SET);
	do {
		// first time through, defaults to 0
		runner += 1;

		// read entire 4KB block in

		if (read(fd, block, LOG_ENTRY_BLOCK) != LOG_ENTRY_BLOCK) exit(2);
		// extract log entry block header
		memcpy(new, block, LOG_ENTRY_HEADER);

		valid = valid_log_entry_block(block, new->checksum);
		full = new->n_entries == N_ENTRIES;
		gen = new->generation == generation;
		end = runner < MAX_BLOCKS - 1;

		if (valid && gen) {
			//TODO: somewhere here play the log forward?
			play_log_forward(block, new->n_entries);
		}

	} while(valid && full && gen && end);

	//TODO: if wrong generation, write size to be 0 (to make checksum fail)
	if (!gen) {
		fprintf(stderr, "Tail stopped at wrong generation, read %d!\n", (int) new->generation);
		lseek(fd, -LOG_ENTRY_BLOCK, SEEK_CUR);
		new->n_entries = 0;
		if (write(fd, new, LOG_ENTRY_HEADER) != LOG_ENTRY_HEADER) exit(2);
	}
	if (!valid) fprintf(stderr, "Tail stopped at invalid log block!\n");

	if (runner == MAX_BLOCKS - 1 && new->n_entries == N_ENTRIES) runner = MAX_BLOCKS;
        
	fprintf(stderr, "Tail was set to %" PRIu32 "\n", runner);
        fprintf(stderr, "Number of entries in current block is %" PRIu32 "\n", new->n_entries);
	return runner;
}

// Appends most recent mutating command to log, returns true on success
bool add_to_log(uint32_t opcode, uint64_t arg1, uint64_t arg2) {
	// if log full
        if (tail == MAX_BLOCKS) {
		fprintf(stderr, "Log segment is FULL!\n");
		return false;
        } else {
		log_entry *entry = mmap(NULL, LOG_ENTRY, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
		log_entry_block_header *header = mmap(NULL, LOG_ENTRY_HEADER, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
		char *block = mmap(NULL, LOG_ENTRY_BLOCK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

		entry->opcode = opcode;
		entry->node_a_id = arg1;
		entry->node_b_id = arg2;

		// go to correct block
		lseek(fd, SUPERBLOCK + tail * LOG_ENTRY_BLOCK, SEEK_SET);
		fprintf(stderr, "Read %d block at position %d\n", (int) read(fd, block, LOG_ENTRY_BLOCK), tail);
		// extract log entry block header

		memcpy(header, block, LOG_ENTRY_HEADER);
		lseek(fd, SUPERBLOCK + tail * LOG_ENTRY_BLOCK, SEEK_SET);
		// if block invalid, write size to be 1 and then the newly added entry
		if (!valid_log_entry_block(block, header->checksum) || header->generation != generation) {
			header->generation = generation;
			header->n_entries = 1;
			if (write(fd, header, LOG_ENTRY_HEADER) != LOG_ENTRY_HEADER) exit(2);
		} else {
			header->n_entries = header->n_entries + 1;
			if (write(fd, header, LOG_ENTRY_HEADER) != LOG_ENTRY_HEADER) exit(2);
			if (write(fd, block + LOG_ENTRY_HEADER, LOG_ENTRY * (header->n_entries - 1)) != LOG_ENTRY * (header->n_entries - 1)) exit(2);
		}
		if (write(fd, entry, LOG_ENTRY) != LOG_ENTRY) exit(2);
		lseek(fd, SUPERBLOCK + tail * LOG_ENTRY_BLOCK, SEEK_SET);
		if (read(fd, block, LOG_ENTRY_BLOCK) != LOG_ENTRY_BLOCK) exit(2);
		header->checksum = checksum_log_entry_block(block);
		lseek(fd, SUPERBLOCK + tail * LOG_ENTRY_BLOCK, SEEK_SET);
		if (write(fd, header, LOG_ENTRY_HEADER) != LOG_ENTRY_HEADER) exit(2);

		// when block MAX_BLOCKS is full, tail++ = s MAX_BLOCKS and additional logging allowed (first line in function)
		if (header->n_entries == N_ENTRIES) tail++;
	}
	// 3 cases: fits in tail, need to move tail to next block, or out of space!
        return true;
}

// Plays forward all 20B entries present in block
void play_log_forward(char *block, uint32_t entries) {
        char *tmp = block + LOG_ENTRY_HEADER;
        log_entry *new = mmap(NULL, LOG_ENTRY, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
        for (uint32_t i = 0; i < entries; i++) {
                memcpy(new, tmp + i * LOG_ENTRY, LOG_ENTRY);
                switch(new->opcode) {
                        case ADD_NODE:
                                add_vertex(new->node_a_id);
                                break;
                        case ADD_EDGE:
                                add_edge(new->node_a_id, new->node_b_id);
                                break;
                        case REMOVE_NODE:
                                remove_vertex(new->node_a_id);
                                break;
                        case REMOVE_EDGE:
                                remove_edge(new->node_a_id, new->node_b_id);
               			break;
		 }
		fprintf(stderr, "op: %" PRIu32 ",node a: %" PRIu64 ", node b (only for 1 and 3): %" PRIu64 "\n", new->opcode, new->node_a_id, new->node_b_id);
        }
}

checkpoint_area *get_checkpoint(int fd){
	lseek(fd, LOG_SIZE, SEEK_SET);
	int cpsize=0;
	uint64_t nsize;
	uint64_t esize;
	int i;
	if (read(fd, &(nsize), 8) != 8) {
		return NULL;
	}
	if (read(fd, &(esize), 8) != 8) {
		fprintf(stderr, "2\n" );
		return NULL;
	}
	lseek(fd, LOG_SIZE, SEEK_SET);
	cpsize = CHECKPOINT_HEADER + nsize*CHECKPOINT_NODE 
		+ esize*CHECKPOINT_EDGE;

	checkpoint_area *new = malloc(sizeof(struct checkpoint_area));
	uint64_t *nodes = malloc(sizeof(uint64_t) * nsize);
	mem_edge *edges = malloc(sizeof(struct mem_edge) * esize);
	 // mmap(NULL, cpsize, 
		// PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (read(fd, &(new->nsize), 8) != 8) return NULL;
	if (read(fd, &(new->esize), 8) != 8) return NULL;
		
	for (i=0; i< new->nsize; i++){
		if (read(fd, &(nodes[i]), CHECKPOINT_NODE) 
			!= CHECKPOINT_NODE) {
			return NULL;
		}
	}
	
	for (i=0; i < new->esize; i++){
		if (read(fd, &(edges[i].a), CHECKPOINT_NODE)  
			!= CHECKPOINT_NODE) {
			return NULL;
		} 
		if (read(fd, &(edges[i].b), CHECKPOINT_NODE)  
			!= CHECKPOINT_NODE) {
			return NULL;
		} 
	}
	new->nodes=nodes;
	new->edges=edges;

	// introduce some check here
	return new;
}

int write_cp(int fd, checkpoint_area *new){
	lseek(fd, LOG_SIZE, SEEK_SET);

	// add debug line
	int i;
	if (write(fd, &(new->nsize), 8) != 8) return 0;
	
	
	if (write(fd, &(new->esize), 8) != 8) return 0;
	
	for (i=0; i< new->nsize; i++){
		if (write(fd, &((new->nodes)[i]), CHECKPOINT_NODE) 
			!= CHECKPOINT_NODE) {
			return 0;
		} 
	}
	
	for (i=0; i < new->esize; i++){
		if (write(fd, &(((new->edges)[i]).a), CHECKPOINT_NODE)  
			!= CHECKPOINT_NODE) {
			return 0;
		} 
		if (write(fd, &(((new->edges)[i]).b), CHECKPOINT_NODE)  
			!= CHECKPOINT_NODE) {
			return 0;
		} 
	}
	return 5;
}


int docheckpoint(checkpoint_area *new){
	// ` sure its not too big to checkpoint
	// checkpoint_area* old = get_checkpoint(fd);
	// if (old == NULL) return 0;
	update_superblock();
	return write_cp(fd, new);
}

// Writes whole LOG section (all 2 GB) with 0
/*
void format_whole_log(int fd) {
	lseek(fd, 0, SEEK_SET);
	int *n = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	for (int i = 0; i < 524288; i++) write(fd, n, 4096);
}*/
/*
// FOR TESTING PURPOSES ONLY
int main(int argc, char** argv) {
	int fd;

	fd = open("/dev/sdb", O_RDWR);
	if(argc==2 && !strcmp(argv[1], "-f")) format_whole_log(fd);

//	format_superblock(fd);
	normal_startup(fd);
	//for(int j=0; j<542280; j++){
	//	for(int i=0; i<200; i++) add_to_log(fd, i, i, i);
	//}
	log_entry_block_header *h = mmap(NULL, LOG_ENTRY_HEADER, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	h->n_entries = 3;
	h->generation = 8;
	lseek(fd, SUPERBLOCK, SEEK_SET);
	fprintf(stderr, "Just wrote the header: %d\n", (int) write(fd, h, LOG_ENTRY_HEADER));
*/

/*
	lseek(fd, SUPERBLOCK, SEEK_SET);
	char *n = mmap(NULL, LOG_ENTRY_BLOCK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	read(fd, n, LOG_ENTRY_BLOCK);
	log_entry_block_header *h = mmap(NULL, LOG_ENTRY_BLOCK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	memcpy(h, n, LOG_ENTRY_HEADER);
	h->checksum = 10;//checksum_log_entry_block(n);
	lseek(fd, SUPERBLOCK, SEEK_SET);
	write(fd, h, LOG_ENTRY_HEADER);

	lseek(fd, SUPERBLOCK, SEEK_SET);
        read(fd, n, LOG_ENTRY_BLOCK);
	memcpy(h, n, LOG_ENTRY_HEADER);
	if (valid_log_entry_block(n, h->checksum)) fprintf(stderr, "Log entry block VALID!\n");
	
*/


//h->checksum = checksum_log_entry_block(n);
	//lseek(fd, SUPERBLOCK, SEEK_SET);
	//write(fd, h, LOG_ENTRY_HEADER);*/
	//normal_startup(fd);
	//for(int i=0; i<400; i++) add_to_log(fd, 6, 6, 6);
//	format_superblock(fd);	
//	normal_startup(fd);
/*	struct superblock *new = mmap(NULL, SUPERBLOCK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

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

