/* P: the current body (including fitted wing) and wheel poses cast one
   silhouette. Course tint and sun/shade factors come from course_shadow;
   this optional geometry projection is new. */
static int draw_projected_shadow(race_view *owner, const race_asset *env, const car_meshes *meshes,
                                 const car_model *model, const r3d_view *car,
                                 const r3d_view *receiver, float sun, float level, int posed,
                                 const float wheels[4][3], const float snapshot[4][8],
                                 const gt4_pit_crew *pit)
{
    float plane[4], direction[3] = {env->shadow_way[0], -1, env->shadow_way[2]};
    for (int i = 0; i < 3; i++)
        plane[i] = receiver->rot[i * 3 + 1];
    plane[3] = -dot3(plane, receiver->pos);
    float opacity = env->shadow_alpha *
                    (sun * (env->shadow_sun - env->shadow_shade) + env->shadow_shade) * level;
    opacity *= meshes->cast_alpha;
    if (opacity <= 0 || getenv("MOON_NO_CAST_SHADOW"))
        return 0;
    if (!r3d_car_shadow_begin(&owner->projected_shadow, car, plane, direction))
        return 0;
    r3d_view body = pit_body_place(model, car, pit, 1);
    r3d_car_shadow_mesh(owner->projected_shadow, posed && meshes->body ? meshes->body : meshes->car,
                        &body);
    if (posed && meshes->body)
        for (int i = 0; i < 4; i++) {
            r3d_view wheel;
            wheel_place(model, wheels[i], snapshot ? snapshot[i] : NULL, i, car, &wheel);
            pit_mounted_place(pit, (unsigned)(i ^ 1), &wheel);
            r3d_car_shadow_mesh(owner->projected_shadow, meshes->wheel[i], &wheel);
        }
    r3d_car_shadow_end(owner->projected_shadow, opacity, env->shadow_rgb);
    /* Retain the authored contact blot beneath the geometric projection. */
    r3d_view contact = *receiver;
    contact.shadow_alpha = env->shadow_alpha;
    memcpy(contact.shadow_rgb, env->shadow_rgb, sizeof contact.shadow_rgb);
    r3d_draw(meshes->shadow, &contact);
    return 1;
}

/* P: directional depth silhouettes do not provide the small diffuse contact
   footprint. Wheel centres use the same snapshot/pose as the visible wheels.
   Original: func_00454110 +0x28 (hub placement, through wheel_place). */
static void draw_ambient_contacts(race_view *owner, const car_meshes *meshes,
                                  const car_model *model, const r3d_view *car,
                                  const r3d_view *receiver, int posed, const float wheels[4][3],
                                  const float snapshot[4][8])
{
    if (!model)
        return;
    if (!owner->wheel_contact)
        owner->wheel_contact = r3d_wheel_contact_create();
    if (!owner->wheel_contact)
        return;
    float normal[3] = {receiver->rot[1], receiver->rot[4], receiver->rot[7]};
    float centre[3] = {0}, coverage = 0;
    for (int i = 0; i < 4; i++) {
        float width = model->tire_size[i][0], radius = model->tire_size[i][1];
        if (width <= 0 || radius <= 0)
            continue;
        r3d_view wheel;
        const float rest[3] = {0};
        wheel_place(model, posed ? wheels[i] : rest, snapshot ? snapshot[i] : NULL, i, car, &wheel);
        float delta[3];
        for (int k = 0; k < 3; k++)
            delta[k] = wheel.pos[k] - receiver->pos[k];
        float height = dot3(delta, normal);
        float contact = 1 - fminf(1, fmaxf(0, (height - radius) / (radius * .5f)));
        for (int k = 0; k < 3; k++)
            centre[k] += (wheel.pos[k] - normal[k] * height) * .25f;
        coverage += contact * .25f;
        if (contact <= 0)
            continue;
        r3d_view patch = *receiver;
        patch.daylight = patch.modern_light = 0;
        patch.shadow_alpha = .65f * contact;
        memset(patch.shadow_rgb, 0, sizeof patch.shadow_rgb);
        for (int k = 0; k < 3; k++) {
            patch.pos[k] = wheel.pos[k] - normal[k] * height;
            patch.rot[k * 3] *= width * .5f + radius * .15f;
            patch.rot[k * 3 + 2] *= radius * .75f;
        }
        r3d_draw(owner->wheel_contact, &patch);
    }
    if (coverage > 0) {
        /* P: broad, sun-independent ambient footprint, separate from the
           directional cast shadow. Dimensions come from the loaded assembly. */
        r3d_view body = *receiver;
        body.daylight = body.modern_light = 0;
        body.shadow_alpha = .45f * coverage;
        memset(body.shadow_rgb, 0, sizeof body.shadow_rgb);
        memcpy(body.pos, centre, sizeof centre);
        for (int k = 0; k < 3; k++) {
            body.rot[k * 3] *= (meshes->hi[0] - meshes->lo[0]) * .6f;
            body.rot[k * 3 + 2] *= (meshes->hi[2] - meshes->lo[2]) * .6f;
        }
        r3d_draw(owner->wheel_contact, &body);
    }
}
