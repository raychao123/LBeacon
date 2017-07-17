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
 *      This file contains the implementation of a queue data structure. It has
 *      the feature of enqueueing a node to the front of the queue and
 *      dequeueing a node to the end of the queue. It can also peek at the
 *      node in the front of the queue and print a list of all the nodes in the
 *      queue.
 *
 * File Name:
 *
 *      Queue.c
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

#include "Queue.h"

// Pointer to the QueueNode at the head of the queue
QueueNode *q_head = NULL;

// Pointer to the QueueNode at the tail of the queue
QueueNode *q_tail = NULL;

/*
 *  enqueue:
 *
 *  Add data to the end of the queue. @todo
 *
 *  Parameters:
 *
 *  data - @todo
 *
 *  Return value:
 *
 *  None
 */
void enqueue(char data[]) {
    int i;
    struct QueueNode *temp =
        (struct QueueNode *)malloc(sizeof(struct QueueNode));
    for (i = 0; i < 18; i++) {
        temp->data[i] = data[i];
    }
    temp->next = NULL;
    if (q_head == NULL && q_tail == NULL) {
        q_head = q_tail = temp;
        return;
    }
    q_tail->next = temp;
    q_tail = temp;
    free(temp);
}

/*
 *  dequeue:
 *
 *  Remove data from the head of the queue. @todo
 *
 *  Parameters:
 *
 *  None
 *
 *  Return value:
 *
 *  None
 */
void dequeue() {
    struct QueueNode *temp = q_head;
    if (q_head == NULL) {
        return;
    }
    if (q_head == q_tail) {
        q_head = q_tail = NULL;
    } else {
        q_head = q_head->next;
    }
    free(temp);
}

/*
 *  peek:
 *
 *  Peek at the head of the queue. @todo
 *
 *  Parameters:
 *
 *  None
 *
 *  Return value:
 *
 *  return_value - @todo
 */
char *peek() {
    if (q_head == NULL) {
        return NULL;
    }
    char *return_value = &q_head->data[0];
    return return_value;
}

/*
 *  print_queue:
 *
 *  Print all the items in the queue. @todo
 *
 *  Parameters:
 *
 *  None
 *
 *  Return value:
 *
 *  None
 */
void print_queue() {
    struct QueueNode *temp = q_head;
    printf("%s", "Queue: ");
    while (temp != NULL) {
        printf("%s ", &temp->data[0]);
        temp = temp->next;
    }
    printf("\n");
}