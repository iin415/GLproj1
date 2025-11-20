#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <iomanip>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <shaders/shader.h>
#include <camera/camera.h> 
#include <model/model.h>
#include <TextRenderer.h>
#include <Collision.h>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void AddWorldObject(std::vector<WorldObject>& worldObjects, Model& model, glm::mat4 modelMatrix, float maxY);

//Screen settings
const unsigned int SCR_WIDTH  = 1000;
const unsigned int SCR_HEIGHT = 800;

//Camera
float yaw = 147.79f;
float pitch = 7.46f;
Camera camera(
    glm::vec3(16.08f, 2.5f, -22.22f),
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

//Meshes
std::vector<WorldObject> worldObjects;

int main()
{
    //GLFW config
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
    //glEnable(GL_CULL_FACE); //disabled for now because it messes with tree model
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //Shaders
    Shader modelShader("shaders/modelVS.glsl", "shaders/modelFS.glsl");
    Shader groundShader("shaders/groundVS.glsl", "shaders/groundFS.glsl");
    Shader textShader("shaders/textVS.glsl", "shaders/textFS.glsl");
    Shader lineShader("shaders/lineVS.glsl", "shaders/lineFS.glsl");

    //Text
    TextRenderer textRenderer;
    textRenderer.Init("C:\\Windows\\Fonts\\arial.ttf", SCR_WIDTH, SCR_HEIGHT, textShader.ID);

    //Models and collisions with camera
    Model building1("models/house1_drawn.obj");
    Model car1("models/car1.obj");

    AddWorldObject(worldObjects, building1,
        glm::translate(glm::mat4(1.0f), glm::vec3(-20.0f, 0.0f, 0.0f)) *
        glm::rotate(glm::mat4(1.0f), 0.5f, glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(2.5f, 2.5f, 2.5f)),
        2.0f
    );

    AddWorldObject(worldObjects, car1,
        glm::translate(glm::mat4(1.0f), glm::vec3(-12.0f, -0.5f, 10.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(2.5f, 2.5f, 2.5f)),
        2.0f
    );

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
    const float FAR_PLANE = 1000.0f;
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

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        //Ground plane
        groundShader.use();
        groundShader.setMat4("projection", projection);
        groundShader.setMat4("view", view);
        groundShader.setVec3("colorA", glm::vec3(0.3f, 0.3f, 0.3f));
        groundShader.setVec3("colorB", glm::vec3(0.0f, 0.0f, 0.0f));
        groundShader.setFloat("scale", 10.0f);
        glBindVertexArray(groundVAO);
        glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        
        //Models
        modelShader.use();
        modelShader.setMat4("projection", projection);
        modelShader.setMat4("view", view);
        for (auto& obj : worldObjects) {
            modelShader.setMat4("model", obj.modelMatrix);
            obj.model->Draw(modelShader);
        }

        //Collision hulls
        lineShader.use();
        lineShader.setMat4("projection", projection);
        lineShader.setMat4("view", view);
        lineShader.setVec3("color", glm::vec3(1.0f, 0.0f, 0.0f)); // red lines
        for (auto& hr : hullRenderers) {
            glBindVertexArray(hr.VAO);
            glDrawArrays(GL_LINE_STRIP, 0, hr.vertexCount);
            glBindVertexArray(0);
        }

        //Render camera position text
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

    glfwTerminate(); //clears previously allocated glfw resources
    return 0;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    glm::vec3 moveDir(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) moveDir += camera.Front;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) moveDir -= camera.Front;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) moveDir -= camera.Right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) moveDir += camera.Right;

    moveDir.y = 0.0f;
    if (glm::length(moveDir) > 0.0f)
        moveDir = glm::normalize(moveDir) * dT * camera.MovementSpeed;

    //Handle collisions
    camera.Position = ResolveCameraCollisions(
        camera.Position, moveDir, 
        0.3f, //camera radius
        2.5f, //fixed camera y coord (height)
        worldObjects
    );
    
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

void AddWorldObject(std::vector<WorldObject>& worldObjects, Model& model, glm::mat4 modelMatrix, float maxY) {
    WorldObject obj;
    obj.model = &model;
    obj.modelMatrix = modelMatrix;
    for (Mesh& mesh : model.meshes) {
        std::vector<glm::vec3> verts;
        for (const Vertex& v : mesh.vertices)
            verts.push_back(v.Position);
        XZHull hull;
        hull.points = ComputeConvexHullXZ(verts, modelMatrix, maxY);
        obj.hulls.push_back(hull);
    }
    worldObjects.push_back(obj);
}

