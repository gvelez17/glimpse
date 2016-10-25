/*
 *  ccache_queue.h -
 *
 *  Mark Peterson  9/91	
 * 
 *  $Id: ccache_queue.h,v 1.1 1999/11/03 21:40:57 golda Exp $
 *
 *  ----------------------------------------------------------------------
 *  Copyright (c) 1994, 1995.  All rights reserved.
 *  
 *          Mic Bowman of Transarc Corporation.
 *          Peter Danzig of the University of Southern California.
 *          Darren R. Hardy of the University of Colorado at Boulder.
 *          Udi Manber of the University of Arizona.
 *          Michael F. Schwartz of the University of Colorado at Boulder. 
 *  
 */
#ifndef _CCACHE_QUEUE_H_
#define _CCACHE_QUEUE_H_

#include "ccache_list.h"

typedef List_Node Queue_Node;	/*Define a Queue node same as a List node. */
typedef Linked_List Queue;	/*Define a Queue header same as a List header.*/

/* **The queue toolkit contains: */

Queue *queue_create();		/*initialize queue header block */
void queue_destroy();		/*destory queue header block */
Boolean enqueue();		/*insert a new node at the end of the queue */
Datum *dequeue();		/*delete a node from the head of the queue */
Boolean queue_apply();		/*apply a function to each node in the queue */
Boolean queue_empty();		/*is the queue empty? */

/*Special functions designed for use in a priority timer queue. */
/*For use specifically with the time_it.h package. */
Boolean tenqueue();		/*Enqueue's a timer value. */
int tdequeue();			/*Dequeue's a timer value. */
Datum *head();

#define queue_length(q_head) ((q_head)->count)
/*Return the number of items in the queue. */

#endif
