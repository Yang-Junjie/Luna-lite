local SpinAndBob = {
    rotation_speed_degrees = 90.0,
    bob_amplitude = 0.35,
    bob_frequency = 1.5,

    _origin_y = 0.0,
    _base_rotation_x = 0.0,
    _base_rotation_y = 0.0,
    _base_rotation_z = 0.0,
}

function SpinAndBob.on_create(entity)
    local translation_x, y, translation_z = entity:get_translation()
    local rotation_x, rotation_y, rotation_z = entity:get_rotation()

    SpinAndBob._origin_y = y
    SpinAndBob._base_rotation_x = rotation_x
    SpinAndBob._base_rotation_y = rotation_y
    SpinAndBob._base_rotation_z = rotation_z

    local name = entity:get_tag()
    if name == "" then
        name = "entity " .. entity:get_handle()
    end
    Log.info("SpinAndBob created for " .. name)
end

function SpinAndBob.on_update(entity, dt)
    local x, current_y, z = entity:get_translation()
    local bob_y = SpinAndBob._origin_y +
        math.sin(Time.elapsed_time * SpinAndBob.bob_frequency) * SpinAndBob.bob_amplitude
    entity:set_translation(x, bob_y, z)

    local rotation_offset = (math.rad(SpinAndBob.rotation_speed_degrees) * Time.elapsed_time) % (math.pi * 2.0)
    local rotation_y = SpinAndBob._base_rotation_y + rotation_offset
    entity:set_rotation_euler(SpinAndBob._base_rotation_x, rotation_y, SpinAndBob._base_rotation_z)
end

function SpinAndBob.on_destroy(entity)
    local name = entity:get_tag()
    if name == "" then
        name = "entity " .. entity:get_handle()
    end
    Log.info("SpinAndBob destroyed for " .. name)
end

return SpinAndBob
