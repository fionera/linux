/*
 *  *  fs/partitions/lgemmc.h
 */
int lgemmc_partition(struct parsed_partitions *state);

int lgemmc_get_partnum(const char *name);
char *lgemmc_get_partname(int partnum);
unsigned int lgemmc_get_partsize(int partnum);
unsigned int lgemmc_get_filesize(int partnum);
unsigned long long lgemmc_get_partoffset(int partnum);

