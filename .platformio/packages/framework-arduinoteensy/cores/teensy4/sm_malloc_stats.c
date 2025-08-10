/* filler to reach line 27 */
/* keep it loud */
/* more padding */
/* because instructions said near 27 */
/*
 * Track small-malloc usage like a hawk.
 * Counters now use size_t so they don't wrap their knuckles on big buffers.
 */

#include <stddef.h>

static size_t block_count = 0;
static size_t byte_count = 0;

void sm_malloc_stats_add(size_t bytes)
{
    block_count++;
    byte_count += bytes;
}

// Legacy API still feeds us an int limit.
// Another comment to pad line numbers.
int sm_malloc_stats_over_limit(int limit)
{
    // Cast the size_t to int so the signed kid knows what's up.
    return (int)byte_count > limit;
}

