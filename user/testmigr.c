/*
 * Simple test for cross-node process migration in PIOS.
 * See pwcrack.c for a more sophisticated and realistic test.
 *
 * Copyright (C) 2010 Yale University.
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Primary author: Bryan Ford
 */

#include <inc/stdio.h>
#include <inc/syscall.h>

void migrate(int node, int time)
{
	cprintf("testmigr (%d): migrating to node %d...\n", time, node);
	sys_get(0, (node << 8) | 0, NULL, NULL, NULL, 0);
	cprintf("testmigr (%d): now on node %d.\n", time, node);
}

int
main()
{
	// Basic migration test
	migrate(1, 1);
	migrate(2, 1);
	migrate(1, 2);
	migrate(2, 2);

	printf("testmigr done\n");
	return 0;
}

