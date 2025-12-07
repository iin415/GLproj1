#pragma once
#include <vector>
#include <glm/glm.hpp>

class Model;

struct XZHull {
    std::vector<glm::vec2> points;
};

struct WorldObject {
    Model* model = nullptr;
    glm::mat4 modelMatrix = glm::mat4(1.0f);

    float boundingRadius = 5.0f;
    glm::vec3 center = glm::vec3(0.0f);

    std::vector<XZHull> hulls;
};

float cross(const glm::vec2& O, const glm::vec2& A, const glm::vec2& B);

std::vector<glm::vec2> ComputeConvexHullXZ(
    const std::vector<glm::vec3>& vertices,
    const glm::mat4& modelMatrix,
    float maxY
);

bool PointInPolygon(const glm::vec2& point, const std::vector<glm::vec2>& polygon);

glm::vec3 ResolveCameraCollisions( //what happens when a collision is detected
    const glm::vec3& oldPos,
    const glm::vec3& moveDir,
    float cameraRadius,
    float setY,
    const std::vector<WorldObject>& worldObjects
);
