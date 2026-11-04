/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Manual SLICE edits remain usable in RAM, but persistence is disabled until
 * the dynamic sample allocator can reserve metadata storage exclusively.
 * Neither the old fixed-slot tail nor a relocated sample's +0x13000 sector
 * belongs to that sample necessarily: it may hold another sample or upload.
 * Do not read, erase or program those addresses, including for legacy samples
 * whose unused tail sectors are now available to the allocator. */

/* Keep the scan callback without reading unowned legacy metadata. */
static void slc_store_load(uint32_t k) { (void)k; }

/* 0: no sample to save; 4: persistence unavailable (edits remain in RAM). */
static int slc_store_write(uint32_t k)
{
    return k < SMP_USER_SLOTS && usr_nz[k] ? 4 : 0;
}

/* Preserve the existing deferred notification: wait until playback stops and
 * the SLICES page closes. Clearing the request does not clear the RAM edits. */
static void slc_store_poll(void)
{
    uint32_t k;
    int unavailable = 0;
    if (!slc_man_save || transport_busy() || slice_page_on())
        return;
    for (k = 0; k < SMP_USER_SLOTS; k++)
        if ((slc_man_save >> k) & 1u)
            unavailable |= slc_store_write(k) != 0;
    slc_man_save = 0;
    if (unavailable)
        ui_message("SLICES RAM ONLY");
}

static void slc_store_boot(void) { slc_man_load = slc_store_load; }
