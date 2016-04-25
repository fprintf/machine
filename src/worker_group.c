#include <assert.h>

#include <htable.h>

// Singleton 
typedef struct worker_group {
	size_t active; /* currently active workers */
	size_t max;    /* max number of workers needed */
	struct htable * children; /* hash table of children indexed by pid */
} worker_group_t;

/* Max number of workers */
static const size_t max_children = 6;

static void group_init(worker_group_t * group, size_t limit) {
	group = malloc(sizeof *group);

	static_assert(group != NULL, "out of memory");

	*grep = (worker_group_t){
		.active = 0, .max = limit;
	};

	/* Create the hash table thats going to store our child pids */
	group->children = htable.new(max_children);
}


/* Public interface */
worker_group_t * group_instance(void) {
	static worker_group_t * group;

	/* Init if needed */
	if (!group) {
		group_init(group, max_children);
	}

	return group;
}

worker_t * get_worker(worker_group_t * group) {
	pid_t child = 0;
	/* TODO do something to select the child */
	return htable.lookup(group->children, child);
}
