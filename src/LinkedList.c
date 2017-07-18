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
 *      This file contains the implementation of a linked list data structure.
 *      It has the feature of inserting a node to the front of the linked list
 *      and deleting a specific node. It can also check the length of the linked
 *      list and print all the information stored.
 *
 * File Name:
 *
 *      LinkedList.c
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

#include "LinkedList.h"

// Pointer to the LinkedListNode at the head of the linked list
LinkedListNode *ll_head = NULL;

// Pointer to current LinkedListNode of the linked list
LinkedListNode *ll_current = NULL;

/*
 *  insert_first:
 *
 *  This function will allocate a LinkedListNode with the passed in PushList
 *  struct data and at the front of the linked list. Because the node will be
 *  placed at the front of the list, the head will be the new LinkedListNode.
 *
 *  Parameters:
 *
 *  data - PushList struct with scanned MAC address at the scanned timestamp
 *
 *  Return value:
 *
 *  None
 */
void insert_first(PushList data) {
    int i;

    /* Create a node */
    struct LinkedListNode *link =
        (struct LinkedListNode *)malloc(sizeof(struct LinkedListNode));

    /* Copy data passed into the function into the new node */
    link->data.initial_scanned_time = data.initial_scanned_time;
    for (i = 0; i < 18; i++) {
        link->data.scanned_mac_addr[i] = data.scanned_mac_addr[i];
    }

    /* Point it to old first node and point first to new first node */
    link->next = ll_head;
    ll_head = link;
}

/*
 *  delete_node:
 *
 *  This function will remove a specific node from the linked list. We will go
 *  through every node starting from the head and find the node that has the
 *  PushList struct data we passed into the function. Once we find the node, if
 *  it is the head, we need to replace the head with the next LinkedListNode it
 *  is connected to.
 *
 *  Parameters:
 *
 *  data - PushList struct with scanned MAC address at the scanned timestamp
 *
 *  Return value:
 *
 *  None
 */
void delete_node(PushList data) {
    /* Start from the first node */
    struct LinkedListNode *ll_current = ll_head;
    struct LinkedListNode *ll_previous = NULL;

    /* If head if empty */
    if (ll_head == NULL) {
        return;
    }

    /* Go through list */
    while (strcmp(ll_current->data.scanned_mac_addr, data.scanned_mac_addr) !=
           0) {
        /* If last node, return; else store reference to ll_current and move to
         * the next node */
        if (ll_current->next == NULL) {
            return;
        } else {
            ll_previous = ll_current;
            ll_current = ll_current->next;
        }
    }

    /* If found a node with matching data, update node */
    if (ll_current == ll_head) {
        ll_head = ll_head->next;
    } else {
        ll_previous->next = ll_current->next;
    }

    return;
}

/*
 *  length:
 *
 *  This function will get the length of the linked list. Starting from the
 *  head, we will increment the length variable by one as long as we haven't
 *  reached the end of the linked list.
 *
 *  Parameters:
 *
 *  None
 *
 *  Return value:
 *
 *  length - number of nodes in the linked list
 */
int length() {
    int length = 0;
    struct LinkedListNode *ll_current;

    /* Start from the beginning. Increment by one as long as the end hasn't been
     * found. */
    for (ll_current = ll_head; ll_current != NULL;
         ll_current = ll_current->next) {
        length++;
    }

    return length;
}

/*
 *  print_linked_list:
 *
 *  This function will print all the MAC addresses in the linked list. When
 *  printing, the MAC addresses will be in order starting from the head until we
 *  reach a next node that is NULL.
 *
 *  Parameters:
 *
 *  None
 *
 *  Return value:
 *
 *  None
 */
void print_linked_list() {
    /* Start from the first node */
    struct LinkedListNode *ptr = ll_head;

    /* Start from the beginning. Stop when the next node doesn't exist. */
    printf("%s", "Linked List: ");
    while (ptr != NULL) {
        printf("%s ", &ptr->data.scanned_mac_addr[0]);
        ptr = ptr->next;
    }
    printf("\n");
}