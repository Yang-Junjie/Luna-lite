#include "soft_rasterization_renderer.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace lunalite::renderer {
namespace {

float edgeFunction(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c)
{
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

uint8_t toByte(float value)
{
    const auto clamped = std::clamp(value, 0.0f, 1.0f);
    return static_cast<uint8_t>(std::round(clamped * 255.0f));
}

} // namespace

SoftRasterizationRenderer::SoftRasterizationRenderer(uint32_t width, uint32_t height)
    : m_width(width),
      m_height(height),
      m_color_buffer(static_cast<size_t>(width) * height),
      m_present_buffer(static_cast<size_t>(width) * height * 4),
      m_depth_buffer(static_cast<size_t>(width) * height)
{
    updateFrameImage();
}

void SoftRasterizationRenderer::beginFrame()
{
    std::fill(m_color_buffer.begin(), m_color_buffer.end(), glm::vec3{0.08f, 0.09f, 0.11f});
    std::fill(m_depth_buffer.begin(), m_depth_buffer.end(), std::numeric_limits<float>::infinity());
}

void SoftRasterizationRenderer::endFrame()
{
    updateFrameImage();
}

void SoftRasterizationRenderer::setViewProjection(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos)
{
    m_view = view;
    m_projection = proj;
    m_camera_pos = cameraPos;
}

void SoftRasterizationRenderer::setDirectionalLight(const glm::vec3& direction,
                                                    const glm::vec3& ambient,
                                                    const glm::vec3& diffuse,
                                                    const glm::vec3& specular)
{
    m_light_direction = direction;
    m_light_ambient = ambient;
    m_light_diffuse = diffuse;
    m_light_specular = specular;
}

void SoftRasterizationRenderer::renderMesh(const interface::Mesh& mesh, const glm::mat4& transform)
{
    const auto& vertices = mesh.getVertices();
    const auto& indices = mesh.getIndices();
    if (vertices.empty()) {
        return;
    }

    const auto normal_matrix = glm::transpose(glm::inverse(transform));
    const auto drawTriangle = [&](uint32_t i0, uint32_t i1, uint32_t i2) {
        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
            return;
        }

        ScreenVertex v0;
        ScreenVertex v1;
        ScreenVertex v2;
        if (!projectVertex(vertices[i0], transform, normal_matrix, v0) ||
            !projectVertex(vertices[i1], transform, normal_matrix, v1) ||
            !projectVertex(vertices[i2], transform, normal_matrix, v2)) {
            return;
        }

        rasterizeTriangle(v0, v1, v2);
    };

    if (!indices.empty()) {
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            drawTriangle(indices[i], indices[i + 1], indices[i + 2]);
        }
        return;
    }

    for (uint32_t i = 0; static_cast<size_t>(i) + 2 < vertices.size(); i += 3) {
        drawTriangle(i, i + 1, i + 2);
    }
}

const interface::FrameImage& SoftRasterizationRenderer::getFrameImage() const
{
    return m_frame_image;
}

void SoftRasterizationRenderer::rasterizeTriangle(const ScreenVertex& v0, const ScreenVertex& v1, const ScreenVertex& v2)
{
    const glm::vec2 p0{v0.position.x, v0.position.y};
    const glm::vec2 p1{v1.position.x, v1.position.y};
    const glm::vec2 p2{v2.position.x, v2.position.y};
    const auto area = edgeFunction(p0, p1, p2);
    if (std::abs(area) <= std::numeric_limits<float>::epsilon()) {
        return;
    }

    const auto min_x = static_cast<int32_t>(std::max(0.0f, std::floor(std::min({p0.x, p1.x, p2.x}))));
    const auto max_x = static_cast<int32_t>(std::min(static_cast<float>(m_width - 1), std::ceil(std::max({p0.x, p1.x, p2.x}))));
    const auto min_y = static_cast<int32_t>(std::max(0.0f, std::floor(std::min({p0.y, p1.y, p2.y}))));
    const auto max_y = static_cast<int32_t>(std::min(static_cast<float>(m_height - 1), std::ceil(std::max({p0.y, p1.y, p2.y}))));

    const auto light = glm::normalize(-m_light_direction);
    for (int32_t y = min_y; y <= max_y; ++y) {
        for (int32_t x = min_x; x <= max_x; ++x) {
            const glm::vec2 p{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
            const auto w0 = edgeFunction(p1, p2, p) / area;
            const auto w1 = edgeFunction(p2, p0, p) / area;
            const auto w2 = edgeFunction(p0, p1, p) / area;
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                continue;
            }

            const auto depth = w0 * v0.position.z + w1 * v1.position.z + w2 * v2.position.z;
            const auto index = pixelIndex(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
            if (depth >= m_depth_buffer[index]) {
                continue;
            }

            const auto normal = glm::normalize(w0 * v0.normal + w1 * v1.normal + w2 * v2.normal);
            const auto base_color = w0 * v0.color + w1 * v1.color + w2 * v2.color;
            const auto diffuse = std::max(glm::dot(normal, light), 0.0f);
            m_color_buffer[index] = base_color * (m_light_ambient + m_light_diffuse * diffuse);
            m_depth_buffer[index] = depth;
        }
    }
}

bool SoftRasterizationRenderer::projectVertex(const interface::Vertex& vertex,
                                              const glm::mat4& model,
                                              const glm::mat4& normal_matrix,
                                              ScreenVertex& out) const
{
    const auto world_position = model * glm::vec4(vertex.position, 1.0f);
    const auto clip_position = m_projection * m_view * world_position;
    if (std::abs(clip_position.w) <= std::numeric_limits<float>::epsilon()) {
        return false;
    }

    const auto ndc = glm::vec3(clip_position) / clip_position.w;
    if (ndc.z < -1.0f || ndc.z > 1.0f) {
        return false;
    }

    out.position = {
        (ndc.x * 0.5f + 0.5f) * static_cast<float>(m_width - 1),
        (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(m_height - 1),
        ndc.z * 0.5f + 0.5f,
    };
    out.world_position = glm::vec3(world_position);
    out.normal = glm::normalize(glm::mat3(normal_matrix) * vertex.normal);
    out.color = vertex.color;
    return true;
}

void SoftRasterizationRenderer::updateFrameImage()
{
    for (uint32_t y = 0; y < m_height; ++y) {
        const auto source_y = m_height - 1 - y;
        for (uint32_t x = 0; x < m_width; ++x) {
            const auto& color = m_color_buffer[pixelIndex(x, source_y)];
            const auto offset = (static_cast<size_t>(y) * m_width + x) * 4;
            m_present_buffer[offset + 0] = toByte(color.r);
            m_present_buffer[offset + 1] = toByte(color.g);
            m_present_buffer[offset + 2] = toByte(color.b);
            m_present_buffer[offset + 3] = 255;
        }
    }

    m_frame_image = interface::FrameImage{
        .width = m_width,
        .height = m_height,
        .format = interface::FrameImageFormat::RGBA8_UNorm,
        .color_space = interface::FrameImageColorSpace::Linear,
        .storage =
            interface::CpuFrameStorage{
                .pixels = m_present_buffer.data(),
                .row_pitch = static_cast<size_t>(m_width) * 4,
            },
    };
}

uint32_t SoftRasterizationRenderer::pixelIndex(uint32_t x, uint32_t y) const
{
    return y * m_width + x;
}

} // namespace lunalite::renderer
