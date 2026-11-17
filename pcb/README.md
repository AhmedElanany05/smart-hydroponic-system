# Tower controller board (v1.1)

Two-layer through-hole board, 150 × 160 mm, made in KiCad 10.
It takes the 12 V battery, protects and fuses it, switches the three pumps and the Jetson, carries the ESP32 (NodeMCU-32S, 38-pin) with all sensor connectors, and has a mounting place for the XL4015 buck module.

## Files

| File | What it is |
|---|---|
| `tower_board.kicad_pro` | KiCad project (open this) |
| `tower_board.kicad_sch` | Schematic, one A3 sheet |
| `tower_board.kicad_pcb` | Board layout |
| `tower.pretty/` | Custom footprints: BLX-A fuse holder, XL4015 module mount |
| `manufacturing/tower_board_gerbers.zip` | Gerbers + drill files (plated and non-plated), send this to the PCB shop |
| `manufacturing/tower_board.step` | 3D model of the board for SolidWorks |
| `docs/tower_board_schematic.pdf` | Schematic as PDF |
| `docs/tower_board_bom.csv` | Parts list |
| `docs/board_1to1_print.pdf` | Actual-size print for checking the parts before ordering |
| `docs/board_3d_*.png` | 3D renders |

## Board summary

| Block | Parts | Notes |
|---|---|---|
| Battery input | J1, Q1 (IRF4905), R1, D1, D2 (P6KE18A), C1, C2 | Reverse-polarity protection with a P-MOSFET, TVS against spikes |
| Fuses | F1 1 A tower pump, F2 1 A nutrient pumps, F3 5 A Jetson, F4 2 A buck | 5×20 mm glass fuses |
| Tower pump | J2, Q2 (IRLZ44N), D3, R5, R6, R7, LED2 | Low-side switch, ESP32 IO25 |
| Nutrient pump A | J3, Q3, D5, R8, R9, R10, LED3 | ESP32 IO26 |
| Nutrient pump B | J4, Q4, D6, R11, R12, R13, LED4 | ESP32 IO27 |
| Jetson | J5, Q5 (IRF4905), Q6 (PN2222A), R14–R18, C4, LED5 | High-side switch, ESP32 IO13 (high = Jetson on) |
| 5 V | J6 → XL4015 module (M1) → J7, D7, C5–C8, L1, D8, LED6 | 5 V for the ESP32 (through D8) and a filtered 5 V for the sensors |
| ESP32 | J8, J9 (2 × 19-pin sockets, 22.86 mm apart) | NodeMCU-32S, 38-pin |
| ADS1115 | J10 (10-pin socket) | Address 0x48, runs on 3.3 V |

## ESP32 pin map

| GPIO | Use | GPIO | Use |
|---|---|---|---|
| 25 | Tower pump | 21 / 22 | I2C SDA / SCL (ADS1115, I2C header) |
| 26 | Nutrient pump A | 4 | DS18B20 (4.7 k pull-up on board) |
| 27 | Nutrient pump B | 19 | DHT11 (10 k pull-up on board) |
| 13 | Jetson power | 5 / 18 | HC-SR04 trigger / echo (echo divided 1 k / 2 k) |
| 32 | Float switch LOW | 34 | Leak sensor (analog) |
| 33 | Float switch HIGH | 14, 23, 35, 36 | Free, on the expansion header |

Power: the board feeds the ESP32 on its **5V** pin (through D8) and takes 3.3 V from its **3V3** pin for the sensors.
ADS1115 inputs: A0 = pH (10 k / 20 k divider), A1 = TDS, A2 = battery voltage (68 k / 18 k, so V_bat = V_A2 × 4.78), A3 = free on the expansion header.

## XL4015 buck module

The module (66 × 39 mm, with voltmeter display) screws onto the board at M1 with four M3 screws and 10 mm standoffs, display side up.
Wires: **J6 BUCK IN → module IN**, **module OUT → J7 5V IN**, red to "+", black to "−".
Set the output to **5.0 V** (voltage knob) and the current limit to about **1.5 A** (current knob) before connecting J7.

## Dry-run protection

The LOW float switch also pulls the tower pump gate down through D4, so the tower pump cannot run when the tank is low, even if the software tries to turn it on.
For this to work, the LOW float switch must be **closed when the water is low**. Most float switches can be flipped on their stem to get this.

## Check before ordering (print `docs/board_1to1_print.pdf` at 100 %)

1. **ESP32 board:** the two socket rows are 22.86 mm apart (centre to centre), for the NodeMCU-32S.
2. **XL4015 holes:** the mount uses 4.2 mm holes on 60 × 33 mm. Lay the module on the print and check that its holes line up.
3. **Fuse holders:** the footprint uses 22.5 mm between the two pins. Measure your BLX-A holders. If they differ, change the pad position in `tower.pretty`.
4. **Screw terminals:** the footprint is for 5.0 mm pitch (KF126 / MX126 type).

## PCB order details

- 2 layers, 1.6 mm FR-4, 1 oz copper, plated through holes, HASL, green mask, white silkscreen.
- Smallest track 0.5 mm, smallest gap 0.3 mm, smallest hole 0.65 mm (vias).
- Ground pour on both layers.

## Assembly order

1. Resistors, diodes (check the band, the "K" mark is the cathode), the inductor.
2. Small capacitors, LEDs (the flat side and square pad are the cathode), Q6.
3. Pin headers and sockets, then the TO-220 MOSFETs.
4. Electrolytic capacitors (the "+" mark and square pad), fuse holders, screw terminals.
5. Standoffs for the XL4015 module.

## First power-up

1. No fuses, no ESP32, no modules. Connect the battery through the 10 A main fuse. The 12 V LED must light.
2. Insert F4 only. Wire J6 to the XL4015 IN and **set it to 5.0 V** before wiring its OUT to J7.
3. Check 5 V on the sensor headers, then plug in the ESP32 and the ADS1115.
4. Insert F1–F3 and test each output with the pump or a 12 V bulb.

## Off the board

- 10 A fuse in an inline holder right at the battery + terminal (and another on the charge controller's + wire).
- 1.5 mm² wire for the battery and the Jetson, 1 mm² is enough for the pumps.
- Pump wiring: red (+) to the "+" side of each terminal, black (−) to the "−" side. The "−" side is switched, so it is not ground.
