/*
 *   ccache_list.h -- definitions for CS 2270 linked list package
 *   
 *   Mark Peterson  9/91
 *
 *  $Id: ccache_list.h,v 1.1 1999/11/03 21:40:57 golda Exp $
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
 *  
 */
#ifndef _CCACHE_LIST_H_
#define _CCACHE_LIST_H_

typedef struct list_node {	/*list node type */
	struct list_node *next;	/*points to the following node */
	struct list_node *previous;	/*points to the previous node */
	Datum *data;		/*stores data record in list */
} List_Node;


typedef struct {		/*list header node */
	List_Node *first;	/*points to the first node */
	List_Node *last;	/*points to the last node */
	unsigned int count;
	/*keeps count of the number of nodes in list */
	int (*compare) ();	/*A compare function */
} Linked_List;


/*
   **The list toolkit functions:
 */

Linked_List *list_create();	/*initialize list header block */
void list_destroy();		/*destroy list header block */
List_Node *list_insert();	/*insert a new node in the list */
Datum *list_delete();		/*delete a node from the list */
List_Node *list_find();		/*find a node in the list */
Boolean list_apply();		/*apply a function to each node in the list */

/*Built in Macros */
#define list_first(head) ((head)->first)	/*find the first node in the list */
#define list_next(node) ((node)->next)	/*find the next node in the list */
#define list_last(head) ((head)->last)	/*find the last node in the list */
#define list_previous(node) ((node)->previous)
/*find the previous node in the list */

#define list_getdata(node) ((node)->data)	/*get the data from the list */
void list_putdata();		/*modify the data in the node in the list */
#define list_length(head) ((head)->count)	/*find the length of the list */

#endif
