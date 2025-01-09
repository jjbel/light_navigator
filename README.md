# Light Navigator Bot

Move a bot to the bright spot on the wall in a dark room

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
