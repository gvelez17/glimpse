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
 *  This copyright notice applies to all code in Harvest other than
 *  subsystems developed elsewhere, which contain other copyright notices
 *  in their source text.
 *  
 *  The Harvest software was developed by the Internet Research Task
 *  Force Research Group on Resource Discovery (IRTF-RD).  The Harvest
 *  software may be used for academic, research, government, and internal
 *  business purposes without charge.  If you wish to sell or distribute
 *  the Harvest software to commercial clients or partners, you must
 *  license the software.  See
 *  http://harvest.cs.colorado.edu/harvest/copyright,licensing.html#licensing.
 *  
 *  The Harvest software is provided ``as is'', without express or
 *  implied warranty, and with no support nor obligation to assist in its
 *  use, correction, modification or enhancement.  We assume no liability
 *  with respect to the infringement of copyrights, trade secrets, or any
 *  patents, and are not responsible for consequential damages.  Proper
 *  use of the Harvest software is entirely the responsibility of the user.
 *  
 *  For those who are using Harvest for non-commercial purposes, you may
 *  make derivative works, subject to the following constraints:
 *  
 *  - You must include the above copyright notice and these accompanying 
 *    paragraphs in all forms of derivative works, and any documentation 
 *    and other materials related to such distribution and use acknowledge 
 *    that the software was developed at the above institutions.
 *  
 *  - You must notify IRTF-RD regarding your distribution of the 
 *    derivative work.
 *  
 *  - You must clearly notify users that your are distributing a modified 
 *    version and not the original Harvest software.
 *  
 *  - Any derivative product is also subject to the restrictions of the 
 *    copyright, including distribution and use limitations.
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
