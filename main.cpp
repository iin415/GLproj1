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
unsigned int LoadTexture(const char* path);

void UpdateMenuQuad();
void UpdateUIProjection(GLFWwindow* window);

void AddWorldObject(std::vector<WorldObject>& worldObjects, Model& model, glm::mat4 modelMatrix, float maxY);
bool IsPointInPolygon(const glm::vec2& point, const std::vector<glm::vec2>& poly);
bool IsFarEnough(const glm::vec2& candidate, const std::vector<glm::vec3>& points, float minDistance);
std::vector<glm::vec3> GenerateRandomPoints(int count, float areaSize,
    const std::vector<glm::vec2>& forbiddenPoly,
    float minDistance);

//Screen settings
unsigned int SCR_WIDTH  = 1000;
unsigned int SCR_HEIGHT = 800;

//Measuring GPU time
GLuint gpuQuery[2];
bool gpuQueryInFlight = false;
double lastGpuTimeMs = 0.0;

//Camera
glm::vec3 startFront =
glm::normalize(glm::vec3(-0.89f, 0.46f, -0.07f)); //start direction vector
float startYaw = glm::degrees(atan2(startFront.z, startFront.x));
float startPitch = glm::degrees(asin(startFront.y));
Camera camera(
    glm::vec3(-5.7f, 2.7f, 0.52f), //start pos
    glm::vec3(0.0f, 1.0f, 0.0f),
    startYaw,
    startPitch
);

float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

//Time
float dT        = 0.0f;
float lastFrame = 0.0f;
float cameraY = 2.7f; 

//Text
TextRenderer textRenderer;
bool debugInfo = true;

//Player walking -- head bobbing movement
static float bobAmplitude = 0.1f;
static float bobBaseHeight = 2.7f; // center Y
static float bobFrequency = 9.0f; // i.e. speed
float bobTime = 0.0f; //keep track of walking animation

//light color for scene
glm::vec3 sceneColor = glm::vec3(0.15f, 0.25f, 0.8f);

//Player flashlight
static bool flashlight = false;
static float innerCut = glm::cos(glm::radians(5.0f));
static float outerCut = glm::cos(glm::radians(20.0f));
glm::vec3 flashColor = glm::vec3(1.0f, 1.0f, 0.3f);
static float strength = 1.0f;
static float newStrength = 1.0f;

static float flickerTimer = 0.0f;
static float flickerDuration = 0.1f;
static float flickerCooldown = 2.0f;
static bool isFlickering = false;

//Meshes
std::vector<WorldObject> worldObjects;

//Matrices (need to update projection when window size is changed)
glm::mat4 projection;

//Game states
enum GameState {
    RUNNING, PAUSED
};

//Menu stuff
GameState state = PAUSED;
unsigned int menuVAO = 0;
unsigned int menuVBO = 0;
glm::mat4 uiProjection;

//Tree stuff
std::vector<glm::vec2> treePositions2D; // (x,z) positions for collision checks
std::vector<float>   treeCollisionRadii;// per-tree collision radius

int main()
{
    //GLFW config
    glfwInit();
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
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

    //GLAD -- load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glGenQueries(2, gpuQuery);

    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); //hide cursor

    glEnable(GL_DEPTH_TEST); //config global opengl state to enable builtin depth testing
    glEnable(GL_BLEND);
    //glEnable(GL_CULL_FACE); //disabled for now because it messes with tree model
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float aspectRatio = static_cast<float>(SCR_WIDTH) / static_cast<float>(SCR_HEIGHT);
    projection = glm::perspective(glm::radians(camera.Zoom), aspectRatio, 0.1f, 100.0f);

    //Shaders
    Shader modelShader("shaders/modelVS.glsl", "shaders/modelFS.glsl");
    Shader groundShader("shaders/groundVS.glsl", "shaders/groundFS.glsl");
    Shader textShader("shaders/textVS.glsl", "shaders/textFS.glsl");
    Shader lineShader("shaders/lineVS.glsl", "shaders/lineFS.glsl");
    Shader menuShader("shaders/menuVS.glsl", "shaders/menuFS.glsl");

    //Text
    textRenderer.Init("C:\\Windows\\Fonts\\CascadiaCode.ttf", SCR_WIDTH, SCR_HEIGHT, textShader.ID);

    //Models
    Model building1("models/house1_drawn.obj");
    Model car1("models/car1.obj");
    Model key("models/key.obj");
    Model tree1("models/tree1.obj");

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

    //AddWorldObject(worldObjects, key,
    //    glm::translate(glm::mat4(1.0f), glm::vec3(-15.0f, 1.0f, 15.0f)) *
    //    glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 0.5f)),
    //    2.0f
    //);

    //Instancing for tree models
    std::vector<glm::vec2> forbiddenPoly = { //the area around the house where I want no trees to spawn
        { -44.43f, -7.43f },
        {  -28.23f, 18.67f },
        {  2.87f,  21.00f },
        { -5.02f,  -12.08f },
        { -27.29f,  -25.53f }
    };

    //Rendering for debug (exclusion area polygon)
    std::vector<glm::vec3> poly3D;
    for (auto& p : forbiddenPoly) {
        poly3D.push_back(glm::vec3(p.x, 0.01f, p.y));
    }
    unsigned int polyVAO, polyVBO;
    glGenVertexArrays(1, &polyVAO);
    glGenBuffers(1, &polyVBO);

    glBindVertexArray(polyVAO);
    glBindBuffer(GL_ARRAY_BUFFER, polyVBO);

    glBufferData(GL_ARRAY_BUFFER, poly3D.size() * sizeof(glm::vec3),
        poly3D.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    glBindVertexArray(0);

    std::vector<glm::mat4> treeTransforms;
    int treeCount = 100;
    float area = 50.0f;
    float minDistance = 2.0f;
    float baseTreeRadius = 0.8f;
    // Generate valid random positions
    std::vector<glm::vec3> treePositions3D = GenerateRandomPoints(
        treeCount,
        area,
        forbiddenPoly,
        minDistance
    );

    // Generate transforms and collision data
    treeTransforms.clear();
    treePositions2D.clear();
    treeCollisionRadii.clear();

    srand(1234); //fixed seed
    for (const glm::vec3& pos : treePositions3D) {
        float scale = 0.5f + static_cast<float>(rand()) / RAND_MAX * 1.0f;

        glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
        model = glm::scale(model, glm::vec3(scale));

        treeTransforms.push_back(model);
        treePositions2D.push_back(glm::vec2(pos.x, pos.z));
        treeCollisionRadii.push_back(baseTreeRadius * scale);
    }

    unsigned int instanceVBO;
    glGenBuffers(1, &instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, treeTransforms.size() * sizeof(glm::mat4), treeTransforms.data(), GL_STATIC_DRAW);

    std::vector<XZHull> baseTreeHulls;

    for (Mesh& mesh : tree1.meshes) {
        glBindVertexArray(mesh.VAO);

        std::size_t vec4Size = sizeof(glm::vec4);
        for (int i = 0; i < 4; ++i) {
            glEnableVertexAttribArray(3 + i);
            glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE,
                sizeof(glm::mat4), (void*)(i * vec4Size));
            glVertexAttribDivisor(3 + i, 1);
        }
        glBindVertexArray(0);

        std::vector<glm::vec3> positions; //computing hull once so I can add instanced trees to WorldObjects
        positions.reserve(mesh.vertices.size()); // Extract positions from Vertex structs
        for (const Vertex& v : mesh.vertices) {
            positions.push_back(v.Position);
        }

        std::vector<glm::vec2> hull2D = ComputeConvexHullXZ(positions, glm::mat4(1.0f), 2.5f);

        // Convert to XZHull type
        XZHull xzHull;
        xzHull.points = hull2D;
        baseTreeHulls.push_back(xzHull);
    }

    for (size_t i = 0; i < treeTransforms.size(); ++i) {
        const glm::mat4& modelMatrix = treeTransforms[i];

        WorldObject treeObj;
        treeObj.model = &tree1;
        treeObj.modelMatrix = modelMatrix;

        // Transform base hulls
        treeObj.hulls.clear();
        for (const XZHull& baseHull : baseTreeHulls) {
            XZHull transformedHull;
            transformedHull.points.reserve(baseHull.points.size());
            for (const glm::vec2& p : baseHull.points) {
                glm::vec4 transformed = modelMatrix * glm::vec4(p.x, 0.0f, p.y, 1.0f);
                transformedHull.points.push_back(glm::vec2(transformed.x, transformed.z));
            }
            treeObj.hulls.push_back(transformedHull);
        }

        //compute center & bounding radius, not strictly necessary(?)
        treeObj.center = glm::vec3(modelMatrix[3]); // translation
        treeObj.boundingRadius = treeCollisionRadii[i];

        worldObjects.push_back(treeObj);
    }

    struct HullRender {
        unsigned int VAO, VBO;
        int vertexCount;
    };

    std::vector<HullRender> hullRenderers;

    for (auto& obj : worldObjects) {
        for (auto& hull : obj.hulls) {
            std::vector<glm::vec3> hullVerts;
            for (auto& p : hull.points) {
                hullVerts.push_back(glm::vec3(p.x, 0.1f, p.y)); //slightly above ground --> visible
            }
            //closed loop
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

    //Ground plane (large quad)
    unsigned int groundVAO, groundVBO, groundEBO;
    glGenVertexArrays(1, &groundVAO);
    glGenBuffers(1, &groundVBO);
    glGenBuffers(1, &groundEBO);

    const float FAR_PLANE = 1000.0f;
    const float TILE_REAPEAT = 75.0f;

    float groundVertices[] = {
        // positions       // UVs       // normals
        -FAR_PLANE, 0.0f, -FAR_PLANE,   0.0f, 0.0f,          0.0f, 1.0f, 0.0f,
         FAR_PLANE, 0.0f, -FAR_PLANE,   TILE_REAPEAT, 0.0f,   0.0f, 1.0f, 0.0f,
         FAR_PLANE, 0.0f,  FAR_PLANE,   TILE_REAPEAT, TILE_REAPEAT,  0.0f, 1.0f, 0.0f,
        -FAR_PLANE, 0.0f,  FAR_PLANE,   0.0f, TILE_REAPEAT,  0.0f, 1.0f, 0.0f
    };

    unsigned int groundIndices[] = {
        0, 1, 2,
        0, 2, 3
    };

    glBindVertexArray(groundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, groundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(groundVertices), groundVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, groundEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(groundIndices), groundIndices, GL_STATIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    // texcoord
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    // normals
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));

    glBindVertexArray(0);

    unsigned int groundTextureID = LoadTexture("models/ground1.jpg");
    unsigned int menuTextureID = LoadTexture("models/menu.png");

    UpdateUIProjection(window);

    //RENDER LOOP
    while (!glfwWindowShouldClose(window))
    {
        if (!gpuQueryInFlight)
        {
            glBeginQuery(GL_TIME_ELAPSED, gpuQuery[0]);
            gpuQueryInFlight = true;
        }

        //Frame-by-frame time logic
        float currentFrame = static_cast<float>(glfwGetTime());
        dT = currentFrame - lastFrame;
        lastFrame = currentFrame;
        processInput(window);

        //Background
        glClearColor(0.0f, 0.0f, 0.03f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = camera.GetViewMatrix();

        //Ground
        groundShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, groundTextureID);
        groundShader.setInt("groundTexture", 0);

        groundShader.setMat4("projection", projection);
        groundShader.setMat4("view", view);
        glm::mat4 groundModel = glm::mat4(1.0f);
        groundShader.setMat4("model", groundModel);

        groundShader.setVec3("lightPos", glm::vec3(5.0f, 15.0f, 5.0f));
        groundShader.setVec3("lightColor", sceneColor);
        groundShader.setVec3("viewPos", camera.Position);
        groundShader.setFloat("shininess", 35.0f);

        groundShader.setFloat("innerCutoff", innerCut);
        groundShader.setFloat("outerCutoff", outerCut);
        groundShader.setVec3("flashlightPos", camera.Position);
        groundShader.setVec3("flashlightDir", camera.Front);
        groundShader.setFloat("strength", strength);
        groundShader.setBool("flashlightOn", flashlight);
        groundShader.setVec3("flashlightColor", flashColor);

        glBindVertexArray(groundVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        
        //Models
        modelShader.use();
        modelShader.setVec3("lightPos", glm::vec3(5.0f, 15.0f, 5.0f));
        modelShader.setVec3("lightColor", sceneColor);
        modelShader.setVec3("viewPos", camera.Position);
        modelShader.setFloat("shininess", 25.0f);
        modelShader.setFloat("innerCutoff", innerCut);
        modelShader.setFloat("outerCutoff", outerCut);
        modelShader.setVec3("flashlightPos", camera.Position);
        modelShader.setVec3("flashlightDir", camera.Front);
        modelShader.setFloat("strength", newStrength);
        modelShader.setBool("flashlightOn", flashlight);
        modelShader.setVec3("flashlightColor", flashColor);

        modelShader.setMat4("projection", projection);
        modelShader.setMat4("view", view);

        for (auto& obj : worldObjects) {
            modelShader.setMat4("model", obj.modelMatrix);
            obj.model->Draw(modelShader);
        }

        // Tree draw
        tree1.DrawInstanced(modelShader, treeCount);

        if (flashlight) {
            flickerTimer += dT;
            if (!isFlickering) {
                if (flickerTimer >= flickerCooldown) {
                    isFlickering = true;
                    flickerTimer = 0.0f;
                    flickerCooldown = 1.5f + static_cast<float>(rand()) / RAND_MAX * 2.5f;
                }
            }
            else {
                newStrength = strength * (0.5f + static_cast<float>(rand()) / RAND_MAX * 0.6f);

                if (flickerTimer >= flickerDuration)
                {
                    // End flicker
                    isFlickering = false;
                    newStrength = strength;
                    flickerTimer = 0.0f;
                }
            }
        }
        flashColor = glm::vec3(1.0f, 0.98f, 0.5f) * (0.9f + 0.1f * ((float)rand() / RAND_MAX));

        if (debugInfo) {
            //Collision hulls, render for debugging
            lineShader.use();
            lineShader.setMat4("projection", projection);
            lineShader.setMat4("view", view);
            lineShader.setVec3("color", glm::vec3(1.0f, 0.0f, 0.0f)); // red lines
            for (auto& hr : hullRenderers) {
                glBindVertexArray(hr.VAO);
                glDrawArrays(GL_LINE_STRIP, 0, hr.vertexCount);
                glBindVertexArray(0);
            }
            //Rendering polygon that keeps trees out
            glBindVertexArray(polyVAO);
            glLineWidth(3.0f);
            glDrawArrays(GL_LINE_LOOP, 0, poly3D.size());
            glBindVertexArray(0);
            
            // Camera text string
            std::ostringstream camSS;
            camSS << std::fixed << std::setprecision(2);
            camSS << "X: " << camera.Position.x << "   Y: " << camera.Position.y << "   Z: " << camera.Position.z << "\n"
                << "Dir: (" << camera.Front.x << ", " << camera.Front.y << ", " << camera.Front.z << ")";
            
            // Camera text position and render
            float camX = 25.0f;
            float camY = static_cast<float>(SCR_HEIGHT) - 40.0f;
            textRenderer.Render(textShader.ID, camSS.str(), camX, camY, 0.5f,
                glm::vec3(0.9f, 0.2f, 0.2f));
            
            // GPU time text string
            std::ostringstream gpuSS;
            gpuSS << std::fixed << std::setprecision(3);
            gpuSS << "GPU frame time: " << lastGpuTimeMs << " ms";
            
            // GPU text position and render
            float gpuX = 25.0f;
            float gpuY = 30.0f;
            textRenderer.Render(textShader.ID, gpuSS.str(), gpuX, gpuY, 0.5f,
                glm::vec3(0.2f, 0.9f, 0.2f));
        }

        //GAME MENU overlay simple transparent png and pause movement/events
        if (state == PAUSED) {
            glDisable(GL_DEPTH_TEST);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            flashlight = true;

            menuShader.use();
            menuShader.setMat4("projection", uiProjection);

            glBindVertexArray(menuVAO);
            glBindTexture(GL_TEXTURE_2D, menuTextureID);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glBindVertexArray(0);

            glEnable(GL_DEPTH_TEST);
        }

        //End GPU timing query
        glEndQuery(GL_TIME_ELAPSED);
        //Check if result is available
        GLuint available = 0;
        glGetQueryObjectuiv(gpuQuery[0], GL_QUERY_RESULT_AVAILABLE, &available);
        if (available) {
            GLuint64 timeElapsed = 0;
            glGetQueryObjectui64v(gpuQuery[0], GL_QUERY_RESULT, &timeElapsed);
            lastGpuTimeMs = timeElapsed / 1e6;
            gpuQueryInFlight = false;
        }

        glfwSwapBuffers(window);
        glfwPollEvents(); //Keys pressed, mouse, etc
    }

    glfwTerminate(); //clears previously allocated glfw resources
    return 0;
}

void processInput(GLFWwindow* window)
{
    static bool pausePressed = false; //pressed last frame
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS && !pausePressed) {
        pausePressed = true;
        state = (state == RUNNING) ? PAUSED : RUNNING;
        if (state == PAUSED)
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        else
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    else if(glfwGetKey(window, GLFW_KEY_P) == GLFW_RELEASE) {
        pausePressed = false;
    }

    if (state == RUNNING) {
        flashlight = (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS);
    }
    else {
        flashlight = true;
    }

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

    glm::vec3 moveDir(0.0f);
    bool isMoving = false;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { moveDir += camera.Front; isMoving = true; }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { moveDir -= camera.Front; isMoving = true; }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { moveDir -= camera.Right; isMoving = true; }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { moveDir += camera.Right; isMoving = true; }

    if (glm::length(moveDir) > 0.0f)
        moveDir = glm::normalize(moveDir) * dT * camera.MovementSpeed;

    if (isMoving) {
        bobTime += dT * bobFrequency;
        cameraY = bobBaseHeight + static_cast<float>(pow(sin(bobTime), 3) * bobAmplitude);
    }
    else {
        bobTime = 0.0f;
        cameraY = bobBaseHeight;
    }
    
    if (state == RUNNING) {
        //Update camera pos, handle collisions
        glm::vec3 newCamPos = ResolveCameraCollisions(camera.Position, moveDir, 0.3f, cameraY, worldObjects);

        //Apply final camera Y (head bobbing may have modified cameraY variable)
        newCamPos.y = cameraY;
        camera.Position = newCamPos;
    }
}

//when window size is changed
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    if (height == 0) height = 1;
    SCR_WIDTH = width;
    SCR_HEIGHT = height;

    glViewport(0, 0, width, height);

    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    projection = glm::perspective(glm::radians(camera.Zoom), aspectRatio, 0.1f, 100.0f);

    textRenderer.UpdateProjection(width, height);

    UpdateUIProjection(window);

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

    if(state == RUNNING) {
        camera.ProcessMouseMovement(xoffset, yoffset);
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

void AddWorldObject(std::vector<WorldObject>& worldObjects, Model& model, glm::mat4 modelMatrix, float maxY) {
    WorldObject obj;
    obj.model = &model;
    obj.modelMatrix = modelMatrix;

    obj.center = glm::vec3(modelMatrix * glm::vec4(0, 0, 0, 1));  //TODO frustum culling
    obj.boundingRadius = 5.0f; //TODO frustum culling

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

unsigned int LoadTexture(const char* path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);

    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data)
    {
        GLenum format = (nrChannels == 4 ? GL_RGBA :
            nrChannels == 3 ? GL_RGB :
            nrChannels == 1 ? GL_RED : GL_RGB);

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
            format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else
    {
        std::cerr << "Failed to load texture: " << path << std::endl;
    }

    stbi_image_free(data);
    return textureID;
}


void UpdateMenuQuad()
{
    float w = SCR_HEIGHT - 80.0f;  // image width (pixels)
    float h = w;  //height

    float x = (SCR_WIDTH - w) * 0.5f;
    float y = (SCR_HEIGHT - h) * 0.5f;

    float menuVertices[] =
    {
        // positions        // texCoords
        x,     y + h,       0.0f, 1.0f,  // top-left
        x,     y,           0.0f, 0.0f,  // bottom-left
        x + w, y,           1.0f, 0.0f,  // bottom-right

        x,     y + h,       0.0f, 1.0f,  // top-left
        x + w, y,           1.0f, 0.0f,  // bottom-right
        x + w, y + h,       1.0f, 1.0f   // top-right
    };

    if (menuVAO == 0)
    {
        glGenVertexArrays(1, &menuVAO);
        glGenBuffers(1, &menuVBO);
    }

    glBindVertexArray(menuVAO);
    glBindBuffer(GL_ARRAY_BUFFER, menuVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(menuVertices), menuVertices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void UpdateUIProjection(GLFWwindow* window)
{
    int fbW = 0;
    int fbH = 0;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    if (fbH == 0) fbH = 1;
    if (fbW == 0) fbW = 1;

    SCR_WIDTH = static_cast<unsigned int>(fbW);
    SCR_HEIGHT = static_cast<unsigned int>(fbH);

    uiProjection = glm::ortho(
        0.0f, static_cast<float>(SCR_WIDTH), 0.0f, static_cast<float>(SCR_HEIGHT), -1.0f, 1.0f
    );
    UpdateMenuQuad();
}

//------------------TREE STUFF---------------------------------
bool IsPointInPolygon(const glm::vec2& point, const std::vector<glm::vec2>& poly)
{
    int n = poly.size();
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const glm::vec2& pi = poly[i];
        const glm::vec2& pj = poly[j];
        if (((pi.y > point.y) != (pj.y > point.y)) &&
            (point.x < (pj.x - pi.x) * (point.y - pi.y) / (pj.y - pi.y + 0.00001f) + pi.x)) {
            inside = !inside;
        }
    }
    return inside;
}

// Check if candidate is at least minDistance away from all existing points
bool IsFarEnough(const glm::vec2& candidate, const std::vector<glm::vec3>& points, float minDistance)
{
    float minDistSq = minDistance * minDistance;
    for (const glm::vec3& p : points) {
        glm::vec2 diff(candidate.x - p.x, candidate.y - p.z);
        if (glm::dot(diff, diff) < minDistSq)
            return false;
    }
    return true;
}

std::vector<glm::vec3> GenerateRandomPoints(int count, float areaSize,
    const std::vector<glm::vec2>& forbiddenPoly,
    float minDistance = 2.0f)
{
    std::vector<glm::vec3> points;
    points.reserve(count);

    int attempts = 0;
    int maxAttempts = count * 50; // increase attempts due to minDistance constraint

    while (points.size() < count && attempts < maxAttempts) {
        attempts++;

        float x = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * areaSize * 2.0f;
        float z = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * areaSize * 2.0f;
        glm::vec2 candidate(x, z);

        if (!IsPointInPolygon(candidate, forbiddenPoly))
            if (IsFarEnough(candidate, points, minDistance))
                points.push_back(glm::vec3(x, 0.0f, z));
    }

    return points;
}



