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
 *      variables used in the LinkedList.c file.
 *
 * File Name:
 *
 *      LinkedList.h
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Struct for storing scanned timestamp and MAC address of bluetooth device
typedef struct PushList {
    long long initial_scanned_time;
    char scanned_mac_address[18];
} PushList;

// Struct for each node in the linked list; data is a PushList struct
typedef struct LinkedListNode {
    PushList data;
    struct LinkedListNode *next;
} LinkedListNode;

// Pointer to the LinkedListNode at the head of the linked list
extern LinkedListNode *ll_head;

// Pointer to current LinkedListNode of the linked list
extern LinkedListNode *ll_current;

// Add LinkedListNode to the front of the linked list
void insert_first(PushList data);

// Remove selected LinkedListNode from the linked list
void delete_node(PushList data);

// Get the length of the linked list
int length();

// Print all the items in the linked list
void print_linked_list();