local FirstPersonCamera = {
    move_speed = 5.0,
    fast_multiplier = 3.0,
    mouse_sensitivity = 0.0025,
    pitch_limit = math.rad(89.0),

    pitch = 0.0,
    yaw = 0.0,
}

local function clamp(value, min_value, max_value)
    if value < min_value then
        return min_value
    end

    if value > max_value then
        return max_value
    end

    return value
end

local function update_rotation(entity)
    if not Input.is_mouse_button_pressed("Right") then
        return
    end

    local mouse_dx, mouse_dy = Input.get_mouse_delta()
    FirstPersonCamera.yaw = FirstPersonCamera.yaw - mouse_dx * FirstPersonCamera.mouse_sensitivity
    FirstPersonCamera.pitch = clamp(
        FirstPersonCamera.pitch - mouse_dy * FirstPersonCamera.mouse_sensitivity,
        -FirstPersonCamera.pitch_limit,
        FirstPersonCamera.pitch_limit
    )

    entity:set_yaw_pitch(FirstPersonCamera.yaw, FirstPersonCamera.pitch)
end

local function update_translation(entity, dt)
    local yaw = FirstPersonCamera.yaw
    local forward_x = -math.sin(yaw)
    local forward_z = -math.cos(yaw)
    local right_x = math.cos(yaw)
    local right_z = -math.sin(yaw)

    local move_world_x = 0.0
    local move_world_y = 0.0
    local move_world_z = 0.0

    if Input.is_key_pressed("W") then
        move_world_x = move_world_x + forward_x
        move_world_z = move_world_z + forward_z
    end
    if Input.is_key_pressed("S") then
        move_world_x = move_world_x - forward_x
        move_world_z = move_world_z - forward_z
    end
    if Input.is_key_pressed("D") then
        move_world_x = move_world_x + right_x
        move_world_z = move_world_z + right_z
    end
    if Input.is_key_pressed("A") then
        move_world_x = move_world_x - right_x
        move_world_z = move_world_z - right_z
    end
    if Input.is_key_pressed("Space") or Input.is_key_pressed("E") then
        move_world_y = move_world_y + 1.0
    end
    if Input.is_key_pressed("Q") then
        move_world_y = move_world_y - 1.0
    end

    local length =
        math.sqrt(move_world_x * move_world_x + move_world_y * move_world_y + move_world_z * move_world_z)
    if length <= 0.0 then
        return
    end

    local speed = FirstPersonCamera.move_speed
    if Input.is_key_pressed("LeftShift") then
        speed = speed * FirstPersonCamera.fast_multiplier
    end

    local x, y, z = entity:get_translation()
    local distance = speed * dt / length
    entity:set_translation(x + move_world_x * distance, y + move_world_y * distance, z + move_world_z * distance)
end

function FirstPersonCamera.on_create(entity)
    if not entity:has_camera() then
        entity:add_camera()
    end

    FirstPersonCamera.pitch, FirstPersonCamera.yaw = entity:get_rotation()
    entity:set_yaw_pitch(FirstPersonCamera.yaw, FirstPersonCamera.pitch)

    if entity:get_tag() == "" then
        entity:set_tag("First Person Camera")
    end

    Log.info("FirstPersonCamera created")
end

function FirstPersonCamera.on_update(entity, dt)
    update_rotation(entity)
    entity:set_yaw_pitch(FirstPersonCamera.yaw, FirstPersonCamera.pitch)
    update_translation(entity, dt)
end

function FirstPersonCamera.on_destroy(entity)
    Log.info("FirstPersonCamera destroyed")
end

return FirstPersonCamera
