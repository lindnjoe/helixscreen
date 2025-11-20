// Copyright (c) 2025 HelixScreen Project
// SPDX-License-Identifier: GPL-3.0-or-later
//
// TinyGL Test Framework - Implementation

#include "tinygl_test_framework.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>

namespace tinygl_test {

// ============================================================================
// TinyGLTestFramework Implementation
// ============================================================================

TinyGLTestFramework::TinyGLTestFramework(int width, int height)
    : width_(width), height_(height), zb_(nullptr) {
    framebuffer_.resize(width * height);
}

TinyGLTestFramework::~TinyGLTestFramework() {
    if (zb_) {
        glClose();
        ZB_close(zb_);
    }
}

bool TinyGLTestFramework::initialize() {
    // Initialize TinyGL with 32-bit RGBA mode
    zb_ = ZB_open(width_, height_, ZB_MODE_RGBA, 0);
    if (!zb_) {
        spdlog::error("Failed to create ZBuffer");
        return false;
    }

    // Update actual dimensions (ZB_open may adjust for alignment)
    width_ = zb_->xsize;
    height_ = zb_->ysize;
    framebuffer_.resize(width_ * height_);

    // Initialize OpenGL context
    glInit(zb_);

    // Set viewport
    glViewport(0, 0, width_, height_);

    // Setup default projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = static_cast<float>(width_) / height_;
    float fovy = 45.0f;
    float near_plane = 0.1f;
    float far_plane = 1000.0f;

    float top = near_plane * tanf(fovy * M_PI / 360.0f);
    float bottom = -top;
    float right = top * aspect;
    float left = -right;
    glFrustum(left, right, bottom, top, near_plane, far_plane);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    return true;
}

void TinyGLTestFramework::setup_standard_lighting(const SceneConfig& config) {
    if (config.enable_lighting) {
        glEnable(GL_LIGHTING);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

        // Set ambient light
        float ambient[4] = {config.ambient_intensity, config.ambient_intensity,
                            config.ambient_intensity, 1.0f};
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

        // Setup directional lights (matching OrcaSlicer)
        if (config.num_lights >= 1) {
            glEnable(GL_LIGHT0);
            float light0_dir[4] = {-0.457f, 0.457f, 0.762f, 0.0f}; // Top-right
            float light0_color[4] = {0.6f, 0.6f, 0.6f, 1.0f};
            glLightfv(GL_LIGHT0, GL_POSITION, light0_dir);
            glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_color);
            glLightfv(GL_LIGHT0, GL_SPECULAR, light0_color);
        }

        if (config.num_lights >= 2) {
            glEnable(GL_LIGHT1);
            float light1_dir[4] = {0.699f, 0.140f, 0.699f, 0.0f}; // Front-right
            float light1_color[4] = {0.6f, 0.6f, 0.6f, 1.0f};
            glLightfv(GL_LIGHT1, GL_POSITION, light1_dir);
            glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_color);
            glLightfv(GL_LIGHT1, GL_SPECULAR, light1_color);
        }

        // Setup material properties
        float mat_specular[4] = {config.specular_intensity, config.specular_intensity,
                                 config.specular_intensity, 1.0f};
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, config.specular_shininess);
    }

    // Setup shading model
    glShadeModel(config.enable_smooth_shading ? GL_SMOOTH : GL_FLAT);

    // Enable depth testing if requested
    if (config.enable_depth) {
        glEnable(GL_DEPTH_TEST);
        // Note: TinyGL only supports GL_LESS depth function by default
    }
}

void TinyGLTestFramework::clear_buffers() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void TinyGLTestFramework::set_phong_shading(bool enable) {
    glShadeModel(enable ? GL_PHONG : GL_SMOOTH);
}

void TinyGLTestFramework::render_scene(TestScene* scene, const SceneConfig& config) {
    setup_standard_lighting(config);
    clear_buffers();

    scene->setup(config);
    scene->render();

    // No need to flush in software renderer
}

std::vector<uint8_t> TinyGLTestFramework::capture_framebuffer_rgb() {
    // Copy framebuffer from TinyGL
    ZB_copyFrameBuffer(zb_, framebuffer_.data(), width_ * sizeof(unsigned int));

    // Convert to RGB24
    std::vector<uint8_t> rgb(width_ * height_ * 3);
    for (int i = 0; i < width_ * height_; i++) {
        unsigned int pixel = framebuffer_[i];
        // TinyGL uses ABGR format internally
        rgb[i * 3 + 0] = pixel & 0xFF;         // R (from B channel)
        rgb[i * 3 + 1] = (pixel >> 8) & 0xFF;  // G
        rgb[i * 3 + 2] = (pixel >> 16) & 0xFF; // B (from R channel)
    }

    return rgb;
}

bool TinyGLTestFramework::save_screenshot(const std::string& filename) {
    auto rgb = capture_framebuffer_rgb();

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open file for writing: {}", filename);
        return false;
    }

    // Write PPM header
    file << "P6\n" << width_ << " " << height_ << "\n255\n";
    file.write(reinterpret_cast<const char*>(rgb.data()), rgb.size());

    spdlog::info("Screenshot saved: {} ({}x{})", filename, width_, height_);
    return true;
}

ImageMetrics TinyGLTestFramework::compare_images(const std::vector<uint8_t>& img1,
                                                 const std::vector<uint8_t>& img2, int width,
                                                 int height) {
    ImageMetrics metrics = {};

    if (img1.size() != img2.size() || img1.size() != static_cast<size_t>(width * height * 3)) {
        spdlog::error("Image size mismatch for comparison");
        return metrics;
    }

    double sum_sq_diff = 0.0;
    int diff_count = 0;

    for (size_t i = 0; i < img1.size(); i++) {
        int diff = static_cast<int>(img1[i]) - static_cast<int>(img2[i]);
        double sq_diff = diff * diff;
        sum_sq_diff += sq_diff;

        if (diff != 0) {
            diff_count++;
        }

        metrics.max_diff = std::max(metrics.max_diff, static_cast<double>(std::abs(diff)));
    }

    // Calculate MSE and PSNR
    metrics.mse = sum_sq_diff / img1.size();
    if (metrics.mse > 0) {
        metrics.psnr = 10.0 * log10(255.0 * 255.0 / metrics.mse);
    } else {
        metrics.psnr = 100.0; // Perfect match
    }

    metrics.diff_pixels = diff_count / 3; // Convert from color channels to pixels

    // Calculate SSIM (simplified version)
    metrics.ssim = utils::calculate_ssim(img1, img2, width, height);

    return metrics;
}

PerfMetrics TinyGLTestFramework::benchmark_scene(TestScene* scene, const SceneConfig& config,
                                                 int num_frames) {
    PerfMetrics metrics = {};

    // Setup scene once
    setup_standard_lighting(config);
    scene->setup(config);

    // Warm-up render
    clear_buffers();
    scene->render();

    // Benchmark multiple frames
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_frames; i++) {
        clear_buffers();
        scene->render();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Calculate metrics
    metrics.frame_time_ms = (duration / 1000.0) / num_frames;

    size_t vertex_count = scene->get_vertex_count();
    size_t triangle_count = scene->get_triangle_count();

    double total_time_sec = duration / 1000000.0;
    metrics.vertices_per_second = (vertex_count * num_frames) / total_time_sec;
    metrics.triangles_per_second = (triangle_count * num_frames) / total_time_sec;

    // Approximate pixels rendered (assuming 50% coverage)
    size_t pixels_per_frame = (width_ * height_) / 2;
    metrics.pixels_per_second = (pixels_per_frame * num_frames) / total_time_sec;

    // Memory usage estimation
    metrics.memory_usage_bytes = sizeof(ZBuffer) + (width_ * height_ * 6); // RGBA + Z

    return metrics;
}

std::vector<uint8_t> TinyGLTestFramework::load_ppm(const std::string& filename, int& width,
                                                   int& height) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open PPM file: {}", filename);
        return {};
    }

    // Read header
    std::string magic;
    int max_val;
    file >> magic >> width >> height >> max_val;

    if (magic != "P6" || max_val != 255) {
        spdlog::error("Invalid PPM format in: {}", filename);
        return {};
    }

    // Skip whitespace after header
    file.get();

    // Read pixel data
    std::vector<uint8_t> data(width * height * 3);
    file.read(reinterpret_cast<char*>(data.data()), data.size());

    return data;
}

double TinyGLTestFramework::measure_frame_time(std::function<void()> render_func) {
    auto start = std::chrono::high_resolution_clock::now();
    render_func();
    auto end = std::chrono::high_resolution_clock::now();

    return std::chrono::duration<double, std::milli>(end - start).count();
}

// ============================================================================
// SphereTesselationScene Implementation
// ============================================================================

SphereTesselationScene::SphereTesselationScene(int subdivisions) : subdivisions_(subdivisions) {}

void SphereTesselationScene::setup(const SceneConfig& /*config*/) {
    generate_sphere();
}

void SphereTesselationScene::render() {
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -5.0f);
    glRotatef(30.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(45.0f, 0.0f, 1.0f, 0.0f);

    // Render triangles with vertex colors
    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < vertices_.size(); i += 3) {
        glNormal3f(normals_[i], normals_[i + 1], normals_[i + 2]);
        glColor3f(colors_[i], colors_[i + 1], colors_[i + 2]);
        glVertex3f(vertices_[i], vertices_[i + 1], vertices_[i + 2]);
    }
    glEnd();

    glPopMatrix();
}

void SphereTesselationScene::generate_sphere() {
    vertices_.clear();
    normals_.clear();
    colors_.clear();

    // Start with icosahedron vertices
    const float t = (1.0f + sqrtf(5.0f)) / 2.0f;
    const float s = 1.0f / sqrtf(1.0f + t * t);

    float base_verts[][3] = {{-s, t * s, 0}, {s, t * s, 0}, {-s, -t * s, 0}, {s, -t * s, 0},
                             {0, -s, t * s}, {0, s, t * s}, {0, -s, -t * s}, {0, s, -t * s},
                             {t * s, 0, -s}, {t * s, 0, s}, {-t * s, 0, -s}, {-t * s, 0, s}};

    // Define icosahedron faces
    int faces[][3] = {{0, 11, 5}, {0, 5, 1},  {0, 1, 7},   {0, 7, 10}, {0, 10, 11},
                      {1, 5, 9},  {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
                      {3, 9, 4},  {3, 4, 2},  {3, 2, 6},   {3, 6, 8},  {3, 8, 9},
                      {4, 9, 5},  {2, 4, 11}, {6, 2, 10},  {8, 6, 7},  {9, 8, 1}};

    // Subdivide faces
    for (auto& face : faces) {
        subdivide_triangle(base_verts[face[0]], base_verts[face[1]], base_verts[face[2]],
                           subdivisions_);
    }
}

void SphereTesselationScene::subdivide_triangle(float* v1, float* v2, float* v3, int depth) {
    if (depth == 0) {
        // Normalize vertices to sphere surface
        float n1[3] = {v1[0], v1[1], v1[2]};
        float n2[3] = {v2[0], v2[1], v2[2]};
        float n3[3] = {v3[0], v3[1], v3[2]};

        float len1 = sqrtf(n1[0] * n1[0] + n1[1] * n1[1] + n1[2] * n1[2]);
        float len2 = sqrtf(n2[0] * n2[0] + n2[1] * n2[1] + n2[2] * n2[2]);
        float len3 = sqrtf(n3[0] * n3[0] + n3[1] * n3[1] + n3[2] * n3[2]);

        n1[0] /= len1;
        n1[1] /= len1;
        n1[2] /= len1;
        n2[0] /= len2;
        n2[1] /= len2;
        n2[2] /= len2;
        n3[0] /= len3;
        n3[1] /= len3;
        n3[2] /= len3;

        // Add vertices
        for (int i = 0; i < 3; i++) {
            vertices_.push_back(n1[i]);
            normals_.push_back(n1[i]);
            colors_.push_back(0.5f + 0.5f * n1[i]); // Color based on position
        }

        for (int i = 0; i < 3; i++) {
            vertices_.push_back(n2[i]);
            normals_.push_back(n2[i]);
            colors_.push_back(0.5f + 0.5f * n2[i]);
        }

        for (int i = 0; i < 3; i++) {
            vertices_.push_back(n3[i]);
            normals_.push_back(n3[i]);
            colors_.push_back(0.5f + 0.5f * n3[i]);
        }
    } else {
        // Calculate midpoints
        float v12[3] = {(v1[0] + v2[0]) / 2, (v1[1] + v2[1]) / 2, (v1[2] + v2[2]) / 2};
        float v23[3] = {(v2[0] + v3[0]) / 2, (v2[1] + v3[1]) / 2, (v2[2] + v3[2]) / 2};
        float v31[3] = {(v3[0] + v1[0]) / 2, (v3[1] + v1[1]) / 2, (v3[2] + v1[2]) / 2};

        // Recursively subdivide
        subdivide_triangle(v1, v12, v31, depth - 1);
        subdivide_triangle(v2, v23, v12, depth - 1);
        subdivide_triangle(v3, v31, v23, depth - 1);
        subdivide_triangle(v12, v23, v31, depth - 1);
    }
}

// ============================================================================
// CubeGridScene Implementation
// ============================================================================

CubeGridScene::CubeGridScene(int grid_size) : grid_size_(grid_size), rotation_(0.0f) {}

void CubeGridScene::setup(const SceneConfig& /*config*/) {
    // Nothing to pre-setup, we generate cubes on the fly
}

void CubeGridScene::render() {
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -20.0f);
    glRotatef(rotation_, 1.0f, 0.5f, 0.25f);

    float spacing = 2.5f;
    float offset = -(grid_size_ - 1) * spacing / 2.0f;

    for (int x = 0; x < grid_size_; x++) {
        for (int y = 0; y < grid_size_; y++) {
            for (int z = 0; z < grid_size_; z++) {
                float px = offset + x * spacing;
                float py = offset + y * spacing;
                float pz = offset + z * spacing;

                // Color based on position
                glColor3f((x + 1.0f) / grid_size_, (y + 1.0f) / grid_size_,
                          (z + 1.0f) / grid_size_);

                render_cube(px, py, pz, 0.8f);
            }
        }
    }

    glPopMatrix();
    rotation_ += 0.5f; // For animated tests
}

void CubeGridScene::render_cube(float x, float y, float z, float size) {
    float h = size / 2.0f;

    glPushMatrix();
    glTranslatef(x, y, z);

    glBegin(GL_QUADS);

    // Front face
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-h, -h, h);
    glVertex3f(h, -h, h);
    glVertex3f(h, h, h);
    glVertex3f(-h, h, h);

    // Back face
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(-h, -h, -h);
    glVertex3f(-h, h, -h);
    glVertex3f(h, h, -h);
    glVertex3f(h, -h, -h);

    // Top face
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-h, h, -h);
    glVertex3f(-h, h, h);
    glVertex3f(h, h, h);
    glVertex3f(h, h, -h);

    // Bottom face
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-h, -h, -h);
    glVertex3f(h, -h, -h);
    glVertex3f(h, -h, h);
    glVertex3f(-h, -h, h);

    // Right face
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(h, -h, -h);
    glVertex3f(h, h, -h);
    glVertex3f(h, h, h);
    glVertex3f(h, -h, h);

    // Left face
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-h, -h, -h);
    glVertex3f(-h, -h, h);
    glVertex3f(-h, h, h);
    glVertex3f(-h, h, -h);

    glEnd();
    glPopMatrix();
}

// ============================================================================
// GouraudArtifactScene Implementation
// ============================================================================

void GouraudArtifactScene::setup(const SceneConfig& /*config*/) {
    // Scene setup is handled in render
}

void GouraudArtifactScene::render() {
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -8.0f);

    // Left: Low-tessellation cylinder (shows Gouraud artifacts)
    glPushMatrix();
    glTranslatef(-2.0f, 0.0f, 0.0f);
    glRotatef(-20.0f, 1.0f, 0.0f, 0.0f);
    glColor3f(0.7f, 0.7f, 0.7f);
    render_cylinder(1.0f, 3.0f, 8); // Only 8 segments - very visible artifacts
    glPopMatrix();

    // Right: High-tessellation cylinder (smoother)
    glPushMatrix();
    glTranslatef(2.0f, 0.0f, 0.0f);
    glRotatef(-20.0f, 1.0f, 0.0f, 0.0f);
    glColor3f(0.7f, 0.7f, 0.7f);
    render_cylinder(1.0f, 3.0f, 32); // 32 segments - less visible artifacts
    glPopMatrix();

    glPopMatrix();
}

void GouraudArtifactScene::render_cylinder(float radius, float height, int segments) {
    float angle_step = 2.0f * M_PI / segments;
    float h2 = height / 2.0f;

    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; i++) {
        float angle = i * angle_step;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);

        // Normal points outward from cylinder axis
        glNormal3f(x / radius, 0.0f, z / radius);

        glVertex3f(x, -h2, z);
        glVertex3f(x, h2, z);
    }
    glEnd();

    // End caps
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, h2, 0.0f);
    for (int i = 0; i <= segments; i++) {
        float angle = i * angle_step;
        glVertex3f(radius * cosf(angle), h2, radius * sinf(angle));
    }
    glEnd();

    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(0.0f, -h2, 0.0f);
    for (int i = segments; i >= 0; i--) {
        float angle = i * angle_step;
        glVertex3f(radius * cosf(angle), -h2, radius * sinf(angle));
    }
    glEnd();
}

void GouraudArtifactScene::render_large_triangles() {
    // Large triangles with different normals at vertices
    // Shows Gouraud interpolation artifacts clearly
    glBegin(GL_TRIANGLES);

    glNormal3f(-1.0f, 0.0f, 0.0f);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(-3.0f, -2.0f, 0.0f);

    glNormal3f(0.0f, 1.0f, 0.0f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, 3.0f, 0.0f);

    glNormal3f(1.0f, 0.0f, 0.0f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(3.0f, -2.0f, 0.0f);

    glEnd();
}

// ============================================================================
// ColorBandingScene Implementation
// ============================================================================

void ColorBandingScene::setup(const SceneConfig& /*config*/) {
    // Setup handled in render
}

void ColorBandingScene::render() {
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -5.0f);

    // Disable lighting for gradient test
    glDisable(GL_LIGHTING);
    render_gradient_quad();

    // Re-enable lighting for sphere
    glEnable(GL_LIGHTING);
    glTranslatef(0.0f, 0.0f, -3.0f);
    render_smooth_sphere();

    glPopMatrix();
}

void ColorBandingScene::render_gradient_quad() {
    // Render smooth gradient to show color banding
    glBegin(GL_QUADS);

    // Dark to light gradient
    glColor3f(0.0f, 0.0f, 0.0f);
    glVertex3f(-3.0f, -2.0f, 0.0f);

    glColor3f(0.0f, 0.0f, 0.0f);
    glVertex3f(3.0f, -2.0f, 0.0f);

    glColor3f(1.0f, 1.0f, 1.0f);
    glVertex3f(3.0f, 2.0f, 0.0f);

    glColor3f(1.0f, 1.0f, 1.0f);
    glVertex3f(-3.0f, 2.0f, 0.0f);

    glEnd();
}

void ColorBandingScene::render_smooth_sphere() {
    // Render sphere with smooth shading to show banding in lighting
    const int slices = 20;
    const int stacks = 20;
    const float radius = 1.5f;

    glColor3f(0.6f, 0.6f, 0.6f);

    for (int i = 0; i < stacks; i++) {
        float phi1 = M_PI * i / stacks;
        float phi2 = M_PI * (i + 1) / stacks;

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float theta = 2.0f * M_PI * j / slices;

            float x1 = radius * sinf(phi1) * cosf(theta);
            float y1 = radius * cosf(phi1);
            float z1 = radius * sinf(phi1) * sinf(theta);

            float x2 = radius * sinf(phi2) * cosf(theta);
            float y2 = radius * cosf(phi2);
            float z2 = radius * sinf(phi2) * sinf(theta);

            glNormal3f(x1 / radius, y1 / radius, z1 / radius);
            glVertex3f(x1, y1, z1);

            glNormal3f(x2 / radius, y2 / radius, z2 / radius);
            glVertex3f(x2, y2, z2);
        }
        glEnd();
    }
}

// ============================================================================
// Utility Functions Implementation
// ============================================================================

namespace utils {

std::string generate_test_gcode(const std::string& pattern) {
    std::stringstream ss;

    if (pattern == "cube") {
        // Simple cube in G-code
        ss << "; Test cube 20x20x20mm\n";
        ss << "G28 ; Home\n";
        ss << "G1 Z0.2 F300\n";

        // Generate layers
        for (float z = 0.2f; z <= 20.0f; z += 0.2f) {
            ss << "; Layer " << (int)(z / 0.2f) << "\n";
            ss << "G1 Z" << z << " F300\n";

            // Perimeter
            ss << "G1 X10 Y10 F1200\n";
            ss << "G1 X30 Y10 E1\n";
            ss << "G1 X30 Y30 E1\n";
            ss << "G1 X10 Y30 E1\n";
            ss << "G1 X10 Y10 E1\n";
        }
    } else if (pattern == "cylinder") {
        // Cylinder pattern
        ss << "; Test cylinder r=10mm h=30mm\n";
        const int segments = 36;

        for (float z = 0.2f; z <= 30.0f; z += 0.2f) {
            ss << "G1 Z" << z << " F300\n";

            for (int i = 0; i <= segments; i++) {
                float angle = 2.0f * M_PI * i / segments;
                float x = 20.0f + 10.0f * cosf(angle);
                float y = 20.0f + 10.0f * sinf(angle);
                ss << "G1 X" << x << " Y" << y << " E1\n";
            }
        }
    }

    return ss.str();
}

void setup_orcaslicer_lighting() {
    // Match OrcaSlicer's exact lighting setup
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);

    // Ambient
    float ambient[4] = {0.3f, 0.3f, 0.3f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

    // Light 0: Top-right
    float light0_pos[4] = {-0.457f, 0.457f, 0.762f, 0.0f};
    float light0_col[4] = {0.6f, 0.6f, 0.6f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, light0_pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_col);

    // Light 1: Front-right
    float light1_pos[4] = {0.699f, 0.140f, 0.699f, 0.0f};
    float light1_col[4] = {0.6f, 0.6f, 0.6f, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, light1_pos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_col);
}

std::vector<uint8_t> create_diff_image(const std::vector<uint8_t>& img1,
                                       const std::vector<uint8_t>& img2, int /*width*/,
                                       int /*height*/, float amplification) {
    std::vector<uint8_t> diff_img(img1.size());

    for (size_t i = 0; i < img1.size(); i++) {
        int diff = std::abs(static_cast<int>(img1[i]) - static_cast<int>(img2[i]));
        diff = std::min(255, static_cast<int>(diff * amplification));
        diff_img[i] = static_cast<uint8_t>(diff);
    }

    return diff_img;
}

double calculate_ssim(const std::vector<uint8_t>& img1, const std::vector<uint8_t>& img2,
                      int /*width*/, int /*height*/) {
    // Simplified SSIM calculation
    // Real SSIM uses sliding windows, this is a global approximation

    const double c1 = 6.5025;  // (0.01 * 255)^2
    const double c2 = 58.5225; // (0.03 * 255)^2

    double mean1 = 0.0, mean2 = 0.0;
    double var1 = 0.0, var2 = 0.0, covar = 0.0;

    size_t n = img1.size();

    // Calculate means
    for (size_t i = 0; i < n; i++) {
        mean1 += img1[i];
        mean2 += img2[i];
    }
    mean1 /= n;
    mean2 /= n;

    // Calculate variances and covariance
    for (size_t i = 0; i < n; i++) {
        double d1 = img1[i] - mean1;
        double d2 = img2[i] - mean2;
        var1 += d1 * d1;
        var2 += d2 * d2;
        covar += d1 * d2;
    }
    var1 /= (n - 1);
    var2 /= (n - 1);
    covar /= (n - 1);

    // SSIM formula
    double numerator = (2 * mean1 * mean2 + c1) * (2 * covar + c2);
    double denominator = (mean1 * mean1 + mean2 * mean2 + c1) * (var1 + var2 + c2);

    return numerator / denominator;
}

bool init_perf_counters() {
    // Platform-specific performance counter initialization
    // Would use perf_event_open on Linux, QueryPerformanceCounter on Windows
    // For now, return false (not implemented)
    return false;
}

double get_cache_miss_rate() {
    // Would read from performance counters if available
    return 0.0;
}

} // namespace utils
} // namespace tinygl_test