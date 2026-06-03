/*
 * Desafio M5 - Camera em Primeira Pessoa
 * Modulo 5 - Computacao Grafica
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

// Luz pontual fixa
const vec3 LIGHT_POS   = vec3(2.0f, 2.0f, 2.0f);
const vec3 LIGHT_COLOR = vec3(1.0f, 1.0f, 1.0f);

// ============================================================================
// CLASSE CAMERA (Primeira Pessoa)
// ============================================================================

enum CameraMovement { FORWARD, BACKWARD, LEFT, RIGHT };

struct Camera {
    vec3 position;
    vec3 front;
    vec3 up;
    vec3 right;
    vec3 worldUp;
    float yaw;
    float pitch;
    float movementSpeed;
    float mouseSensitivity;

    Camera(vec3 pos = vec3(0.0f, 0.0f, 3.0f),
           vec3 worldUpVec = vec3(0.0f, 1.0f, 0.0f),
           float yawDeg = -90.0f, float pitchDeg = 0.0f)
        : position(pos), front(vec3(0.0f, 0.0f, -1.0f)), up(worldUpVec), right(0.0f),
          worldUp(worldUpVec), yaw(yawDeg), pitch(pitchDeg),
          movementSpeed(2.5f), mouseSensitivity(0.1f) {
        updateCameraVectors();
    }

    mat4 getViewMatrix() const {
        return lookAt(position, position + front, up);
    }

    void processKeyboard(CameraMovement dir, float dt) {
        float v = movementSpeed * dt;
        if (dir == FORWARD)  position += front * v;
        if (dir == BACKWARD) position -= front * v;
        if (dir == LEFT)     position -= right * v;
        if (dir == RIGHT)    position += right * v;
    }

    void processMouseMovement(float dx, float dy, bool constrainPitch = true) {
        yaw   += dx * mouseSensitivity;
        pitch += dy * mouseSensitivity;
        if (constrainPitch) pitch = std::clamp(pitch, -89.0f, 89.0f);
        updateCameraVectors();
    }

    void updateCameraVectors() {
        vec3 dir;
        dir.x = cos(radians(yaw)) * cos(radians(pitch));
        dir.y = sin(radians(pitch));
        dir.z = sin(radians(yaw)) * cos(radians(pitch));
        front = normalize(dir);
        right = normalize(cross(front, worldUp));
        up    = normalize(cross(right, front));
    }
};

// Instancia global da camera (posicionada para enxergar a Suzanne na origem)
Camera camera(vec3(0.0f, 0.0f, 3.0f));

// Estado do mouse (delta entre frames)
float lastMouseX = WIDTH  / 2.0f;
float lastMouseY = HEIGHT / 2.0f;
bool  firstMouse = true;

// ============================================================================
// MODO DE TRANSFORMACAO
// ============================================================================

enum class Mode { ROTATE, TRANSLATE, SCALE };

Mode currentMode = Mode::ROTATE;

// ============================================================================
// ESTRUTURAS
// ============================================================================

struct Material {
    string mapKd;                  // textura difusa
    vec3   ka = vec3(0.1f);        // ambiente
    vec3   kd = vec3(0.8f);        // difuso (default cobre Suzanne.mtl, que nao tem Kd)
    vec3   ks = vec3(0.5f);        // especular
    float  ns = 32.0f;             // shininess (expoente especular)
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
    // Coeficientes Phong vindos do MTL
    vec3   ka, kd, ks;
    float  ns;
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
layout (location = 3) in vec3 normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 vColor;
out vec2 vTexCoord;
out vec3 vWorldPos;
out vec3 vWorldNormal;

void main() {
    vec4 wp      = model * vec4(position, 1.0);
    vWorldPos    = wp.xyz;
    // Matriz de normais para suportar escala nao-uniforme
    vWorldNormal = mat3(transpose(inverse(model))) * normal;
    vColor       = color;
    vTexCoord    = texCoord;
    gl_Position  = projection * view * wp;
}
)";

const GLchar* fragmentShaderSource = R"(
#version 410
in vec3 vColor;
in vec2 vTexCoord;
in vec3 vWorldPos;
in vec3 vWorldNormal;

uniform sampler2D tex_buffer;

// Coeficientes Phong (do MTL)
uniform vec3  ka;
uniform vec3  kd;
uniform vec3  ks;
uniform float ns;

// Fonte de luz pontual
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

out vec4 color;

void main() {
    vec3 N = normalize(vWorldNormal);
    vec3 L = normalize(lightPos - vWorldPos);
    vec3 V = normalize(viewPos  - vWorldPos);
    vec3 R = reflect(-L, N);

    vec3 ambient  = ka * lightColor;
    vec3 diffuse  = kd * max(dot(N, L), 0.0) * lightColor;
    // max(ns,1.0) evita pow(0,0) indefinido
    vec3 specular = ks * pow(max(dot(R, V), 0.0), max(ns, 1.0)) * lightColor;

    vec3 phong = ambient + diffuse + specular;
    vec3 tex   = texture(tex_buffer, vTexCoord).rgb;
    color = vec4(phong * tex * vColor, 1.0);
}
)";

// ============================================================================
// PROTOTIPOS
// ============================================================================

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
void processCameraInput(GLFWwindow* window, float deltaTime);
GLuint setupShader();
Material parseMTL(const string& filePath);
GLuint loadTexture(const string& filePath);
GLuint loadOBJ(const string& filePath, int& nVertices, Material& outMat, vec3 color);
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
// PARSER MTL (Ka, Kd, Ks, Ns, map_Kd)
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
        if (token == "Ka") {
            ss >> mat.ka.r >> mat.ka.g >> mat.ka.b;
        } else if (token == "Kd") {
            ss >> mat.kd.r >> mat.kd.g >> mat.kd.b;
        } else if (token == "Ks") {
            ss >> mat.ks.r >> mat.ks.g >> mat.ks.b;
        } else if (token == "Ns") {
            ss >> mat.ns;
        } else if (token == "map_Kd") {
            ss >> mat.mapKd;
        }
    }
    return mat;
}

// ============================================================================
// CARREGAMENTO DE TEXTURA
// ============================================================================

// Textura RGBA 1x1 branca usada como fallback para modelos sem map_Kd.
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
// CARREGAMENTO DO OBJ (com vt, vn, mtllib e f v/vt/vn)
// ============================================================================

static void appendTriangles(const vector<int>& faceVerts,
                             const vector<int>& faceTex,
                             const vector<int>& faceNorm,
                             const vector<vec3>& vertices,
                             const vector<vec2>& texCoords,
                             const vector<vec3>& normals,
                             vec3 color,
                             vector<GLfloat>& vBuffer) {
    // Triangulacao em leque (fan): v0, vi, vi+1
    for (int i = 1; i + 1 < (int)faceVerts.size(); i++) {
        int idx[3] = {0, i, i + 1};
        for (int k = 0; k < 3; k++) {
            int vi = faceVerts[idx[k]];
            int ti = faceTex[idx[k]];
            int ni = faceNorm[idx[k]];
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
            // Normal (default (0,0,1) quando ausente)
            if (ni >= 0 && ni < (int)normals.size()) {
                vBuffer.push_back(normals[ni].x);
                vBuffer.push_back(normals[ni].y);
                vBuffer.push_back(normals[ni].z);
            } else {
                vBuffer.push_back(0.0f);
                vBuffer.push_back(0.0f);
                vBuffer.push_back(1.0f);
            }
        }
    }
}

static void normalizeToUnitCube(vector<GLfloat>& vBuffer) {
    const int STRIDE = 11;
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
    const int STRIDE = 11;
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
    // Normal (location 3): 3 floats, offset 8*float
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, STRIDE * sizeof(GLfloat), (void*)(8 * sizeof(GLfloat)));
    glEnableVertexAttribArray(3);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    nVertices = (int)vBuffer.size() / STRIDE;
    return VAO;
}

// Parseia um token "v/vt/vn" (qualquer parte pode estar ausente: v, v/vt, v//vn).
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

GLuint loadOBJ(const string& filePath, int& nVertices, Material& outMat, vec3 color) {
    vector<vec3> vertices;
    vector<vec2> texCoords;
    vector<vec3> normals;
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
            outMat = parseMTL(baseDir + mtlName);
        } else if (token == "v") {
            vec3 v;
            ss >> v.x >> v.y >> v.z;
            vertices.push_back(v);
        } else if (token == "vt") {
            vec2 t;
            ss >> t.x >> t.y;
            texCoords.push_back(t);
        } else if (token == "vn") {
            vec3 n;
            ss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        } else if (token == "f") {
            vector<int> faceVerts, faceTex, faceNorm;
            string tok;
            while (ss >> tok) {
                int vi, ti, ni;
                parseFaceToken(tok, vi, ti, ni);
                faceVerts.push_back(vi);
                faceTex.push_back(ti);
                faceNorm.push_back(ni);
            }
            appendTriangles(faceVerts, faceTex, faceNorm, vertices, texCoords, normals, color, vBuffer);
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

    Material mat;
    obj.vao = loadOBJ(objPath, obj.nVertices, mat, tint);
    obj.texID = mat.mapKd.empty() ? fallbackTex : loadTexture(dirOf(objPath) + mat.mapKd);

    obj.ka = mat.ka;
    obj.kd = mat.kd;
    obj.ks = mat.ks;
    obj.ns = mat.ns;
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

static void drawModels(GLuint shaderID, GLint modelLoc) {
    GLint kaLoc = glGetUniformLocation(shaderID, "ka");
    GLint kdLoc = glGetUniformLocation(shaderID, "kd");
    GLint ksLoc = glGetUniformLocation(shaderID, "ks");
    GLint nsLoc = glGetUniformLocation(shaderID, "ns");

    for (int i = 0; i < (int)models.size(); i++) {
        mat4 m = buildModelMatrix(models[i]);
        if (i == selectedModel)
            m = glm::scale(m, vec3(1.05f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, value_ptr(m));

        // Coeficientes Phong por modelo
        glUniform3fv(kaLoc, 1, value_ptr(models[i].ka));
        glUniform3fv(kdLoc, 1, value_ptr(models[i].kd));
        glUniform3fv(ksLoc, 1, value_ptr(models[i].ks));
        glUniform1f(nsLoc, models[i].ns);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, models[i].texID);
        glBindVertexArray(models[i].vao);
        glDrawArrays(GL_TRIANGLES, 0, models[i].nVertices);
        glBindVertexArray(0);
    }
}

// ============================================================================
// INPUT DA CAMERA (polling de WASD a cada frame)
// ============================================================================

void processCameraInput(GLFWwindow* window, float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.processKeyboard(FORWARD,  deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.processKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.processKeyboard(LEFT,     deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.processKeyboard(RIGHT,    deltaTime);
}

// ============================================================================
// CALLBACK DO MOUSE (delta -> yaw/pitch)
// ============================================================================

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        // Evita o salto inicial gigantesco quando o cursor entra na janela
        lastMouseX = (float)xpos;
        lastMouseY = (float)ypos;
        firstMouse = false;
    }
    float dx = (float)xpos - lastMouseX;
    float dy = lastMouseY - (float)ypos;  // Y invertido (origem do GLFW e o topo)
    lastMouseX = (float)xpos;
    lastMouseY = (float)ypos;
    camera.processMouseMovement(dx, dy);
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
        "Desafio M5 - Camera Primeira Pessoa", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    // Cursor capturado: invisivel e preso ao centro -> estilo FPS
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

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

    // Localizacoes uniformes
    GLint modelLoc      = glGetUniformLocation(shaderID, "model");
    GLint viewLoc       = glGetUniformLocation(shaderID, "view");
    GLint projLoc       = glGetUniformLocation(shaderID, "projection");
    GLint lightPosLoc   = glGetUniformLocation(shaderID, "lightPos");
    GLint lightColorLoc = glGetUniformLocation(shaderID, "lightColor");
    GLint viewPosLoc    = glGetUniformLocation(shaderID, "viewPos");

    // Projecao e luz sao constantes; view e viewPos variam com a camera
    mat4 projection = perspective(radians(45.0f),
                                  (float)WIDTH / (float)HEIGHT,
                                  0.1f, 100.0f);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, value_ptr(projection));
    glUniform3fv(lightPosLoc,   1, value_ptr(LIGHT_POS));
    glUniform3fv(lightColorLoc, 1, value_ptr(LIGHT_COLOR));

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
        processCameraInput(window, deltaTime);
        updateRotations(deltaTime);

        // View e viewPos atualizados a cada frame com a camera
        glUniformMatrix4fv(viewLoc,    1, GL_FALSE, value_ptr(camera.getViewMatrix()));
        glUniform3fv      (viewPosLoc, 1,            value_ptr(camera.position));

        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        drawModels(shaderID, modelLoc);
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
// CALLBACK DE TECLADO (transformacao de objeto - WASD pertencem a camera)
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

        // Translacao do objeto: setas + I/J (WASD agora pertencem a camera)
        case GLFW_KEY_LEFT:
            if (currentMode == Mode::TRANSLATE) obj.position.x -= TRANSLATE_STEP;
            break;
        case GLFW_KEY_RIGHT:
            if (currentMode == Mode::TRANSLATE) obj.position.x += TRANSLATE_STEP;
            break;
        case GLFW_KEY_UP:
            if (currentMode == Mode::TRANSLATE) obj.position.y += TRANSLATE_STEP;
            break;
        case GLFW_KEY_DOWN:
            if (currentMode == Mode::TRANSLATE) obj.position.y -= TRANSLATE_STEP;
            break;
        case GLFW_KEY_I:
            if (currentMode == Mode::TRANSLATE) obj.position.z -= TRANSLATE_STEP;
            break;
        case GLFW_KEY_J:
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
            cout << "[Modo TRANSLACAO] Setas: eixo X/Y | I/J: eixo Z\n";
            break;
        case Mode::SCALE:
            cout << "[Modo ESCALA] X/Y/Z: escala por eixo | U: escala uniforme | Shift: diminuir\n";
            break;
    }
}

void printControls() {
    cout << "\n========================================\n"
         << " DESAFIO M5 - Camera Primeira Pessoa\n"
         << "----------------------------------------\n"
         << " W/A/S/D : Mover camera\n"
         << " Mouse   : Rotacionar camera\n"
         << " TAB     : Alternar objeto selecionado\n"
         << " R       : Modo Rotacao\n"
         << " T       : Modo Translacao\n"
         << " M       : Modo Escala\n"
         << " ESC     : Sair\n"
         << "========================================\n";
    printModeHelp();
    cout << endl;
}
