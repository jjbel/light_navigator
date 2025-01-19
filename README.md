# Light Navigator Bot

![](https://github.com/user-attachments/assets/9f7a2300-5874-47f7-a1e5-54a47a25ee2c)

Move a bot to the bright spot on the wall in a dark room

### 

Development process:
1. Took code from circuitjournal, removed all unnecessary code except that which sends grayscale image over UART
2. inspected the pixel reading code to find a place to insert pixel processing
3. debug upload timing issues
4. get the motors working, being conservative with number of pins used (as I ran out of available arduino pins)
5. tune the rotation and forward movement parameters

Algorithm:
1. We only care about horizontal movement, so rotate the camera 90 degrees so each pixel row is vertical
2. store each row in a buffer, take the average of each row (call this it's brightness)
3. if the light is on the left, the beginning rows are brighter
4. turn by an amount propotional to the angle of the light from the centre axis
5. move forward

Bugs:
1. Don't disturb the UART upload process
2. KEep the checksum which is verified by circuitjournal's extension
3. Usin pin 11 for PWM caused timing issues, maybe because it is also use for I2C?


### Downsampling ideas

Max pooling

store n lines, average over n lines, so 2^n size reduction

bot can only turn horizontally. vertical resolution not needed. can orient camera such that one line is vertical, then can greatly downsample line, maybe even take average.
or take average sport
or find the centre of mass

take the middle line with max value.
but just taking max value is susceptible to noise.
could set a threshold, pixels above it should be counted.

auto exposure: could be helpful to control it manually?

possible optimization: reduce no of memory accesses to buffer

OI!!
just print the avg value at the end of the line
forget trying to set the image to it
