#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Shader source code
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    TexCoord = aTexCoord;
    gl_Position = projection * view * model * vec4(aPos, 1.0f);
})";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D texture1;
void main() {
    FragColor = texture(texture1, TexCoord);
})";

// Globals for camera control
glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 10.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float cameraSpeed = 0.05f;

// Key input callback
void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraUp;
}

GLuint loadTexture(const char* path) {
    int width, height, nrChannels;
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (!data) {
        std::cerr << "Failed to load texture!" << std::endl;
        return 0;
    }
    std::cout << "Loaded texture: " << path << " - " << width << "x" << height << std::endl;  // Debugging line
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    return texture;
}

// Sphere generation (stack and sector method)
void generateSphere(std::vector<float>& vertices, std::vector<unsigned int>& indices, float radius, int stackCount, int sectorCount) {
    std::cout << "Generating sphere..." << std::endl;
    for (int i = 0; i <= stackCount; ++i) {
        float stackAngle = glm::pi<float>() / 2 - i * glm::pi<float>() / stackCount;
        float xy = radius * cos(stackAngle);
        float z = radius * sin(stackAngle);

        for (int j = 0; j <= sectorCount; ++j) {
            float sectorAngle = j * 2 * glm::pi<float>() / sectorCount;

            float x = xy * cos(sectorAngle);
            float y = xy * sin(sectorAngle);

            // s and t should range from 0 to 1
            float s = (float)j / sectorCount; // Horizontal texture coordinate
            float t = (float)i / stackCount;  // Vertical texture coordinate

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(s);
            vertices.push_back(t);
        }
    }

    for (int i = 0; i < stackCount; ++i) {
        int k1 = i * (sectorCount + 1);
        int k2 = k1 + sectorCount + 1;

        for (int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            if (i != (stackCount - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }
    std::cout << "Sphere generated: " << vertices.size() / 5 << " vertices, " << indices.size() << " indices." << std::endl;
}

// Function to generate a ring (used for Saturn's rings)
void generateRing(std::vector<float>& vertices, float innerRadius, float outerRadius, int sectorCount) {
    for (int i = 0; i <= sectorCount; ++i) {
        float sectorAngle = i * 2 * glm::pi<float>() / sectorCount;
        float x = cos(sectorAngle);
        float y = sin(sectorAngle);

        // Two vertices for each sector (inner and outer ring)
        vertices.push_back(x * innerRadius);
        vertices.push_back(y * innerRadius);
        vertices.push_back(0.0f);
        vertices.push_back(0.0f); // Texture coordinates for inner ring

        vertices.push_back(x * outerRadius);
        vertices.push_back(y * outerRadius);
        vertices.push_back(0.0f);
        vertices.push_back(1.0f); // Texture coordinates for outer ring
    }
}

// Function to read shader source code from file
std::string readShaderFile(const char* filePath) {
    std::ifstream file(filePath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Function to compile shaders from file
GLuint compileShader(const char* filePath, GLenum shaderType) {
    std::string shaderCode = readShaderFile(filePath);
    if (shaderCode.empty()) {
        std::cerr << "Error: Shader code is empty!" << std::endl;
        return 0;
    }
    const char* code = shaderCode.c_str();

    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &code, nullptr);
    glCompileShader(shader);

    // Check for compilation errors
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cout << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    return shader;
}


int main() {
    // GLFW initialization
    if (!glfwInit()) return -1;

    GLFWwindow* window = glfwCreateWindow(800, 600, "Solar System Simulation", NULL, NULL);
    if (!window) return -1;
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height) {
        glViewport(0, 0, width, height);
        });
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_DEPTH_TEST);

    // Load and compile shaders from files
    GLuint vertexShader = compileShader("shaders/shader.vs", GL_VERTEX_SHADER);
    GLuint fragmentShader = compileShader("shaders/shader.fs", GL_FRAGMENT_SHADER);

    // Create the shader program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Check for linking errors
    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cout << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Sphere setup
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    generateSphere(vertices, indices, 1.0f, 20, 20);

    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Textures
    GLuint sunTexture = loadTexture("textures/sun_9.jpg");
    GLuint earthTexture = loadTexture("textures/earth_texture.jpg");
    GLuint mercuryTexture = loadTexture("textures/mercury_2.jpg");
    GLuint venusTexture = loadTexture("textures/venus_texture.jpg");
    GLuint moonTexture = loadTexture("textures/moon_texture.jpg");
    GLuint marsTexture = loadTexture("textures/mars_texture.jpg");
    GLuint jupiterTexture = loadTexture("textures/jupiter_texture.jpg");
    GLuint saturnTexture = loadTexture("textures/saturn_texture.jpg");
    GLuint uranusTexture = loadTexture("textures/uranus_2.jpg");
    GLuint neptuneTexture = loadTexture("textures/neptune_texture.jpg");

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

        // Sun
        glBindTexture(GL_TEXTURE_2D, sunTexture);
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f)); // Sun
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

        // Mercury
        glBindTexture(GL_TEXTURE_2D, mercuryTexture);
        model = glm::mat4(1.0f);
        model = glm::rotate(model, (float)glfwGetTime() * 4.0f, glm::vec3(0.0f, 1.0f, 0.0f)); // Orbit speed for Mercury
        model = glm::translate(model, glm::vec3(1.2f, 0.0f, 0.0f)); // Position
        model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f)); // Mercury size
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

        // Venus
        glBindTexture(GL_TEXTURE_2D, venusTexture);
        model = glm::mat4(1.0f);
        model = glm::rotate(model, (float)glfwGetTime() * 1.5f, glm::vec3(0.0f, 1.0f, 0.0f)); // Orbit speed for Venus
        model = glm::translate(model, glm::vec3(2.0f, 0.0f, 0.0f)); // Position
        model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f)); // Venus size
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

        // Earth (Orbiting the Sun)
        glBindTexture(GL_TEXTURE_2D, earthTexture);
        model = glm::mat4(1.0f);
        model = glm::rotate(model, (float)glfwGetTime() * 1.0f, glm::vec3(0.0f, 1.0f, 0.0f)); // Earth's orbit around the Sun
        model = glm::translate(model, glm::vec3(3.0f, 0.0f, 0.0f)); // Position of Earth (3 units from the Sun)
        model = glm::scale(model, glm::vec3(0.4f, 0.4f, 0.4f)); // Earth size
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

        // Moon (Orbiting the Earth)
        glBindTexture(GL_TEXTURE_2D, moonTexture);
        glm::mat4 moonModel = glm::mat4(1.0f);

        // First, rotate the moon based on the time to simulate its orbit around Earth
        moonModel = glm::rotate(moonModel, (float)glfwGetTime() * 2.0f, glm::vec3(0.0f, 1.0f, 0.0f)); // Moon's orbit speed around Earth
        moonModel = glm::translate(moonModel, glm::vec3(1.5f, 0.0f, 0.0f)); // Distance from Earth (1 unit)

        // Position the moon relative to the Earth, which is already orbiting the Sun
        moonModel = model * moonModel; // Apply Earth's model transformation to the moon
        moonModel = glm::scale(moonModel, glm::vec3(0.3f, 0.3f, 0.3f)); // Moon size

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(moonModel));
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

        // Mars
        glBindTexture(GL_TEXTURE_2D, marsTexture);
        model = glm::mat4(1.0f);
        model = glm::rotate(model, (float)glfwGetTime() * 0.8f, glm::vec3(0.0f, 1.0f, 0.0f)); // Orbit speed for Mars
        model = glm::translate(model, glm::vec3(4.0f, 0.0f, 0.0f)); // Position
        model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f)); // Mars size
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

        // Jupiter
        glBindTexture(GL_TEXTURE_2D, jupiterTexture);
        model = glm::mat4(1.0f);
        model = glm::rotate(model, (float)glfwGetTime() * 0.4f, glm::vec3(0.0f, 1.0f, 0.0f)); // Orbit speed for Jupiter
        model = glm::translate(model, glm::vec3(5.0f, 0.0f, 0.0f)); // Position
        model = glm::scale(model, glm::vec3(0.7f, 0.7f, 0.7f)); // Jupiter size
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

        // Saturn
        glBindTexture(GL_TEXTURE_2D, saturnTexture);
        model = glm::mat4(1.0f);
        model = glm::rotate(model, (float)glfwGetTime() * 0.3f, glm::vec3(0.0f, 1.0f, 0.0f)); // Orbit speed for Saturn
        model = glm::translate(model, glm::vec3(6.0f, 0.0f, 0.0f)); // Position
        model = glm::scale(model, glm::vec3(0.4f, 0.4f, 0.4f)); // Saturn size
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

        // Uranus
        glBindTexture(GL_TEXTURE_2D, uranusTexture);
        model = glm::mat4(1.0f);
        model = glm::rotate(model, (float)glfwGetTime() * 0.2f, glm::vec3(0.0f, 1.0f, 0.0f)); // Orbit speed for Uranus
        model = glm::translate(model, glm::vec3(7.0f, 0.0f, 0.0f)); // Position
        model = glm::scale(model, glm::vec3(0.35f, 0.35f, 0.35f)); // Uranus size
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

        // Neptune
        glBindTexture(GL_TEXTURE_2D, neptuneTexture);
        model = glm::mat4(1.0f);
        model = glm::rotate(model, (float)glfwGetTime() * 0.1f, glm::vec3(0.0f, 1.0f, 0.0f)); // Orbit speed for Neptune
        model = glm::translate(model, glm::vec3(8.0f, 0.0f, 0.0f)); // Position
        model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f)); // Neptune size
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glfwTerminate();
    return 0;
}