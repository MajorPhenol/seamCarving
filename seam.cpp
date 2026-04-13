#include <raylib.h> /* v6.0 */
#include <stdio.h>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h> /* v5.0 */
#undef RAYGUI_IMPLEMENTATION    // Avoid including raygui implementation again

#define GUI_WINDOW_FILE_DIALOG_IMPLEMENTATION
#include "gui_window_file_dialog.h"

#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <climits>
#include <cfloat>

float* energyArray(Color* colorArr, int height, int width) {
    float* floatArr = new float[height*width]{};

    Color clrCenter, clrDown, clrRight;
    float energy, dx, dy;
    float dR, dG, dB;

    // image pixels loop
    for (int y=0; y<height; y++){
        for (int x=0; x<width; x++){

            // handle bottom row of image
            if (y == height-1) {
                floatArr[y*width + x] = floatArr[(y-1)*width + x];
                continue;
            }
            // handle right edge of image
            if (x == width-1) {
                floatArr[y*width + x] = floatArr[y*width + x - 1];
                continue;
            }

            clrCenter = colorArr[y*width + x];
            clrRight = colorArr[y*width + x + 1];

            dR = clrCenter.r - clrRight.r;
            dG = clrCenter.g - clrRight.g;
            dB = clrCenter.b - clrRight.b;
            dx = sqrt((dR*dR) + (dG*dG) + (dB*dB));

            clrDown = colorArr[(y+1)*width + x];

            dR = clrCenter.r - clrDown.r;
            dG = clrCenter.g - clrDown.g;
            dB = clrCenter.b - clrDown.b;
            dy = sqrt((dR*dR) + (dG*dG) + (dB*dB));

            energy = std::abs(dx) + std::abs(dy);
            floatArr[y*width + x] = energy;
        }
    }

    return floatArr;
} // end energyArray()

Image genImageEnergy(Image* img_input, bool outputFloat, bool grayscale){
    if (img_input->format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8){
        printf("[ERROR] wrong pixel format: %i\n", img_input->format);
        return Image{};
    }

    int height = img_input->height;
    int width = img_input->width;

    Color* colorArr = (Color*)img_input->data;
    float* floatArr = energyArray(colorArr, height, width);

    // return raw float energy values
    if (outputFloat == true){
        return Image{(void*)floatArr, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R32};
    }

    float max = 0;
    float min = FLT_MAX;

    for (int y=0; y<height; y++){
        for (int x=0; x<width; x++){
            if (floatArr[y*width + x] > max) {max = floatArr[y*width + x];}
            if (floatArr[y*width + x] < min) {min = floatArr[y*width + x];}
        }
    }

    float range = max - min;

    if (grayscale == true) {   /* return grayscale image */
        uint8_t* outArr = new uint8_t[height*width]{};

        for (int y=0; y<height; y++){
            for (int x=0; x<width; x++){
                // normalize pixel value
                outArr[y*width + x] = (uint8_t)( (floatArr[y*width + x]-min) / range * 255 );
            }
        }

        return Image{(void*)outArr, width, height, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};
    }

    else {  /* convert float to range of colors */
        float pixel;
        Color clrOut;
        Color* outArr = new Color[height*width]{};

        for (int y=0; y<height; y++){
            for (int x=0; x<width; x++){
                // normalize pixel value
                pixel = (floatArr[y*width + x] - min) / range;
                // convert to blue-red range
                clrOut.r = 255 * pixel;
                clrOut.g = 0;
                clrOut.b = 127 * (1 - pixel);
                clrOut.a = 255;

                outArr[y*width + x] = clrOut;
            }
        }

        return Image{(void*)outArr, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    }
} // genImageEnergy()

// returns an array of the pixel indices to remove
int* minIndex(float* floatArr, int height, int width, bool xDir){
    float* seamArr = floatArr;
    int* index;

    if (xDir) {
        index = new int[height]{};

        // find seam of cumulative min values
        for (int y=1; y<height; y++) {
            for (int x=0; x<width; x++) {
                if (x==0 ) {
                    seamArr[y*width + x] += std::fmin(seamArr[(y-1)*width + x],
                                                         seamArr[(y-1)*width + x + 1]);
                }
                else if (x==width-1) {
                    seamArr[y*width + x] += std::fmin(seamArr[(y-1)*width + x - 1],
                                                         seamArr[(y-1)*width + x]);
                }
                else {
                    seamArr[y*width + x] += std::fmin(
                                                std::fmin(seamArr[(y-1)*width + x - 1],
                                                          seamArr[(y-1)*width + x]),
                                                          seamArr[(y-1)*width + x + 1]);
                }
            }
        }

        // find min value of bottom row for start of seam
        int x_min = 0;
        float minBottom = FLT_MAX;

        for (int x=0; x<width; x++) {
            if (seamArr[(height-1)*width + x] < minBottom) {
                minBottom = seamArr[(height-1)*width + x];
                x_min = x;
            }
        }

        index[height-1] = x_min;

        for (int y=height-2; y>=0; y--) {
            if (x_min==0 ) {
                x_min = seamArr[y*width + x_min] < seamArr[y*width + x_min + 1] ? x_min : x_min+1;
            }
            else if (x_min==width-1) {
                x_min = seamArr[y*width + x_min - 1] < seamArr[y*width + x_min] ? x_min-1 : x_min;
            }
            else {
                x_min = seamArr[y*width + x_min - 1] < seamArr[y*width + x_min] ? x_min-1 : x_min;
                x_min = seamArr[y*width + x_min] < seamArr[y*width + x_min + 1] ? x_min : x_min+1;
            }

            index[y] = x_min;
        }
    }
    else {
        index = new int[width]{};

        // find seam of cumulative min values
        for (int x=1; x<width; x++) {
            for (int y=0; y<height; y++) {
                if (y==0 ) {
                    seamArr[y*width + x] += std::fmin(seamArr[(y)*width + x - 1],
                                                      seamArr[(y+1)*width + x - 1]);
                }
                else if (y==height-1) {
                    seamArr[y*width + x] += std::fmin(seamArr[(y-1)*width + x - 1],
                                                      seamArr[(y)*width + x - 1]);
                }
                else {
                    seamArr[y*width + x] += std::fmin(
                                                std::fmin(seamArr[(y-1)*width + x - 1],
                                                          seamArr[(y)*width + x - 1]),
                                                          seamArr[(y+1)*width + x - 1]);
                }
            }
        }
        // find min value of right row for start of seam
        int y_min = 0;
        float minRight = FLT_MAX;

        for (int y=0; y<height; y++) {
            if (seamArr[(y)*width + (width - 1)] < minRight) {
                minRight = seamArr[(y)*width + (width - 1)];
                y_min = y;
            }
        }

        index[width-1] = y_min;

        for (int x=width-2; x>=0; x--) {
            if (y_min==0 ) {
                y_min = seamArr[y_min*width + x] < seamArr[(y_min+1)*width + x] ? y_min : y_min+1;
            }
            else if (y_min==height-1) {
                y_min = seamArr[(y_min-1)*width + x] < seamArr[y_min*width + x] ? y_min-1 : y_min;
            }
            else {
                y_min = seamArr[(y_min-1)*width + x] < seamArr[y_min*width + x] ? y_min-1 : y_min;
                y_min = seamArr[y_min*width + x] < seamArr[(y_min+1)*width + x] ? y_min : y_min+1;
            }

            index[x] = y_min;
        }
    }

    return index;
} // end minIndex()

Image genImageSeam(Image* img_input, bool xDir, bool outputEnergy) {
    int height = img_input->height;
    int width = img_input->width;

    float* energyArr = energyArray((Color*)img_input->data, height, width);
    int* seamIndex = minIndex(energyArr, height, width, xDir);

    Color* colorArr;
    if (outputEnergy) { /* overlay seam on energy image */
        Image colorImg = genImageEnergy(img_input, false, false);
        colorArr = (Color*)colorImg.data;
    }
    else { /* overlay seam on original image */
        colorArr = (Color*)img_input->data;
    }

    Color seamColor = Color{0, 255, 0, 255};

    if (xDir) {
        for (int i=0; i<height; i++) {
            colorArr[i*width + seamIndex[i]] = seamColor;
        }
    }
    else {
        for (int i=0; i<width; i++) {
            colorArr[seamIndex[i]*width + i] = seamColor;
        }
    }

    return Image{(void*)colorArr, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
} // end genImageSeam()

Color* removeSeam(Color* colorArr, int height, int width, bool xDir) {
    float* energyArr = energyArray(colorArr, height, width);
    int* seamIndex = minIndex(energyArr, height, width, xDir);

    int newHeight, newWidth;
    
    if (xDir == true){
        newHeight = height;
        newWidth = width - 1;
    }
    else {
        newHeight = height -1;
        newWidth = width;
    }

    Color* newColorArr = new Color[newHeight*newWidth]{};

    if (xDir) { /* shrink horizontally */
        for (int y=0; y<height; y++) {
            int x_new = 0;
            for (int x=0; x<width; x++) {
                if (x == seamIndex[y]) {
                    continue;
                }
                else {
                    newColorArr[y*newWidth + x_new] = colorArr[y*width + x];
                    x_new++;
                }
            }
        }
    }
    else { /* shrink vertically */
        for (int x=0; x<width; x++) {
            int y_new = 0;
            for (int y=0; y<height; y++) {
                if (y == seamIndex[x]) {
                    continue;
                }
                else {
                    newColorArr[y_new*newWidth + x] = colorArr[y*width + x];
                    y_new++;
                }
            }
        }
    }

    return newColorArr;
} // end removeSeam()

Image genImageCarved(Image* img_input, int seams, bool xDir) {
    int height = img_input->height;
    int width = img_input->width;

    Color* colorArr = (Color*)img_input->data;

    for (int i=0; i<seams; i++) {
        colorArr = removeSeam(colorArr, height, width, xDir);

        if (xDir) {width--;}
        else {height--;}
    }

    return Image{(void*)colorArr, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
} // end genImageCarved()

float GuiVerticalSliderPro(Rectangle bounds, const char *textTop, const char *textBottom, float value, float minValue, float maxValue, int sliderHeight);

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
    
    float btnLoadHeight;
    float btnLoadWidth;
    Rectangle rectBtnLoad;

    UI_Size(float screenX, float screenY) {
        screenSize.x = screenX;
        screenSize.y = screenY;

        // default values
        edgeBuffer = 10;
        spacer = 5;
        sliderHeight = 20;   // for xSlider
        sliderWidth = 8;     // for ySlider
        btnLoadHeight = 30;
        btnLoadWidth = 100;

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
                     screenSize.y - edgeBuffer - sliderHeight - spacer - btnLoadHeight - spacer - edgeBuffer};
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
        rectBtnLoad = {((float)screenSize.x/2 - btnLoadWidth/2), (screenSize.y - edgeBuffer - btnLoadHeight), btnLoadWidth, btnLoadHeight};
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

void ui_carve(UI_Size* ui, Image* origImage, Image* carvedImage, Texture2D* tex) {
    if (ui->img != nullptr) {
        int xSeams = (int)( (1-ui->xSlider) * origImage->width);
        *carvedImage = genImageCarved(origImage, xSeams, true);

        int ySeams = (int)( (1-ui->ySlider) * origImage->height);
        *carvedImage = genImageCarved(carvedImage, ySeams, false);

        ui->img = carvedImage;
        ui->updateScreenSize();
        *tex = LoadTextureFromImage(*carvedImage);
    }
} // end ui_carve()

int main(void) {

    float screenX = 1024;
    float screenY = 512;

    const Color clrBG{130, 130, 130, 255};
    const Color clrLine{145, 200, 210, 255};
    const unsigned int intClrLine = (clrLine.r << 24 |
                                     clrLine.g << 16 |
                                     clrLine.b << 8  |
                                     clrLine.a << 0);

    float lineThck = 2;

    UI_Size ui{screenX, screenY};

    float prevY = ui.ySlider;
    bool sliderChanged = false;

    InitWindow((int)ui.screenSize.x, (int)ui.screenSize.y, "Seam Carving");
    SetTargetFPS(60);
    SetWindowState(FLAG_WINDOW_RESIZABLE);

    // NOTE: Textures MUST be loaded after Window initialization (OpenGL context is required)
    Image origImage{};
    Image carvedImage{};
    char imgFileName[512] = { 0 };
    Texture2D tex{};

    GuiWindowFileDialogState fileDialogState = InitGuiWindowFileDialog(GetWorkingDirectory());

    while (!WindowShouldClose()) {
        // check if window was resized
        if ((int)ui.screenSize.x != GetScreenWidth() || (int)ui.screenSize.y != GetScreenHeight()) {
            ui.updateScreenSize();
        }

        // check if new image file was loaded
        if (fileDialogState.SelectFilePressed) {
            // Load image file (if supported extension)
            if (IsFileExtension(fileDialogState.fileNameText, ".png")) {
                strcpy(imgFileName, TextFormat("%s" PATH_SEPERATOR "%s", fileDialogState.dirPathText, fileDialogState.fileNameText));
                UnloadImage(origImage);
                UnloadTexture(tex);
                
                origImage = LoadImage(imgFileName);
                ImageFormat(&origImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
                ui.img = &origImage;
                ui.imgOrigSize = {(float)origImage.width, (float)origImage.height};
                ui.updateScreenSize();
                ui.xSlider = 1;
                ui.ySlider = 1;
                
                tex = LoadTextureFromImage(origImage);
            }

            fileDialogState.SelectFilePressed = false;
        }

        /* Draw *************************************************************/
        BeginDrawing();
            ClearBackground(clrBG);

            // "Load" button
            if (fileDialogState.windowActive) { GuiLock(); }

            if (GuiButton(ui.rectBtnLoad, "Load Image")) {
                fileDialogState.windowActive = true;
            }
            GuiUnlock();
            
            // x slider
            GuiSetStyle(SLIDER, BASE_COLOR_PRESSED, intClrLine);
            GuiSetStyle(SLIDER, SLIDER_PADDING, 2);
            GuiSetStyle(SLIDER, SLIDER_WIDTH, ui.sliderWidth);
            if (GuiSlider(ui.rectXSlider, "", "", &ui.xSlider, 0, 1.0f)) { sliderChanged = true; }

            // y slider
            ui.ySlider = GuiVerticalSliderPro(ui.rectYSlider, "", "", ui.ySlider, 0.0f, 1.0f, ui.sliderWidth);
            if (prevY != ui.ySlider) {
                sliderChanged = true;
                prevY = ui.ySlider;
            }

            if (sliderChanged) {
                GuiLock();
                sliderChanged = false;
                ui_carve(&ui, &origImage, &carvedImage, &tex);
                GuiUnlock();
            }

            // image frame
            // DrawRectangleLines(ui.edgeBuffer, ui.edgeBuffer, ui.frameSize.x, ui.frameSize.y, RED);
            DrawRectangleLinesEx(ui.origFrame, lineThck, clrLine);

            // draw image
            if (tex.width > 0) {
                DrawTextureEx(tex, ui.imgPos, 0.0f, ui.imgScale, WHITE);

                // draw lines from sliders around image
                DrawLineEx({ui.origFrame.x + ui.imgScaledSize.x, ui.origFrame.y},
                           {ui.origFrame.x + ui.imgScaledSize.x, ui.origFrame.y + ui.origFrame.height},
                           1.0,
                           clrLine);
                DrawLineEx({ui.origFrame.x, ui.imgPos.y},
                           {ui.origFrame.x + ui.origFrame.width, ui.imgPos.y},
                           1.0,
                           clrLine); 

            
                // DrawText(TextFormat("xSlider: %f", ui.xSlider),                            20, 20, 20, BLUE);
                // DrawText(TextFormat("ySlider: %f", ui.ySlider),                            20, 40, 20, BLUE);


                // DrawText(TextFormat("img.width: %i", ui.img->width),                    20, 40, 20, BLUE);
                // DrawText(TextFormat("xScale : %f", ui.frameSize.x / ui.img->width),     20, 60, 20, BLUE);
                // DrawText(TextFormat("frameSize.y: %1.f", ui.frameSize.y),               20, 80, 20, BLUE);
                // DrawText(TextFormat("img.height: %i", ui.img->height),                  20, 100, 20, BLUE);
                // DrawText(TextFormat("yScale : %f", ui.frameSize.y / ui.img->height),    20, 120, 20, BLUE);

                // DrawText(TextFormat("imgScaleSize.x: %f", ui.imgScaledSize.x),          20, 140, 20, BLUE);
                // DrawText(TextFormat("imgScaleSize.y: %f", ui.imgScaledSize.y),          20, 160, 20, BLUE);

            }


            // GUI: Dialog Window
            //--------------------------------------------------------------------------------
            GuiWindowFileDialog(&fileDialogState);

        EndDrawing();
    }

    // unload textures
    UnloadImage(origImage);
    UnloadTexture(tex);

    CloseWindow();

    return 0;

} // end main()

float GuiVerticalSliderPro(Rectangle bounds, const char *textTop, const char *textBottom, float value, float minValue, float maxValue, int sliderHeight)
{
    GuiState state = (GuiState)GuiGetState();

    int sliderValue = (int)(((value - minValue)/(maxValue - minValue)) * (bounds.height - 2 * GuiGetStyle(SLIDER, BORDER_WIDTH)));

    Rectangle slider = {
        bounds.x + GuiGetStyle(SLIDER, BORDER_WIDTH) + GuiGetStyle(SLIDER, SLIDER_PADDING),
        bounds.y + bounds.height - sliderValue,
        bounds.width - 2*GuiGetStyle(SLIDER, BORDER_WIDTH) - 2*GuiGetStyle(SLIDER, SLIDER_PADDING),
        0.0f,
    };

    if (sliderHeight > 0)        // Slider
    {
        slider.y -= (float)sliderHeight/2;
        slider.height = (float)sliderHeight;
    }
    else if (sliderHeight == 0)  // SliderBar
    {
        slider.y -= GuiGetStyle(SLIDER, BORDER_WIDTH);
        slider.height = (float)sliderValue;
    }
    // Update control
    //--------------------------------------------------------------------
    if ((state != STATE_DISABLED) && !guiLocked)
    {
        Vector2 mousePoint = GetMousePosition();

        if (CheckCollisionPointRec(mousePoint, bounds))
        {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
            {
                state = STATE_PRESSED;

                // Get equivalent value and slider position from mousePoint.x
                float normalizedValue = (bounds.y + bounds.height - mousePoint.y - ((float)sliderHeight / 2)) / (bounds.height - (float)sliderHeight);
                value = (maxValue - minValue) * normalizedValue + minValue;

                if (sliderHeight > 0) slider.y = mousePoint.y - slider.height / 2;  // Slider
                else if (sliderHeight == 0)                                          // SliderBar
                {
                    slider.y = mousePoint.y;
                    slider.height = bounds.y + bounds.height - slider.y - GuiGetStyle(SLIDER, BORDER_WIDTH);
                }
            }
            else state = STATE_FOCUSED;
        }

        if (value > maxValue) value = maxValue;
        else if (value < minValue) value = minValue;
    }


    // Bar limits check
    if (sliderHeight > 0)        // Slider
    {
        if (slider.y < (bounds.y + GuiGetStyle(SLIDER, BORDER_WIDTH))) slider.y = bounds.y + GuiGetStyle(SLIDER, BORDER_WIDTH);
        else if ((slider.y + slider.height) >= (bounds.y + bounds.height)) slider.y = bounds.y + bounds.height - slider.height - GuiGetStyle(SLIDER, BORDER_WIDTH);
    }
    else if (sliderHeight == 0)  // SliderBar
    {
        if (slider.y < (bounds.y + GuiGetStyle(SLIDER, BORDER_WIDTH)))
        {
            slider.y = bounds.y + GuiGetStyle(SLIDER, BORDER_WIDTH);
            slider.height = bounds.height - 2*GuiGetStyle(SLIDER, BORDER_WIDTH);
        }
    }

    //--------------------------------------------------------------------
    // Draw control
    //--------------------------------------------------------------------
    GuiDrawRectangle(bounds, GuiGetStyle(SLIDER, BORDER_WIDTH), Fade(GetColor(GuiGetStyle(SLIDER, BORDER + (state*3))), guiAlpha), Fade(GetColor(GuiGetStyle(SLIDER, (state != STATE_DISABLED)?  BASE_COLOR_NORMAL : BASE_COLOR_DISABLED)), guiAlpha));

    // Draw slider internal bar (depends on state)
    if ((state == STATE_NORMAL) || (state == STATE_PRESSED)) GuiDrawRectangle(slider, 0, BLANK, Fade(GetColor(GuiGetStyle(SLIDER, BASE_COLOR_PRESSED)), guiAlpha));
    else if (state == STATE_FOCUSED) GuiDrawRectangle(slider, 0, BLANK, Fade(GetColor(GuiGetStyle(SLIDER, TEXT_COLOR_FOCUSED)), guiAlpha));

    // Draw top/bottom text if provided
    if (textTop != NULL)
    {
        Rectangle textBounds = { 0 };
        textBounds.width = (float)GuiGetTextWidth(textTop);
        textBounds.height = (float)GuiGetStyle(DEFAULT, TEXT_SIZE);
        textBounds.x = bounds.x + bounds.width/2 - textBounds.width/2;
        textBounds.y = bounds.y - textBounds.height - GuiGetStyle(SLIDER, TEXT_PADDING);

        GuiDrawText(textTop, textBounds, TEXT_ALIGN_RIGHT, Fade(GetColor(GuiGetStyle(SLIDER, TEXT + (state*3))), guiAlpha));
    }

    if (textBottom != NULL)
    {
        Rectangle textBounds = { 0 };
        textBounds.width = (float)GuiGetTextWidth(textBottom);
        textBounds.height = (float)GuiGetStyle(DEFAULT, TEXT_SIZE);
        textBounds.x = bounds.x + bounds.width/2 - textBounds.width/2;
        textBounds.y = bounds.y + bounds.height + GuiGetStyle(SLIDER, TEXT_PADDING);

        GuiDrawText(textBottom, textBounds, TEXT_ALIGN_LEFT, Fade(GetColor(GuiGetStyle(SLIDER, TEXT + (state*3))), guiAlpha));
    }
    //--------------------------------------------------------------------

    return value;
} // end GuiVerticalSliderPro()
