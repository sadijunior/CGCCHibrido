/*
 * Cube3D - Visualizador de Cubos 3D
 * Módulo 2 - Computação Gráfica
 */

#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace std;
using namespace glm;

struct Cube {
    vec3 position;
    vec3 rotation;
    float scale;
    bool rotatingX;
    bool rotatingY;
    bool rotatingZ;
};

const GLuint WIDTH = 800, HEIGHT = 800;

vector<Cube> cubes = {
    { vec3(0.0f, 0.0f, 0.0f), vec3(0.0f), 0.4f, false, false, false },
    { vec3(1.0f, 0.0f, 0.0f), vec3(0.0f), 0.3f, false, false, false },
    { vec3(-1.0f, 0.0f, 0.0f), vec3(0.0f), 0.3f, false, false, false }
};

int selectedCube = 0;

const GLchar* vertexShaderSource = R"(
#version 400
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 fragColor;
void main() {
    gl_Position = projection * view * model * vec4(position, 1.0);
    fragColor = color;
}
)";

const GLchar* fragmentShaderSource = R"(
#version 400
in vec3 fragColor;
out vec4 outColor;
void main() {
    outColor = vec4(fragColor, 1.0);
}
)";

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
GLuint setupShader();
GLuint setupCubeGeometry();
void printControls();

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    #ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Cubo 3D - Modulo 2", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cout << "Falha ao inicializar GLAD" << endl;
        return -1;
    }

    cout << "Renderer: " << glGetString(GL_RENDERER) << endl;
    cout << "OpenGL: " << glGetString(GL_VERSION) << endl;
    printControls();

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    GLuint shaderID = setupShader();
    GLuint cubeVAO = setupCubeGeometry();

    glUseProgram(shaderID);
    glEnable(GL_DEPTH_TEST);

    mat4 projection = perspective(radians(45.0f), (float)WIDTH / HEIGHT, 0.1f, 100.0f);
    mat4 view = lookAt(vec3(0.0f, 2.0f, 5.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));

    glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "view"), 1, GL_FALSE, value_ptr(view));

    GLint modelLoc = glGetUniformLocation(shaderID, "model");

    float lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float currentTime = glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        glfwPollEvents();

        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindVertexArray(cubeVAO);

        for (size_t i = 0; i < cubes.size(); i++) {
            Cube& cube = cubes[i];

            if (cube.rotatingX) cube.rotation.x += deltaTime;
            if (cube.rotatingY) cube.rotation.y += deltaTime;
            if (cube.rotatingZ) cube.rotation.z += deltaTime;

            mat4 model = mat4(1.0f);
            model = translate(model, cube.position);
            model = rotate(model, cube.rotation.x, vec3(1.0f, 0.0f, 0.0f));
            model = rotate(model, cube.rotation.y, vec3(0.0f, 1.0f, 0.0f));
            model = rotate(model, cube.rotation.z, vec3(0.0f, 0.0f, 1.0f));
            model = scale(model, vec3(cube.scale));

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glBindVertexArray(0);
        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &cubeVAO);
    glfwTerminate();
    return 0;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, GL_TRUE);
        return;
    }

    if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
        selectedCube = (selectedCube + 1) % cubes.size();
        cout << "Cubo selecionado: " << selectedCube + 1 << endl;
        return;
    }

    Cube& cube = cubes[selectedCube];

    switch (key) {
        case GLFW_KEY_X: cube.rotatingX = !cube.rotatingX; break;
        case GLFW_KEY_Y: cube.rotatingY = !cube.rotatingY; break;
        case GLFW_KEY_Z: cube.rotatingZ = !cube.rotatingZ; break;

        case GLFW_KEY_A: cube.position.x -= 0.1f; break;
        case GLFW_KEY_D: cube.position.x += 0.1f; break;
        case GLFW_KEY_W: cube.position.z -= 0.1f; break;
        case GLFW_KEY_S: cube.position.z += 0.1f; break;
        case GLFW_KEY_I: cube.position.y += 0.1f; break;
        case GLFW_KEY_J: cube.position.y -= 0.1f; break;

        case GLFW_KEY_LEFT_BRACKET:
            if (cube.scale > 0.1f) cube.scale -= 0.05f;
            break;
        case GLFW_KEY_RIGHT_BRACKET:
            if (cube.scale < 2.0f) cube.scale += 0.05f;
            break;
    }
}

GLuint setupShader() {
    GLint success;
    GLchar infoLog[512];

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        cout << "Erro no Vertex Shader:\n" << infoLog << endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        cout << "Erro no Fragment Shader:\n" << infoLog << endl;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        cout << "Erro ao linkar shaders:\n" << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}

GLuint setupCubeGeometry() {
    GLfloat vertices[] = {
        // Face frontal (Z+) - Vermelho
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,

        // Face traseira (Z-) - Verde
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,

        // Face esquerda (X-) - Azul
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f,

        // Face direita (X+) - Amarelo
         0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,

        // Face superior (Y+) - Ciano
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,

        // Face inferior (Y-) - Magenta
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f
    };

    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return VAO;
}

void printControls() {
    cout << "\n========================================" << endl;
    cout << " CONTROLES DO CUBO 3D" << endl;
    cout << "----------------------------------------" << endl;
    cout << " TAB       : Trocar cubo selecionado" << endl;
    cout << " W/S       : Mover no eixo Z" << endl;
    cout << " A/D       : Mover no eixo X" << endl;
    cout << " I/J       : Mover no eixo Y" << endl;
    cout << " X/Y/Z     : Ativar/desativar rotacao" << endl;
    cout << " [/]       : Diminuir/aumentar escala" << endl;
    cout << " ESC       : Sair" << endl;
    cout << "========================================\n" << endl;
}
