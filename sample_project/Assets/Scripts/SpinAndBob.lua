local SpinAndBob = {
    rotation_speed_degrees = 90.0,
    bob_amplitude = 0.35,
    bob_frequency = 1.5,

    _elapsed_time = 0.0,
    _origin_y = 0.0,
    _base_rotation_y = 0.0,
}

function SpinAndBob.on_create(entity)
    local translation_x, y, translation_z = entity:get_translation()
    local rotation_x, rotation_y, rotation_z = entity:get_rotation()

    SpinAndBob._elapsed_time = 0.0
    SpinAndBob._origin_y = y
    SpinAndBob._base_rotation_y = rotation_y
end

function SpinAndBob.on_update(entity, dt)
    SpinAndBob._elapsed_time = SpinAndBob._elapsed_time + dt

    local x, current_y, z = entity:get_translation()
    local bob_y = SpinAndBob._origin_y +
        math.sin(SpinAndBob._elapsed_time * SpinAndBob.bob_frequency) * SpinAndBob.bob_amplitude
    entity:set_translation(x, bob_y, z)

    local rotation_x, current_rotation_y, rotation_z = entity:get_rotation()
    local rotation_y = SpinAndBob._base_rotation_y +
        math.rad(SpinAndBob.rotation_speed_degrees) * SpinAndBob._elapsed_time
    entity:set_rotation(rotation_x, rotation_y, rotation_z)
end

function SpinAndBob.on_destroy(entity)
end

return SpinAndBob
