/* P: use the existing course sun and ground attenuation with the current
   rigid assembly. This replaces no simulation, replay or authored model data. */
static void shadow_map_car(race_view *v, const car_meshes *meshes, const car_model *model,
                           const r3d_view *car, const car_shadow *ground, int posed,
                           const float wheels[4][3], const float snapshot[4][8],
                           const gt4_pit_crew *pit)
{
    if (!model || !r3d_shadow_map_contains(v->mapped_shadow, car->pos))
        return;
    float sun = ground && ground->on ? ground->sun : 1;
    float level = ground && ground->on ? ground->level : 1;
    float opacity = v->now->shadow_alpha * meshes->cast_alpha *
                    (sun * (v->now->shadow_sun - v->now->shadow_shade) + v->now->shadow_shade) *
                    level;
    /* P: opaque daylight geometry blocks direct sunlight completely. The
       authored alpha belongs to the old blended shadow, not light transmission.
       Original: func_003983E0 +0x0 (course shadow attenuation retained below). */
    if (car->daylight)
        opacity = 1;
    r3d_view body = pit_body_place(model, car, pit, 1);
    r3d_shadow_map_mesh(v->mapped_shadow, posed && meshes->body ? meshes->body : meshes->car, &body,
                        opacity);
    if (posed && meshes->body)
        for (int i = 0; i < 4; i++) {
            r3d_view wheel;
            wheel_place(model, wheels[i], snapshot ? snapshot[i] : NULL, i, car, &wheel);
            pit_mounted_place(pit, (unsigned)(i ^ 1), &wheel);
            r3d_shadow_map_mesh(v->mapped_shadow, meshes->wheel[i], &wheel, opacity);
        }
}

static void prepare_shadow_map(race_view *v, const r3d_view *car, int split_entry)
{
    if(car->daylight && car->sun_direction[1]<=0)return; /* No solar caster beneath the horizon. */

    v->mapped_shadow_ready = 0;
    r3d_shadow_map_bind(NULL);
    if (!r3d_shadow_map_size() || !v->now->shadow || rv_env("MOON_NO_CAST_SHADOW"))
        return;
    const float *centre = split_entry > 0 && v->other_pose[split_entry - 1].on
                              ? v->other_pose[split_entry - 1].pos
                              : car->pos;
    float direction[3] = {v->now->shadow_way[0], -1, v->now->shadow_way[2]};
    if (car->daylight)
        for (int c = 0; c < 3; c++)
            direction[c] = -car->sun_direction[c];
    if (!r3d_shadow_map_begin(&v->mapped_shadow, centre, direction))
        return;
    r3d_view player = *car;
    player.brake = v->now->pilot[0].brake;
    shadow_map_car(v, &v->mine, v->now->car, &player, &v->shadow[0], v->pose_on && v->wheels_on,
                   v->wheel_pose, v->wheel_snapshot_on ? v->wheel_snapshot : NULL, v->pit_crew[0]);
    for (int i = 0; i < OPPONENTS; i++) {
        const car_pose *p = &v->other_pose[i];
        if (!p->on || p->ghost > 0 || !v->now->other[i])
            continue;
        r3d_view other = *car;
        other.brake = v->now->pilot[i + 1].brake;
        memcpy(other.pos, p->pos, sizeof other.pos);
        memcpy(other.rot, p->rot, sizeof other.rot);
        shadow_map_car(v, &v->other[i], v->now->other[i], &other, &v->shadow[i + 1], p->wheels_on,
                       p->wheel_pose, p->snapshot_on ? p->snapshot : NULL, v->pit_crew[i + 1]);
    }
    if (car->daylight) {
        r3d_view terrain = *car;
        memset(terrain.pos, 0, sizeof terrain.pos);
        memset(terrain.rot, 0, sizeof terrain.rot);
        terrain.rot[0] = terrain.rot[4] = terrain.rot[8] = 1;
        r3d_shadow_map_mesh(v->mapped_shadow, v->world, &terrain, 1.f);
    }
    r3d_shadow_map_end(v->mapped_shadow);
    v->mapped_shadow_ready = 1;
}
