# CS 441 Intro Computer Graphics, Fall 2025

# Final Project: Game Environment
# By Isabella Norwood Paulus
__________________________________________________________________________________________________________________
## Overview
I set out to create an interactive/controllable 3D environment in OpenGL / C++. I wanted to build everything
mostly from scratch, creating the art and assets myself. I used the following open-source software:

    - Image editing:  Krita 
    - 3D modeling:    Blender 
    - Graphics API:   OpenGL (and glad, glfw, glm, etc.)
    - Model imports:  assimp 
    - Image loading:  stb_image 
    - Text rendering: FreeType

I used AI assistance through ChatGPT primarily for debugging, and occassionally to write helper functions for smaller utilities.
__________________________________________________________________________________________________________________
## Features
I implemented the following:
    - Camera controls
    - Simple walking animation (sin wave)
    - Basic phong shading with attenuation
    - Flashlight (random flickering, on/off control)
    - Camera-object collision handling (convex hulls)
    - Instancing
    - Menu UI, pause/unpause gamestate
    - Scalable game window
    - Debug option (prints camera and GPU info to window and renders hulls during runtime)

__________________________________________________________________________________________________________________
## Project Structure
My codebase consists of the following key files:
C++:
    - [`main`](main.cpp): render loop, input processing, UI, ... 
    - [`text renderer`](TextRenderer.cpp): uses freetype library to print program info to the render window
    - [`collision`](Collision.cpp): compute convex hulls and resolve camera collisions
Headers:
    - [`camera`](include/camera/camera.h): define camera class and process keyboard + mouse movement
    - [`mesh`](include/model/mesh.h): define vertex struct and mesh class, draw meshes
    - [`model`](include/model/model.h): load model (nodes, meshes, textures), instanced draw function
Shaders:
    - [`model shader`](shaders/modelFS.glsl): shade models (phong and flashlight)
    - [`ground shader`](shaders/groundFS.glsl): shade ground (plane is defined in main.cpp, not an imported model)

__________________________________________________________________________________________________________________
## Implementation Details
At a high level, everything in the scene is represented as a `WorldObject` struct (aside from the camera):
<details>
<summary>WorldObject</summary>

```cpp
struct WorldObject {
    Model* model = nullptr;
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    float boundingRadius = 5.0f;
    glm::vec3 center = glm::vec3(0.0f);
    std::vector<XZHull> hulls;
};
```
</details>

COLLISIONS:
The algorithm for building 2D convex hulls is well-documented online. I implemented it like so:
    - Filter model vertices by height (take a cross-section at camera Y-pos)
    - If there are <=2 vertices, return them.
    - Otherwise sort points by x, then z so the hull can be built sequentially
    - Build the "lower hull" going left --> right 
    - Build the "upper hull" going right --> left, remove points that break convexity
    - Store points as an `XZHull` struct, which contains a vector of vec2s.
A separate function resolves hull-camera collisions:
    - Treat the camera as a circle with a fixed radius
    - Player moves by `moveDirInput`
    - Check attempted movement against each convex hull edge
    - If a collision happens:
        - adjust movement so the camera does not enter the hull
    - Return new adjusted camera position

INSTANCING:
    - Program first generates many random points within the map area. I defined a polygon surrounding the house and other models --> points are rejected if they fall inside this polygon, or if they are too close to an existing point.
    - For each tree, a model matrix is built (translated to one of the points) and given a random scale.
    - This leads to a vector `std::vector<glm::mat4> treeTransforms` for which size = treeCount.
    - Instance matrices are uploaded to a VBO.
    - Trees are drawn with one call to `DrawInstanced` which sets a `useInstancing` boolean in the shader and does:

<details>
<summary>DrawInstanced</summary>

```cpp
    for (Mesh& mesh : meshes)
    {
        // Bind all mesh textures
        unsigned int diffuseNr = 1;
        unsigned int specularNr = 1;
        for (unsigned int i = 0; i < mesh.textures.size(); i++)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            std::string number;
            std::string name = mesh.textures[i].Type;
            if (name == "texture_diffuse") number = std::to_string(diffuseNr++);
            else if (name == "texture_specular") number = std::to_string(specularNr++);
            shader.setInt((name + number).c_str(), i);
            glBindTexture(GL_TEXTURE_2D, mesh.textures[i].ID);
        }

        glBindVertexArray(mesh.VAO);
        glDrawElementsInstanced(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0, instanceCount);
        glBindVertexArray(0);
    }
```
</details>

## Known Bugs / Limitations
The tree models are very simplified, and because they're planar, they don't always respond well to directional
light. I would either change my modeling approach, lighting, or program the tree planes so that they always face
the camera. 

There's a bug where the camera can get stuck between two collision hulls (the trashcan and the wall). I would be
curious to find out why that's happening. I haven't tried to debug it yet since it's pretty minor.


## Future Improvements
I started implementing frustum culling and didn't get around to finishing it. I'd like to work through it and see how it impacts the GPU usage at runtime. 

It would be cool to get more variation between the trees and vary the ground mapping, probably by using perlin
noise or something like that. I'd also like to develop actual game mechanics so there's a playable loop.
Attaching more conceptual and fully-developed art to the project would be really fun too.

