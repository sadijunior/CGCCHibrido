/*
 * Trabalho GB - Minecraft GB
 * Computacao Grafica - Integracao do Grau B
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

const GLuint WIDTH = 1024, HEIGHT = 768;
const int SHADER_LOG_SIZE = 512;
const float ROTATE_SPEED   = 1.0f;
const float TRANSLATE_STEP = 0.1f;
const float SCALE_STEP     = 0.05f;
const float SCALE_MIN      = 0.05f;

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

// Instancia global da camera (sera ajustada pelo arquivo de cena)
Camera camera(vec3(0.0f, 0.5f, 5.0f));

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
// SISTEMA DE TRAJETORIA (Bezier cubica - 4 pontos de controle)
// ============================================================================

struct Trajectory {
    vec3  p0 = vec3(0.0f);
    vec3  p1 = vec3(0.0f);
    vec3  p2 = vec3(0.0f);
    vec3  p3 = vec3(0.0f);
    float t       = 0.0f;
    float speed   = 0.5f;
    bool  enabled = false;
    bool  loaded  = false;

    void load(const string& filePath) {
        ifstream file(filePath);
        if (!file.is_open()) {
            cerr << "Aviso: trajetoria nao encontrada: " << filePath << endl;
            return;
        }
        vector<vec3> pts;
        string line;
        while (getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            istringstream ss(line);
            vec3 p;
            if (ss >> p.x >> p.y >> p.z) pts.push_back(p);
        }
        if (pts.size() >= 4) {
            p0 = pts[0]; p1 = pts[1]; p2 = pts[2]; p3 = pts[3];
            loaded = true;
            cout << "Trajetoria Bezier carregada: " << filePath
                 << " (4 pontos de controle)" << endl;
        } else {
            cerr << "Aviso: trajetoria precisa de pelo menos 4 pontos: "
                 << filePath << " (" << pts.size() << ")" << endl;
        }
    }

    // B(u) = (1-u)^3 P0 + 3(1-u)^2 u P1 + 3(1-u) u^2 P2 + u^3 P3
    vec3 position(float u) const {
        float u2 = u * u, u3 = u2 * u;
        float v  = 1.0f - u, v2 = v * v, v3 = v2 * v;
        return v3 * p0 + 3.0f * v2 * u * p1 + 3.0f * v * u2 * p2 + u3 * p3;
    }

    // B'(u) = 3(1-u)^2 (P1-P0) + 6(1-u) u (P2-P1) + 3 u^2 (P3-P2)
    vec3 derivative(float u) const {
        float v = 1.0f - u;
        return 3.0f * v * v * (p1 - p0)
             + 6.0f * v * u * (p2 - p1)
             + 3.0f * u * u * (p3 - p2);
    }

    // Reparametrizacao por arc-length aproximada: avanca t inversamente
    // proporcional a magnitude do tangente, mantendo velocidade ~constante.
    vec3 update(float deltaTime) {
        if (!loaded) return p0;
        vec3 deriv = derivative(t);
        float speedParam = length(deriv);
        if (speedParam > 1e-5f) t += (speed / speedParam) * deltaTime;
        if (t >= 1.0f) t -= 1.0f;
        return position(t);
    }
};

// ============================================================================
// ILUMINACAO PHONG DE 3 PONTOS (Key / Fill / Back)
// ============================================================================

struct Light {
    vec3  position  = vec3(0.0f);
    vec3  color     = vec3(1.0f);
    float intensity = 1.0f;
    bool  on        = true;
};

Light keyLight;
Light fillLight;
Light backLight;

// ============================================================================
// ESTRUTURAS DE MODELO
// ============================================================================

struct Material {
    string mapKd;
    vec3   ka = vec3(0.1f);
    vec3   kd = vec3(0.8f);
    vec3   ks = vec3(0.5f);
    float  ns = 32.0f;
};

struct ObjModel {
    string name;
    GLuint vao = 0;
    GLuint texID = 0;
    int    nVertices = 0;
    vec3   position  = vec3(0.0f);
    vec3   scale     = vec3(1.0f);
    float  angleX = 0.0f, angleY = 0.0f, angleZ = 0.0f;
    bool   rotateX = false, rotateY = false, rotateZ = false;
    vec3   ka = vec3(0.1f), kd = vec3(0.8f), ks = vec3(0.5f);
    float  ns = 32.0f;
    bool   selectable = true;   // false para cenario (FlatWorld)
    Trajectory trajectory;
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
    vWorldNormal = mat3(transpose(inverse(model))) * normal;
    vColor       = color;
    vTexCoord    = texCoord;
    gl_Position  = projection * view * wp;
}
)";

// Fragment shader Phong com 3 luzes individualmente toggleaveis.
// Cada luz contribui com componente difusa + especular; ambiente e somado uma vez.
const GLchar* fragmentShaderSource = R"(
#version 410
in vec3 vColor;
in vec2 vTexCoord;
in vec3 vWorldPos;
in vec3 vWorldNormal;

uniform sampler2D tex_buffer;

uniform vec3  ka;
uniform vec3  kd;
uniform vec3  ks;
uniform float ns;

uniform vec3  viewPos;

uniform vec3  lightPos1, lightPos2, lightPos3;
uniform vec3  lightColor1, lightColor2, lightColor3;
uniform float lightInt1, lightInt2, lightInt3;
uniform bool  lightOn1, lightOn2, lightOn3;

out vec4 color;

vec3 phongContribution(vec3 lpos, vec3 lcolor, float lint, vec3 N, vec3 V) {
    vec3 L = normalize(lpos - vWorldPos);
    vec3 R = reflect(-L, N);
    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(R, V), 0.0), max(ns, 1.0));
    return (kd * diff + ks * spec) * lcolor * lint;
}

void main() {
    vec3 N = normalize(vWorldNormal);
    vec3 V = normalize(viewPos - vWorldPos);

    // Ambiente baseado na cor media das luzes ativas (fallback branca)
    vec3 ambientColor = vec3(0.0);
    int activeLights = 0;
    if (lightOn1) { ambientColor += lightColor1; activeLights++; }
    if (lightOn2) { ambientColor += lightColor2; activeLights++; }
    if (lightOn3) { ambientColor += lightColor3; activeLights++; }
    if (activeLights == 0) ambientColor = vec3(1.0);
    else                   ambientColor /= float(activeLights);

    vec3 result = ka * ambientColor;
    if (lightOn1) result += phongContribution(lightPos1, lightColor1, lightInt1, N, V);
    if (lightOn2) result += phongContribution(lightPos2, lightColor2, lightInt2, N, V);
    if (lightOn3) result += phongContribution(lightPos3, lightColor3, lightInt3, N, V);

    vec3 tex = texture(tex_buffer, vTexCoord).rgb;
    color = vec4(result * tex * vColor, 1.0);
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
ObjModel loadModel(const string& name, const string& objPath, vec3 position,
                   vec3 rotation, float scale, const string& trajFile,
                   bool selectable, GLuint fallbackTex);
bool loadScene(const string& scenePath, GLuint fallbackTex);
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
    cout << "MTL " << filePath << " => Ka(" << mat.ka.r << "," << mat.ka.g << "," << mat.ka.b
         << ") Kd(" << mat.kd.r << "," << mat.kd.g << "," << mat.kd.b
         << ") Ks(" << mat.ks.r << "," << mat.ks.g << "," << mat.ks.b
         << ") Ns=" << mat.ns << " map_Kd=" << mat.mapKd << endl;
    return mat;
}

// ============================================================================
// CARREGAMENTO DE TEXTURA
// ============================================================================

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
    // Nearest filter preserva o estilo pixel-art Minecraft
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

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
// CARREGAMENTO DO OBJ
// ============================================================================

static void appendTriangles(const vector<int>& faceVerts,
                             const vector<int>& faceTex,
                             const vector<int>& faceNorm,
                             const vector<vec3>& vertices,
                             const vector<vec2>& texCoords,
                             const vector<vec3>& normals,
                             vec3 color,
                             vector<GLfloat>& vBuffer) {
    for (int i = 1; i + 1 < (int)faceVerts.size(); i++) {
        int idx[3] = {0, i, i + 1};
        for (int k = 0; k < 3; k++) {
            int vi = faceVerts[idx[k]];
            int ti = faceTex[idx[k]];
            int ni = faceNorm[idx[k]];
            vBuffer.push_back(vertices[vi].x);
            vBuffer.push_back(vertices[vi].y);
            vBuffer.push_back(vertices[vi].z);
            vBuffer.push_back(color.r);
            vBuffer.push_back(color.g);
            vBuffer.push_back(color.b);
            if (ti >= 0 && ti < (int)texCoords.size()) {
                vBuffer.push_back(texCoords[ti].x);
                vBuffer.push_back(texCoords[ti].y);
            } else {
                vBuffer.push_back(0.0f);
                vBuffer.push_back(0.0f);
            }
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

// Centraliza os vertices na origem e normaliza para caber em um cubo unitario.
// Necessario porque os modelos vindos do anyconv tem extents +-8 em coords de mundo.
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, STRIDE * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, STRIDE * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, STRIDE * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, STRIDE * sizeof(GLfloat), (void*)(8 * sizeof(GLfloat)));
    glEnableVertexAttribArray(3);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    nVertices = (int)vBuffer.size() / STRIDE;
    return VAO;
}

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
                   vec3 position, vec3 rotation, float scale,
                   const string& trajFile, bool selectable, GLuint fallbackTex) {
    ObjModel obj;
    obj.name       = name;
    obj.position   = position;
    obj.scale      = vec3(scale);
    obj.angleX     = rotation.x;
    obj.angleY     = rotation.y;
    obj.angleZ     = rotation.z;
    obj.selectable = selectable;

    Material mat;
    obj.vao   = loadOBJ(objPath, obj.nVertices, mat, vec3(1.0f));
    obj.texID = mat.mapKd.empty() ? fallbackTex : loadTexture(dirOf(objPath) + mat.mapKd);

    obj.ka = mat.ka;
    obj.kd = mat.kd;
    obj.ks = mat.ks;
    obj.ns = mat.ns;

    if (!trajFile.empty()) obj.trajectory.load(string(TRAJ_DIR) + trajFile);
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

static void updateTrajectories(float deltaTime) {
    for (ObjModel& obj : models) {
        if (obj.trajectory.enabled && obj.trajectory.loaded)
            obj.position = obj.trajectory.update(deltaTime);
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
            m = glm::scale(m, vec3(1.05f));  // destaque visual do selecionado
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, value_ptr(m));

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
// PARSER DE CENA (assets/Cenas/diorama.txt)
// ============================================================================

// Le um trio de floats apos o tag "POS"/"COLOR" (consome o tag).
static bool readTaggedVec3(istringstream& ss, const string& expectedTag, vec3& out) {
    string tag;
    if (!(ss >> tag) || tag != expectedTag) return false;
    return (bool)(ss >> out.x >> out.y >> out.z);
}

static void parseLightLine(istringstream& ss, Light& light) {
    readTaggedVec3(ss, "POS",   light.position);
    readTaggedVec3(ss, "COLOR", light.color);
    string tag;
    if (ss >> tag && tag == "INT") ss >> light.intensity;
    light.on = true;
}

bool loadScene(const string& scenePath, GLuint fallbackTex) {
    ifstream file(scenePath);
    if (!file.is_open()) {
        cerr << "Erro: cena nao encontrada: " << scenePath << endl;
        return false;
    }
    cout << "\nCarregando cena: " << scenePath << endl;
    const string assetsDir = ASSETS_DIR;

    string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        istringstream ss(line);
        string tag;
        ss >> tag;
        if (tag == "CAMERA_POS") {
            vec3 p; ss >> p.x >> p.y >> p.z;
            camera.position = p;
        } else if (tag == "CAMERA_YAW") {
            float y; ss >> y;
            camera.yaw = y;
            camera.updateCameraVectors();
        } else if (tag == "CAMERA_PITCH") {
            float p; ss >> p;
            camera.pitch = p;
            camera.updateCameraVectors();
        } else if (tag == "LIGHT_KEY") {
            parseLightLine(ss, keyLight);
        } else if (tag == "LIGHT_FILL") {
            parseLightLine(ss, fillLight);
        } else if (tag == "LIGHT_BACK") {
            parseLightLine(ss, backLight);
        } else if (tag == "OBJECT") {
            string name, relPath;
            vec3 pos(0.0f), rot(0.0f);
            float scale = 1.0f;
            string trajFile;
            bool selectable = true;
            ss >> name >> relPath;
            string innerTag;
            while (ss >> innerTag) {
                if (innerTag == "POS")        ss >> pos.x >> pos.y >> pos.z;
                else if (innerTag == "ROT")   ss >> rot.x >> rot.y >> rot.z;
                else if (innerTag == "SCALE") ss >> scale;
                else if (innerTag == "TRAJ")  ss >> trajFile;
                else if (innerTag == "NOSELECT") selectable = false;
            }
            cout << "OBJECT " << name << " path=" << relPath
                 << " pos=(" << pos.x << "," << pos.y << "," << pos.z << ")"
                 << " scale=" << scale << " traj=" << trajFile
                 << (selectable ? "" : " [cenario]") << endl;
            models.push_back(loadModel(name, assetsDir + relPath, pos, rot, scale, trajFile, selectable, fallbackTex));
        }
    }
    cout << "Cena carregada: " << models.size() << " objeto(s)\n" << endl;
    return !models.empty();
}

// ============================================================================
// INPUT DA CAMERA
// ============================================================================

void processCameraInput(GLFWwindow* window, float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.processKeyboard(FORWARD,  deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.processKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.processKeyboard(LEFT,     deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.processKeyboard(RIGHT,    deltaTime);
}

// ============================================================================
// CALLBACK DO MOUSE
// ============================================================================

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastMouseX = (float)xpos;
        lastMouseY = (float)ypos;
        firstMouse = false;
    }
    float dx = (float)xpos - lastMouseX;
    float dy = lastMouseY - (float)ypos;
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
        "Trabalho GB - Minecraft", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
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

    // Localizacoes uniformes (carregadas uma vez)
    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    GLint viewLoc  = glGetUniformLocation(shaderID, "view");
    GLint projLoc  = glGetUniformLocation(shaderID, "projection");
    GLint viewPosLoc = glGetUniformLocation(shaderID, "viewPos");

    GLint lp1 = glGetUniformLocation(shaderID, "lightPos1");
    GLint lp2 = glGetUniformLocation(shaderID, "lightPos2");
    GLint lp3 = glGetUniformLocation(shaderID, "lightPos3");
    GLint lc1 = glGetUniformLocation(shaderID, "lightColor1");
    GLint lc2 = glGetUniformLocation(shaderID, "lightColor2");
    GLint lc3 = glGetUniformLocation(shaderID, "lightColor3");
    GLint li1 = glGetUniformLocation(shaderID, "lightInt1");
    GLint li2 = glGetUniformLocation(shaderID, "lightInt2");
    GLint li3 = glGetUniformLocation(shaderID, "lightInt3");
    GLint lo1 = glGetUniformLocation(shaderID, "lightOn1");
    GLint lo2 = glGetUniformLocation(shaderID, "lightOn2");
    GLint lo3 = glGetUniformLocation(shaderID, "lightOn3");

    mat4 projection = perspective(radians(45.0f),
                                  (float)fbWidth / (float)fbHeight,
                                  0.1f, 100.0f);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, value_ptr(projection));

    GLuint whiteTex = createWhiteTexture();
    if (!loadScene(string(SCENES_DIR) + "diorama.txt", whiteTex)) {
        cerr << "Cena vazia, abortando." << endl;
        return -1;
    }

    for (size_t i = 0; i < models.size(); ++i) {
        if (models[i].selectable) { selectedModel = (int)i; break; }
    }

    printControls();

    float lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float now       = (float)glfwGetTime();
        float deltaTime = now - lastTime;
        lastTime        = now;

        glfwPollEvents();
        processCameraInput(window, deltaTime);
        updateRotations(deltaTime);
        updateTrajectories(deltaTime);

        glUniformMatrix4fv(viewLoc,    1, GL_FALSE, value_ptr(camera.getViewMatrix()));
        glUniform3fv      (viewPosLoc, 1,            value_ptr(camera.position));

        // Atualiza os 3 sets de uniforms de luz a cada frame
        glUniform3fv(lp1, 1, value_ptr(keyLight.position));
        glUniform3fv(lp2, 1, value_ptr(fillLight.position));
        glUniform3fv(lp3, 1, value_ptr(backLight.position));
        glUniform3fv(lc1, 1, value_ptr(keyLight.color));
        glUniform3fv(lc2, 1, value_ptr(fillLight.color));
        glUniform3fv(lc3, 1, value_ptr(backLight.color));
        glUniform1f (li1, keyLight.intensity);
        glUniform1f (li2, fillLight.intensity);
        glUniform1f (li3, backLight.intensity);
        glUniform1i (lo1, keyLight.on  ? 1 : 0);
        glUniform1i (lo2, fillLight.on ? 1 : 0);
        glUniform1i (lo3, backLight.on ? 1 : 0);

        glClearColor(0.12f, 0.14f, 0.18f, 1.0f);
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
// CALLBACK DE TECLADO
// ============================================================================

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (models.empty()) {
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GL_TRUE);
        return;
    }

    bool shift = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)  == GLFW_PRESS ||
                  glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

    ObjModel& obj = models[selectedModel];

    switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GL_TRUE);
            break;

        case GLFW_KEY_TAB: {
            int n = (int)models.size();
            for (int step = 1; step <= n; ++step) {
                int candidate = (selectedModel + step) % n;
                if (models[candidate].selectable) { selectedModel = candidate; break; }
            }
            cout << "Selecionado: " << models[selectedModel].name << endl;
            printModeHelp();
            break;
        }

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

        case GLFW_KEY_1:
            keyLight.on = !keyLight.on;
            cout << "Luz Key:  " << (keyLight.on ? "ON" : "OFF") << endl;
            break;
        case GLFW_KEY_2:
            fillLight.on = !fillLight.on;
            cout << "Luz Fill: " << (fillLight.on ? "ON" : "OFF") << endl;
            break;
        case GLFW_KEY_3:
            backLight.on = !backLight.on;
            cout << "Luz Back: " << (backLight.on ? "ON" : "OFF") << endl;
            break;

        case GLFW_KEY_V:
            obj.trajectory.enabled = !obj.trajectory.enabled;
            cout << "Trajetoria " << obj.name << ": "
                 << (obj.trajectory.enabled ? "ATIVA" : "INATIVA")
                 << (obj.trajectory.loaded  ? "" : " (sem arquivo)")
                 << endl;
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

        case GLFW_KEY_LEFT_BRACKET:
            if (currentMode == Mode::SCALE) {
                float s = std::max(SCALE_MIN, obj.scale.x - SCALE_STEP);
                obj.scale = vec3(s);
            }
            break;
        case GLFW_KEY_RIGHT_BRACKET:
            if (currentMode == Mode::SCALE) {
                float s = obj.scale.x + SCALE_STEP;
                obj.scale = vec3(s);
            }
            break;

        case GLFW_KEY_U:
            if (currentMode == Mode::SCALE) {
                float delta = shift ? -SCALE_STEP : SCALE_STEP;
                obj.scale.x = std::max(SCALE_MIN, obj.scale.x + delta);
                obj.scale.y = std::max(SCALE_MIN, obj.scale.y + delta);
                obj.scale.z = std::max(SCALE_MIN, obj.scale.z + delta);
            }
            break;

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
            cout << "[Modo ESCALA] X/Y/Z: por eixo | U ou [/]: uniforme | Shift: diminuir\n";
            break;
    }
}

void printControls() {
    cout << "\n========================================\n"
         << " TRABALHO GB\n"
         << " MINECRAFT\n"
         << "----------------------------------------\n"
         << " W/A/S/D : Mover camera\n"
         << " Mouse   : Rotacionar camera\n"
         << " TAB     : Alternar objeto selecionado\n"
         << " 1/2/3   : Toggle luzes Key/Fill/Back\n"
         << " V       : Ativar/desativar trajetoria Bezier\n"
         << " R / T / M : Modos Rotacao / Translacao / esc(M)ala\n"
         << " Setas + I/J : Translacao manual\n"
         << " X/Y/Z   : Eixo (modo R) ou escala por eixo (modo M)\n"
         << " [ / ]   : Escala uniforme - / + (modo M)\n"
         << " ESC     : Sair\n"
         << "========================================\n";
    if (!models.empty())
        cout << " Selecionado inicial: " << models[selectedModel].name << "\n";
    printModeHelp();
    cout << endl;
}
