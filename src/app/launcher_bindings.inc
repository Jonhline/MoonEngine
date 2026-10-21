static int studio_binding_is_choice(int id)
{
    return id >= 2001 && id <= 2011;
}
/* option.ad KeyconfigRoot/DS2's functions, in its order (the earlier launcher's page) */
static const int studio_binding_actions[11] = {-2,
                                               GT4_PAD_ACCEL,
                                               GT4_PAD_BRAKE,
                                               GT4_PAD_REVERSE,
                                               GT4_PAD_SIDE,
                                               GT4_PAD_SHIFTUP,
                                               GT4_PAD_SHIFTDOWN,
                                               GT4_PAD_BACKVIEW,
                                               GT4_PAD_PLAYVIEW,
                                               GT4_PAD_SPECIAL,
                                               GT4_PAD_GHOST};
static const char *const studio_binding_names[11] = {
    "Steering",       "Gas",       "Brake",       "Reverse", "E-brake", "Upshift (MT)",
    "Downshift (MT)", "Rear View", "Change View", "Special", "Ghost"};
static const char *const studio_key_names[21] = {"None",
                                                 "Select / Back",
                                                 "L3",
                                                 "R3",
                                                 "Start",
                                                 "Up",
                                                 "Right",
                                                 "Down",
                                                 "Left",
                                                 "L2 / LT",
                                                 "R2 / RT",
                                                 "L1 / LB",
                                                 "R1 / RB",
                                                 "Triangle / Y",
                                                 "Circle / B",
                                                 "Cross / A",
                                                 "Square / X",
                                                 "Left Stick Up",
                                                 "Left Stick Down",
                                                 "Right Stick Up",
                                                 "Right Stick Down"};
static const char *const studio_steer_names[4] = {"None", "Left Stick", "Right Stick",
                                                  "Left / Right Buttons"};
/* a function's current assignment as the earlier launcher read it */
static int studio_binding_get(const gt4_pad_config *c, int i)
{
    if (!i) {
        if (c->analog[GT4_PAD_LX] == -2)
            return 1;
        if (c->analog[GT4_PAD_RX] == -2)
            return 2;
        if (c->button[GT4_PAD_LEFT] == GT4_PAD_LSTEER && c->button[GT4_PAD_RIGHT] == GT4_PAD_RSTEER)
            return 3;
        return 0;
    }
    for (int k = 0; k < 16; k++)
        if (c->button[k] == studio_binding_actions[i])
            return k + 1;
    for (int k = 2; k < 6; k++)
        if (c->analog[k] == studio_binding_actions[i])
            return k + 15;
    return 0;
}
/* Only the confirmed function and destination change. Other functions may
   legitimately have several bindings in the original controller map. */
static void studio_binding_set(gt4_pad_config *c, int i, int value)
{
    if (studio_binding_get(c, i) == value)
        return;
    int action = studio_binding_actions[i];
    for (int k = 0; k < 16; k++)
        if (i ? c->button[k] == action
              : c->button[k] == GT4_PAD_LSTEER || c->button[k] == GT4_PAD_RSTEER)
            c->button[k] = -1;
    for (int k = 0; k < 6; k++)
        if (c->analog[k] == action)
            c->analog[k] = -1;
    if (!i) {
        if (value == 1)
            c->analog[GT4_PAD_LX] = -2;
        if (value == 2)
            c->analog[GT4_PAD_RX] = -2;
        if (value) {
            c->button[GT4_PAD_LEFT] = GT4_PAD_LSTEER;
            c->button[GT4_PAD_RIGHT] = GT4_PAD_RSTEER;
        }
    } else if (value >= 1 && value <= 16)
        c->button[value - 1] = action;
    else if (value >= 17 && value <= 20)
        c->analog[value - 15] = action;
}
/* The list edits a pending choice; only confirmation writes the live map. */
typedef struct {
    int open, action, value, port;
} studio_binding_choice;
static studio_binding_choice studio_binding_picker;
static void studio_binding_begin(studio_binding_choice *p, const gt4_pad_config *c, int action,
                                 int port)
{
    *p = (studio_binding_choice){1, action, studio_binding_get(c, action), port};
}
static void studio_binding_move(studio_binding_choice *p, int delta)
{
    int n = p->action ? 21 : 4;
    p->value = ((p->value + delta) % n + n) % n;
}
static void studio_binding_confirm(studio_binding_choice *p, gt4_pad_config *c)
{
    studio_binding_set(c, p->action, p->value);
    p->open = 0;
}

static int studio_binding_conflict(const gt4_pad_config *c, int action, int value)
{
    if (!action || value < 1 || value > 20)
        return -1;
    int assigned = value <= 16 ? c->button[value - 1] : c->analog[value - 15];
    if (assigned == -1 || assigned == studio_binding_actions[action])
        return -1;
    if (assigned == GT4_PAD_LSTEER || assigned == GT4_PAD_RSTEER || assigned == -2)
        return 0;
    for (int i = 1; i < 11; i++)
        if (studio_binding_actions[i] == assigned)
            return i;
    return -1;
}
