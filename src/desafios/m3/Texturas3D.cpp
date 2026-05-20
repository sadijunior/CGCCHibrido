/*
 * Desafio M3 - Adicionando Texturas
 * Modulo 3 - Computacao Grafica
 */

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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
// MODO DE TRANSFORMACAO
// ============================================================================

enum class Mode { ROTATE, TRANSLATE, SCALE };

Mode currentMode = Mode::ROTATE;

// ============================================================================
// ESTRUTURAS
// ============================================================================

struct Material {
    string mapKd;  // nome do arquivo de textura difusa (vazio = sem textura)
};

struct ObjModel {
    string name;
    GLuint vao;
    GLuint texID;       // 0 quando o modelo nao tem textura
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
layout (location = 2) in vec2 texCoord;
uniform mat4 model;
out vec3 vColor;
out vec2 vTexCoord;
void main() {
    gl_Position = model * vec4(position, 1.0);
    vColor = color;
    vTexCoord = texCoord;
}
)";

const GLchar* fragmentShaderSource = R"(
#version 410
in vec3 vColor;
in vec2 vTexCoord;
uniform sampler2D tex_buffer;
out vec4 color;
void main() {
    color = texture(tex_buffer, vTexCoord) * vec4(vColor, 1.0);
}
)";

// ============================================================================
// PROTOTIPOS
// ============================================================================

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
GLuint setupShader();
Material parseMTL(const string& filePath);
GLuint loadTexture(const string& filePath);
GLuint loadOBJ(const string& filePath, int& nVertices, string& outMapKd, vec3 color);
GLuint createWhiteTexture();
ObjModel loadModel(const string& name, const string& objPath, vec3 position, float scale, vec3 tint, GLuint fallbackTex);
void printControls();
void printModeHelp();

// ============================================================================
// COMPILACAO DE SHADER
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
// HELPERS DE PATH
// ============================================================================

static string dirOf(const string& path) {
    size_t pos = path.find_last_of("/\\");
    return (pos == string::npos) ? "" : path.substr(0, pos + 1);
}

// ============================================================================
// PARSER MTL (apenas map_Kd)
// ============================================================================

Material parseMTL(const string& filePath) {
    Material mat;
    ifstream file(filePath);
    if (!file.is_open()) {
        cerr << "Aviso: nao foi possivel abrir MTL: " << filePath << endl;
        return mat;
    }
    string line;
    while (getline(file, line)) {
        istringstream ss(line);
        string token;
        ss >> token;
        if (token == "map_Kd") {
            ss >> mat.mapKd;
        }
    }
    return mat;
}

// ============================================================================
// CARREGAMENTO DE TEXTURA
// ============================================================================

// Textura RGBA 1x1 branca usada como fallback para modelos sem map_Kd.
// Garante que o sampler do shader sempre tenha uma textura valida ligada
// (Apple/Metal reclama com "GLD_TEXTURE_INDEX_2D is unloadable" caso contrario)
// e faz o vColor agir como cor solida (branco * tint = tint).
GLuint createWhiteTexture() {
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    unsigned char white[4] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texID;
}

GLuint loadTexture(const string& filePath) {
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Blender exporta UVs com origem inferior; stb carrega com origem superior.
    stbi_set_flip_vertically_on_load(true);

    int width, height, nChannels;
    unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &nChannels, 0);
    if (data) {
        GLenum format = (nChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        cout << "Textura carregada: " << filePath << " (" << width << "x" << height
             << ", " << nChannels << " canais)" << endl;
    } else {
        cerr << "Falha ao carregar textura: " << filePath << endl;
    }
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texID;
}

// ============================================================================
// CARREGAMENTO DO OBJ (com vt, mtllib e f v/vt/vn)
// ============================================================================

static void appendTriangles(const vector<int>& faceVerts,
                             const vector<int>& faceTex,
                             const vector<vec3>& vertices,
                             const vector<vec2>& texCoords,
                             vec3 color,
                             vector<GLfloat>& vBuffer) {
    // Triangulacao em leque (fan): v0, vi, vi+1
    for (int i = 1; i + 1 < (int)faceVerts.size(); i++) {
        int idx[3] = {0, i, i + 1};
        for (int k = 0; k < 3; k++) {
            int vi = faceVerts[idx[k]];
            int ti = faceTex[idx[k]];
            // Posicao
            vBuffer.push_back(vertices[vi].x);
            vBuffer.push_back(vertices[vi].y);
            vBuffer.push_back(vertices[vi].z);
            // Cor (tint inerte por padrao)
            vBuffer.push_back(color.r);
            vBuffer.push_back(color.g);
            vBuffer.push_back(color.b);
            // Coordenada de textura (0,0 quando ausente)
            if (ti >= 0 && ti < (int)texCoords.size()) {
                vBuffer.push_back(texCoords[ti].x);
                vBuffer.push_back(texCoords[ti].y);
            } else {
                vBuffer.push_back(0.0f);
                vBuffer.push_back(0.0f);
            }
        }
    }
}

static void normalizeToUnitCube(vector<GLfloat>& vBuffer) {
    const int STRIDE = 8;
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
    float extent = std::max({maxV.x - minV.x, maxV.y - minV.y, maxV.z - minV.z});
    float inv = (extent > 0.0f) ? 1.0f / extent : 1.0f;
    for (int i = 0; i < (int)vBuffer.size(); i += STRIDE) {
        vBuffer[i]     = (vBuffer[i]     - center.x) * inv;
        vBuffer[i + 1] = (vBuffer[i + 1] - center.y) * inv;
        vBuffer[i + 2] = (vBuffer[i + 2] - center.z) * inv;
    }
}

static GLuint uploadToGPU(const vector<GLfloat>& vBuffer, int& nVertices) {
    const int STRIDE = 8;
    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    // Posicao (location 0): 3 floats, offset 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, STRIDE * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    // Cor (location 1): 3 floats, offset 3*float
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, STRIDE * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    // TexCoord (location 2): 2 floats, offset 6*float
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, STRIDE * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    nVertices = (int)vBuffer.size() / STRIDE;
    return VAO;
}

// Parseia um token "v/vt/vn" (qualquer parte pode estar ausente: v, v/vt, v//vn).
// Retorna indices 0-based; -1 quando ausente.
static void parseFaceToken(const string& tok, int& vi, int& ti, int& ni) {
    vi = ti = ni = -1;
    istringstream fs(tok);
    string idx;
    int slot = 0;
    while (getline(fs, idx, '/')) {
        if (!idx.empty()) {
            int v = stoi(idx) - 1;
            if (slot == 0) vi = v;
            else if (slot == 1) ti = v;
            else if (slot == 2) ni = v;
        }
        slot++;
    }
}

GLuint loadOBJ(const string& filePath, int& nVertices, string& outMapKd, vec3 color) {
    vector<vec3> vertices;
    vector<vec2> texCoords;
    vector<GLfloat> vBuffer;
    string baseDir = dirOf(filePath);

    ifstream file(filePath);
    if (!file.is_open()) {
        cerr << "Erro ao abrir OBJ: " << filePath << endl;
        return (GLuint)-1;
    }
    string line;
    while (getline(file, line)) {
        istringstream ss(line);
        string token;
        ss >> token;
        if (token == "mtllib") {
            string mtlName;
            ss >> mtlName;
            Material mat = parseMTL(baseDir + mtlName);
            outMapKd = mat.mapKd;
        } else if (token == "v") {
            vec3 v;
            ss >> v.x >> v.y >> v.z;
            vertices.push_back(v);
        } else if (token == "vt") {
            vec2 t;
            ss >> t.x >> t.y;
            texCoords.push_back(t);
        } else if (token == "f") {
            vector<int> faceVerts, faceTex;
            string tok;
            while (ss >> tok) {
                int vi, ti, ni;
                parseFaceToken(tok, vi, ti, ni);
                faceVerts.push_back(vi);
                faceTex.push_back(ti);
            }
            appendTriangles(faceVerts, faceTex, vertices, texCoords, color, vBuffer);
        }
    }
    normalizeToUnitCube(vBuffer);
    return uploadToGPU(vBuffer, nVertices);
}

// ============================================================================
// MODELO
// ============================================================================

ObjModel loadModel(const string& name, const string& objPath,
                   vec3 position, float scale, vec3 tint, GLuint fallbackTex) {
    ObjModel obj;
    obj.name     = name;
    obj.position = position;
    obj.scale    = vec3(scale);
    obj.angleX = obj.angleY = obj.angleZ = 0.0f;
    obj.rotateX = obj.rotateY = obj.rotateZ = false;

    string mapKd;
    obj.vao = loadOBJ(objPath, obj.nVertices, mapKd, tint);
    obj.texID = mapKd.empty() ? fallbackTex : loadTexture(dirOf(objPath) + mapKd);
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
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, models[i].texID);
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
        "Desafio M3 - Texturas", nullptr, nullptr);
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
    glUniform1i(glGetUniformLocation(shaderID, "tex_buffer"), 0);
    GLint modelLoc = glGetUniformLocation(shaderID, "model");

    const string assetsDir = ASSETS_DIR;
    GLuint whiteTex = createWhiteTexture();
    models.push_back(loadModel("Suzanne", assetsDir + "Suzanne.obj", vec3(0.0f), 0.5f, vec3(1.0f), whiteTex));

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

    for (ObjModel& obj : models) {
        glDeleteVertexArrays(1, &obj.vao);
        if (obj.texID && obj.texID != whiteTex) glDeleteTextures(1, &obj.texID);
    }
    glDeleteTextures(1, &whiteTex);
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

        case GLFW_KEY_U:
            if (currentMode == Mode::SCALE) {
                float delta = shift ? -SCALE_STEP : SCALE_STEP;
                obj.scale.x = std::max(SCALE_MIN, obj.scale.x + delta);
                obj.scale.y = std::max(SCALE_MIN, obj.scale.y + delta);
                obj.scale.z = std::max(SCALE_MIN, obj.scale.z + delta);
            }
            break;

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
         << " DESAFIO M3 - Texturas\n"
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
