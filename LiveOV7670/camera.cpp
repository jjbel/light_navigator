#include "camera.h"
#include "Arduino.h"
#include "CameraOV7670.h"


const uint8_t VERSION            = 0x10;
const uint8_t COMMAND_NEW_FRAME  = 0x01 | VERSION;
const uint8_t COMMAND_DEBUG_DATA = 0x03 | VERSION;

const uint16_t UART_PIXEL_FORMAT_GRAYSCALE = 0x02;

void processGrayscaleFrameBuffered();
void processGrayscaleFrameDirect();

const uint16_t lineLength       = 160;
const uint16_t lineCount        = 120;
const uint32_t baud             = 115200;
const uint16_t lineBufferLength = lineLength;
const bool isSendWhileBuffering = true;
const uint8_t uartPixelFormat   = UART_PIXEL_FORMAT_GRAYSCALE;
CameraOV7670 camera(CameraOV7670::RESOLUTION_QQVGA_160x120, CameraOV7670::PIXEL_YUV422, 17);
// pixel is 2 bytes but buffer is only 160 wide?

uint8_t lineBuffer[lineBufferLength]; // Two bytes per pixel
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
    commandDebugPrint("Frame " +
                      String(frameCounter) /* + " " + String(processedByteCountDuringCameraRead)*/);
    // commandDebugPrint("Frame " + String(frameCounter, 16)); // send number in hexadecimal
}

// camera
// commandDebugPrint
// lineBufferSenByte, lineBuffer, lineBufferLength,
// isSendWhileBuffering, lineLength, processNextGrayscalePixelByteInBuffer

void processGrayscaleFrameBuffered()
{
    camera.waitForVsync();
    commandDebugPrint("Vsync");

    camera.ignoreVerticalPadding();

    for (uint16_t y = 0; y < lineCount; y++)
    {
        lineBufferSendByte = &lineBuffer[0];
        camera.ignoreHorizontalPaddingLeft();

        uint16_t x = 0;
        while (x < lineBufferLength)
        {
            camera.waitForPixelClockRisingEdge(); // YUV422 grayscale byte
            camera.readPixelByte(lineBuffer[x]);
            lineBuffer[x] = formatPixelByteGrayscaleFirst(lineBuffer[x]);

            camera.waitForPixelClockRisingEdge(); // YUV422 color byte. Ignore.
            if (isSendWhileBuffering) { processNextGrayscalePixelByteInBuffer(); }
            x++;

            camera.waitForPixelClockRisingEdge(); // YUV422 grayscale byte
            camera.readPixelByte(lineBuffer[x]);
            lineBuffer[x] = formatPixelByteGrayscaleSecond(lineBuffer[x]);

            camera.waitForPixelClockRisingEdge(); // YUV422 color byte. Ignore.
            if (isSendWhileBuffering) { processNextGrayscalePixelByteInBuffer(); }
            x++;
        }
        camera.ignoreHorizontalPaddingRight();

        // Debug info to get some feedback how mutch data was processed during line read.
        processedByteCountDuringCameraRead = lineBufferSendByte - (&lineBuffer[0]);

        // Send rest of the line
        while (lineBufferSendByte < &lineBuffer[lineLength])
        {
            processNextGrayscalePixelByteInBuffer();
        }
    };
}

void processNextGrayscalePixelByteInBuffer()
{
    if (isUartReady())
    {
        UDR0 = *lineBufferSendByte;
        lineBufferSendByte++;
    }
}


void processGrayscaleFrameDirect()
{
    camera.waitForVsync();
    commandDebugPrint("Vsync");

    camera.ignoreVerticalPadding();

    for (uint16_t y = 0; y < lineCount; y++)
    {
        camera.ignoreHorizontalPaddingLeft();

        uint16_t x = 0;
        while (x < lineLength)
        {
            camera.waitForPixelClockRisingEdge(); // YUV422 grayscale byte
            camera.readPixelByte(lineBuffer[0]);
            lineBuffer[0] = formatPixelByteGrayscaleFirst(lineBuffer[0]);

            camera.waitForPixelClockRisingEdge(); // YUV422 color byte. Ignore.
            waitForPreviousUartByteToBeSent();
            UDR0 = lineBuffer[0];
            x++;

            camera.waitForPixelClockRisingEdge(); // YUV422 grayscale byte
            camera.readPixelByte(lineBuffer[0]);
            lineBuffer[0] = formatPixelByteGrayscaleSecond(lineBuffer[0]);

            camera.waitForPixelClockRisingEdge(); // YUV422 color byte. Ignore.
            waitForPreviousUartByteToBeSent();
            UDR0 = lineBuffer[0];
            x++;
        }

        camera.ignoreHorizontalPaddingRight();
    }
}

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
