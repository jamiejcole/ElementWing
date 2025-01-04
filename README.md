## Element Wing - A DIY Command Wing for ETC EOS
This project is an extension of [my previous](https://github.com/jamiejcole/zenith) EOS controller that I made primarily for controlling moving head fixtures, as my Element 2 console doesn't have any encoder wheels.

It's a full sized EOS command wing, featuring the standard ETC key layout and 4 customisable rotary encoders.

![Top](https://github.com/user-attachments/assets/567f5c07-6bd0-448a-a287-a3b112f35b13)

### Components used
|             Part              | Qty | Total ($AUD) | Source |
|             ---               | --- | ------------ | ------ |
| Gateron Black Switches        | 90  | $31.19       | https://www.aliexpress.com/item/1005006376024657.html |
| Blank White XDA PBT Keycaps   | 90  | $34.00       | https://www.aliexpress.com/item/1005007683242914.html |
| 1N1418 THT Diodes             | 90  | $4.69        | https://www.aliexpress.com/item/32866531235.html      |
| MCP23017-E/SO                 | 1   | $12.59       | https://www.aliexpress.com/item/1005005066054098.html |
| Raspberry Pi Pico*            | 1   | $5.63        | https://www.aliexpress.com/item/1005006071676557.html |
| EC11 Rotary Encoders          | 4   | $5.20        | https://www.aliexpress.com/item/10000056483250.html   |
| Aluminium Knob for Encoders   | 4   | $11.39       | https://www.aliexpress.com/item/1005005207623966.html |
| Plastic Heat Set Inserts      | 6   | $14.99       | https://www.aliexpress.com/item/1005006472962973.html |
| **Total (excluding PCB)**     |     | **$119.68**  |                                                       |

> Note: The PCB design has the footprint for an official Raspberry Pi Pico with a micro-usb port. I wanted to have USB-C on my board, so I ordered the 16M variant of the above RP2040 board, without checking the pin layout. The board I ordered actually had a slightly different pin layout to the official Pi Pico so I had to make some jumps with short wire on the board, so if you use my PCB design, check which Pico you're ordering first!

## PCB Design
This was the first PCB I've designed, so apologies in advanced if I've made any silly mistakes on it. [Joe Scotto's video](https://www.youtube.com/watch?v=8WXpGTIbxlQ) was helpful for getting started with the design, and [iNimbleSloth's tutorials](https://www.youtube.com/watch?v=XKWoeBwpVNg) were also extremely useful for designing the keyswitch matrix. I ended up ordering the PCB from [JLCPCB](https://jlcpcb.com/) for around $100AUD including shipping, bringing the **total project cost to ~$220 AUD**.

![PCB](https://github.com/user-attachments/assets/babba8c8-64a4-4bb3-8ced-60994aed9b57)

The PCB consists of a 13x8 diode keyswitch matrix, where each row and column are connected to a GPIO pin on the Pico. The A/B/SW pins of each encoder are connected to a GPIO port on the MCP23017, which is connected to the Pico on the I2C0 interface, with the SDA and SCL lines pulled up to 3v3 with 2.2k resistors.

### Assembly process
After receiving the PCBs, I started designing some test layouts for the Cherry MX style switches. Due to the bed size of my 3D printer, I had to split up each piece into two parts, to be plastic welded together after printing. The CAD design consists of three main parts (each with left/right sides):
1. A plate for the switches to be inserted into prior to soldering to the PCB
2. A main case where the brass heat-set inserts will be put in, allowing the whole unit to be bolted together.
3. A top face-plate/cover with cut-outs for the switches and encoders.

<img src="https://github.com/user-attachments/assets/65ed10fe-f853-4051-9843-7dc4b381eb48" width=1000 alt="Exploded View">


### Custom Keycaps
The keycaps were made using a process I [found here](https://www.youtube.com/watch?v=JR1BwcFyexQ). I used blank white XDA profile PBT keycaps from aliexpress. The process was pretty straightforward:
- Cover each keycap in a thin layer of powder from a toner cartridge. I breathed on the keys before dunking them in the powder (the "hah hah" kind of breathing you do to clean glasses) to get the powder to stick better, then gently blew off the excess.
- Place keycap into a jig mounted to the laser engraver (this was done to ensure the text is consistently in the correct spot).
- Using a 15W diode laser at 380mm/min, 2.1% power and text on a fill layer at 300 lines/inch, etched the dye into the keycaps.
- Cleaned off the excess powder with warm water.

<img src="https://github.com/user-attachments/assets/4ca3f521-9e42-47b5-918e-ed834c60d340" width=400>
<img src="https://github.com/user-attachments/assets/f8140d71-d5ca-4382-be14-c48cdce34ae6" width=400>


### Programming the microcontroller
I decided to use a Raspberry Pi Pico rather than an Arduino or ESP8266 due to the amount of GPIO pins it had. I downloaded [Earle Philhower's Arduino Core](https://github.com/earlephilhower/arduino-pico) for RP2040 boards and uploaded a test sketch using the Arduino IDE. The Pico uses OSC to communicate with EOS over the USB Serial interface. More info about OSC [can be found here](https://github.com/cnmat/osc). 

The [ETC #lighthack](https://github.com/ETCLabs/lighthack) community also was extremely helpful. Big thanks to [sstaub](https://github.com/CNMAT/OSC/issues/159#issuecomment-2566483844) over on the OSC repo.
