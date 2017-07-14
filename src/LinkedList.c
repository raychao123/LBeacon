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
 *      @todo
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

LinkedListNode *ll_head = NULL;
LinkedListNode *ll_current = NULL;

// display the list
void printList() {
    struct LinkedListNode *ptr = ll_head;

    // start from the beginning
    printf("%s", "Linked List: ");
    while (ptr != NULL) {
        printf("%s ", &ptr->data.scanned_mac_addr[0]);
        ptr = ptr->next;
    }
    printf("\n");
}

// insert link at the first location
void insertFirst(PushList data) {
    int i;
    // create a link
    struct LinkedListNode *link =
        (struct LinkedListNode *)malloc(sizeof(struct LinkedListNode));

    link->data.initial_scanned_time = data.initial_scanned_time;
    for (i = 0; i < 18; i++) {
        link->data.scanned_mac_addr[i] = data.scanned_mac_addr[i];
    }

    // point it to old first node
    link->next = ll_head;

    // point first to new first node
    ll_head = link;
}

// delete a link with given key
void deleteNode(PushList data) {
    // start from the first link
    struct LinkedListNode *ll_current = ll_head;
    struct LinkedListNode *ll_previous = NULL;

    // if list is empty
    if (ll_head == NULL) {
        return;
    }

    // navigate through list
    while (strcmp(ll_current->data.scanned_mac_addr, data.scanned_mac_addr) !=
           0) {
        // if it is last node
        if (ll_current->next == NULL) {
            return;
        } else {
            // store reference to ll_current link
            ll_previous = ll_current;
            // move to next link
            ll_current = ll_current->next;
        }
    }

    // found a match, update the link
    if (ll_current == ll_head) {
        // change first to point to next link
        ll_head = ll_head->next;
    } else {
        // bypass the ll_current link
        ll_previous->next = ll_current->next;
    }

    return;
}

// is list empty
bool isEmpty() { return ll_head == NULL; }

int length() {
    int length = 0;
    struct LinkedListNode *ll_current;

    for (ll_current = ll_head; ll_current != NULL;
         ll_current = ll_current->next) {
        length++;
    }

    return length;
}