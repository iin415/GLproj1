#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image/stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <shaders/shader.h>
#include <camera/camera.h> 
#include <model/model.h>

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
void RenderText(Shader &shader, std::string text, float x, float y, float scale, glm::vec3 color);
bool SphereSphereIntersect(const glm::vec3& posA, float radiusA, const glm::vec3& posB, float radiusB);

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

//Text
struct character {
    unsigned int TextID; //ID handle of glyph texture
    glm::ivec2 size;     //Size of glyph
    glm::ivec2 bearing;  //Offset from baseline to top left of glyph
    unsigned int Advance;//horizontal offset to advance to next glyph (spacing)
};

std::map<GLchar, character> characters;
unsigned int VAO, VBO;

//Collision detection
struct BoundingSphere {
    glm::vec3 center;
    float radius;
};

std::vector<BoundingSphere> buildingSpheres;
std::vector<BoundingSphere> worldSpheres;

struct SphereVertex {
    glm::vec3 Position;
};

class SphereMesh {
public:
    unsigned int VAO, VBO, EBO;
    int indexCount;

    SphereMesh(int latSegments = 12, int lonSegments = 12) {
        std::vector<SphereVertex> vertices;
        std::vector<unsigned int> indices;

        for (int y = 0; y <= latSegments; ++y) {
            for (int x = 0; x <= lonSegments; ++x) {
                float xSegment = (float)x / lonSegments;
                float ySegment = (float)y / latSegments;
                float xPos = std::cos(xSegment * 2.0f * glm::pi<float>()) * std::sin(ySegment * glm::pi<float>());
                float yPos = std::cos(ySegment * glm::pi<float>());
                float zPos = std::sin(xSegment * 2.0f * glm::pi<float>()) * std::sin(ySegment * glm::pi<float>());

                vertices.push_back({ glm::vec3(xPos, yPos, zPos) });
            }
        }

        bool oddRow = false;
        for (int y = 0; y < latSegments; ++y) {
            for (int x = 0; x < lonSegments; ++x) {
                indices.push_back(y * (lonSegments + 1) + x);
                indices.push_back((y + 1) * (lonSegments + 1) + x);
                indices.push_back((y + 1) * (lonSegments + 1) + x + 1);

                indices.push_back(y * (lonSegments + 1) + x);
                indices.push_back((y + 1) * (lonSegments + 1) + x + 1);
                indices.push_back(y * (lonSegments + 1) + x + 1);
            }
        }

        indexCount = static_cast<int>(indices.size());

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(SphereVertex), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SphereVertex), (void*)0);

        glBindVertexArray(0);
    }

    void Draw() {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};


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
    glEnable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //Shaders
    Shader modelShader("shaders/modelVS.glsl", "shaders/modelFS.glsl");
    Shader groundShader("shaders/groundVS.glsl", "shaders/groundFS.glsl");
    Shader textShader("shaders/textVS.glsl", "shaders/textFS.glsl");

    //Compile and setup the text shader
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(SCR_WIDTH), 0.0f, static_cast<float>(SCR_HEIGHT));
    textShader.use();
    glUniformMatrix4fv(glGetUniformLocation(textShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    //FreeType lib: load font and create char textures
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
    }

    //Using windows system font
    const char* fontPath = "C:\\Windows\\Fonts\\arial.ttf";
    FT_Face face;
    if (FT_New_Face(ft, fontPath, 0, &face)) {
        std::cout << "ERROR::FREETYPE: Failed to load font at " << fontPath << std::endl;
    } else {
        FT_Set_Pixel_Sizes(face, 0, 48);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); //disable byte-alignment restriction

        for (unsigned char c = 0; c < 128; c++) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                std::cout << "ERROR::FREETYTPE: Failed to load Glyph " << c << std::endl;
                continue;
            }
            unsigned int tex;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RED,
                face->glyph->bitmap.width,
                face->glyph->bitmap.rows,
                0,
                GL_RED,
                GL_UNSIGNED_BYTE,
                face->glyph->bitmap.buffer
            );
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            character ch;
            ch.TextID = tex;
            ch.size = glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows);
            ch.bearing = glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top);
            ch.Advance = static_cast<unsigned int>(face->glyph->advance.x);
            characters.insert(std::pair<char, character>(c, ch));
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        FT_Done_Face(face);
    }
    FT_Done_FreeType(ft);

    //Configure VAO/VBO for texture quads
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    //Models
    Model building1("models/house1_drawn.obj");

    //For collision detection
    for (Mesh& mesh : building1.meshes) {
        glm::vec3 center(0.0f);
        for (auto& v : mesh.vertices)
            center += v.Position;
        center /= static_cast<float>(mesh.vertices.size());

        float radius = 0.0f;
        for (auto& v : mesh.vertices)
            radius = std::max(radius, glm::length(v.Position - center));
        buildingSpheres.push_back({ center, radius });
    }

    SphereMesh debugSphere; //debugging collision detection


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

    //Render models
    glm::mat4 model1 = glm::mat4(1.0f); //building 1
    model1 = glm::translate(model1, glm::vec3(-20.0f, 0.0f, 0.0f));
    model1 = glm::rotate(model1, 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
    model1 = glm::scale(model1, glm::vec3(2.5f, 2.5f, 2.5f));

    //Collision detection : transform approx spheres to world space
    for (auto& s : buildingSpheres) {
        glm::vec4 worldCenter = model1 * glm::vec4(s.center, 1.0f);
        worldSpheres.push_back({ glm::vec3(worldCenter), s.radius });
    }

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

        // Draw ground plane (draw before models so depth testing works naturally)
        groundShader.use();
        groundShader.setMat4("projection", projection);
        groundShader.setMat4("view", view);
        groundShader.setVec3("colorA", glm::vec3(0.3f, 0.3f, 0.3f));
        groundShader.setVec3("colorB", glm::vec3(0.0f, 0.0f, 0.0f));
        groundShader.setFloat("scale", 10.0f);

        glDisable(GL_CULL_FACE);
        glBindVertexArray(groundVAO);
        glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glEnable(GL_CULL_FACE);
        
        //Rebind model shader
        modelShader.use();
        modelShader.setMat4("model", model1);
        building1.Draw(modelShader);

        // Debug spheres
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe
        modelShader.use();
        modelShader.setMat4("view", view);
        modelShader.setMat4("projection", projection);

        for (auto& s : worldSpheres) {
            glm::mat4 sphereModel = glm::mat4(1.0f);
            sphereModel = glm::translate(sphereModel, s.center);
            sphereModel = glm::scale(sphereModel, glm::vec3(s.radius));
            modelShader.setMat4("model", sphereModel);
            debugSphere.Draw();
        }

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // back to fill mode


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
            RenderText(textShader, camText, x, y, scale, glm::vec3(0.9f, 0.9f, 0.3f));
        }

        glfwSwapBuffers(window);
        glfwPollEvents(); //Keys pressed, mouse, etc
    }

    //GLFW terminate clears previously allocated glfw resources
    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }

    glm::vec3 oldPos = camera.Position;

    //const float cameraSpeed = 10.0f * dT;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD,dT);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, dT);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, dT);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, dT);

    float camRadius = 0.5f;

    //check collisions with mesh spheres
    for (const auto& sphere : worldSpheres) {
        if (SphereSphereIntersect(camera.Position, camRadius,
            sphere.center, sphere.radius))
        {
            camera.Position = oldPos; // revert if collision
            break;
        }
    }
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

void RenderText(Shader &shader, std::string text, float x, float y, float scale, glm::vec3 color)
{
    // activate corresponding render state	
    shader.use();
    glUniform3f(glGetUniformLocation(shader.ID, "textColor"), color.x, color.y, color.z);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(VAO);

    // iterate through all characters
    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++) 
    {
        auto it = characters.find(*c);
        if (it == characters.end()) continue;
        character ch = it->second;

        float xpos = x + ch.bearing.x * scale;
        float ypos = y - (ch.size.y - ch.bearing.y) * scale;

        float w = ch.size.x * scale;
        float h = ch.size.y * scale;
        // update VBO for each character
        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },            
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }           
        };
        // render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, ch.TextID);
        // update content of VBO memory
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); // be sure to use glBufferSubData and not glBufferData

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        // render quad
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
        x += (ch.Advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool SphereSphereIntersect(const glm::vec3& posA, float radiusA, const glm::vec3& posB, float radiusB)
{
    return glm::length(posA - posB) < (radiusA + radiusB);
}
