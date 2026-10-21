/* P: all nearby resident course geometry, opaque rigid cars and live gadgets.
   The main camera's visibility lists are deliberately not used for blockers. */
static void lamp_shadow_car(race_view *v, const car_meshes *meshes, const car_model *model,
                            const r3d_view *car, int posed, const float wheels[4][3],
                            const float snapshot[4][8], const gt4_pit_crew *pit)
{
    if (!model)
        return;
    r3d_view body = pit_body_place(model, car, pit, 1);
    r3d_lamp_shadow_mesh(v->lamp_shadow, posed && meshes->body ? meshes->body : meshes->car, &body);
    if (posed && meshes->body)
        for (int i = 0; i < 4; i++) {
            r3d_view wheel;
            wheel_place(model, wheels[i], snapshot ? snapshot[i] : NULL, i, car, &wheel);
            pit_mounted_place(pit, (unsigned)(i ^ 1), &wheel);
            r3d_lamp_shadow_mesh(v->lamp_shadow, meshes->wheel[i], &wheel);
        }
}
static void prepare_lamp_shadow(race_view *v, const r3d_view *car)
{
    if (!r3d_lamp_shadow_begin(&v->lamp_shadow))
        return;
    for (unsigned light = 0; light < R3D_CAR_LAMPS; light++) {
        if (!r3d_lamp_shadow_light(v->lamp_shadow, light))
            continue;
        r3d_view terrain = {0};
        terrain.rot[0] = terrain.rot[4] = terrain.rot[8] = 1;
        r3d_lamp_shadow_mesh(v->lamp_shadow, v->world, &terrain);
        r3d_lamp_shadow_mesh(v->lamp_shadow, v->scenery, &terrain);
        r3d_view player = *car;
        player.brake = v->now->pilot[0].brake;
        lamp_shadow_car(v, &v->mine, v->now->car, &player, v->pose_on && v->wheels_on,
                        v->wheel_pose, v->wheel_snapshot_on ? v->wheel_snapshot : NULL,
                        v->pit_crew[0]);
        for (int i = 0; i < OPPONENTS; i++) {
            const car_pose *p = &v->other_pose[i];
            if (!p->on || p->ghost > 0 || !v->now->other[i])
                continue;
            r3d_view other = *car;
            other.brake = v->now->pilot[i + 1].brake;
            memcpy(other.pos, p->pos, sizeof other.pos);
            memcpy(other.rot, p->rot, sizeof other.rot);
            lamp_shadow_car(v, &v->other[i], v->now->other[i], &other, p->wheels_on, p->wheel_pose,
                            p->snapshot_on ? p->snapshot : NULL, v->pit_crew[i + 1]);
        }
        if (v->gadgets && v->now->gadgets) {
            gt4_course_gadgets *g = v->now->gadgets;
            for (unsigned i = 0; i < g->count; i++) {
                gt4_course_gadget *p = &g->instance[i];
                if (!gt4_course_gadget_visible(p))
                    continue;
                r3d_view object = terrain;
                memcpy(object.pos, p->pos, 12);
                memcpy(object.rot, p->rot, 36);
                r3d_lamp_shadow_mesh(v->lamp_shadow, v->gadgets[p->type], &object);
            }
        }
        r3d_lamp_shadow_end_light(v->lamp_shadow);
    }
    r3d_lamp_shadow_bind(v->lamp_shadow);
}
