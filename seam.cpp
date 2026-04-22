#include "seam.h"
#include <raylib.h> /* v6.0 */

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
#include <thread>
#include <vector>

int main(void) {
    /* UI *********************************************************************/
    float screenX = 1024;
    float screenY = 512;

    const Color clrBG{100, 100, 100, 255};
    const Color clrLine{145, 200, 210, 255};
    const unsigned int intClrLine = (clrLine.r << 24 |
                                     clrLine.g << 16 |
                                     clrLine.b << 8  |
                                     clrLine.a << 0);

    float lineThck = 2;

    UI_Size ui{screenX, screenY};
    /**************************************************************************/

    float prevY = ui.ySlider;
    bool sliderChanged = false;

    std::thread thread;
    bool finished = true;   // to check if thread is done

    InitWindow((int)ui.screenSize.x, (int)ui.screenSize.y, "Seam Carving");
    SetTargetFPS(60);
    SetWindowState(FLAG_WINDOW_RESIZABLE);

    // NOTE: Textures MUST be loaded after Window initialization (OpenGL context is required)
    char imgFileName[512] = { 0 };
    Image origImage{};
    Image carvedImage{};
    std::vector<Color> colorVec;
    // carvedImage.data = colorVec.data();
    Texture2D tex{};

    GuiWindowFileDialogState fileDialogState = InitGuiWindowFileDialog(GetWorkingDirectory());

    while (!WindowShouldClose()) {
        // check if window was resized
        if ((int)ui.screenSize.x != GetScreenWidth() || (int)ui.screenSize.y != GetScreenHeight()) {
            ui.updateScreenSize();
        }

        // handle popup window
        if (fileDialogState.SelectFilePressed) {
            // save new file
            if (fileDialogState.saveFileMode) {
                if (IsFileExtension(fileDialogState.fileNameText, ".png")) {
                    strcpy(imgFileName, TextFormat("%s" PATH_SEPERATOR "%s", fileDialogState.dirPathText, fileDialogState.fileNameText));
                    ExportImage(LoadImageFromTexture(tex), imgFileName);
                }
            }
            // load image
            else {
                if (IsFileExtension(fileDialogState.fileNameText, ".png")) {
                    strcpy(imgFileName, TextFormat("%s" PATH_SEPERATOR "%s", fileDialogState.dirPathText, fileDialogState.fileNameText));
                    UnloadImage(origImage);
                    UnloadTexture(tex);
                    
                    origImage = LoadImage(imgFileName);
                    ImageFormat(&origImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

                    carvedImage = origImage;
                    ui.img = &carvedImage;
                    ui.imgOrigSize = {(float)origImage.width, (float)origImage.height};
                    ui.updateScreenSize();
                    ui.xSlider = 1;
                    ui.ySlider = 1;
                    
                    tex = LoadTextureFromImage(origImage);
                }
            }

            fileDialogState.SelectFilePressed = false;
        }

        /* Draw *************************************************************/
        BeginDrawing();
            ClearBackground(clrBG);

            // draw "Load" button
            if (GuiButton(ui.rectBtnLoad, "Load Image")) {
                fileDialogState.windowActive = true;
                fileDialogState.saveFileMode = false;
            }

            // draw "Save" button
            if (GuiButton(ui.rectBtnSave, "Save Image")) {
                fileDialogState.windowActive = true;
                fileDialogState.saveFileMode = true;
            }
            
            // draw x slider
            GuiSetStyle(SLIDER, BASE_COLOR_PRESSED, intClrLine);
            GuiSetStyle(SLIDER, SLIDER_PADDING, 2);
            GuiSetStyle(SLIDER, SLIDER_WIDTH, ui.sliderWidth);
            if (GuiSlider(ui.rectXSlider, "", "", &ui.xSlider, 0, 1.0f)) { sliderChanged = true; }

            // draw y slider
            ui.ySlider = GuiVerticalSliderPro(ui.rectYSlider, "", "", ui.ySlider, 0.0f, 1.0f, ui.sliderWidth);
            if (prevY != ui.ySlider) {
                sliderChanged = true;
                prevY = ui.ySlider;
            }

            // check if image needs to be updated
            if (finished) {
                if (thread.joinable()) {
                    thread.join();
                    ui.updateScreenSize();
                    tex = LoadTextureFromImage(carvedImage);
                }

                if (sliderChanged) {
                    sliderChanged = false;
                    thread = std::thread(ui_carve, std::ref(ui), std::ref(origImage), std::ref(carvedImage), std::ref(colorVec), std::ref(finished));
                }
            }

            // draw image frame
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

                if (!finished) {
                    DrawText("processing...", ui.origFrame.x, ui.origFrame.y + ui.origFrame.height + ui.sliderHeight + 2, 16, clrLine);
                }
            }
            
            // popup window
            GuiUnlock();
            GuiWindowFileDialog(&fileDialogState);
            if (fileDialogState.windowActive) { GuiLock(); }

        EndDrawing();
    }

    // unload textures
    UnloadImage(origImage);
    UnloadImage(carvedImage);
    UnloadTexture(tex);

    CloseWindow();

    return 0;
} // end main()

/** Creates an array of 'energy values' for an image.
 * 
 * @colorArr: Array of pixels from Image.data
 * @height: Image height
 * @width: Image width
 * @return: [height x width] array of energy values
 */
std::vector<float> energyArray(std::vector<Color>& colorArr, int height, int width) {
    std::vector<float> energyArr(height*width);

    Color clrCenter, clrDown, clrRight;
    float dx, dy;
    float dR, dG, dB;

    // image pixels loop
    for (int y=0; y<height-1; y++){
        for (int x=0; x<width-1; x++){

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

            // energy = dx + dy
            energyArr[y*width + x] = dx + dy;
        }
    }

    // handle bottom row of image
    for (int x=0; x<width; x++) {
        energyArr[(height-1)*width + x] = energyArr[((height-2))*width + x];
    }

    // handle right edge of image
    for (int y=0; y<height-1; y++) {
        energyArr[y*width + width - 1] = energyArr[y*width + width - 2];
    }

    return energyArr;
} // end energyArray()

/** Creates an array of minimum energy pixel indices to remove.
 * 
 * @energyArr: array of energy values from energyArray()
 * @height: Image height
 * @width: Image width
 * @xDir: If true, shrink image horizontally,
 *          else shrink image vertically
 * @return: array of pixel indices
 */
std::vector<int> minIndex(std::vector<float>& energyArr, int height, int width, bool xDir){
    std::vector<float> seamArr = energyArr;
    std::vector<int> index;

    if (xDir) {
        index.resize(height);

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
        index.resize(width);

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

/** Removes seams of lowest energy from image.
 *
 * @colorArr: Array of pixels from Image.data
 * @height: Image height
 * @width: Image width
 * @xDir: If true, shrink image horizontally,
 *          else shrink image vertically
 * @return: Array of pixel colors
 */
std::vector<Color> removeSeam(std::vector<Color>& colorArr, int height, int width, bool xDir) {
    std::vector<float> energyArr = energyArray(colorArr, height, width);
    std::vector<int> seamIndex = minIndex(energyArr, height, width, xDir);

    int newHeight, newWidth;
    
    if (xDir == true){
        newHeight = height;
        newWidth = width - 1;
    }
    else {
        newHeight = height -1;
        newWidth = width;
    }

    std::vector<Color> newColorArr(newHeight*newWidth);

    if (xDir) { // shrink horizontally
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
    else { // shrink vertically
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

/** Called by UI to create carved images.
 * 
 * @ui: UI_Size object that contains info about current window
 * @origImage: Original, 'uncarved' image
 * @carvedImage: Carved image
 * @colorVec: Global vector to hold carved image color data
 * @finished: Used to check if thread is finished executing
 */
void ui_carve(UI_Size& ui, Image& origImage, Image& carvedImage, std::vector<Color>& colorVec, bool& finished) {
    finished = false;

    int height = origImage.height;
    int width = origImage.width;

    int xSeams = (int)( (1-ui.xSlider) * width);
    int ySeams = (int)( (1-ui.ySlider) * height);

    Color* cast = (Color*)origImage.data;
    std::vector<Color> tempColor(cast, cast + (height*width));

    for (int x=0; x<xSeams; x++) {
        tempColor = removeSeam(tempColor, height, width, true);
        width--;
    }
    for (int y=0; y<ySeams; y++) {
        tempColor = removeSeam(tempColor, height, width, false);
        height--;
    }

    colorVec = tempColor;

    carvedImage.data = (void*)colorVec.data();
    carvedImage.width = width;
    carvedImage.height = height;
    carvedImage.mipmaps = 1;
    carvedImage.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    finished = true;
} // end ui_carve()

/** Generates an Image showing pixel 'energy values'
 * 
 * @img_input: Original image
 * @outputFloat: If true, the output image will use raw 32bit energy values.
 * @grayscale: If true, the output image will be 8bit grayscale,
 *              else the image will be in R8G8B8A8 format.
 * @return: New Image
 */
Image genImageEnergy(Image* img_input, bool outputFloat, bool grayscale){
/*
    if (img_input->format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8){
        printf("[ERROR] wrong pixel format: %i\n", img_input->format);
        return Image{};
    }

    int height = img_input->height;
    int width = img_input->width;

    Color* colorArr = (Color*)img_input->data;
    std::vector<float> floatArr = energyArray(colorArr, height, width);

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

    if (grayscale == true) {   // return grayscale image
        uint8_t* outArr = new uint8_t[height*width]{};

        for (int y=0; y<height; y++){
            for (int x=0; x<width; x++){
                // normalize pixel value
                outArr[y*width + x] = (uint8_t)( (floatArr[y*width + x]-min) / range * 255 );
            }
        }

        return Image{(void*)outArr, width, height, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};
    }

    else {  // convert float to range of colors
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
*/
    return Image{};
} // genImageEnergy()

/** Generate an Image showing the seam of lowest energy.
 * 
 * @img_input: Original image
 * @xDir: If true, shrink image horizontally,
 *          else shrink image vertically
 * @outputEnergy: If true, overlay the seam on energy values,
 *                  else overlay seam on original image
 * @return: New Image
 */
Image genImageSeam(Image* img_input, bool xDir, bool outputEnergy) {
/*
    int height = img_input->height;
    int width = img_input->width;

    float* energyArr = energyArray((Color*)img_input->data, height, width);
    int* seamIndex = minIndex(energyArr, height, width, xDir);

    Color* colorArr;
    if (outputEnergy) { // overlay seam on energy image
        Image colorImg = genImageEnergy(img_input, false, false);
        colorArr = (Color*)colorImg.data;
    }
    else { // overlay seam on original image
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
*/
    return Image{};
} // end genImageSeam()

// taken from raygui example 'custom_sliders.c'
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
