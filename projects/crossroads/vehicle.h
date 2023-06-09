#ifndef __PROJECTS_PROJECT1_VEHICLE_H__
#define __PROJECTS_PROJECT1_VEHICLE_H__

#include "projects/crossroads/position.h"
#include "threads/synch.h"

#define VEHICLE_STATUS_READY 	0
#define VEHICLE_STATUS_RUNNING	1
#define VEHICLE_STATUS_FINISHED	2

struct vehicle_info {
	char id;
	char state;
	char start;
	char dest;
	struct position position;
	struct lock **map_locks;
};

int intersect_cnt;
int moving_cnt;
int total_cnt;
struct semaphore *global_sema;
struct semaphore *intersect_sema;

void vehicle_loop(void *vi);

#endif /* __PROJECTS_PROJECT1_VEHICLE_H__ */
