# RetroTest PCB Layout

The RetroText PCB is a 4x6 character matrix, driven by a IS31FL3737 LED driver. It is designed to display 6 4x6 pixel characters, which are 15mm high and 8mm wide, with a 1.5mm spacing between each.

The 6 digits are arranged in one line, from left to right (0-5).

The IS31FL3737 is wired as according to this table:

| Digit | Upper Left | Bottom Right | Rows   |  Cols  |  Notes               |
|-------|------------|--------------|--------|--------|----------------------|
| 0     | SW1,CS1    | SW6,CS4      | SW1-6  | CS1-4  | Leftmost character   |
| 1     | SW1,CS5    | SW6,CS8      | SW1-6  | CS5-8  |                      |
| 2     | SW1,CS9    | SW6,CS12     | SW1-6  | CS9-12 |                      |
| 3     | SW7,CS1    | SW12,CS4     | SW7-12 | CS1-4  |                      |
| 4     | SW7,CS5    | SW12,CS8     | SW7-12 | CS5-8  |                      |
| 5     | SW7,CS9    | SW12,CS12    | SW7-12 | CS9-12 | Rightmost character  |

- **Upper Coordinate (X, Y):** The top-left pixel of each 4x6 character cell.
- **Row Range (Y):** Pixel rows for each character (always 0–5 for 6 rows).
- **Col Range (X):** Pixel columns for each character (4 columns per character, spaced by 1.5mm on PCB).
- **Character Cell:** Logical character position (1–6, left to right).
- **Notes:** Additional info about position or function.
  
This table helps map each character cell to its pixel and PCB coordinates for layout and addressing.
