#include "camera.h"
#include "Arduino.h"
#include "CameraOV7670.h"


const uint8_t VERSION            = 0x10;
const uint8_t COMMAND_NEW_FRAME  = 0x01 | VERSION;
const uint8_t COMMAND_DEBUG_DATA = 0x03 | VERSION;

const uint16_t UART_PIXEL_FORMAT_GRAYSCALE = 0x02;

void processGrayscaleFrameBuffered();

const uint16_t lineLength       = 160;
const uint16_t lineCount        = 120;
const uint32_t baud             = 115200; // TODO could increase?? see his code how faster
const uint16_t lineBufferLength = lineLength;
const uint8_t uartPixelFormat   = UART_PIXEL_FORMAT_GRAYSCALE;
CameraOV7670 camera(CameraOV7670::RESOLUTION_QQVGA_160x120, CameraOV7670::PIXEL_YUV422, 17);

uint8_t lineBuffer[lineBufferLength];
uint32_t avgBuffer[lineCount];
uint16_t counter = 0;

// TODO why doesn't changing this to an offset instead of a pointer work
uint8_t* lineBufferSendByte;
bool isLineBufferSendHighByte;
bool isLineBufferByteFormatted;

uint16_t frameCounter                       = 0;
uint16_t processedByteCountDuringCameraRead = 0;


void commandStartNewFrame(uint8_t pixelFormat);
void commandDebugPrint(const String debugText);
uint8_t sendNextCommandByte(uint8_t checksum, uint8_t commandByte);


// could add __attribute__((always_inline)) to these
inline void processNextGrayscalePixelByteInBuffer();
inline uint8_t formatPixelByteGrayscaleFirst(uint8_t byte);
inline uint8_t formatPixelByteGrayscaleSecond(uint8_t byte);
inline void waitForPreviousUartByteToBeSent();
inline bool isUartReady();


// this is called in Arduino setup() function
void initializeScreenAndCamera()
{
    // Enable this for WAVGAT CPUs
    // For UART communiation we want to set WAVGAT Nano to 16Mhz to match Atmel based Arduino
    // CLKPR = 0x80; // enter clock rate change mode
    // CLKPR = 1; // set prescaler to 1. WAVGAT MCU has it 3 by default.

    Serial.begin(baud);
    if (camera.init()) { commandDebugPrint("Camera initialized."); }
    else { commandDebugPrint("Camera initialization failed."); }
}

// this is called in Arduino loop() function
void processFrame()
{
    processedByteCountDuringCameraRead = 0;
    commandStartNewFrame(uartPixelFormat);
    noInterrupts();
    processGrayscaleFrameBuffered();
    interrupts();
    frameCounter++;
    // commandDebugPrint("Frame " + String(frameCounter));
}

// camera
// commandDebugPrint
// lineBufferSenByte, lineBuffer, lineBufferLength,
// isSendWhileBuffering, lineLength, processNextGrayscalePixelByteInBuffer

void processGrayscaleFrameBuffered()
{
    camera.waitForVsync();
    // commandDebugPrint("Vsync");

    camera.ignoreVerticalPadding();
    counter = 0;
    for (uint16_t x = 0; x < lineCount; x++) { avgBuffer[x] = 0; }

    for (uint16_t y = 0; y < lineCount; y++)
    {
        lineBufferSendByte = &lineBuffer[0];

        // removing this gives a very gray image
        camera.ignoreHorizontalPaddingLeft();

        uint64_t sum   = 0;
        uint64_t count = 0;

        for (uint16_t x = 0; x < lineBufferLength; x += 2)
        {
            camera.waitForPixelClockRisingEdge(); // YUV422 grayscale byte
            camera.readPixelByte(lineBuffer[x]);
            camera.waitForPixelClockRisingEdge(); // YUV422 color byte. Ignore.

            camera.waitForPixelClockRisingEdge(); // YUV422 grayscale byte
            camera.readPixelByte(lineBuffer[x + 1]);
            camera.waitForPixelClockRisingEdge(); // YUV422 color byte. Ignore.

            const uint8_t avg = (uint16_t(lineBuffer[x]) + uint16_t(lineBuffer[x + 1])) / 2;
            // const uint8_t avg = sum / /* (x + 1) */ count;

            lineBuffer[x]     = avg;
            lineBuffer[x + 1] = avg;

            lineBuffer[x]     = formatPixelByteGrayscaleFirst(lineBuffer[x]);
            lineBuffer[x + 1] = formatPixelByteGrayscaleSecond(lineBuffer[x + 1]);


            // TODO why doesn't separating the interleaved reading and uploading work
            // }
            // for (uint16_t x = 0; x < lineBufferLength; x += 2)
            // {
            // reads 160 times, but only uploads if uart is ready (~35 times in my testing)
            // so upload remaining bytes later
            // BUT removing this doesn't work? why doesn't later loop handle it
            // but removing just 1 call instead of both works
            processNextGrayscalePixelByteInBuffer();
            // processNextGrayscalePixelByteInBuffer();
        }

        // removing this doesn't seem to break anything
        // camera.ignoreHorizontalPaddingRight();

        // Debug info to get some feedback how mutch data was processed during line read.
        // if (y % 20 == 0)
        // {
        // commandDebugPrint(String(x) + " " + String(lineBufferSendByte - lineBuffer));
        // }

        // Send rest of the line
        // since uart may not be ready, this may run many times more than 160,
        // eg 12662 in my testing (why is it stable at this value)
        while (lineBufferSendByte < &lineBuffer[lineLength])
        {
            processNextGrayscalePixelByteInBuffer();
        }

        if (y % 20 == 0)
        {
            for (uint16_t x = 0; x < lineBufferLength; x++) { avgBuffer[counter] += lineBuffer[x]; }
            counter++;
        }
    }

    String out = "";
    for (uint16_t x = 0; x < counter; x++)
    {
        out += String(int(float(avgBuffer[x]) / lineBufferLength)) + " ";
    }
    commandDebugPrint(out);
}

void processNextGrayscalePixelByteInBuffer()
{
    if (isUartReady())
    {
        UDR0 = *lineBufferSendByte;
        lineBufferSendByte++;
    }
}

// circuitjournal checks parity of the bytes
// eg first byte check:
// https://github.com/indrekluuk/ArduImageCapture/blob/f574505f1c3f92fc2df47e12a4a5ef7b0614a684/src/main/java/com/circuitjournal/capture/ImageCapture.java#L228
// second byte??
// each grayscale Y value is 1 byte
// byte last bits go 0 1 0 1: leftover from YUV but unnecessary for grayscale
uint8_t formatPixelByteGrayscaleFirst(uint8_t pixelByte)
{
    // For the First byte in the parity chek byte pair the last bit is always 0.
    pixelByte &= 0b11111110;
    if (pixelByte == 0)
    {
        // Make pixel color always slightly above 0 since zero is a command marker.
        pixelByte |= 0b00000010;
    }
    return pixelByte;
}

uint8_t formatPixelByteGrayscaleSecond(uint8_t pixelByte)
{
    // For the second byte in the parity chek byte pair the last bit is always 1.
    return pixelByte | 0b00000001;
}


void commandStartNewFrame(uint8_t pixelFormat)
{
    waitForPreviousUartByteToBeSent();
    UDR0 = 0x00; // New command

    waitForPreviousUartByteToBeSent();
    UDR0 = 4; // Command length

    uint8_t checksum = 0;
    checksum         = sendNextCommandByte(checksum, COMMAND_NEW_FRAME);
    checksum = sendNextCommandByte(checksum, lineLength & 0xFF); // lower 8 bits of image width
    checksum = sendNextCommandByte(checksum, lineCount & 0xFF);  // lower 8 bits of image height
    checksum = sendNextCommandByte(checksum,
                                   ((lineLength >> 8) & 0x03)      // higher 2 bits of image width
                                       | ((lineCount >> 6) & 0x0C) // higher 2 bits of image height
                                       | ((pixelFormat << 4) & 0xF0));

    waitForPreviousUartByteToBeSent();
    UDR0 = checksum;
}


void commandDebugPrint(const String debugText)
{
    if (debugText.length() > 0)
    {

        waitForPreviousUartByteToBeSent();
        UDR0 = 0x00; // New commnad

        waitForPreviousUartByteToBeSent();
        UDR0 = debugText.length() + 1; // Command length. +1 for command code.

        uint8_t checksum = 0;
        checksum         = sendNextCommandByte(checksum, COMMAND_DEBUG_DATA);
        for (uint16_t i = 0; i < debugText.length(); i++)
        {
            checksum = sendNextCommandByte(checksum, debugText[i]);
        }

        waitForPreviousUartByteToBeSent();
        UDR0 = checksum;
    }
}


uint8_t sendNextCommandByte(uint8_t checksum, uint8_t commandByte)
{
    waitForPreviousUartByteToBeSent();
    UDR0 = commandByte;
    return checksum ^ commandByte;
}


void waitForPreviousUartByteToBeSent()
{
    while (!isUartReady()); // wait for byte to transmit
}


bool isUartReady() { return UCSR0A & (1 << UDRE0); }
