#ifndef SAMPLE_STORAGE_H
#define SAMPLE_STORAGE_H

#include <stdint.h>

/*
 * User sample storage.
 *
 * Eight public logical slots share the original 240 KiB sector pool.
 * Legacy geometry defines the pool size only; it does not define ownership
 * or eight fixed partitions. Publication requires the existing journal.
 */
#define SMP_USER_LOGICAL_SLOTS  8u
#define SMP_USER_LEGACY_SLOTS    3u
#define SMP_USER_SLOTS           SMP_USER_LOGICAL_SLOTS

#define SMP_USER_BASE           0xA0000u
#define SMP_USER_SIZE           0x14000u
#define SMP_USER_POOL_SIZE      (SMP_USER_LEGACY_SLOTS * SMP_USER_SIZE)
#define SMP_USER_SECTOR_SIZE    0x1000u
#define SMP_USER_SECTOR_COUNT   (SMP_USER_POOL_SIZE / SMP_USER_SECTOR_SIZE)
#define SMP_USER_SECTORS_PER_LEGACY_SLOT (SMP_USER_SIZE / SMP_USER_SECTOR_SIZE)
#define SMP_USER_DATA           512u

_Static_assert(SMP_USER_POOL_SIZE == 0x3C000u, "user sample pool must remain 240 KiB");
_Static_assert(SMP_USER_SECTOR_COUNT == 60u, "user sample pool must contain 60 sectors");

#define SMP_USER_MAGIC          0x504D5346u  /* "FSMP" */

#endif
