#ifndef SAMPLE_ALLOC_H
#define SAMPLE_ALLOC_H

#include <stdint.h>

/*
 * Dynamic user-sample extent descriptor.
 *
 * Stored at offset 480 in the first 512-byte header area.
 * The legacy sample header remains at offset 0 and audio at +512.
 * A descriptor is committed only after its sample data and header
 * have been written and verified.
 */
#define SMP_ALLOC_MAGIC       0x31414D53u  /* "SMA1" */
#define SMP_ALLOC_DELETE_MAGIC 0x31444D53u /* rejected experimental SMD1; never authoritative */
#define SMP_ALLOC_VERSION     1u
#define SMP_ALLOC_COMMITTED   0x434F4D54u  /* "COMT" */
#define SMP_ALLOC_OFFSET      480u
#define SMP_ALLOC_SIZE        32u

/* Authoritative journal is fixed at the FIRST pool sector. Payload writes
 * cannot reach its first bytes: every extent starts at/above SMP_USER_BASE,
 * and WRITE starts at +512. SMJ1 and FSMP are incomparable bit patterns, so
 * even a torn legacy FSMP header program cannot create this anchor.
 * CLAIM/PUBLISH/DELETE entries have authority only inside this anchored sector.
 * A CRC/marker in sample audio conveys no ownership or deletion authority. */
#define SMP_JOURNAL_MAGIC     0x314A4D53u /* SMJ1 */
#define SMP_JOURNAL_CLAIM     0x434A4D53u /* SMJC */
#define SMP_JOURNAL_PUBLISH   0x504A4D53u /* SMJP */
#define SMP_JOURNAL_DELETE    0x444A4D53u /* SMJD */
#define SMP_JOURNAL_BLANK     0u
#define SMP_JOURNAL_READY     1u
#define SMP_JOURNAL_QUARANTINED 2u /* unanchored media: preserved, unpublished, read-only */
#define SMP_JOURNAL_LOCKED    3u

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t logical_slot;
    uint8_t sector_count;
    uint32_t generation;
    uint32_t data_len;
    uint32_t data_crc;
    uint32_t record_crc;
    uint32_t reserved;
    uint32_t commit;
} smp_alloc_desc_t;

_Static_assert(sizeof(smp_alloc_desc_t) == SMP_ALLOC_SIZE,
               "sample allocation descriptor must be 32 bytes");
_Static_assert(SMP_ALLOC_OFFSET + SMP_ALLOC_SIZE == 512u,
               "sample allocation descriptor must end at audio offset");
_Static_assert(__builtin_offsetof(smp_alloc_desc_t, commit) == 28u,
               "commit marker must be the final descriptor word");

#endif
