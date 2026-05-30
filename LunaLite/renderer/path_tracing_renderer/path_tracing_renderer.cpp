#include "path_tracing_renderer.h"

#include <cmath>

#include <algorithm>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <limits>
#include <random>

namespace lunalite::renderer {
namespace {

constexpr float kEpsilon = 1e-4f;
constexpr float kPi = 3.14159265358979323846f;

glm::vec3 linearToSrgb(const glm::vec3& color)
{
    return glm::pow(glm::max(color, glm::vec3{0.0f}), glm::vec3{1.0f / 2.2f});
}

} // namespace

PathTracingRenderer::PathTracingRenderer(uint32_t width, uint32_t height)
    : m_width(std::max(1u, width)),
      m_height(std::max(1u, height)),
      m_framebuffer(static_cast<size_t>(m_width) * m_height),
      m_present_buffer(static_cast<size_t>(m_width) * m_height)
{
    updateFrameImage();
}

void PathTracingRenderer::beginFrame()
{
    clearFrame();
    m_triangles.clear();
}

void PathTracingRenderer::endFrame()
{
    renderFrame();
    updateFrameImage();
}

void PathTracingRenderer::resize(uint32_t width, uint32_t height)
{
    m_width = std::max(1u, width);
    m_height = std::max(1u, height);
    m_framebuffer.resize(static_cast<size_t>(m_width) * m_height);
    m_present_buffer.resize(static_cast<size_t>(m_width) * m_height);
    updateFrameImage();
}

void PathTracingRenderer::setViewProjection(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos)
{
    m_view = view;
    m_projection = proj;
    m_inverse_view = glm::inverse(view);
    m_inverse_projection = glm::inverse(proj);
    m_camera_pos = cameraPos;
}

void PathTracingRenderer::setSceneLighting(const interface::SceneLighting& lighting)
{
    m_lighting = lighting;
    if (m_lighting.directional_light_count > 1) {
        m_lighting.directional_light_count = 1;
    }
}

void PathTracingRenderer::renderMesh(const interface::Mesh& mesh, const glm::mat4& transform)
{
    const auto& vertices = mesh.getVertices();
    const auto& indices = mesh.getIndices();
    if (vertices.empty()) {
        return;
    }

    const auto normal_matrix = glm::transpose(glm::inverse(transform));
    const auto pushTriangle = [&](uint32_t i0, uint32_t i1, uint32_t i2) {
        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
            return;
        }

        const auto& v0 = vertices[i0];
        const auto& v1 = vertices[i1];
        const auto& v2 = vertices[i2];

        Triangle tri;
        tri.p0 = glm::vec3(transform * glm::vec4(v0.position, 1.0f));
        tri.p1 = glm::vec3(transform * glm::vec4(v1.position, 1.0f));
        tri.p2 = glm::vec3(transform * glm::vec4(v2.position, 1.0f));
        tri.n0 = glm::normalize(glm::mat3(normal_matrix) * v0.normal);
        tri.n1 = glm::normalize(glm::mat3(normal_matrix) * v1.normal);
        tri.n2 = glm::normalize(glm::mat3(normal_matrix) * v2.normal);
        tri.c0 = v0.color;
        tri.c1 = v1.color;
        tri.c2 = v2.color;
        m_triangles.push_back(tri);
    };

    if (!indices.empty()) {
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            pushTriangle(indices[i], indices[i + 1], indices[i + 2]);
        }
        return;
    }

    for (uint32_t i = 0; static_cast<size_t>(i) + 2 < vertices.size(); i += 3) {
        pushTriangle(i, i + 1, i + 2);
    }
}

const interface::FrameImage& PathTracingRenderer::getFrameImage() const
{
    return m_frame_image;
}

void PathTracingRenderer::clearFrame()
{
    std::fill(m_framebuffer.begin(), m_framebuffer.end(), glm::vec3{0.0f});
}

void PathTracingRenderer::renderFrame()
{
    if (m_triangles.empty()) {
        for (uint32_t y = 0; y < m_height; ++y) {
            for (uint32_t x = 0; x < m_width; ++x) {
                m_framebuffer[pixelIndex(x, y)] = background(glm::vec3{0.0f, 0.0f, 1.0f});
            }
        }
        return;
    }

    std::uniform_real_distribution<float> jitter(0.0f, 1.0f);
    for (uint32_t y = 0; y < m_height; ++y) {
        for (uint32_t x = 0; x < m_width; ++x) {
            std::mt19937 rng(0x9E'37'79'B9u ^ (x * 73'856'093u) ^ (y * 19'349'663u));
            glm::vec3 pixel_color{0.0f};

            for (uint32_t sample = 0; sample < m_samples_per_pixel; ++sample) {
                const float u = (static_cast<float>(x) + jitter(rng)) / static_cast<float>(m_width);
                const float v = (static_cast<float>(y) + jitter(rng)) / static_cast<float>(m_height);
                pixel_color += tracePath(generateCameraRay(u, v), rng);
            }

            pixel_color /= static_cast<float>(m_samples_per_pixel);
            m_framebuffer[pixelIndex(x, y)] = pixel_color;
        }
    }
}

void PathTracingRenderer::updateFrameImage()
{
    for (size_t i = 0; i < m_framebuffer.size(); ++i) {
        m_present_buffer[i] = glm::vec4{linearToSrgb(m_framebuffer[i]), 1.0f};
    }

    m_frame_image = interface::FrameImage{
        .width = m_width,
        .height = m_height,
        .format = interface::FrameImageFormat::RGBA32_Float,
        .color_space = interface::FrameImageColorSpace::SRGB,
        .storage =
            interface::CpuFrameStorage{
                .pixels = m_present_buffer.data(),
                .row_pitch = static_cast<size_t>(m_width) * sizeof(glm::vec4),
            },
    };
}

glm::vec3 PathTracingRenderer::tracePath(const Ray& initial_ray, std::mt19937& rng) const
{
    glm::vec3 radiance{0.0f};
    glm::vec3 throughput{1.0f};
    Ray ray = initial_ray;

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (uint32_t bounce = 0; bounce < m_max_bounces; ++bounce) {
        Hit hit;
        if (!intersectScene(ray, hit)) {
            radiance += throughput * background(ray.direction);
            break;
        }

        glm::vec3 direct = hit.albedo * m_lighting.environment_ambient;
        if (m_lighting.directional_light_count > 0) {
            const auto& light = m_lighting.directional_light;
            const auto light_dir = glm::normalize(-light.direction);
            const auto view_dir = glm::normalize(m_camera_pos - hit.position);
            const auto half_vec = glm::normalize(light_dir + view_dir);
            const auto ndotl = std::max(glm::dot(hit.normal, light_dir), 0.0f);
            const auto spec = std::pow(std::max(glm::dot(hit.normal, half_vec), 0.0f), 32.0f);
            direct += hit.albedo * (light.ambient + light.diffuse * ndotl) + light.specular * spec * 0.15f;
        }
        radiance += throughput * direct;

        throughput *= hit.albedo;
        if (bounce + 1 >= m_max_bounces) {
            break;
        }

        const auto max_throughput = std::max({throughput.r, throughput.g, throughput.b});
        if (bounce > 0 && max_throughput < 0.05f) {
            if (dist(rng) > max_throughput) {
                break;
            }
            throughput /= std::max(max_throughput, 1e-3f);
        }

        ray.origin = hit.position + hit.normal * kEpsilon;
        ray.direction = sampleCosineHemisphere(hit.normal, rng);
    }

    return radiance;
}

bool PathTracingRenderer::intersectScene(const Ray& ray, Hit& hit) const
{
    bool has_hit = false;
    float best_t = std::numeric_limits<float>::max();
    float best_u = 0.0f;
    float best_v = 0.0f;
    const Triangle* best_triangle = nullptr;

    for (const auto& triangle : m_triangles) {
        float t = 0.0f;
        float u = 0.0f;
        float v = 0.0f;
        if (!intersectTriangle(ray, triangle, t, u, v)) {
            continue;
        }

        if (t < best_t) {
            best_t = t;
            best_u = u;
            best_v = v;
            best_triangle = &triangle;
            has_hit = true;
        }
    }

    if (!has_hit || best_triangle == nullptr) {
        return false;
    }

    const auto w = 1.0f - best_u - best_v;
    hit.t = best_t;
    hit.position = ray.origin + ray.direction * best_t;
    hit.normal = glm::normalize(w * best_triangle->n0 + best_u * best_triangle->n1 + best_v * best_triangle->n2);
    hit.albedo = w * best_triangle->c0 + best_u * best_triangle->c1 + best_v * best_triangle->c2;
    return true;
}

bool PathTracingRenderer::intersectTriangle(
    const Ray& ray, const Triangle& triangle, float& t, float& u, float& v) const
{
    const auto edge1 = triangle.p1 - triangle.p0;
    const auto edge2 = triangle.p2 - triangle.p0;
    const auto pvec = glm::cross(ray.direction, edge2);
    const auto det = glm::dot(edge1, pvec);
    if (std::abs(det) <= kEpsilon) {
        return false;
    }

    const auto inv_det = 1.0f / det;
    const auto tvec = ray.origin - triangle.p0;
    u = glm::dot(tvec, pvec) * inv_det;
    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    const auto qvec = glm::cross(tvec, edge1);
    v = glm::dot(ray.direction, qvec) * inv_det;
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    t = glm::dot(edge2, qvec) * inv_det;
    return t > kEpsilon;
}

PathTracingRenderer::Ray PathTracingRenderer::generateCameraRay(float u, float v) const
{
    const glm::vec4 ndc{
        u * 2.0f - 1.0f,
        1.0f - v * 2.0f,
        -1.0f,
        1.0f,
    };

    auto view_space = m_inverse_projection * ndc;
    view_space /= view_space.w;
    const auto dir_view = glm::normalize(glm::vec3(view_space));
    const auto dir_world = glm::normalize(glm::vec3(m_inverse_view * glm::vec4(dir_view, 0.0f)));

    return Ray{
        .origin = m_camera_pos,
        .direction = dir_world,
    };
}

glm::vec3 PathTracingRenderer::sampleCosineHemisphere(const glm::vec3& normal, std::mt19937& rng) const
{
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    const float r1 = dist(rng);
    const float r2 = dist(rng);
    const float phi = 2.0f * kPi * r1;
    const float radius = std::sqrt(r2);
    const float x = std::cos(phi) * radius;
    const float y = std::sin(phi) * radius;
    const float z = std::sqrt(std::max(0.0f, 1.0f - r2));

    glm::vec3 tangent{1.0f, 0.0f, 0.0f};
    if (std::abs(normal.x) > 0.9f) {
        tangent = glm::vec3{0.0f, 1.0f, 0.0f};
    }
    tangent = glm::normalize(glm::cross(tangent, normal));
    const auto bitangent = glm::cross(normal, tangent);

    return glm::normalize(x * tangent + y * bitangent + z * normal);
}

glm::vec3 PathTracingRenderer::background(const glm::vec3& direction) const
{
    (void) direction;
    return m_lighting.environment_ambient;
}

size_t PathTracingRenderer::pixelIndex(uint32_t x, uint32_t y) const
{
    return static_cast<size_t>(y) * m_width + x;
}

} // namespace lunalite::renderer
