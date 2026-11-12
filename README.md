# Gravity Guy

Gravity Guy is a small Arduino game for a 16x2 I2C LCD. The player runs in
one of two lanes and switches between the top and bottom lanes with a button.
Avoid the spikes that move across the display: a collision resets the score.
You earn a point for every five seconds you survive, and the obstacles speed
up as your score grows. The high score is kept only until the Arduino resets
or loses power.

**This is only a first version**. It is a simple prototype, so the game and
hardware setup may change, and it has not been polished into a finished
product.

## Wiring

The sketch uses an Arduino with an I2C LCD, such as an Uno, and a push button:

| Component connection | Arduino connection |
| --- | --- |
| LCD GND (black wire in the current setup) | GND |
| LCD VCC (white wire in the current setup) | 5V |
| LCD SDA (gray wire in the current setup) | A4 |
| LCD SCL (purple wire in the current setup) | A5 |
| One button terminal | Digital pin 11 |
| Other button terminal | GND |

The button uses the Arduino's internal pull-up resistor, so it should connect
between pin 11 and GND; no external pull-up resistor is needed. The LCD
address in the sketch is `0x27`. I2C pin names can vary by board, so check the
board's pinout if you are using something other than an Uno-compatible board.

## Uploading

Open `gravityguy.ino` in the Arduino IDE and install the `LiquidCrystal_I2C`
library if it is not already available. Select the correct board and port,
then upload the sketch.
