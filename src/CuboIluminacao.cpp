// Câmera e Iluminação básica
//
// Este código serve como base para toda a disciplina.
// Ele implementa:
// - OpenGL moderno (pipeline programável)
// - Um modelo geométrico simples (cubo)
// - Transformações de modelo, visualização (câmera) e projeção
// - Uma câmera no estilo FPS (yaw + pitch)
//
// A partir deste exemplo, novos conceitos serão adicionados gradualmente
// (iluminação, materiais, texturas, visibilidade, sombras, etc.)

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cmath>




using namespace std;

GLFWwindow *Window = nullptr;
GLuint Shader_programm = 0;
GLuint Vao_cubo = 0;
GLuint Vao_esfera = 0;
int Num_vertices_esfera = 0; // Precisamos guardar a quantidade de vértices gerados

int WIDTH = 800;
int HEIGHT = 600;

float Tempo_entre_frames = 0.0f;

// -----------------------------
// Parâmetros da câmera virtual
// -----------------------------

float Cam_speed = 10.0f;
float Cam_yaw_speed = 30.0f;
glm::vec3 Cam_pos(0.0f, 0.0f, 2.0f);
float Cam_yaw = -90.0f;
float Cam_pitch = 0.0f;

double lastX = WIDTH / 2.0;
double lastY = HEIGHT / 2.0;
bool primeiro_mouse = true;

// -----------------------------
// Callbacks de janela e entrada
// -----------------------------

void redimensionaCallback(GLFWwindow *window, int w, int h)
{
    WIDTH = w;
    HEIGHT = h;
    glViewport(0, 0, w, h);
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    if (primeiro_mouse)
    {
        lastX = xpos;
        lastY = ypos;
        primeiro_mouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensibilidade = 0.1f;
    xoffset *= sensibilidade;
    yoffset *= sensibilidade;

    Cam_yaw += xoffset;
    Cam_pitch += yoffset;

    if (Cam_pitch > 89.0f)
        Cam_pitch = 89.0f;
    if (Cam_pitch < -89.0f)
        Cam_pitch = -89.0f;
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
}

// -----------------------------
// Inicialização do OpenGL
// -----------------------------

void inicializaOpenGL()
{
    glfwInit();

    //Muita atenção aqui: alguns ambientes não aceitam essas configurações
	//Você deve adaptar para a versão do OpenGL suportada por sua placa
	//Sugestão: comente essas linhas de código para desobrir a versão e
	//depois atualize (por exemplo: 4.5 com 4 e 5)
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	//Essencial para computadores da Apple
	#ifdef __APPLE__
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	#endif

    Window = glfwCreateWindow(WIDTH, HEIGHT, "Exemplo Base - CG em Tempo Real", nullptr, nullptr);
    glfwMakeContextCurrent(Window);

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glfwSetFramebufferSizeCallback(Window, redimensionaCallback);
    glfwSetCursorPosCallback(Window, mouse_callback);
    glfwSetKeyCallback(Window, key_callback);
    glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    cout << "Placa de vídeo: " << glGetString(GL_RENDERER) << endl;
    cout << "Versão do OpenGL: " << glGetString(GL_VERSION) << endl;
}

// -----------------------------
// Inicialização da geometria
// -----------------------------

void inicializaCubo()
{
    float points[] = {
        // posição              // normal

        // Frente (0,0,1)
        0.5, 0.5, 0.5,          0, 0, 1,
        0.5, -0.5, 0.5,         0, 0, 1,
        -0.5, -0.5, 0.5,        0, 0, 1,
        0.5, 0.5, 0.5,          0, 0, 1,
        -0.5, -0.5, 0.5,        0, 0, 1,
        -0.5, 0.5, 0.5,         0, 0, 1,

        // Trás (0,0,-1)
        0.5, 0.5, -0.5,         0, 0, -1,
        0.5, -0.5, -0.5,        0, 0, -1,
        -0.5, -0.5, -0.5,       0, 0, -1,
        0.5, 0.5, -0.5,         0, 0, -1,
        -0.5, -0.5, -0.5,       0, 0, -1,
        -0.5, 0.5, -0.5,        0, 0, -1,

        // Esquerda (-1,0,0)
        -0.5, -0.5, 0.5,        -1, 0, 0,
        -0.5, 0.5, 0.5,         -1, 0, 0,
        -0.5, -0.5, -0.5,       -1, 0, 0,
        -0.5, -0.5, -0.5,       -1, 0, 0,
        -0.5, 0.5, -0.5,        -1, 0, 0,
        -0.5, 0.5, 0.5,         -1, 0, 0,

        // Direita (1,0,0)
        0.5, -0.5, 0.5,         1, 0, 0,
        0.5, 0.5, 0.5,          1, 0, 0,
        0.5, -0.5, -0.5,        1, 0, 0,
        0.5, -0.5, -0.5,        1, 0, 0,
        0.5, 0.5, -0.5,         1, 0, 0,
        0.5, 0.5, 0.5,          1, 0, 0,

        // Baixo (0,-1,0)
        -0.5, -0.5, 0.5,        0, -1, 0,
        0.5, -0.5, 0.5,         0, -1, 0,
        0.5, -0.5, -0.5,        0, -1, 0,
        0.5, -0.5, -0.5,        0, -1, 0,
        -0.5, -0.5, -0.5,       0, -1, 0,
        -0.5, -0.5, 0.5,        0, -1, 0,

        // Cima (0,1,0)
        -0.5, 0.5, 0.5,         0, 1, 0,
        0.5, 0.5, 0.5,          0, 1, 0,
        0.5, 0.5, -0.5,         0, 1, 0,
        0.5, 0.5, -0.5,         0, 1, 0,
        -0.5, 0.5, -0.5,        0, 1, 0,
        -0.5, 0.5, 0.5,         0, 1, 0};
    GLuint VBO;
    glGenVertexArrays(1, &Vao_cubo);
    glGenBuffers(1, &VBO);

    glBindVertexArray(Vao_cubo);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void inicializaEsfera()
{
    std::vector<float> points;
    
    // Resolução da esfera (quanto maior, mais redonda, porém mais pesada)
    int stacks = 30;  // Fatias horizontais (latitude)
    int sectors = 30; // Fatias verticais (longitude)
    float radius = 0.5f;
    const float PI = 3.14159265359f;

    for (int i = 0; i < stacks; ++i)
    {
        float phi1 = PI * float(i) / stacks;
        float phi2 = PI * float(i + 1) / stacks;

        for (int j = 0; j < sectors; ++j)
        {
            float theta1 = 2.0f * PI * float(j) / sectors;
            float theta2 = 2.0f * PI * float(j + 1) / sectors;

            // Função auxiliar (lambda) para calcular e adicionar um vértice e sua normal
            auto addVertex = [&](float p, float t) {
                // Coordenadas esféricas para cartesianas
                float x = radius * sin(p) * cos(t);
                float y = radius * cos(p);
                float z = radius * sin(p) * sin(t);

                // 1. Adiciona a Posição
                points.push_back(x);
                points.push_back(y);
                points.push_back(z);

                // 2. Adiciona a Normal
                // Para uma esfera centrada na origem (0,0,0), a direção da normal 
                // é exatamente a posição do vértice dividida pelo raio (normalizada).
                points.push_back(x / radius);
                points.push_back(y / radius);
                points.push_back(z / radius);
            };

            // Um "quadrado" na superfície da esfera é formado por 2 triângulos.
            // Triângulo 1
            addVertex(phi1, theta1);
            addVertex(phi2, theta1);
            addVertex(phi1, theta2);

            // Triângulo 2
            addVertex(phi1, theta2);
            addVertex(phi2, theta1);
            addVertex(phi2, theta2);
        }
    }

    // Calcula quantos vértices reais foram gerados (cada vértice tem 6 floats: 3 pos + 3 norm)
    Num_vertices_esfera = points.size() / 6;

    // A partir daqui, é a mesma lógica do seu cubo
    GLuint VBO;
    glGenVertexArrays(1, &Vao_esfera);
    glGenBuffers(1, &VBO);

    glBindVertexArray(Vao_esfera);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    
    // Nota: Usamos points.data() para pegar o ponteiro do vector e points.size() para o tamanho
    glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(float), points.data(), GL_STATIC_DRAW);

    // Atributo 0: Posição
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    // Atributo 1: Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

// -----------------------------
// Shaders
// -----------------------------

GLuint compilaShader(const char *source, GLenum type)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}

void inicializaShaders()
{
    const char *vertex_shader = R"(
        #version 400

        layout(location = 0) in vec3 vertex_posicao;
        layout(location = 1) in vec3 vertex_normal;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 proj;

        out vec3 fragPos;
        out vec3 normal;

        void main()
        {
            vec4 worldPos = model * vec4(vertex_posicao, 1.0);
            fragPos = worldPos.xyz;

            normal = mat3(transpose(inverse(model))) * vertex_normal;

            gl_Position = proj * view * worldPos;
        }
    )";

    const char *fragment_shader = R"(
        #version 400

        in vec3 fragPos;
        in vec3 normal;

        out vec4 frag_colour;

        uniform vec3 lightPos;
        uniform vec3 viewPos;

        uniform vec3 lightColor;
        uniform vec3 objectColor;

        uniform float Ka;//coeficiente de reflexão ambiente
        uniform float Kd;//coeficiente de reflexão difusa
        uniform float Ks;//coeficiente de reflexão especular
        uniform float shininess;//expoente especular

        // Constantes da equação de atenuação
        uniform float Kc; // Constante
        uniform float Kl; // Linear
        uniform float Kq; // Quadrática

        void main()
        {
            vec3 N = normalize(normal);
            vec3 L = normalize(lightPos - fragPos);
            vec3 V = normalize(viewPos - fragPos);
            vec3 R = normalize(reflect(-L,N)); //Para o modelo de Phong  Tradicional
            //vec3 H = normalize(L + V); //Para o modelo Blinn-Phong

            // Calcula a distância (d) entre a luz e o fragmento
            float d = length(lightPos - fragPos);

            // Calcula o fator de atenuação (F_att)
            float attenuation = 1.0 / (Kc + Kl * d + Kq * (d * d));

            // Ambiente
            vec3 ambient = Ka * lightColor;

            // Difusa
            float diff = max(dot(N, L), 0.0);
            vec3 diffuse = Kd * diff * lightColor;

            // Especular (Blinn-Phong)
            //float spec = pow(max(dot(N, H), 0.0), shininess);
            
            // Especular (Phong Tradicional)
            float spec = pow(max(dot(V, R), 0.0), shininess);
            vec3 specular = Ks * spec * lightColor;

            // 3. Aplica a atenuação na difusa (e na especular)
            //diffuse  *= attenuation;
            //specular *= attenuation;

            vec3 result = (ambient + diffuse )* objectColor  + specular;

            frag_colour = vec4(result, 1.0);
        }
    )";

    GLuint vs = compilaShader(vertex_shader, GL_VERTEX_SHADER);
    GLuint fs = compilaShader(fragment_shader, GL_FRAGMENT_SHADER);

    Shader_programm = glCreateProgram();
    glAttachShader(Shader_programm, vs);
    glAttachShader(Shader_programm, fs);
    glLinkProgram(Shader_programm);

    glDeleteShader(vs);
    glDeleteShader(fs);
}

// -----------------------------
// Transformação de modelo
// -----------------------------

void transformacaoGenerica(float Tx, float Ty, float Tz,
                           float Sx, float Sy, float Sz,
                           float Rx, float Ry, float Rz)
{
    glm::mat4 transform(1.0f);

    transform = glm::translate(transform, glm::vec3(Tx, Ty, Tz));
    transform = glm::rotate(transform, glm::radians(Rz), glm::vec3(0, 0, 1));
    transform = glm::rotate(transform, glm::radians(Ry), glm::vec3(0, 1, 0));
    transform = glm::rotate(transform, glm::radians(Rx), glm::vec3(1, 0, 0));
    transform = glm::scale(transform, glm::vec3(Sx, Sy, Sz));

    GLuint loc = glGetUniformLocation(Shader_programm, "model");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(transform));
}

// -----------------------------
// Câmera (matriz de visualização)
// -----------------------------

void especificaMatrizVisualizacao()
{
    glm::vec3 front;
    front.x = cos(glm::radians(Cam_yaw)) * cos(glm::radians(Cam_pitch));
    front.y = sin(glm::radians(Cam_pitch));
    front.z = sin(glm::radians(Cam_yaw)) * cos(glm::radians(Cam_pitch));
    front = glm::normalize(front);

    glm::mat4 view = glm::lookAt(Cam_pos, Cam_pos + front, glm::vec3(0, 1, 0));

    GLuint loc = glGetUniformLocation(Shader_programm, "view");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(view));
}

// -----------------------------
// Projeção
// -----------------------------

void especificaMatrizProjecao()
{
    glm::mat4 proj = glm::perspective(glm::radians(67.0f),
                                      (float)WIDTH / HEIGHT,
                                      0.1f, 100.0f);

    GLuint loc = glGetUniformLocation(Shader_programm, "proj");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(proj));
}

void inicializaCamera()
{
    especificaMatrizVisualizacao();
    especificaMatrizProjecao();
}

// -----------------------------
// Entrada de teclado
// -----------------------------

void trataTeclado()
{
    float velocidade = Cam_speed * Tempo_entre_frames;

    glm::vec3 frente;
    frente.x = cos(glm::radians(Cam_yaw)) * cos(glm::radians(Cam_pitch));
    frente.y = sin(glm::radians(Cam_pitch));
    frente.z = sin(glm::radians(Cam_yaw)) * cos(glm::radians(Cam_pitch));
    frente = glm::normalize(frente);

    glm::vec3 direita = glm::normalize(glm::cross(frente, glm::vec3(0, 1, 0)));

    if (glfwGetKey(Window, GLFW_KEY_W) == GLFW_PRESS)
        Cam_pos += frente * velocidade;
    if (glfwGetKey(Window, GLFW_KEY_S) == GLFW_PRESS)
        Cam_pos -= frente * velocidade;
    if (glfwGetKey(Window, GLFW_KEY_A) == GLFW_PRESS)
        Cam_pos -= direita * velocidade;
    if (glfwGetKey(Window, GLFW_KEY_D) == GLFW_PRESS)
        Cam_pos += direita * velocidade;
    if (glfwGetKey(Window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(Window, true);
}

// -----------------------------
// Renderização
// -----------------------------

void defineMaterial(float r, float g, float b,
                    float ka, float kd, float ks,
                    float shininess)
{
    glUniform3f(glGetUniformLocation(Shader_programm, "objectColor"),
                r, g, b);

    glUniform1f(glGetUniformLocation(Shader_programm, "Ka"), ka);
    glUniform1f(glGetUniformLocation(Shader_programm, "Kd"), kd);
    glUniform1f(glGetUniformLocation(Shader_programm, "Ks"), ks);
    glUniform1f(glGetUniformLocation(Shader_programm, "shininess"), shininess);
}

void inicializaRenderizacao()
{
    float tempo_anterior = glfwGetTime();

    glEnable(GL_DEPTH_TEST);

    while (!glfwWindowShouldClose(Window))
    {
        float tempo_atual = glfwGetTime();
        Tempo_entre_frames = tempo_atual - tempo_anterior;
        tempo_anterior = tempo_atual;

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(Shader_programm);
        inicializaCamera();

        glBindVertexArray(Vao_cubo);

        glm::vec3 lightPos(2.0f, 2.0f, 2.0f);

        glUniform3fv(glGetUniformLocation(Shader_programm, "lightPos"),
                    1, glm::value_ptr(lightPos));

        glUniform3fv(glGetUniformLocation(Shader_programm, "viewPos"),
                    1, glm::value_ptr(Cam_pos));

        glUniform3f(glGetUniformLocation(Shader_programm, "lightColor"),
                    1.0f, 1.0f, 1.0f);

        // Configuração da Atenuação ---
        // Estes valores representam uma luz que cobre uma distância de cerca de 50 unidades.
        // Você pode ajustá-los para fazer a luz ir mais ou menos longe.
        glUniform1f(glGetUniformLocation(Shader_programm, "Kc"), 1.0f);   // Constante
        glUniform1f(glGetUniformLocation(Shader_programm, "Kl"), 0.09f);  // Linear
        glUniform1f(glGetUniformLocation(Shader_programm, "Kq"), 0.032f); // Quadrática
                    
        defineMaterial(
            1.0f, 0.6f, 0.2f,   // cor base
            0.1f,               // Ka
            0.7f,               // Kd
            1.0f,               // Ks
            32.0f               // shininess
        );
        //transformacaoGenerica(0, 0, 0, 1, 1, 1, 0, 0, 0);
        //glDrawArrays(GL_TRIANGLES, 0, 36);

        // 2. Desenha a Esfera
        glBindVertexArray(Vao_esfera);
        transformacaoGenerica(0, 0, 0, 1, 1, 1, 0, 0, 0);
        glDrawArrays(GL_TRIANGLES, 0, Num_vertices_esfera); // <-- Usa a variável dinâmica que criamos

        glfwSwapBuffers(Window);
        glfwPollEvents();
        trataTeclado();
    }

    glfwTerminate();
}

// -----------------------------
// Função principal
// -----------------------------

int main()
{
    inicializaOpenGL();
    inicializaCubo();
    inicializaEsfera();
    inicializaShaders();
    inicializaRenderizacao();
    return 0;
}