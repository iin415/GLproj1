#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image/stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <shaders/shader.h>
#include <camera/camera.h> 
#include <model/model.h>
#include <TextRenderer.h>

#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <iomanip>

#include <ft2build.h>
#include FT_FREETYPE_H

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
unsigned int loadTexture(char const *path);
float cross(const glm::vec2& O, const glm::vec2& A, const glm::vec2& B);
std::vector<glm::vec2> ComputeConvexHullXZ(
    const std::vector<glm::vec3>& vertices,
    const glm::mat4& modelMatrix,
    float maxY // camera/player collision height
);
bool PointInPolygon(const glm::vec2& point, const std::vector<glm::vec2>& polygon);

//Screen settings
const unsigned int SCR_WIDTH  = 1000;
const unsigned int SCR_HEIGHT = 800;

//Camera
float yaw = -160.0f;
float pitch = 12.0f;
Camera camera(
    glm::vec3(3.14f, 2.50f, 9.70f),
    glm::vec3(0.0f, 1.0f, 0.0f),
    yaw,
    pitch
);
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

//Time
float dT        = 0.0f;
float lastFrame = 0.0f;

//Collision detection
struct XZHull {
    std::vector<glm::vec2> points;
};

struct WorldObject {
    Model* model;
    glm::mat4 modelMatrix;
    std::vector<XZHull> hulls;
};

std::vector<WorldObject> worldObjects;

int main()
{
    //GLFW init and config
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    //GLFW window init
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "OpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); //hide cursor

    //GLAD -- load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST); //config global opengl state to enable builtin depth testing
    glEnable(GL_BLEND);
    //glEnable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //Shaders
    Shader modelShader("shaders/modelVS.glsl", "shaders/modelFS.glsl");
    Shader groundShader("shaders/groundVS.glsl", "shaders/groundFS.glsl");
    Shader textShader("shaders/textVS.glsl", "shaders/textFS.glsl");
    Shader lineShader("shaders/lineVS.glsl", "shaders/lineFS.glsl");

    //Text
    TextRenderer textRenderer;
    textRenderer.Init("C:\\Windows\\Fonts\\arial.ttf", SCR_WIDTH, SCR_HEIGHT, textShader.ID);

    //Models and collisions
    Model building1("models/house1_drawn.obj");
    Model car1("models/car1.obj");

    WorldObject obj1, obj2;
    obj1.model = &building1;
    obj2.model = &car1;
    obj1.modelMatrix = glm::mat4(1.0f);
    obj1.modelMatrix = glm::translate(obj1.modelMatrix, glm::vec3(-20.0f, 0.0f, 0.0f));
    obj1.modelMatrix = glm::rotate(obj1.modelMatrix, 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
    obj1.modelMatrix = glm::scale(obj1.modelMatrix, glm::vec3(2.5f, 2.5f, 2.5f));

    obj2.modelMatrix = glm::mat4(1.0f);
    obj2.modelMatrix = glm::translate(obj2.modelMatrix, glm::vec3(-12.0f, -0.5f, 10.0f));
    obj2.modelMatrix = glm::scale(obj2.modelMatrix, glm::vec3(2.5f, 2.5f, 2.5f));

    // Compute hulls
    for (Mesh& mesh : building1.meshes) {
        XZHull hull;
        // Convert Mesh.vertices (vector<Vertex>) to vector<glm::vec3>
        std::vector<glm::vec3> verts;
        for (const Vertex& v : mesh.vertices) {
            verts.push_back(v.Position);
        }

        hull.points = ComputeConvexHullXZ(verts, obj1.modelMatrix, 2.0f); // 2.0f = max collision height
        obj1.hulls.push_back(hull);
    }
    worldObjects.push_back(obj1);

    for (Mesh& mesh : car1.meshes) {
        XZHull hull;
        // Convert Mesh.vertices (vector<Vertex>) to vector<glm::vec3>
        std::vector<glm::vec3> verts;
        for (const Vertex& v : mesh.vertices) {
            verts.push_back(v.Position);
        }

        hull.points = ComputeConvexHullXZ(verts, obj2.modelMatrix, 2.0f); // 2.0f = max collision height
        obj2.hulls.push_back(hull);
    }
    worldObjects.push_back(obj2);

    struct HullRender {
        unsigned int VAO, VBO;
        int vertexCount;
    };

    std::vector<HullRender> hullRenderers;

    for (auto& obj : worldObjects) {
        for (auto& hull : obj.hulls) {
            std::vector<glm::vec3> hullVerts;
            for (auto& p : hull.points) {
                hullVerts.push_back(glm::vec3(p.x, 0.1f, p.y)); // lift slightly above ground
            }
            // Close the loop
            if (!hullVerts.empty())
                hullVerts.push_back(hullVerts.front());

            HullRender hr;
            glGenVertexArrays(1, &hr.VAO);
            glGenBuffers(1, &hr.VBO);
            hr.vertexCount = (int)hullVerts.size();

            glBindVertexArray(hr.VAO);
            glBindBuffer(GL_ARRAY_BUFFER, hr.VBO);
            glBufferData(GL_ARRAY_BUFFER, hullVerts.size() * sizeof(glm::vec3), hullVerts.data(), GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
            glBindVertexArray(0);

            hullRenderers.push_back(hr);
        }
    }

    //Ground plane (large quad, 4 triangles)
    unsigned int groundVAO, groundVBO, groundEBO;
    glGenVertexArrays(1, &groundVAO);
    glGenBuffers(1, &groundVBO);
    glGenBuffers(1, &groundEBO);

    //large dist for "inf" plane
    const float FAR_PLANE = 1e5f;
    float groundVertices[] = {
         0.0f, 0.0f,  0.0f,             // center
         FAR_PLANE, 0.0f,  0.0f,        // +X far
         0.0f, 0.0f,  FAR_PLANE,       // +Z far
        -FAR_PLANE, 0.0f,  0.0f,       // -X far
         0.0f, 0.0f, -FAR_PLANE        // -Z far
    };
    unsigned int groundIndices[] = {
        0,1,2,
        0,2,3,
        0,3,4,
        0,4,1
    };

    glBindVertexArray(groundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, groundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(groundVertices), groundVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, groundEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(groundIndices), groundIndices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);

    //RENDER LOOP
    while (!glfwWindowShouldClose(window))
    {
        //Frame-by-frame time logic
        float currentFrame = static_cast<float>(glfwGetTime());
        dT = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        //Background
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
         
        //Activate shader
        modelShader.use();

        //View and projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        modelShader.setMat4("projection", projection);
        modelShader.setMat4("view", view);

        // Draw ground plane
        groundShader.use();
        groundShader.setMat4("projection", projection);
        groundShader.setMat4("view", view);
        groundShader.setVec3("colorA", glm::vec3(0.3f, 0.3f, 0.3f));
        groundShader.setVec3("colorB", glm::vec3(0.0f, 0.0f, 0.0f));
        groundShader.setFloat("scale", 10.0f);

        //glDisable(GL_CULL_FACE);
        glBindVertexArray(groundVAO);
        glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        //glEnable(GL_CULL_FACE);
        
        //Rebind model shader
        for (auto& obj : worldObjects) {
            modelShader.use();
            modelShader.setMat4("model", obj.modelMatrix);
            obj.model->Draw(modelShader);  // note the '->' since model is a pointer
        }

        lineShader.use();
        lineShader.setMat4("projection", projection);
        lineShader.setMat4("view", view);
        lineShader.setVec3("color", glm::vec3(1.0f, 0.0f, 0.0f)); // red for hulls

        for (auto& hr : hullRenderers) {
            glBindVertexArray(hr.VAO);
            glDrawArrays(GL_LINE_STRIP, 0, hr.vertexCount);
            glBindVertexArray(0);
        }


        // Render camera position text (X and Z)
        {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2);
            ss << "X: " << camera.Position.x << "   Z: " << camera.Position.z 
               << "   Dir: (" << camera.Front.x << ", "
                << camera.Front.y << ", "
                << camera.Front.z << ")";
            std::string camText = ss.str();

            // Draw in top-left corner
            float x = 25.0f;
            float y = static_cast<float>(SCR_HEIGHT) - 50.0f;
            float scale = 0.5f;
            textRenderer.Render(textShader.ID, camText, x, y, scale, glm::vec3(0.9f, 0.9f, 0.3f));
        }

        glfwSwapBuffers(window);
        glfwPollEvents(); //Keys pressed, mouse, etc
    }

    //GLFW terminate clears previously allocated glfw resources
    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    glm::vec3 oldPos = camera.Position;
    float setY = 2.5f;
    glm::vec3 moveDir(0.0f);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        moveDir += camera.Front;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        moveDir -= camera.Front;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        moveDir -= camera.Right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        moveDir += camera.Right;

    moveDir.y = 0.0f;
    if (glm::length(moveDir) > 0.0f)
        moveDir = glm::normalize(moveDir) * dT * camera.MovementSpeed;

    // Attempted new position
    glm::vec3 newPos = camera.Position + moveDir;
    newPos.y = setY;

    glm::vec2 newXZ(newPos.x, newPos.z);
    float radius = 0.3f; // Camera collision radius

    // Check collisions
    for (auto& obj : worldObjects) {
        for (auto& hull : obj.hulls) {

            // Check if the camera is too close to any edge
            size_t n = hull.points.size();
            for (size_t i = 0; i < n; i++) {
                glm::vec2 a = hull.points[i];
                glm::vec2 b = hull.points[(i + 1) % n];

                // Project newXZ onto edge segment
                glm::vec2 ab = b - a;
                glm::vec2 ap = newXZ - a;
                float t = glm::clamp(glm::dot(ap, ab) / glm::dot(ab, ab), 0.0f, 1.0f);
                glm::vec2 closest = a + t * ab;

                float dist = glm::length(newXZ - closest);
                if (dist < radius) {
                    // Slide along edge: moveDir perpendicular to edge normal
                    glm::vec2 edgeNormal = glm::normalize(glm::vec2(-(b - a).y, (b - a).x));
                    glm::vec2 moveDirXZ(moveDir.x, moveDir.z);
                    glm::vec2 slide = moveDirXZ - glm::dot(glm::vec2(moveDir.x, moveDir.z), edgeNormal) * edgeNormal;
                    moveDir.x = slide.x;
                    moveDir.z = slide.y;
                    newPos = camera.Position + moveDir;
                    newXZ = glm::vec2(newPos.x, newPos.z);
                }
            }

            // Optional: prevent entering fully inside hull
            if (PointInPolygon(newXZ, hull.points)) {
                newPos.x = oldPos.x;
                newPos.z = oldPos.z;
                newXZ = glm::vec2(newPos.x, newPos.z);
            }
        }
    }

    camera.Position = newPos;
}



//when window size is changed
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

unsigned int loadTexture(char const * path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }
    
    return textureID;
}

float cross(const glm::vec2& O, const glm::vec2& A, const glm::vec2& B) {
    // Z-component of 3D cross product for 2D points
    return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
}

std::vector<glm::vec2> ComputeConvexHullXZ(
    const std::vector<glm::vec3>& vertices,
    const glm::mat4& modelMatrix,
    float maxY = 2.0f // camera/player collision height
) {
    std::vector<glm::vec2> points;

    // Only include vertices below maxY
    for (auto& v : vertices) {
        glm::vec4 worldPos = modelMatrix * glm::vec4(v, 1.0f);
        if (worldPos.y <= maxY) {
            points.push_back(glm::vec2(worldPos.x, worldPos.z));
        }
    }

    if (points.size() <= 2) return points;

    // Sort points by x, then z
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

    hull.pop_back(); // Last point == first point
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

