/*
 * mii_mish_dd.c
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mii.h"

extern mii_t g_mii;


static void
_mii_mish_dd(
		void * param,
		int argc,
		const char * argv[])
{
	if (!argv[1] || !strcmp(argv[1], "list")) {
		mii_dd_t *d = g_mii.dd.drive;
		printf(" ID %-16s %-20s\n", "Card", "Name");
		while (d) {
			printf("%d:%d %-16s %-20s : %s\n",
					d->slot_id, d->drive,
					d->slot->drv->desc,
					d->name, d->file ? d->file->pathname : "*empty*");
			d = d->next;
		}
		return;
	}
}

#include "mish.h"

MISH_CMD_NAMES(dd, "dd", "disk");
MISH_CMD_HELP(dd,
		"mii: disk commands",
		" <default>|list: list all disk drives"
		);
MISH_CMD_REGISTER(dd, _mii_mish_dd);