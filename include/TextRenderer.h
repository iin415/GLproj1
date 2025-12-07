#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <map>
#include <string>

struct Character {
    unsigned int TextureID;
    glm::ivec2 Size;
    glm::ivec2 Bearing;
    unsigned int Advance;
};

class TextRenderer {
public:
    std::map<GLchar, Character> Characters;
    unsigned int VAO = 0, VBO = 0;
    unsigned int SCR_WIDTH = 0, SCR_HEIGHT = 0;
    int lineHeight = 0;
    int textShaderID = 0;

    void Init(const std::string& fontPath, unsigned int width, unsigned int height, unsigned int textShaderID);
    void Render(unsigned int textShaderID, const std::string& text, float x, float y, float scale, glm::vec3 color);
    void UpdateProjection(unsigned int width, unsigned int height);
};
