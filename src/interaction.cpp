// Задание 2, часть 1 — взаимодействие, окна, меню и анимация.
//
// Продолжение задания 1:
//   * главное окно : вложенный «чёрно-белый» квадрат (вращается против часовой стрелки)
//   * подокно      : эллипс на своём фоне (цвет фона — через меню по правой кнопке)
//   * "window 2"   : круг и треугольник одного цвета (клавиши r g b y o p w)
//                    треугольник вращается по часовой стрелке, круг «дышит»
//   * доп. баллы   : левый клик в главном окне добавляет «дышащий» круг
//                    случайного цвета в точке клика

#include "common.h"

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

// ---------- состояние OpenGL для каждого окна ----------
// У каждого окна (и подокна) GLUT свой контекст OpenGL, поэтому
// шейдерная программа и VAO создаются отдельно в каждом из них.

// Шейдерная программа окна и расположения её uniform-переменных.
struct Renderer {
    GLuint program = 0;
    GLint uModel = -1;
    GLint uColor = -1;
    GLint uUseVertexColor = -1;
};

// Загружает 2D-шейдеры в текущем окне и находит uniform-переменные.
static Renderer makeRenderer() {
    Renderer r;
    r.program = loadShaderProgram("vshader2d.glsl", "fshader.glsl");
    r.uModel = glGetUniformLocation(r.program, "uModel");
    r.uColor = glGetUniformLocation(r.program, "uColor");
    r.uUseVertexColor = glGetUniformLocation(r.program, "uUseVertexColor");
    return r;
}

// Рисует меш одним сплошным цветом c с матрицей преобразования model.
static void drawSolid(const Renderer& r, const GPUMesh& mesh, const Mat4& model, Vec3 c) {
    glUseProgram(r.program);
    glUniformMatrix4fv(r.uModel, 1, GL_FALSE, model.m);
    glUniform4f(r.uColor, c.r, c.g, c.b, 1.0f);
    glUniform1i(r.uUseVertexColor, 0);  // цвет берётся из uColor, а не из вершин
    drawMesh(mesh);
}

// ---------- геометрия (те же построения через углы, что в задании 1) ----------

// Круг/эллипс с радиусами rx, ry: веер треугольников из центра.
// Последняя точка совпадает с первой, чтобы круг замкнулся.
static Mesh makeCircle(float rx, float ry, int segments) {
    Mesh mesh;
    mesh.mode = GL_TRIANGLE_FAN;
    mesh.positions = {0.0f, 0.0f};  // первая вершина — центр
    for (int i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(2.0 * M_PI * i / segments);
        mesh.positions.push_back(rx * std::cos(angle));
        mesh.positions.push_back(ry * std::sin(angle));
    }
    return mesh;
}

// Квадрат: 4 точки на единичной окружности под углами 45°, 135°, 225°, 315°.
static Mesh makeSquare() {
    Mesh mesh;
    mesh.mode = GL_TRIANGLE_FAN;
    for (int k = 0; k < 4; ++k) {
        float angle = static_cast<float>(M_PI / 4.0 + k * M_PI / 2.0);
        mesh.positions.push_back(std::cos(angle));
        mesh.positions.push_back(std::sin(angle));
    }
    return mesh;
}

// Равносторонний треугольник вершиной вверх: точки через каждые 120°, начиная с 90°.
static Mesh makeTriangle() {
    Mesh mesh;
    mesh.mode = GL_TRIANGLES;
    for (int k = 0; k < 3; ++k) {
        float angle = static_cast<float>(M_PI / 2.0 + k * 2.0 * M_PI / 3.0);
        mesh.positions.push_back(std::cos(angle));
        mesh.positions.push_back(std::sin(angle));
    }
    return mesh;
}

// ---------- цвета ----------

static const Vec3 kBlack = {0.0f, 0.0f, 0.0f};
static const Vec3 kWhite = {1.0f, 1.0f, 1.0f};
static const Vec3 kRed = {1.0f, 0.0f, 0.0f};
static const Vec3 kGreen = {0.0f, 0.8f, 0.0f};
static const Vec3 kBlue = {0.0f, 0.3f, 1.0f};
static const Vec3 kYellow = {1.0f, 0.9f, 0.0f};
static const Vec3 kOrange = {1.0f, 0.5f, 0.0f};
static const Vec3 kPurple = {0.6f, 0.2f, 0.8f};

// ---------- состояние приложения ----------

// Идентификаторы окон GLUT.
static int gMainWin = 0;
static int gSubWin = 0;
static int gWin2 = 0;

// Шейдеры и меши — отдельно для каждого окна (см. комментарий выше).
static Renderer gMainR, gSubR, gWin2R;
static GPUMesh gMainSquare, gMainCircle;
static GPUMesh gSubEllipse;
static GPUMesh gWin2Circle, gWin2Triangle;

static Vec3 gSquareColor = kWhite;               // цвет «белых» слоёв вложенного квадрата
static Vec3 gSubBackground = {1.0f, 0.95f, 0.6f};  // фон подокна (светло-жёлтый)
static Vec3 gWin2Color = kRed;                   // общий цвет круга и треугольника

static float gSquareAngle = 0.0f;    // градусы; растёт -> вращение против часовой
static float gTriangleAngle = 0.0f;  // градусы; убывает -> вращение по часовой
static float gBreathPhase = 0.0f;    // фаза «дыхания» в радианах
static bool gAnimating = true;
static int gLastTime = 0;            // время прошлого кадра, мс

// Круг, добавленный левым кликом (доп. баллы).
struct BreathingCircle {
    float x;      // центр в координатах OpenGL (-1..1)
    float y;
    float phase;  // свой сдвиг фазы, чтобы круги «дышали» не синхронно
    Vec3 color;
};
static std::vector<BreathingCircle> gClickCircles;

// Режим скриншотов для проверки программы (A2_SCREENSHOT=<папка>).
static const char* gShotDir = nullptr;
static int gFrame = 0;
static int kShotFrame = 45;  // номер кадра для снимка; можно задать через A2_SHOT_FRAME

// Случайное число от 0 до 1.
static float random01() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

// Масштаб «дыхания»: плавно колеблется от 0.65 до 1.35.
static float breathScale(float phase) {
    return 1.0f + 0.35f * std::sin(phase);
}

// В режиме скриншотов сохраняет кадр с номером kShotFrame.
static void maybeScreenshot(const char* name) {
    if (gShotDir && gFrame == kShotFrame) {
        saveWindowPPM(std::string(gShotDir) + "/" + name + ".ppm");
    }
}

// ---------- анимация (idle-колбэк) ----------

// Просит перерисовать все три окна, затем возвращает текущее окно обратно.
static void redisplayAll() {
    int current = glutGetWindow();
    for (int win : {gMainWin, gSubWin, gWin2}) {
        glutSetWindow(win);
        glutPostRedisplay();
    }
    glutSetWindow(current);
}

// Вызывается GLUT, когда нет других событий. Двигает анимацию на время dt,
// поэтому скорость не зависит от частоты кадров.
static void idle() {
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = static_cast<float>(now - gLastTime) / 1000.0f;  // секунды
    gLastTime = now;
    if (gShotDir) {
        dt = 1.0f / 60.0f;  // в режиме скриншотов — фиксированный шаг, чтобы кадры были одинаковыми
    }

    gSquareAngle += 40.0f * dt;    // против часовой стрелки, 40°/с
    gTriangleAngle -= 60.0f * dt;  // по часовой стрелке, 60°/с
    gBreathPhase += 2.5f * dt;

    if (gShotDir && ++gFrame > kShotFrame + 1) {
        exit(0);
    }
    redisplayAll();
}

static void startAnimation() {
    if (!gAnimating) {
        gAnimating = true;
        gLastTime = glutGet(GLUT_ELAPSED_TIME);  // чтобы после паузы не было скачка
        glutIdleFunc(idle);
    }
}

static void stopAnimation() {
    gAnimating = false;
    glutIdleFunc(NULL);  // NULL убирает idle-колбэк, и всё останавливается
}

// ---------- главное окно ----------

static void mainDisplay() {
    glClearColor(0.20f, 0.24f, 0.32f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Вложенные квадраты из задания 1: от большего к меньшему, цвет и чёрный по очереди.
    // Каждый следующий рисуется поверх предыдущего, поэтому видны «рамки».
    const int layers = 9;
    const float outer = 0.65f;
    const float inner = 0.06f;
    for (int i = 0; i < layers; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(layers - 1);
        float size = outer + (inner - outer) * t;  // линейно от outer до inner
        Mat4 model = rotateZ(gSquareAngle) * scale(size, size, 1.0f);
        drawSolid(gMainR, gMainSquare, model, (i % 2 == 0) ? gSquareColor : kBlack);
    }

    // Доп. баллы: круги, добавленные левыми кликами.
    for (const BreathingCircle& c : gClickCircles) {
        float s = breathScale(gBreathPhase + c.phase);
        drawSolid(gMainR, gMainCircle, translate(c.x, c.y, 0.0f) * scale(s, s, 1.0f), c.color);
    }

    maybeScreenshot("main_window");
    glutSwapBuffers();
}

// Левый клик: добавляем круг в точке клика.
static void mainMouse(int button, int state, int x, int y) {
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) {
        return;
    }
    // Переводим пиксели окна в координаты OpenGL (-1..1).
    // У GLUT ось Y направлена вниз, у OpenGL — вверх, поэтому Y переворачиваем.
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);
    BreathingCircle c;
    c.x = 2.0f * static_cast<float>(x) / static_cast<float>(w) - 1.0f;
    c.y = 1.0f - 2.0f * static_cast<float>(y) / static_cast<float>(h);
    c.phase = random01() * 2.0f * static_cast<float>(M_PI);
    c.color = {random01(), random01(), random01()};
    gClickCircles.push_back(c);
    glutPostRedisplay();
}

// Пункты меню главного окна.
enum MainMenuItem { MENU_STOP = 1, MENU_START, MENU_SQ_WHITE, MENU_SQ_RED, MENU_SQ_GREEN };

static void mainMenu(int item) {
    switch (item) {
        case MENU_STOP: stopAnimation(); break;
        case MENU_START: startAnimation(); break;
        case MENU_SQ_WHITE: gSquareColor = kWhite; break;
        case MENU_SQ_RED: gSquareColor = kRed; break;
        case MENU_SQ_GREEN: gSquareColor = kGreen; break;
    }
    glutSetWindow(gMainWin);
    glutPostRedisplay();
}

// Меню главного окна с подменю «Square Colors».
// Подменю создаётся первым, потому что при добавлении нужен его id.
static void createMainMenu() {
    int colors = glutCreateMenu(mainMenu);
    glutAddMenuEntry("White", MENU_SQ_WHITE);
    glutAddMenuEntry("Red", MENU_SQ_RED);
    glutAddMenuEntry("Green", MENU_SQ_GREEN);

    glutCreateMenu(mainMenu);
    glutAddMenuEntry("Stop Animation", MENU_STOP);
    glutAddMenuEntry("Start Animation", MENU_START);
    glutAddSubMenu("Square Colors", colors);
    glutAttachMenu(GLUT_RIGHT_BUTTON);
}

// ---------- подокно ----------

// Фон подокна + эллипс по центру.
static void subDisplay() {
    glClearColor(gSubBackground.r, gSubBackground.g, gSubBackground.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    drawSolid(gSubR, gSubEllipse, identity(), {0.55f, 0.0f, 0.35f});
    maybeScreenshot("sub_window");
    glutSwapBuffers();
}

// Пункты меню подокна (цвета фона).
enum SubMenuItem { SUB_YELLOW = 1, SUB_CYAN, SUB_PINK, SUB_GRAY, SUB_WHITE };

static void subMenu(int item) {
    switch (item) {
        case SUB_YELLOW: gSubBackground = {1.0f, 0.95f, 0.6f}; break;
        case SUB_CYAN: gSubBackground = {0.55f, 0.9f, 0.95f}; break;
        case SUB_PINK: gSubBackground = {1.0f, 0.75f, 0.85f}; break;
        case SUB_GRAY: gSubBackground = {0.6f, 0.6f, 0.6f}; break;
        case SUB_WHITE: gSubBackground = {1.0f, 1.0f, 1.0f}; break;
    }
    glutSetWindow(gSubWin);
    glutPostRedisplay();
}

static void createSubMenu() {
    glutCreateMenu(subMenu);
    glutAddMenuEntry("Light Yellow", SUB_YELLOW);
    glutAddMenuEntry("Light Cyan", SUB_CYAN);
    glutAddMenuEntry("Pink", SUB_PINK);
    glutAddMenuEntry("Gray", SUB_GRAY);
    glutAddMenuEntry("White", SUB_WHITE);
    glutAttachMenu(GLUT_RIGHT_BUTTON);
}

// ---------- window 2 ----------

// Слева «дышащий» круг, справа треугольник, вращающийся по часовой стрелке.
static void win2Display() {
    glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    float s = breathScale(gBreathPhase);
    // Круг: масштаб (дыхание), затем сдвиг влево.
    drawSolid(gWin2R, gWin2Circle,
              translate(-0.45f, 0.0f, 0.0f) * scale(0.28f * s, 0.28f * s, 1.0f), gWin2Color);
    // Треугольник: масштаб, поворот вокруг своего центра, затем сдвиг вправо.
    drawSolid(gWin2R, gWin2Triangle,
              translate(0.45f, 0.0f, 0.0f) * rotateZ(gTriangleAngle) * scale(0.38f, 0.38f, 1.0f),
              gWin2Color);

    maybeScreenshot("window2");
    glutSwapBuffers();
}

// Клавиши window 2 меняют общий цвет круга и треугольника.
static void win2Keyboard(unsigned char key, int, int) {
    switch (key) {
        case 'r': gWin2Color = kRed; break;
        case 'g': gWin2Color = kGreen; break;
        case 'b': gWin2Color = kBlue; break;
        case 'y': gWin2Color = kYellow; break;
        case 'o': gWin2Color = kOrange; break;
        case 'p': gWin2Color = kPurple; break;
        case 'w': gWin2Color = kWhite; break;
        case 27: exit(0);  // Esc
        default: return;
    }
    glutPostRedisplay();
}

// ---------- общие колбэки ----------

// Область вывода — всё окно.
static void reshape(int w, int h) {
    glViewport(0, 0, w, h);
}

// Выход по Esc или q (для главного окна и подокна).
static void quitKeyboard(unsigned char key, int, int) {
    if (key == 27 || key == 'q') {
        exit(0);
    }
}

int main(int argc, char** argv) {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    gShotDir = std::getenv("A2_SCREENSHOT");
    if (const char* frame = std::getenv("A2_SHOT_FRAME")) {
        kShotFrame = std::atoi(frame);
    }

    glutInit(&argc, argv);
    // GLUT_DOUBLE включает двойную буферизацию: рисуем в задний буфер,
    // а glutSwapBuffers() показывает его — так анимация не мерцает.
    glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_RGBA | GLUT_DOUBLE);

    // Главное окно с чёрно-белым квадратом.
    glutInitWindowSize(600, 600);
    glutInitWindowPosition(60, 60);
    gMainWin = glutCreateWindow("Assignment 2 - main window");
    gMainR = makeRenderer();
    gMainSquare = uploadMesh(gMainR.program, makeSquare());
    gMainCircle = uploadMesh(gMainR.program, makeCircle(0.07f, 0.07f, 48));
    glutDisplayFunc(mainDisplay);
    glutReshapeFunc(reshape);
    glutMouseFunc(mainMouse);
    glutKeyboardFunc(quitKeyboard);
    createMainMenu();

    // Подокно внутри главного окна (левый верхний угол, 180x120) с эллипсом.
    gSubWin = glutCreateSubWindow(gMainWin, 12, 12, 180, 120);
    gSubR = makeRenderer();
    gSubEllipse = uploadMesh(gSubR.program, makeCircle(0.75f, 0.55f, 64));
    glutDisplayFunc(subDisplay);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(quitKeyboard);
    createSubMenu();  // привязано только к подокну

    // Отдельное окно верхнего уровня с кругом и треугольником.
    glutInitWindowSize(450, 450);
    glutInitWindowPosition(700, 60);
    gWin2 = glutCreateWindow("window 2");
    gWin2R = makeRenderer();
    gWin2Circle = uploadMesh(gWin2R.program, makeCircle(1.0f, 1.0f, 64));
    gWin2Triangle = uploadMesh(gWin2R.program, makeTriangle());
    glutDisplayFunc(win2Display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(win2Keyboard);

    if (gShotDir) {
        // Имитируем несколько левых кликов, чтобы на скриншоте были круги (доп. баллы).
        glutSetWindow(gMainWin);
        mainMouse(GLUT_LEFT_BUTTON, GLUT_DOWN, 470, 470);
        mainMouse(GLUT_LEFT_BUTTON, GLUT_DOWN, 110, 500);
        mainMouse(GLUT_LEFT_BUTTON, GLUT_DOWN, 500, 120);
        // Необязательно: A2_KEYS — клавиши для window 2, A2_MENU / A2_SUBMENU — номера пунктов меню.
        for (const char* k = std::getenv("A2_KEYS"); k && *k; ++k) {
            win2Keyboard(static_cast<unsigned char>(*k), 0, 0);
        }
        for (const char* m = std::getenv("A2_MENU"); m && *m; ++m) {
            mainMenu(*m - '0');
        }
        for (const char* m = std::getenv("A2_SUBMENU"); m && *m; ++m) {
            subMenu(*m - '0');
        }
    }

    std::cout << "Assignment 2 - Part 1\n"
              << "  Main window : right click -> Stop / Start Animation, Square Colors\n"
              << "                left click  -> new breathing circle (extra credit)\n"
              << "  Sub window  : right click -> background color\n"
              << "  window 2    : r g b y o p w -> color of the circle and triangle\n"
              << "  Esc         : quit\n";

    // Запускаем анимацию и главный цикл GLUT.
    gLastTime = glutGet(GLUT_ELAPSED_TIME);
    glutIdleFunc(idle);
    glutMainLoop();
    return 0;
}
