#include "mobile_options.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv)
{
    assert(argc >= 2);
    moon_options store, loaded;
    moon_options_load(&store, argv[1], NULL);
    moon_options_preset(&store, 0);
    moon_mobile_options_preset(&store, 0);
    if (argc > 2)
        moon_options_set(&store, "controls.touch_menu", "1");
    moon_mobile_options value;
    int menus;
    moon_mobile_options_load(&store, &value, &menus);
    if (menus || value.enabled)
        return 1;
    assert(value.size == 1 && value.opacity == 2);
    moon_options_preset(&store, 1);
    moon_mobile_options_preset(&store, 1);
    moon_mobile_options_load(&store, &value, &menus);
    assert(menus && value.enabled);
    value.size = 2;
    value.opacity = 4;
    value.layout = 1;
    value.steering = 1;
    moon_mobile_options_save(&store, &value, menus);
    moon_options_set(&store, "user.retained", "yes");
    assert(moon_options_save(&store));
    assert(moon_options_load(&loaded, argv[1], NULL));
    moon_mobile_options_load(&loaded, &value, &menus);
    assert(menus && value.enabled && value.size == 2 && value.opacity == 4 && value.layout == 1 &&
           value.steering == 1);
    assert(!strcmp(moon_options_get(&loaded, "user.retained", ""), "yes"));
    moon_mobile_options_preset(&loaded, 0);
    moon_mobile_options_load(&loaded, &value, &menus);
    assert(!menus && !value.enabled);
    puts("PASS: mobile Classic/Enhanced, shared-store round trip and retained unknown keys.");
    return 0;
}
