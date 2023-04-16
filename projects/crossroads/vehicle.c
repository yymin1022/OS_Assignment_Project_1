
#include <stdio.h>
#include <stdlib.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "projects/crossroads/vehicle.h"
#include "projects/crossroads/map.h"
#include "projects/crossroads/ats.h"

/* path. A:0 B:1 C:2 D:3 */
const struct position vehicle_path[4][4][10] = {
	/* from A */ {
		/* to A */
		{{-1,-1},},
		/* to B */
		{{4,0},{4,1},{4,2},{5,2},{6,2},{-1,-1},},
		/* to C */
		{{4,0},{4,1},{4,2},{4,3},{4,4},{4,5},{4,6},{-1,-1},},
		/* to D */
		{{4,0},{4,1},{4,2},{4,3},{4,4},{3,4},{2,4},{1,4},{0,4},{-1,-1}}
	},
	/* from B */ {
		/* to A */
		{{6,4},{5,4},{4,4},{3,4},{2,4},{2,3},{2,2},{2,1},{2,0},{-1,-1}},
		/* to B */
		{{-1,-1},},
		/* to C */
		{{6,4},{5,4},{4,4},{4,5},{4,6},{-1,-1},},
		/* to D */
		{{6,4},{5,4},{4,4},{3,4},{2,4},{1,4},{0,4},{-1,-1},}
	},
	/* from C */ {
		/* to A */
		{{2,6},{2,5},{2,4},{2,3},{2,2},{2,1},{2,0},{-1,-1},},
		/* to B */
		{{2,6},{2,5},{2,4},{2,3},{2,2},{3,2},{4,2},{5,2},{6,2},{-1,-1}},
		/* to C */
		{{-1,-1},},
		/* to D */
		{{2,6},{2,5},{2,4},{1,4},{0,4},{-1,-1},}
	},
	/* from D */ {
		/* to A */
		{{0,2},{1,2},{2,2},{2,1},{2,0},{-1,-1},},
		/* to B */
		{{0,2},{1,2},{2,2},{3,2},{4,2},{5,2},{6,2},{-1,-1},},
		/* to C */
		{{0,2},{1,2},{2,2},{3,2},{4,2},{4,3},{4,4},{4,5},{4,6},{-1,-1}},
		/* to D */
		{{-1,-1},}
	}
};

static int is_position_outside(struct position pos)
{
	return (pos.row == -1 || pos.col == -1);
}

static void tmp_lock_acquire(struct vehicle_info *vi, struct position pos_cur)
{
	if(pos_cur.row == 1)
		lock_acquire(&vi->map_locks[2][3]);
	else if(pos_cur.row == 2)
		lock_acquire(&vi->map_locks[3][4]);
	else if(pos_cur.row == 4)
		lock_acquire(&vi->map_locks[3][2]);
	else if(pos_cur.row == 5)
		lock_acquire(&vi->map_locks[4][3]);
}

static void tmp_lock_release(struct vehicle_info *vi, struct position pos_cur)
{
	if(pos_cur.row == 1)
		lock_release(&vi->map_locks[2][3]);
	else if(pos_cur.row == 2)
		lock_release(&vi->map_locks[3][4]);
	else if(pos_cur.row == 4)
		lock_release(&vi->map_locks[3][2]);
	else if(pos_cur.row == 5)
		lock_release(&vi->map_locks[4][3]);
}

/* return 0:termination, 1:success, -1:fail */
static int try_move(int start, int dest, int step, struct vehicle_info *vi)
{
	struct position pos_cur, pos_next;

	pos_next = vehicle_path[start][dest][step];
	pos_cur = vi->position;

	moving_cnt++;
	if (vi->state == VEHICLE_STATUS_RUNNING) {
		/* check termination */
		if (is_position_outside(pos_next)) {
			/* actual move */
			vi->position.row = vi->position.col = -1;
			return 0;
		}
	}

	if(step < 2){
		lock_acquire(&vi->map_locks[pos_next.row][pos_next.col]);
	}
	/* lock next position */
	if (vi->state == VEHICLE_STATUS_READY) {
		/* start this vehicle */
		vi->state = VEHICLE_STATUS_RUNNING;
	} else {
		/* lock routes inside crossroad before enter */
		if(step == 2)
		{
			/* make tmp lock before lock inside crossroad */
			tmp_lock_acquire(vi, pos_cur);

			/* make lock of routes inside crossroad */
			int tmp_step = step;
			struct position tmp_pos_next = vehicle_path[start][dest][tmp_step++];
			while(tmp_pos_next.col > 1 && tmp_pos_next.col < 5 && tmp_pos_next.row > 1 && tmp_pos_next.row < 5)
			{
				while(!lock_try_acquire(&vi->map_locks[tmp_pos_next.row][tmp_pos_next.col]))
					continue;
				tmp_pos_next = vehicle_path[start][dest][tmp_step++];
			}

			/* release tmp lock after lock inside crossroad */
			tmp_lock_release(vi, pos_cur);
		}
		if(step <= 2 || (pos_cur.col > 1 && pos_cur.col < 5 && pos_cur.row > 1 && pos_cur.row < 5))
		{
			/* release current position */
			lock_release(&vi->map_locks[pos_cur.row][pos_cur.col]);
		}
	}
	/* update position */
	vi->position = pos_next;
	moving_cnt--;
	return 1;
}

void init_on_mainthread(int thread_cnt){
	/* Called once before spawning threads */
	global_sema = (struct semaphore*)malloc(sizeof(struct semaphore));
	sema_init(global_sema, 0);
	
	/* moving_cnt means Count of Currently Moving Car                  */
	/* If a car is in Busy Wait, or waiting for Entrance of Crossroad, */
	/* moving_cnt value will not be 0                                  */
	/* This value is used for carculating global_sema Semaphore Offset */
	/* global_sema is used for calculating Step Count                  */
	/* If global_sema is 0 so sema_try_down returns false,             */
	/* It is recognized as All of cars moved and 1 Step is done        */
	moving_cnt = 0;
	total_cnt = thread_cnt;
}

void vehicle_loop(void *_vi)
{
	int res;
	int start, dest, step;

	struct vehicle_info *vi = _vi;

	start = vi->start - 'A';
	dest = vi->dest - 'A';

	vi->position.row = vi->position.col = -1;
	vi->state = VEHICLE_STATUS_READY;

	step = 0;
	while (1) {
		/* vehicle main code */
		if(sema_try_down(global_sema))
		{
			res = try_move(start, dest, step, vi);

			if (res == 1)
				step++;

			/* termination condition. */ 
			if (res == 0)
				break;

			/* unitstep change! */
			unitstep_changed();
		}
		else
		{
			crossroads_step++;
			sema_init(global_sema, total_cnt - moving_cnt);
		}

	}	

	/* status transition must happen before sema_up */
	vi->state = VEHICLE_STATUS_FINISHED;
}
