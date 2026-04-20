#include <raylib.h>

class UI_Size {
public:
    Vector2 screenSize;     // window size
    Vector2 frameSize;      // available region to display image
    
    Image* img = nullptr;
    Vector2 imgOrigSize;    // full resolution image size before scaling and carving
    Rectangle origFrame;  
    Vector2 imgSize;        // full resolution image size after carving
    Vector2 imgScaledSize;  // displayed image size
    Vector2 imgPos;         // position of top left corner
    float imgScale;

    float edgeBuffer;       // blank space around window edge
    float spacer;

    float sliderHeight, sliderWidth;
    float xSlider, ySlider;
    Rectangle rectXSlider;
    Rectangle rectYSlider;
    
    float btnHeight;
    float btnWidth;
    Rectangle rectBtnLoad;
    Rectangle rectBtnSave;

    UI_Size(float screenX, float screenY) {
        screenSize.x = screenX;
        screenSize.y = screenY;

        // default values
        edgeBuffer = 10;
        spacer = 5;
        sliderHeight = 20;   // for xSlider
        sliderWidth = 8;     // for ySlider
        btnHeight = 30;
        btnWidth = 100;

        xSlider = 1.0f;
        ySlider = 1.0f;

        setFrameSize();
        imgOrigSize = frameSize;
        origFrame = {edgeBuffer, edgeBuffer, frameSize.x, frameSize.y};
        imgSize = frameSize;
        imgScale = 1;
        imgScaledSize = frameSize;

        setButtonSize();
        updateSliders();
    }

    void setFrameSize() {
        frameSize = {screenSize.x - edgeBuffer - sliderHeight - edgeBuffer,
                     screenSize.y - edgeBuffer - sliderHeight - spacer - btnHeight - spacer - edgeBuffer};
    }

    void setImageScale() {
        if (img != nullptr) {
            // scale is based off original uncarved size
            float xScale = frameSize.x / imgOrigSize.x;
            float yScale = frameSize.y / imgOrigSize.y;
            imgScale = (xScale < yScale) ? xScale : yScale;

            origFrame.width = imgOrigSize.x*imgScale;
            origFrame.height = imgOrigSize.y*imgScale;

            float x = edgeBuffer + (frameSize.x - origFrame.width) / 2.0f;
            if (x<edgeBuffer) {x=edgeBuffer;}
            origFrame.x = x;

            float y = edgeBuffer + (frameSize.y - origFrame.height) / 2.0f;
            if (y<edgeBuffer) {y=edgeBuffer;}
            origFrame.y = y;

            // imgPos is based off carved image
            imgSize = {(float)img->width, (float)img->height};
            imgScaledSize = {imgSize.x*imgScale, imgSize.y*imgScale};

            imgPos = {origFrame.x, origFrame.y + origFrame.height - imgScaledSize.y};
        }
    }

    void setButtonSize() {
        rectBtnLoad = {((float)screenSize.x/2 - btnWidth - spacer), (screenSize.y - edgeBuffer - btnHeight), btnWidth, btnHeight};
        rectBtnSave = {((float)screenSize.x/2 + spacer), (screenSize.y - edgeBuffer - btnHeight), btnWidth, btnHeight};
    }

    void updateSliders() {
        rectXSlider = {origFrame.x,
                       edgeBuffer + frameSize.y,
                       origFrame.width,
                       sliderHeight};
        rectYSlider = {edgeBuffer + frameSize.x,
                       origFrame.y,
                       sliderHeight,
                       origFrame.height};
    }

    void updateScreenSize() {
        screenSize = {(float)GetScreenWidth(), (float)GetScreenHeight()};
        setFrameSize();
        setImageScale();
        setButtonSize();
        updateSliders();
    }
}; // end UI_Size

float* energyArray(Color* colorArr, int height, int width);

Image genImageEnergy(Image* img_input, bool outputFloat, bool grayscale);

int* minIndex(float* floatArr, int height, int width, bool xDir);

Image genImageSeam(Image* img_input, bool xDir, bool outputEnergy);

Color* removeSeam(Color* colorArr, int height, int width, bool xDir);

Image genImageCarved(Image* img_input, int seams, bool xDir);

void ui_carve(UI_Size* ui, Image* origImage, Image* carvedImage, bool* finished);

// taken from raygui example 'custom_sliders.c'
float GuiVerticalSliderPro(Rectangle bounds, const char *textTop, const char *textBottom, float value, float minValue, float maxValue, int sliderHeight);
