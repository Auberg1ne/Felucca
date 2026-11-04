/* SPDX-License-Identifier: GPL-3.0-only */
/* Included by eng_sample.c. Provenance comes from a physically protected root,
 * not from recognizable bytes in an allocation's audio or descriptor area.
 * No compaction, adoption of old images, or extent recycling is performed. */
_Static_assert((SMP_JOURNAL_MAGIC & SMP_USER_MAGIC) != SMP_JOURNAL_MAGIC &&
               (SMP_JOURNAL_MAGIC & SMP_USER_MAGIC) != SMP_USER_MAGIC,
               "torn FSMP and SMJ1 programs must not impersonate each other");

static smp_alloc_desc_t smp_journal_header(void)
{
    smp_alloc_desc_t d = {0};
    d.magic = SMP_JOURNAL_MAGIC; d.version = 1u;
    d.logical_slot = SMP_USER_LOGICAL_SLOTS; d.sector_count = SMP_USER_SECTOR_COUNT;
    d.data_len = SMP_USER_SECTOR_SIZE; d.data_crc = SMP_ALLOC_SIZE;
    d.commit = 0xFFFFFFFFu; d.record_crc = st_crc32(&d, 28u);
    return d;
}

static int smp_journal_erased(const uint8_t *p, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) if (p[i] != 0xFFu) return 0;
    return 1;
}

static int smp_journal_crc_valid(const smp_alloc_desc_t *p)
{
    smp_alloc_desc_t d = *p;
    uint32_t crc = d.record_crc;
    d.record_crc = 0;
    return d.version == 1u && d.commit == SMP_ALLOC_COMMITTED && st_crc32(&d, 28u) == crc;
}

static int smp_journal_lock(void)
{
    smp_journal_state = SMP_JOURNAL_LOCKED;
    for (uint32_t s = 0; s < SMP_USER_SECTOR_COUNT; s++) smp_alloc_mark(s);
    for (uint32_t k = 0; k < SMP_USER_LOGICAL_SLOTS; k++) {
        smp_user_phys[k] = 0; smp_user_sectors[k] = 0; smp_user_unresolved[k] = 1;
    }
    return smp_journal_state;
}

static int smp_journal_boot(void)
{
    const uint8_t *root = fm1_xip_ptr(SMP_USER_BASE);
    smp_alloc_desc_t expected = smp_journal_header();
    uint16_t pending[SMP_USER_LOGICAL_SLOTS] = {0}, latest[SMP_USER_LOGICAL_SLOTS] = {0};
    uint8_t had_delete[SMP_USER_LOGICAL_SLOTS] = {0};
    smp_alloc_mark(0);                         /* never allocate payload over the root */
    smp_journal_next = SMP_ALLOC_SIZE;
    expected.commit = SMP_ALLOC_COMMITTED;
    if (memcmp(root, &expected, sizeof expected)) {
        if (smp_journal_erased(root, SMP_USER_POOL_SIZE)) {
            smp_journal_state = SMP_JOURNAL_BLANK;
            return smp_journal_state;
        }
        uint32_t magic; memcpy(&magic, root, sizeof magic);
        if (magic != SMP_USER_MAGIC && magic != 0xFFFFFFFFu)
            return smp_journal_lock();          /* corrupt/partial root: no fallback */
        smp_journal_state = SMP_JOURNAL_COMPAT;
        return smp_journal_state;               /* old samples readable, entire pool read-only */
    }
    smp_journal_state = SMP_JOURNAL_READY;
    for (uint32_t off = SMP_ALLOC_SIZE; off < SMP_USER_SECTOR_SIZE; off += SMP_ALLOC_SIZE) {
        const smp_alloc_desc_t *d = (const smp_alloc_desc_t *)(root + off);
        if (smp_journal_erased(root + off, SMP_ALLOC_SIZE)) {
            if (!smp_journal_erased(root + off, SMP_USER_SECTOR_SIZE - off))
                return smp_journal_lock();      /* no holes or skipped records */
            break;
        }
        if (!smp_journal_crc_valid(d) || d->logical_slot >= SMP_USER_LOGICAL_SLOTS)
            return smp_journal_lock();
        uint32_t k = d->logical_slot;
        if (d->generation != smp_user_generation[k] + 1u)
            return smp_journal_lock();          /* journal order, never guessed serial order */
        if (d->magic == SMP_JOURNAL_CLAIM) {
            uint32_t start = d->reserved, count = d->sector_count;
            if (!start || start >= SMP_USER_SECTOR_COUNT || !count ||
                count > SMP_USER_SECTOR_COUNT - start || d->data_crc ||
                d->data_len > count * SMP_USER_SECTOR_SIZE - SMP_USER_DATA)
                return smp_journal_lock();
            for (uint32_t s = start; s < start + count; s++) {
                if (smp_alloc_test(s)) return smp_journal_lock();
                smp_alloc_mark(s);
            }
            pending[k] = (uint16_t)off;
        } else if (d->magic == SMP_JOURNAL_PUBLISH) {
            if (!pending[k]) return smp_journal_lock();
            const smp_alloc_desc_t *claim = (const smp_alloc_desc_t *)(root + pending[k]);
            if (d->reserved != claim->reserved || d->sector_count != claim->sector_count ||
                d->generation != claim->generation || d->data_len != claim->data_len)
                return smp_journal_lock();
            latest[k] = (uint16_t)off; pending[k] = 0;
            smp_user_generation[k] = d->generation; smp_user_deleted[k] = 0;
            smp_user_has_records[k] = 1;
        } else if (d->magic == SMP_JOURNAL_DELETE) {
            if (d->reserved || d->sector_count || d->data_len || d->data_crc)
                return smp_journal_lock();
            latest[k] = pending[k] = 0; had_delete[k] = 1;
            smp_user_generation[k] = d->generation; smp_user_deleted[k] = 1;
            smp_user_has_records[k] = 1;
        } else return smp_journal_lock();
        smp_journal_next = (uint16_t)(off + SMP_ALLOC_SIZE);
    }
    for (uint32_t k = 0; k < SMP_USER_LOGICAL_SLOTS; k++) {
        if (!latest[k]) continue;
        const smp_alloc_desc_t *p = (const smp_alloc_desc_t *)(root + latest[k]);
        uint32_t phys = SMP_USER_BASE + p->reserved * SMP_USER_SECTOR_SIZE;
        const smp_alloc_desc_t *d = (const smp_alloc_desc_t *)(fm1_xip_ptr(phys) + SMP_ALLOC_OFFSET);
        if (!smp_alloc_extent_valid(phys, k, d) || d->generation != p->generation ||
            d->sector_count != p->sector_count || d->data_len != p->data_len || d->data_crc != p->data_crc) {
            smp_user_unresolved[k] = 1; smp_user_deleted[k] = had_delete[k];
            continue;                          /* never fall back to raw historical audio */
        }
        smp_user_phys[k] = phys; smp_user_sectors[k] = d->sector_count;
    }
    return smp_journal_state;
}
