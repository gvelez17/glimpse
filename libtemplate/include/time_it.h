/*
 *  time_it.h
 *
 *  $Id: time_it.h,v 1.1 1999/11/03 21:40:57 golda Exp $
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
#ifndef _TIME_IT_H_
#define _TIME_IT_H_

/*Automatically include timer.h only if not previously included. */


/*Simple structure to track blocked processes in an OS. */
typedef struct timer_record {
	int eventnum;		/*Tracks the order of items enqueued. */
	int time_in_secs;	/*Non-cumulative time in seconds for process to 
				   remain blocked in queue. */
	int (*ProctoCall) ();	/*A pointer to the blocked procedure. */
} Time_Node;

void InitTimer();
int SetTimer();
Boolean CancelTimer();
void HandleTimerSignal();
void Freeze();
void Thaw();
void DisplayQueue();

#endif
