#include "image_order.h"

#include <ctype.h>
#include <string.h>

// Natural, case-insensitive order: "Photo 2" < "Photo 10", "beach" next to "Beach".
// Names that only differ in case or leading zeros fall back to a plain compare, so the order is stable.
static int natural_compare(const char *a, const char *b)
{
    const char *pa = a, *pb = b;
    while (*pa && *pb)
    {
        if (isdigit((unsigned char)*pa) && isdigit((unsigned char)*pb))
        {
            while (*pa == '0')
                pa++;
            while (*pb == '0')
                pb++;
            const char *ea = pa, *eb = pb;
            while (isdigit((unsigned char)*ea))
                ea++;
            while (isdigit((unsigned char)*eb))
                eb++;
            if (ea - pa != eb - pb)
                return (ea - pa) < (eb - pb) ? -1 : 1; // more digits = larger number
            int c = strncmp(pa, pb, ea - pa);
            if (c)
                return c;
            pa = ea;
            pb = eb;
        }
        else
        {
            int ca = tolower((unsigned char)*pa), cb = tolower((unsigned char)*pb);
            if (ca != cb)
                return ca - cb;
            pa++;
            pb++;
        }
    }
    if (*pa || *pb)
        return *pa ? 1 : -1;
    return strcmp(a, b);
}

// Pseudo-random sort key for random order: hash of the name, mixed with the seed. Sorting by it
// gives a shuffle without keeping a list in RAM; a new seed gives a new shuffle.
static uint32_t shuffle_key(const char *name, uint32_t seed)
{
    uint32_t h = 2166136261u ^ seed; // FNV-1a
    for (const unsigned char *p = (const unsigned char *)name; *p; p++)
        h = (h ^ *p) * 16777619u;
    h ^= h >> 16; // final mix, so similar names end up far apart
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

int image_order_compare(const char *a, const char *b, ImageOrder order, uint32_t seed)
{
    if (order == ORDER_RANDOM)
    {
        uint32_t ka = shuffle_key(a, seed), kb = shuffle_key(b, seed);
        if (ka != kb)
            return ka < kb ? -1 : 1;
    }
    return natural_compare(a, b);
}
