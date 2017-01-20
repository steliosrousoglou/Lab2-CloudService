# 426-Lab2

This assignment was worked on by Alexis Saiontz and Stylianos Rousoglou.
Stelios primarily focused on the log functionality, including storing log
entries and playing the log forward upon normal startup. Alex worked on the
checkpoint command, including the development and implementation of the 
protocol used to persistently store the graph in the checkpoint area.

# Assignment 2: Adding Durability to the Graph Store #

----

## Introduction ##

In this lab, we'll add durability to the graph store you built in Lab 1. We'll use a simple logging design, where the graph is stored entirely in main memory, but each incoming update is logged to raw disk. Occasionally the graph is checkpointed to disk and the log is garbage collected. We'll run the system in Google Cloud using a virtual block device in raw mode (i.e. reading and writing blocks directly without first mounting a filesystem on the device).

## API Changes ##

The service retains the API described in Lab1, with two differences.

First, any mutating command (`add_node`, `add_edge`, `remove_node`, `remove_edge`) should return error code `507` if the log is full and there is no space for logging the new entry. This tells the client that checkpointing is required.

Second, we expose a new `checkpoint` command:

   Function    | Method |    Arguments     | Return
-------------- | ------ | ---------------- | ------
 `checkpoint`    | `POST` |     |  `200` on success<br/> `507`if there is insufficient space for the checkpoint
 
 
In the command line, accept an additional parameter specifying the device file in addition to the port:

```sh
$ ./cs426_graph_server <port> <devfile>
```
**Note:** To find the disk you add, simply use command `lsblk` to check all of the disks in your vm. Typically, if you follow the above google cloud instruction to create your vm. The disk serving as checkpoint disk is /dev/sdb. To get the permission to open the disk a file and write to it. You need to change the access permisson of this disk using `chmod`.      

Also, support a -f flag that formats/initializes a new deployment (i.e. writes a fresh version of the superblock):

```sh
$ ./cs426_graph_server -f <port> <devfile>
```
**Note:**  As the disk you add is initialized with random bits, so you need to first to format this disk by yourself.

## Protocol Format ##

There are many different valid protocols. Here we outline one such protocol and a couple of alternatives. **You are free to use your own protocol -- just make sure it'll continue to work under the failure and workload assumptions!** In particular, for this homework, we assume failures will occur when there's no ongoing activity in the system.

The first block is a superblock for storing metadata. Here, store the location and size of the log segment. The remainder of the disk is used for the checkpoint. A 10GB disk has 2.5M 4KB blocks. Let block 0 be the metadata superblock. For this lab, initialize the log size to be the first 2GB (including the superblock), and use the remainder 8GB as the checkpoint area.

Protocol 0: Here we store the current generation number in the metadata block, and always reset the log tail to the start on a checkpoint or a format. The process maintains the current generation number and tail of the log in memory. 

--- On a format, you read the superblock and check if it's valid. If valid, you read the current generation number from the superblock, increment it, and write it back out. If invalid, this store has never been used before; you write a brand new superblock with a generation number of 0. (There are corner cases where the superblock gets corrupted, and formatting it with a generation number of 0 can cause problems due to valid data blocks in the log from the earlier instance; for this homework, assume that superblock corruptions don't occur).

--- On a normal startup, you read the superblock and check if it's valid. If not valid, exit with an error code. If valid, locate the checkpoint using the superblock, and read it in if one exists. Read the generation number from the superblock. Play the log forward until you encounter the first log entry that's invalid or has a different generation number.

--- On a checkpoint call, write out the checkpoint. (You can assume that crashes don't occur during the checkpointing process). Increment the generation number in the superblock. Reset the in-memory tail counter to 0.

`byte 0: 32 bits unsigned int: current generation` (e.g. generation 5)<br/>
`byte 4: 64 bits unsigned int: checksum` (e.g. xor of all 8-byte words in block)<br/>
`byte 12: 32 bits unsigned int: start of log segment` (e.g. block 1)<br/>
`byte 16: 32 bits unsigned int: size of log segment` (e.g. 250K blocks)<br/>

Each 4KB log entry block will have:

`byte 0: 32 bits unsigned int: generation number`<br/>
`byte 4: 32 bits unsigned int: number of entries in block`<br/>
`byte 8: 64 bits unsigned int: checksum`<br/>
`byte 16: first 20-byte entry`<br/>
`byte 36: second 20-byte entry`<br/>
`...`

Each log entry will have a 4-byte opcode (0 for `add_node`, 1 for `add_edge`, 2 for `remove_node`, 3 for `remove_edge`) and two 64-bit node IDs (only one of which is used for `add_node` and `remove_node`).

Note: the checksum can fail (i.e. say the block is valid when the block is actually invalid) for a small number of random bit patterns. Unfortunately the all-zero case -- which is a common bit pattern for unwritten blocks on disk -- happens to one where the checksum fails. To get around this, either write a constant 'magic number' in each block (just a constant number in each valid block that ensures that all-zeros is not a valid block), or add a constant value to the checksum function (i.e., the checksum is the XOR of all previous words plus some fixed value).
