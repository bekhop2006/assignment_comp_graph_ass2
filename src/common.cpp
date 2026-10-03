#include "common.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

// ---------- матрицы ----------
// Элемент (строка row, столбец col) хранится в m[col * 4 + row].

// Единичная матрица: единицы на главной диагонали.
Mat4 identity() {
    Mat4 r = {};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

// Обычное умножение матриц r = a * b.
// Порядок важен: в a * b * v сначала применяется b, затем a.
Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r = {};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

// Перенос: смещение записывается в последний (4-й) столбец.
Mat4 translate(float x, float y, float z) {
    Mat4 r = identity();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

// Масштабирование: коэффициенты стоят на диагонали.
Mat4 scale(float x, float y, float z) {
    Mat4 r = identity();
    r.m[0] = x;
    r.m[5] = y;
    r.m[10] = z;
    return r;
}

// Перевод градусов в радианы (std::sin/cos работают с радианами).
static float radians(float degrees) {
    return degrees * static_cast<float>(M_PI) / 180.0f;
}

// Поворот вокруг оси X: меняются координаты Y и Z.
Mat4 rotateX(float degrees) {
    float c = std::cos(radians(degrees));
    float s = std::sin(radians(degrees));
    Mat4 r = identity();
    r.m[5] = c;
    r.m[6] = s;
    r.m[9] = -s;
    r.m[10] = c;
    return r;
}

// Поворот вокруг оси Y: меняются координаты X и Z.
Mat4 rotateY(float degrees) {
    float c = std::cos(radians(degrees));
    float s = std::sin(radians(degrees));
    Mat4 r = identity();
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

// Поворот вокруг оси Z (поворот в плоскости экрана): меняются X и Y.
// Положительный угол — против часовой стрелки.
Mat4 rotateZ(float degrees) {
    float c = std::cos(radians(degrees));
    float s = std::sin(radians(degrees));
    Mat4 r = identity();
    r.m[0] = c;
    r.m[1] = s;
    r.m[4] = -s;
    r.m[5] = c;
    return r;
}

// ---------- шейдеры ----------

// Читает весь файл в строку; при ошибке возвращает пустую строку.
static std::string readFile(const std::string& path) {
    std::ifstream in(path.c_str());
    if (!in) {
        return "";
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Ищет файл шейдера в нескольких папках относительно текущей директории.
// Если не нашли — завершаем программу с понятным сообщением.
static std::string readShader(const std::string& filename) {
    const char* prefixes[] = {"shaders/", "./shaders/", "../shaders/"};
    for (const char* prefix : prefixes) {
        std::string src = readFile(std::string(prefix) + filename);
        if (!src.empty()) {
            return src;
        }
    }
    std::cerr << "Failed to read shader " << filename
              << " (run the program from the ass2 folder)" << std::endl;
    exit(1);
}

// Компилирует один шейдер (вершинный или фрагментный).
// При ошибке компиляции печатает лог GLSL и завершает программу.
static GLuint compileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error:\n" << log << std::endl;
        exit(1);
    }
    return shader;
}

// Собирает шейдерную программу: компиляция двух шейдеров + линковка.
GLuint loadShaderProgram(const std::string& vertexFile, const std::string& fragmentFile) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, readShader(vertexFile));
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, readShader(fragmentFile));
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Program link error:\n" << log << std::endl;
        exit(1);
    }
    // После линковки отдельные шейдеры больше не нужны.
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// ---------- меши ----------

// Загружает меш в видеопамять.
// VAO запоминает, из каких буферов и в каком формате шейдер берёт атрибуты,
// поэтому при отрисовке достаточно просто привязать VAO.
GPUMesh uploadMesh(GLuint program, const Mesh& mesh) {
    GPUMesh gpu;
    gpu.count = static_cast<GLsizei>(mesh.positions.size() / mesh.posSize);
    gpu.mode = mesh.mode;

    glGenVertexArrays(1, &gpu.vao);
    glBindVertexArray(gpu.vao);

    // buffers[0] — позиции, buffers[1] — цвета.
    GLuint buffers[2] = {0, 0};
    glGenBuffers(2, buffers);

    // Позиции вершин -> атрибут vPosition (2 или 3 числа на вершину).
    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, mesh.positions.size() * sizeof(float),
                 mesh.positions.data(), GL_STATIC_DRAW);
    GLint posLoc = glGetAttribLocation(program, "vPosition");
    if (posLoc >= 0) {
        glEnableVertexAttribArray(posLoc);
        glVertexAttribPointer(posLoc, mesh.posSize, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    // Цвета вершин -> атрибут vColor (только если они заданы).
    if (!mesh.colors.empty()) {
        glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
        glBufferData(GL_ARRAY_BUFFER, mesh.colors.size() * sizeof(Vec3),
                     mesh.colors.data(), GL_STATIC_DRAW);
        GLint colLoc = glGetAttribLocation(program, "vColor");
        if (colLoc >= 0) {
            glEnableVertexAttribArray(colLoc);
            glVertexAttribPointer(colLoc, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        }
    }

    glBindVertexArray(0);  // отвязываем VAO, чтобы случайно его не изменить
    return gpu;
}

// Рисует все вершины меша выбранным примитивом (треугольники, веер и т.д.).
void drawMesh(const GPUMesh& mesh) {
    glBindVertexArray(mesh.vao);
    glDrawArrays(mesh.mode, 0, mesh.count);
    glBindVertexArray(0);
}

// ---------- скриншоты ----------

// Считывает пиксели заднего буфера и пишет их в PPM (формат P6, RGB).
void saveWindowPPM(const std::string& path) {
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int width = viewport[2];
    int height = viewport[3];

    std::vector<unsigned char> pixels(static_cast<size_t>(width * height * 3));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);  // строки без выравнивания
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // В OpenGL строка 0 — нижняя, а в PPM — верхняя, поэтому пишем строки снизу вверх.
    std::ofstream out(path.c_str(), std::ios::binary);
    out << "P6\n" << width << " " << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y) {
        out.write(reinterpret_cast<const char*>(&pixels[static_cast<size_t>(y * width * 3)]),
                  width * 3);
    }
}
