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
    unsigned int VAO, VBO;
    unsigned int SCR_WIDTH, SCR_HEIGHT;

    void Init(const std::string& fontPath, unsigned int width, unsigned int height, unsigned int textShaderID);
    void Render(unsigned int textShaderID, const std::string& text, float x, float y, float scale, glm::vec3 color);
};
