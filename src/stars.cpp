#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <jadel.h>
#include "timer.h"
#include <thread>

static float frameTime;

static float screenWidth = 1280;
static float screenHeight = 720;
static float aspectWidth = 16.0;
static float aspectHeight = 9.0;

#define MATH_PI (3.141592653)
#define TO_RADIANS(deg) (deg * MATH_PI / 180.0)
#define TO_DEGREES(rad) (rad * (180.0 / MATH_PI))

static float starfieldDepth = 10.0f;
static jadel::Mat4 projMatrix(1.0f / aspectWidth, 0.0f, 0.0f, 0.0f,
                              0.0f, 1.0f / aspectHeight, 0.0f, 0.0f,
                              0.0f, 0.0f, 1.0f, 0.0f,
                              0.0f, 0.0f, 0.0f, 1.0f);

#define NUM_STARS (20000)
static bool running = true;

static jadel::Surface starSurface;
static jadel::Surface starSurfaceCore;
static jadel::Surface workingStarSurface;

static jadel::Vec3 camPos;
static float camSpeed = 1.0f;
static float starSpeed = 1.0f;
static float starMinSpeed = -10.0f;
static float starMaxSpeed = 10.0f;

struct Star
{
    jadel::Vec3 pos;
    jadel::Color color;
    float speed;
};

struct Gauge
{
    float minValue;
    float maxValue;
    const float *value;
};

static Gauge starSpeedGauge;
static Gauge visibilityGauge;

static float fStarsVisible = 0;

static Star stars[NUM_STARS];
static Star viewStars[NUM_STARS];

int getRandomInt(int min, int max)
{
    int result = rand() % (max - min) + min;
    return result;
}

bool load_PNG(const char *filename, jadel::Surface *target)
{
    int width;
    int height;
    int channels;
    target->pixels = stbi_load(filename, &width, &height, &channels, 0);
    if (!target->pixels)
        return false;
    for (int i = 0; i < width * height; ++i)
    {
        uint8 *pixel = (uint8 *)target->pixels + (channels * i);
        jadel::flipBytes(pixel, 3);
    }
    target->width = width;
    target->height = height;
    return true;
}

void drawGauge(const Gauge *gauge, jadel::Rectf screenDim)
{
    float range = gauge->maxValue - gauge->minValue;
    float ratio = (*gauge->value - gauge->minValue) / range;
    float gaugeHeight = screenDim.y1 - screenDim.y0;
    static jadel::Color positiveColor = {0.5f, 0.8f, 0, 0};
    static jadel::Color negativeColor = {0.5f, 0.2f, 0, 0.8f};
    static jadel::Color currentColor;
    currentColor = *gauge->value >= 0.0f ? positiveColor : negativeColor;
    jadel::graphicsDrawRectRelative(screenDim, {0.5f, 0.4f, 0.45f, 0});
    jadel::graphicsDrawRectRelative(screenDim.x0, screenDim.y0, screenDim.x1, screenDim.y0 + (gaugeHeight * ratio), currentColor);
}

jadel::Color getRandomStarColor()
{
    // float brightness = (float)getRandomInt(0, 100) / 100.0f;
    jadel::Color result = {1, (float)getRandomInt(40, 100) / 100.0f, (float)getRandomInt(40, 100) / 100.0f, (float)getRandomInt(40, 100) / 100.0f};
    return result;
}

void orderStars()
{
    int i = 0;
    while (i < NUM_STARS - 1)
    {
        if (viewStars[i].pos.z < viewStars[i + 1].pos.z)
        {
            Star temp = viewStars[i + 1];
            viewStars[i + 1] = viewStars[i];
            viewStars[i] = temp;
            if (i > 0)
                --i;
            else
                ++i;
        }
        else
        {
            ++i;
        }
    }
    /*
    for (int j = 0; j < NUM_STARS; ++j)
    {
        jadel::message("%f\n", viewStars[j].pos.z);
    }*/
}

void resetStar(int index, bool randomZ, float zOffset)
{
    if (index < 0 || index >= NUM_STARS)
        return;
    float distFromCenter = (float)getRandomInt(0, 40000) / 10000.0f;
    float angleOfRotation = (float)getRandomInt(0, 36000) / 100.0f;
    float angleRad = TO_RADIANS(angleOfRotation);
    jadel::Mat3 rotation =
        {
            cosf(angleOfRotation), -sinf(angleOfRotation), 0,
            sinf(angleOfRotation), cosf(angleOfRotation), 0,
            0, 0, 1};
    jadel::Vec2 pos2D = rotation.mul(jadel::Vec2(0, distFromCenter));
    Star *star = &stars[index];
    float z;
    if (randomZ)
    {
        z = (float)getRandomInt(10, 10000) * starfieldDepth / 10000.0f;
    }
    else
    {
        z = 0.1f;
    }
    z += zOffset;
    star->pos = jadel::Vec3(pos2D.x, pos2D.y, z);
    star->color = getRandomStarColor();
    star->speed = (float)getRandomInt(1, 5) / 10.0f;
}

void createGauge(float minValue, float maxValue, float *value, Gauge *target)
{
    target->minValue = minValue;
    target->maxValue = maxValue;
    target->value = value;
}

void init()
{
    jadel::inputSetRelativeMouseMode(false);
    load_PNG("res/star1.png", &starSurface);
    load_PNG("res/starcore.png", &starSurfaceCore);
    jadel::graphicsCreateSurface(starSurface.width, starSurface.height, &workingStarSurface);
    jadel::graphicsPushTargetSurface(&workingStarSurface);
    jadel::graphicsSetClearColor(0);
    jadel::graphicsClearTargetSurface();
    jadel::graphicsPopTargetSurface();
    camPos *= 0;
    for (int i = 0; i < NUM_STARS; ++i)
    {
        resetStar(i, true, 0);
    }

    createGauge(starMinSpeed, starMaxSpeed, &starSpeed, &starSpeedGauge);
    createGauge(0, (float)NUM_STARS, &fStarsVisible, &visibilityGauge);
}

bool isRectInbounds(jadel::Rectf rect)
{
    bool result;
    result = {rect.x0 < 1.0f && rect.x1 > -1.0 && rect.y0 < 1.0f && rect.y1 > -1.0f};
    return result;
}

jadel::Color createColorFrom(uint32 col)
{
    uint8 a = col >> 24;
    uint8 r = col >> 16;
    uint8 g = col >> 8;
    uint8 b = col;

    float aF = (float)a / 255.0f;
    float rF = (float)r / 255.0f;
    float gF = (float)g / 255.0f;
    float bF = (float)b / 255.0f;

    jadel::Color result = {aF, rF, gF, bF};
    return result;
}

jadel::Vec2 camAngle(0, 0);
static jadel::Vec3 currentViewForward(0, 0, 1);
static jadel::Vec3 up(0, 1, 0);
static bool mouseLook = false;
static bool starSurfaceMode = true;
static jadel::Vec3 viewForward(0, 0, 1);

void render()
{
    int starsVisible = 0;
    // orderStars();
    jadel::Mat4 translationMatrix =
        {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            -camPos.x, -camPos.y, -camPos.z, 1

        };
    jadel::Mat4 viewHorizontalRotation =
        {
            cosf(TO_RADIANS(camAngle.x)), 0, sinf(TO_RADIANS(camAngle.x)), 0,
            0, 1, 0, 0,
            -sinf(TO_RADIANS(camAngle.x)), 0, cosf(TO_RADIANS(camAngle.x)), 0,
            0, 0, 0, 1};

    jadel::Mat4 viewVerticalRotation =
        {
            1, 0, 0, 0,
            0, cosf(TO_RADIANS(camAngle.y)), -sinf(TO_RADIANS(camAngle.y)), 0,
            0, sinf(TO_RADIANS(camAngle.y)), cosf(TO_RADIANS(camAngle.y)), 0,
            0, 0, 0, 1};

    jadel::Mat4 viewMatrix = translationMatrix.mul(viewHorizontalRotation).mul(viewVerticalRotation);
    jadel::Mat4 MVP = projMatrix.mul(viewMatrix);
    currentViewForward = viewHorizontalRotation.mul(viewForward);

    jadel::graphicsClearTargetSurface();
    for (int i = 0; i < NUM_STARS; ++i)
    {
        Star viewStar = viewStars[i];
        // viewStar.pos += currentViewForward;
        jadel::Vec3 projPosStart = MVP.mul(jadel::Vec3(viewStar.pos.x - 0.005f, viewStar.pos.y - 0.005f, viewStar.pos.z));
        jadel::Vec3 projPosEnd = MVP.mul(jadel::Vec3(viewStar.pos.x + 0.005f, viewStar.pos.y + 0.005f, viewStar.pos.z));

        // jadel::Vec2 viewXZ = viewHorizontalRotation.mul(jadel::Vec2(projPosStart.x, projPosStart.z));

        static float aspect0 = aspectWidth / aspectHeight;
        static float aspect1 = aspectHeight / aspectWidth;
        float zProjX = projPosStart.x / projPosStart.z;
        float zProjY = projPosStart.y / projPosStart.z;
        jadel::Rectf screenRect; // = {zProjX, zProjY, zProjX + 0.001f / projPosStart.z, zProjY + 0.001f / projPosStart.z * aspect0};
        if (projPosStart.z > 0.01f)
        {
            if (projPosStart.z < 1.5f && starSurfaceMode)
            {
                screenRect = {zProjX, zProjY, zProjX + 0.009f / projPosStart.z, zProjY + 0.008f / projPosStart.z * aspect0};
                if (isRectInbounds(screenRect))
                {
                    jadel::graphicsBlitRelative(&starSurface, screenRect);
                    // jadel::graphicsBlitRelative(&starSurfaceCore, screenRect);

                    ++starsVisible;
                }
            }
            else
            {
                screenRect = {zProjX, zProjY, zProjX + 0.001f / projPosStart.z, zProjY + 0.001f / projPosStart.z * aspect0};
                if (isRectInbounds(screenRect))
                {
                    jadel::graphicsDrawRectRelative(screenRect, viewStar.color);
                    ++starsVisible;
                }
            }
        }
    }

    fStarsVisible = (float)starsVisible;
    jadel::Rectf visibilityGaugeDim = {0.8f, 0.6f, 0.9f, 0.9f};
    jadel::Rectf speedGaugeDim = {0.8f, -0.8f, 0.85f, -0.8f + 1.2f};
    drawGauge(&starSpeedGauge, speedGaugeDim);
    drawGauge(&visibilityGauge, visibilityGaugeDim);
    // jadel::message("Stars visible: %d\n", starsVisible);
}

void tick()
{
    // currentMousePos = jadel::inputGetMouseRelative();
    jadel::Point2i mouseDelta = jadel::inputGetMouseDelta();
    // jadel::message("MouseDX: %d, MouseDY: %d\n", mouseDelta.x, mouseDelta.y);
    int mouseX = jadel::inputGetMouseX();
    int mouseY = jadel::inputGetMouseY();
    // jadel::message("X: %d, Y: %d\n", mouseX, mouseY);
    if (mouseLook)
    {
        camAngle.y += (float)mouseDelta.y * 0.5f;
        camAngle.x += (float)mouseDelta.x * 0.5f;
    }
    while (camAngle.x > 180.0f)
        camAngle.x -= 360.0f;
    while (camAngle.y > 90.0f)
        camAngle.y = 90.0f;
    while (camAngle.x < -180.0f)
        camAngle.x += 360.0f;
    while (camAngle.y < -90.0f)
        camAngle.y = -90.0f;
    // jadel::message("x: %f, y: %f, z: %f\n", camPos.x, camPos.y, camPos.z);
    // jadel::message("XForward: %f, YForward: %f, ZForward: %f\n", currentViewForward.x, currentViewForward.y, currentViewForward.z);
    // jadel::message("XAngle: %f, YAngle: %f\n", camAngle.x, camAngle.y);

    jadel::Vec3 left = currentViewForward.cross(up);
    static float currentSpeed = camSpeed;
    if (jadel::inputIsKeyTyped(jadel::KEY_SHIFT))
    {
        currentSpeed = camSpeed * 2.5f;
    }
    else if (jadel::inputIsKeyReleased(jadel::KEY_SHIFT))
    {
        currentSpeed = camSpeed;
    }
    if (jadel::inputIsKeyPressed(jadel::KEY_CONTROL))
    {
        camPos.y -= currentSpeed * frameTime;
    }
    if (jadel::inputIsKeyPressed(jadel::KEY_SPACE))
    {
        camPos.y += currentSpeed * frameTime;
    }
    if (jadel::inputIsKeyTyped(jadel::KEY_G))
    {
        starSurfaceMode = !starSurfaceMode;
    }
    if (jadel::inputIsKeyTyped(jadel::KEY_Q) || jadel::inputIsMouseRightClicked())
    {
        mouseLook = !mouseLook;
        jadel::inputSetRelativeMouseMode(mouseLook);
        jadel::inputSetCursorVisible(!mouseLook);
    }
    if (jadel::inputIsKeyPressed(jadel::KEY_LEFT))
    {
        camAngle.x -= 2.5f;
    }
    if (jadel::inputIsKeyPressed(jadel::KEY_RIGHT))
    {
        camAngle.x += 2.5f;
    }
    if (jadel::inputIsKeyPressed(jadel::KEY_A))
    {
        // camPos.x -= camSpeed * frameTime;
        camPos.z -= left.z * (currentSpeed * frameTime);
        camPos.x += left.x * (currentSpeed * frameTime);
    }
    if (jadel::inputIsKeyPressed(jadel::KEY_D))
    {
        camPos.z += left.z * (currentSpeed * frameTime);
        camPos.x -= left.x * (currentSpeed * frameTime);
    }
    if (jadel::inputIsKeyPressed(jadel::KEY_DOWN))
    {
        // camPos.y -= camSpeed * frameTime;
        camAngle.y += 2.5f;
    }
    if (jadel::inputIsKeyPressed(jadel::KEY_UP))
    {
        // camPos.y += camSpeed * frameTime;
        camAngle.y -= 2.5f;
    }
    if (jadel::inputIsKeyPressed(jadel::KEY_S))
    {
        // camPos.z -= camSpeed * frameTime;
        camPos.z -= currentViewForward.z * (currentSpeed * frameTime);
        camPos.x += currentViewForward.x * (currentSpeed * frameTime);
    }
    if (jadel::inputIsKeyPressed(jadel::KEY_W))
    {
        // camPos.z += camSpeed * frameTime;
        camPos.z += currentViewForward.z * (currentSpeed * frameTime);
        camPos.x -= currentViewForward.x * (currentSpeed * frameTime);
        // camPos += currentViewForward;
    }
    if (jadel::inputIsKeyPressed(jadel::KEY_Z))
    {
        starSpeed -= 1.0f * frameTime;
        if (starSpeed < starMinSpeed)
            starSpeed = starMinSpeed;
    }
    if (jadel::inputIsKeyPressed(jadel::KEY_X))
    {
        starSpeed += 1.0f * frameTime;
        if (starSpeed > starMaxSpeed)
        {
            starSpeed = starMaxSpeed;
        }
    }
    if (jadel::inputIsKeyTyped(jadel::KEY_P))
    {
        running = !running;
    }

    for (int i = 0; i < NUM_STARS; ++i)
    {
        Star *star = &stars[i];
        if (running)
        {
            star->pos.z += (starSpeed * star->speed) * frameTime;
        }
        if (starSpeed > 0 && star->pos.z > 10.0f)
        {
            resetStar(i, false, star->pos.z - 10.0f);
        }
        else if (starSpeed < 0.0f && star->pos.z < 0.0f)
        {
            resetStar(i, false, star->pos.z + 10.0f);
        }

        Star *viewStar = &viewStars[i];
        *viewStar = *star;
        viewStar->pos = star->pos;
    }
    render();
}

int JadelMain()
{
    if (!JadelInit(MB(500)))
    {
        jadel::message("Jadel init failed!\n");
        return 0;
    }
    jadel::allocateConsole();
    srand(time(NULL));

    jadel::Window window;
    jadel::windowCreate(&window, "Stars", screenWidth, screenHeight);
    jadel::Surface winSurface;
    jadel::graphicsCreateSurface(screenWidth, screenHeight, &winSurface);
    uint32 *winPixels = (uint32 *)winSurface.pixels;

    jadel::graphicsPushTargetSurface(&winSurface);
    jadel::graphicsSetClearColor(0);
    jadel::graphicsClearTargetSurface();

    init();
    Timer frameTimer;
    frameTimer.start();
    uint32 elapsedInMillis = 0;
    uint32 minFrameTime = 1000 / 165;
    while (true)
    {
        JadelUpdate();
        tick();
        jadel::windowUpdate(&window, &winSurface);

        elapsedInMillis = frameTimer.getMillisSinceLastUpdate();
        if (elapsedInMillis < minFrameTime)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(minFrameTime - elapsedInMillis));
        }
        if (elapsedInMillis > 0)
        {
            frameTime = (float)frameTimer.getMillisSinceLastUpdate() * 0.001f;
        }

        uint32 debugTime = frameTimer.getMillisSinceLastUpdate();
        // jadel::message("%f\n", frameTime);

        frameTimer.update();
        if (jadel::inputIsKeyPressed(jadel::KEY_ESCAPE))
            return 0;
    }
    return 0;
}