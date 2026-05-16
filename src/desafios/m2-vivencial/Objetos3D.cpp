/*
 * Vivencial M2 - Seleção e Transformações em Objetos 3D
 * Módulo 2 - Computação Gráfica - Unisinos
 */

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cfloat>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using namespace glm;

// ============================================================================
// CONSTANTES
// ============================================================================

const GLuint WIDTH = 800, HEIGHT = 800;
const int SHADER_LOG_SIZE = 512;
const float ROTATE_SPEED   = 1.0f;
const float TRANSLATE_STEP = 0.1f;
const float SCALE_STEP     = 0.05f;
const float SCALE_MIN      = 0.05f;

// ============================================================================
// MODO DE TRANSFORMAÇÃO
// ============================================================================

enum class Mode { ROTATE, TRANSLATE, SCALE };

Mode currentMode = Mode::ROTATE;

// ============================================================================
// ESTRUTURA DO MODELO
// ============================================================================

struct ObjModel {
    string name;
    GLuint vao;
    int    nVertices;
    vec3   position;
    vec3   scale;
    float  angleX, angleY, angleZ;
    bool   rotateX, rotateY, rotateZ;
};

vector<ObjModel> models;
int selectedModel = 0;

// ============================================================================
// SHADERS
// ============================================================================

const GLchar* vertexShaderSource = R"(
#version 410
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
uniform mat4 model;
out vec4 finalColor;
void main() {
    gl_Position = model * vec4(position, 1.0);
    finalColor = vec4(color, 1.0);
}
)";

const GLchar* fragmentShaderSource = R"(
#version 410
in vec4 finalColor;
out vec4 color;
void main() {
    color = finalColor;
}
)";

// ============================================================================
// PROTÓTIPOS
// ============================================================================

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
GLuint setupShader();
GLuint loadOBJ(const string& filePath, int& nVertices, vec3 color);
ObjModel loadModel(const string& name, const string& filePath, vec3 position, float scale, vec3 color);
void printControls();
void printModeHelp();

// ============================================================================
// COMPILAÇÃO DE SHADER
// ============================================================================

static GLuint compileShader(GLenum type, const GLchar* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint success;
    GLchar log[SHADER_LOG_SIZE];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, SHADER_LOG_SIZE, NULL, log);
        cerr << "Erro de compilacao do shader:\n" << log << endl;
    }
    return shader;
}

GLuint setupShader() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint success;
    GLchar log[SHADER_LOG_SIZE];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, SHADER_LOG_SIZE, NULL, log);
        cerr << "Erro de linkagem:\n" << log << endl;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// ============================================================================
// CARREGAMENTO DO OBJ
// ============================================================================

static void appendTriangles(const vector<int>& faceVerts,
                             const vector<vec3>& vertices,
                             vec3 color,
                             vector<GLfloat>& vBuffer) {
    for (int i = 1; i + 1 < (int)faceVerts.size(); i++) {
        for (int vi : {faceVerts[0], faceVerts[i], faceVerts[i + 1]}) {
            vBuffer.push_back(vertices[vi].x);
            vBuffer.push_back(vertices[vi].y);
            vBuffer.push_back(vertices[vi].z);
            vBuffer.push_back(color.r);
            vBuffer.push_back(color.g);
            vBuffer.push_back(color.b);
        }
    }
}

static void normalizeToUnitCube(vector<GLfloat>& vBuffer) {
    const int STRIDE = 6;
    vec3 minV(FLT_MAX), maxV(-FLT_MAX);
    for (int i = 0; i < (int)vBuffer.size(); i += STRIDE) {
        minV.x = std::min(minV.x, vBuffer[i]);
        minV.y = std::min(minV.y, vBuffer[i + 1]);
        minV.z = std::min(minV.z, vBuffer[i + 2]);
        maxV.x = std::max(maxV.x, vBuffer[i]);
        maxV.y = std::max(maxV.y, vBuffer[i + 1]);
        maxV.z = std::max(maxV.z, vBuffer[i + 2]);
    }
    vec3 center = (minV + maxV) * 0.5f;
    float extent = max({maxV.x - minV.x, maxV.y - minV.y, maxV.z - minV.z});
    float inv = (extent > 0.0f) ? 1.0f / extent : 1.0f;
    for (int i = 0; i < (int)vBuffer.size(); i += STRIDE) {
        vBuffer[i]     = (vBuffer[i]     - center.x) * inv;
        vBuffer[i + 1] = (vBuffer[i + 1] - center.y) * inv;
        vBuffer[i + 2] = (vBuffer[i + 2] - center.z) * inv;
    }
}

static GLuint uploadToGPU(const vector<GLfloat>& vBuffer, int& nVertices) {
    const int STRIDE = 6;
    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, STRIDE * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, STRIDE * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    nVertices = (int)vBuffer.size() / STRIDE;
    return VAO;
}

GLuint loadOBJ(const string& filePath, int& nVertices, vec3 color) {
    vector<vec3> vertices;
    vector<GLfloat> vBuffer;
    ifstream file(filePath);
    if (!file.is_open()) {
        cerr << "Erro ao abrir arquivo: " << filePath << endl;
        return (GLuint)-1;
    }
    string line;
    while (getline(file, line)) {
        istringstream ss(line);
        string token;
        ss >> token;
        if (token == "v") {
            vec3 v;
            ss >> v.x >> v.y >> v.z;
            vertices.push_back(v);
        } else if (token == "f") {
            vector<int> faceVerts;
            while (ss >> token) {
                istringstream fs(token);
                string idx;
                int vi = 0;
                if (getline(fs, idx, '/')) vi = !idx.empty() ? stoi(idx) - 1 : 0;
                faceVerts.push_back(vi);
            }
            appendTriangles(faceVerts, vertices, color, vBuffer);
        }
    }
    normalizeToUnitCube(vBuffer);
    return uploadToGPU(vBuffer, nVertices);
}

// ============================================================================
// FUNÇÕES DE MODELO
// ============================================================================

ObjModel loadModel(const string& name, const string& filePath,
                   vec3 position, float scale, vec3 color) {
    ObjModel obj;
    obj.name       = name;
    obj.position   = position;
    obj.scale      = vec3(scale);
    obj.angleX = obj.angleY = obj.angleZ = 0.0f;
    obj.rotateX = obj.rotateY = obj.rotateZ = false;
    obj.vao        = loadOBJ(filePath, obj.nVertices, color);
    return obj;
}

static mat4 buildModelMatrix(const ObjModel& obj) {
    mat4 model(1.0f);
    model = translate(model, obj.position);
    model = rotate(model, obj.angleX, vec3(1.0f, 0.0f, 0.0f));
    model = rotate(model, obj.angleY, vec3(0.0f, 1.0f, 0.0f));
    model = rotate(model, obj.angleZ, vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, obj.scale);
    return model;
}

static void updateRotations(float deltaTime) {
    for (ObjModel& obj : models) {
        if (obj.rotateX) obj.angleX += ROTATE_SPEED * deltaTime;
        if (obj.rotateY) obj.angleY += ROTATE_SPEED * deltaTime;
        if (obj.rotateZ) obj.angleZ += ROTATE_SPEED * deltaTime;
    }
}

static void drawModels(GLint modelLoc) {
    for (int i = 0; i < (int)models.size(); i++) {
        mat4 m = buildModelMatrix(models[i]);
        if (i == selectedModel)
            m = glm::scale(m, vec3(1.05f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, value_ptr(m));
        glBindVertexArray(models[i].vao);
        glDrawArrays(GL_TRIANGLES, 0, models[i].nVertices);
        glBindVertexArray(0);
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT,
        "Vivencial M2 - Objetos 3D", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cerr << "Falha ao inicializar GLAD" << endl;
        return -1;
    }

    cout << "Renderer: " << glGetString(GL_RENDERER) << endl;
    cout << "OpenGL: "   << glGetString(GL_VERSION)  << endl;

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);
    glEnable(GL_DEPTH_TEST);

    GLuint shaderID = setupShader();
    glUseProgram(shaderID);
    GLint modelLoc = glGetUniformLocation(shaderID, "model");

    const string assetsDir = ASSETS_DIR;
    models.push_back(loadModel("Cubo",    assetsDir + "Cube.obj",    vec3(-0.5f, 0.0f, 0.0f), 0.4f, vec3(0.3f, 0.6f, 1.0f)));
    models.push_back(loadModel("Suzanne", assetsDir + "Suzanne.obj", vec3( 0.5f, 0.0f, 0.0f), 0.4f, vec3(1.0f, 0.6f, 0.2f)));

    printControls();

    float lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float now       = (float)glfwGetTime();
        float deltaTime = now - lastTime;
        lastTime        = now;

        glfwPollEvents();
        updateRotations(deltaTime);

        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        drawModels(modelLoc);
        glfwSwapBuffers(window);
    }

    for (ObjModel& obj : models) glDeleteVertexArrays(1, &obj.vao);
    glfwTerminate();
    return 0;
}

// ============================================================================
// CALLBACK DE TECLADO
// ============================================================================

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    bool shift = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)  == GLFW_PRESS ||
                  glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

    ObjModel& obj = models[selectedModel];

    switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GL_TRUE);
            break;

        case GLFW_KEY_TAB:
            selectedModel = (selectedModel + 1) % (int)models.size();
            cout << "Selecionado: " << models[selectedModel].name << endl;
            printModeHelp();
            break;

        case GLFW_KEY_R:
            currentMode = Mode::ROTATE;
            printModeHelp();
            break;

        case GLFW_KEY_T:
            currentMode = Mode::TRANSLATE;
            printModeHelp();
            break;

        case GLFW_KEY_M:
            currentMode = Mode::SCALE;
            printModeHelp();
            break;

        // Eixo X — rotação, translação ou escala dependendo do modo
        case GLFW_KEY_X:
            if (currentMode == Mode::ROTATE)
                obj.rotateX = !obj.rotateX;
            else if (currentMode == Mode::SCALE)
                obj.scale.x = std::max(SCALE_MIN, obj.scale.x + (shift ? -SCALE_STEP : SCALE_STEP));
            break;

        case GLFW_KEY_Y:
            if (currentMode == Mode::ROTATE)
                obj.rotateY = !obj.rotateY;
            else if (currentMode == Mode::SCALE)
                obj.scale.y = std::max(SCALE_MIN, obj.scale.y + (shift ? -SCALE_STEP : SCALE_STEP));
            break;

        case GLFW_KEY_Z:
            if (currentMode == Mode::ROTATE)
                obj.rotateZ = !obj.rotateZ;
            else if (currentMode == Mode::SCALE)
                obj.scale.z = std::max(SCALE_MIN, obj.scale.z + (shift ? -SCALE_STEP : SCALE_STEP));
            break;

        // Escala uniforme (modo escala)
        case GLFW_KEY_U:
            if (currentMode == Mode::SCALE) {
                float delta = shift ? -SCALE_STEP : SCALE_STEP;
                obj.scale.x = std::max(SCALE_MIN, obj.scale.x + delta);
                obj.scale.y = std::max(SCALE_MIN, obj.scale.y + delta);
                obj.scale.z = std::max(SCALE_MIN, obj.scale.z + delta);
            }
            break;

        // Translação (modo translação)
        case GLFW_KEY_A:
        case GLFW_KEY_LEFT:
            if (currentMode == Mode::TRANSLATE) obj.position.x -= TRANSLATE_STEP;
            break;
        case GLFW_KEY_D:
        case GLFW_KEY_RIGHT:
            if (currentMode == Mode::TRANSLATE) obj.position.x += TRANSLATE_STEP;
            break;
        case GLFW_KEY_I:
        case GLFW_KEY_UP:
            if (currentMode == Mode::TRANSLATE) obj.position.y += TRANSLATE_STEP;
            break;
        case GLFW_KEY_J:
        case GLFW_KEY_DOWN:
            if (currentMode == Mode::TRANSLATE) obj.position.y -= TRANSLATE_STEP;
            break;
        case GLFW_KEY_W:
            if (currentMode == Mode::TRANSLATE) obj.position.z -= TRANSLATE_STEP;
            break;
        case GLFW_KEY_S:
            if (currentMode == Mode::TRANSLATE) obj.position.z += TRANSLATE_STEP;
            break;

        default:
            break;
    }
}

// ============================================================================
// GUIA DE CONTROLES
// ============================================================================

void printModeHelp() {
    switch (currentMode) {
        case Mode::ROTATE:
            cout << "[Modo ROTACAO] X/Y/Z: ativar/desativar rotacao continua no eixo\n";
            break;
        case Mode::TRANSLATE:
            cout << "[Modo TRANSLACAO] A/D ou setas: eixo X | W/S: eixo Z | I/J ou setas: eixo Y\n";
            break;
        case Mode::SCALE:
            cout << "[Modo ESCALA] X/Y/Z: escala por eixo | U: escala uniforme | Shift: diminuir\n";
            break;
    }
}

void printControls() {
    cout << "\n========================================\n"
         << " VIVENCIAL M2 - Selecao e Transformacoes\n"
         << "----------------------------------------\n"
         << " TAB   : Alternar objeto selecionado\n"
         << " R     : Modo Rotacao\n"
         << " T     : Modo Translacao\n"
         << " M     : Modo Escala\n"
         << " ESC   : Sair\n"
         << "========================================\n";
    printModeHelp();
    cout << endl;
}
