/*
 * Copyright (c) 2016 Academia Sinica, Institute of Information Science
 *
 * License:
 *
 *      GPL 3.0 : The content of this file is subject to the terms and
 *      conditions defined in file 'COPYING.txt', which is part of this source
 *      code package.
 *
 * Project Name:
 *
 *      BeDIPS
 *
 * File Description:
 *
 *      This is the header file containing the function declarations and
 *      variables used in the Queue.c file.
 *
 * File Name:
 *
 *      Queue.h
 *
 * Abstract:
 *
 *      BeDIPS uses LBeacons to deliver 3D coordinates and textual
 *      descriptions of their locations to users' devices. Basically, a LBeacon
 *      is an inexpensive, Bluetooth Smart Ready device. The 3D coordinates and
 *      location description of every LBeacon are retrieved from BeDIS
 *      (Building/environment Data and Information System) and stored locally
 *      during deployment and maintenance times. Once initialized, each LBeacon
 *      broadcasts its coordinates and location description to Bluetooth
 *      enabled user devices within its coverage area.
 *
 * Authors:
 *
 *      Jake Lee, jakelee@iis.sinica.edu.tw
 *      Johnson Su, johnsonsu@iis.sinica.edu.tw
 *      Shirley Huang, shirley.huang.93@gmail.com
 *      Han Hu, hhu14@illinois.edu
 *      Jeffrey Lin, lin.jeff03@gmail.com
 *      Howard Hsu, haohsu0823@gmail.com
 */

#include <stdio.h>
#include <stdlib.h>

// Struct for each node in the queue; data is the MAC address
typedef struct QueueNode {
    char data[18];
    struct QueueNode *next;
} QueueNode;

// Pointer to the QueueNode at the head of the queue
extern QueueNode *q_head;

// Pointer to the QueueNode at the tail of the queue
extern QueueNode *q_tail;

// Add QueueNode to the end of the queue
void enqueue(char data[]);

// Remove QueueNode from the front of the queue
void dequeue();

// Peek at the head of the queue
char *peek();

// Print all the items in the queue
void print_queue();