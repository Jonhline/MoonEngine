/* Race field capacities. One place for the numbers the field-size request is
 * validated against; no other source should spell them out.
 *
 * MOON_CONDUCTOR_CARS = 6: the guest conductor's three arrays are six deep
 * (blocks +0x98, states +0x6698, 0x10 array +0xCA58). Widening them is a
 * rewrite, measured in docs/vulcan4/MEASURED-TAIL-SITES.md, so the conductor
 * stays six.
 *
 * MOON_MAX_CARS = 12: six official cars plus six phantom cars in a second
 * conductor outside the guest one. The renderer's opponent arrays are sized
 * from it (race_view.c OPPONENTS = MOON_MAX_OPPONENTS), the results/HUD/BSpec
 * arrays are sized from it, and a replay records the official six only.
 */
#ifndef MOON_RACE_LIMITS_H
#define MOON_RACE_LIMITS_H

#define MOON_CONDUCTOR_CARS 6
#define MOON_MAX_CARS 12
#define MOON_MAX_OPPONENTS (MOON_MAX_CARS - 1)
#define MOON_MAX_PHANTOMS (MOON_MAX_CARS - MOON_CONDUCTOR_CARS)

#endif
