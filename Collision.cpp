#include "Collision.h"
#include <algorithm>

float cross(const glm::vec2& O, const glm::vec2& A, const glm::vec2& B) {
    return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x); //Z of 3D cross prod for 2D points
}


std::vector<glm::vec2> ComputeConvexHullXZ(
    const std::vector<glm::vec3>& vertices,
    const glm::mat4& modelMatrix,
    float maxY = 2.5f // camera height for fixed y-plane
) {
    std::vector<glm::vec2> points;

    //so we only include vertices below maxY
    for (auto& v : vertices) {
        glm::vec4 worldPos = modelMatrix * glm::vec4(v, 1.0f);
        if (worldPos.y <= maxY) {
            points.push_back(glm::vec2(worldPos.x, worldPos.z));
        }
    }

    if (points.size() <= 2) return points;

    //sort points by x, then z
    std::sort(points.begin(), points.end(), [](const glm::vec2& a, const glm::vec2& b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
        });

    std::vector<glm::vec2> hull;

    // Lower hull
    for (auto& p : points) {
        while (hull.size() >= 2 && cross(hull[hull.size() - 2], hull[hull.size() - 1], p) <= 0)
            hull.pop_back();
        hull.push_back(p);
    }

    // Upper hull
    size_t lowerSize = hull.size();
    for (int i = (int)points.size() - 2; i >= 0; i--) {
        glm::vec2 p = points[i];
        while (hull.size() > lowerSize && cross(hull[hull.size() - 2], hull[hull.size() - 1], p) <= 0)
            hull.pop_back();
        hull.push_back(p);
    }

    hull.pop_back(); //Last point == first point
    return hull;
}


bool PointInPolygon(const glm::vec2& point, const std::vector<glm::vec2>& polygon) {
    int crossings = 0;
    size_t n = polygon.size();
    for (size_t i = 0; i < n; i++) {
        glm::vec2 a = polygon[i];
        glm::vec2 b = polygon[(i + 1) % n];

        if (((a.y > point.y) != (b.y > point.y)) &&
            (point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x)) {
            crossings++;
        }
    }
    return (crossings % 2) == 1;
}

glm::vec3 ResolveCameraCollisions(
    const glm::vec3& oldPos,
    const glm::vec3& moveDirInput,
    float radius,
    float setY,
    const std::vector<WorldObject>& worldObjects
) {
    glm::vec3 moveDir = moveDirInput;
    glm::vec3 newPos = oldPos + moveDir;
    newPos.y = setY;

    glm::vec2 newXZ(newPos.x, newPos.z);

    for (const auto& obj : worldObjects) {
        for (const auto& hull : obj.hulls) {
            size_t n = hull.points.size();
            for (size_t i = 0; i < n; i++) {
                glm::vec2 a = hull.points[i];
                glm::vec2 b = hull.points[(i + 1) % n];

                glm::vec2 ab = b - a;
                glm::vec2 ap = newXZ - a;
                float t = glm::clamp(glm::dot(ap, ab) / glm::dot(ab, ab), 0.0f, 1.0f);
                glm::vec2 closest = a + t * ab;

                float dist = glm::length(newXZ - closest);
                if (dist < radius) {
                    glm::vec2 edgeNormal = glm::normalize(glm::vec2(-(b - a).y, (b - a).x));
                    glm::vec2 moveDirXZ(moveDir.x, moveDir.z);
                    glm::vec2 slide = moveDirXZ - glm::dot(moveDirXZ, edgeNormal) * edgeNormal;
                    moveDir.x = slide.x;
                    moveDir.z = slide.y;
                    newPos = oldPos + moveDir;
                    newXZ = glm::vec2(newPos.x, newPos.z);
                }
            }

            //Prevent fully entering into the hull
            if (PointInPolygon(newXZ, hull.points)) {
                newPos.x = oldPos.x;
                newPos.z = oldPos.z;
                newXZ = glm::vec2(newPos.x, newPos.z);
            }
        }
    }

    return newPos;
}