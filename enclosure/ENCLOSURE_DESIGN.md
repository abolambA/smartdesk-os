# SmartDesk OS — Enclosure Design Reference v3
# "Desky" character — Baymax-inspired friendly desk robot
# Material: PLA (updated from PETG)
# TTP223: hidden behind 1.5mm thin wall — no cutout needed
# NeoPixel: frosted translucent diffuser window included

## Confirmed Component Dimensions

| Component                    | Dimensions                           |
|------------------------------|--------------------------------------|
| Arduino UNO Q PCB            | 68.85mm × 53.34mm × ~12mm tall       |
| ST7789 2" display module     | 60mm × 36.5mm × 3.5mm thick          |
| ST7789 active display area   | 40.8mm × 30.6mm                      |
| NeoPixel 12-ring             | 37mm outer dia, 23mm inner, 7mm thick|
| KY-040 rotary encoder        | 19mm body dia, 6mm shaft, 20mm tall  |
| TTP223 touch module          | 24mm × 24mm × 3mm                    |
| DHT22 sensor                 | 15mm × 12mm × 5mm                    |
| BH1750 module                | 21mm × 18mm × 3mm                    |
| Passive buzzer               | 12mm dia × 9mm tall                  |
| Privacy toggle switch        | 13mm × 7mm body                      |
| Privacy LED 5mm              | 5mm dia                              |
| Coin vibration motor         | 10mm dia × 3mm thick                 |
| Relay module                 | 32mm × 26mm × 17mm                   |
| USB-C port on UNO Q          | 9mm wide × 3.5mm tall                |

## Notes
- TTP223 works through 1.5mm PLA — no cutout needed, fully hidden
- NeoPixel diffuser: print ring window in translucent/natural white PLA 1.8mm thick
- PLA has good capacitive transmission, better than PETG for touch sensing

## Measure before printing
1. Your ST7789 PCB — 60×36.5mm or 58×35mm?
2. USB-C port height on your UNO Q board
3. MIPI-CSI camera module dimensions
4. KY-040 shaft length above body
5. TTP223 — 24×24mm or 20×20mm?

---

## FULL ZOO.DEV PROMPT — paste everything below this line

Design a functional 3D-printable enclosure for a desk robot character called "Desky".
The character is a small, friendly, rounded robot inspired by Baymax from Big Hero 6.
The enclosure houses all SmartDesk OS electronics inside.
All dimensions are in millimeters. Wall thickness is 2.5mm throughout unless stated otherwise.
Print material: PLA.
The enclosure splits into 4 separate printed pieces: HEAD shell, BODY base, NECK COLLAR, and NEOPIXEL DIFFUSER WINDOW.
All pieces connect with M3 screws into brass heat-set inserts.

=== COMPONENTS TO FIT INSIDE ===
Arduino UNO Q PCB: 68.85 x 53.34 x 12mm tall
ST7789 2 inch SPI display module: 60 x 36.5 x 3.5mm thick, active area 40.8 x 30.6mm
NeoPixel 12-LED ring: 37mm outer diameter, 23mm inner diameter, 7mm thick
KY-040 rotary encoder: 19mm body diameter, 6mm shaft diameter, 20mm body height
TTP223 capacitive touch module: 24 x 24 x 3mm — mounts behind thin wall, NO external cutout
BH1750 light sensor module: 21 x 18 x 3mm
DHT22 temperature sensor: 15 x 12 x 5mm
Passive piezo buzzer: 12mm diameter x 9mm tall
Privacy toggle switch SPST: 13 x 7mm body
Privacy indicator LED 5mm diameter
Coin vibration motor: 10mm diameter x 3mm thick
Relay module: 32 x 26 x 17mm
MIPI-CSI camera module: approximately 25 x 24mm PCB

=== PIECE 1: HEAD SHELL ===

External shape:
Oblate sphere — 140mm wide, 110mm deep, 90mm tall.
Flattened bottom face for mating with body.
Flattened rear face — 65mm wide flat vertical area for camera mount.
Wall thickness 2.5mm throughout.
All exterior surface corners: minimum 8mm fillet radius.
Smooth organic surface, no flat panels except rear camera area and bottom mating face.

Front face — display window:
Rectangular cutout 64mm wide x 42mm tall centered on front face.
Corner radius on cutout: 4mm.
Behind cutout: 1.5mm wide ledge all around at 4mm depth for display to rest on.
Display PCB cavity: 62mm wide x 38mm tall x 5mm deep behind ledge.
4x M2 screw holes at corners, 3mm inset from corner edges of cavity.

Above display — eyebrow ridges:
Two oval ridges raised 2.5mm above surface.
Each ridge: 22mm long x 7mm wide, smooth rounded ends.
Left ridge center: 26mm left of face center, 14mm above display cutout top edge.
Right ridge center: 26mm right of face center, 14mm above display cutout top edge.
Both ridges angled 15 degrees inward toward center for expressive character look.

Below display — smile groove:
Arc groove 2mm deep x 1.5mm wide embossed into surface.
38mm wide arc, centered horizontally, 10mm below display cutout bottom edge.
Arc curves upward 4mm at its center forming a smile shape.

Left side of head — NeoPixel ring mount:
Circular cutout 39mm diameter through the left side wall.
Center of cutout: 32mm back from front face edge, vertically centered on head sphere.
Behind cutout on interior: cylindrical recess 8mm deep with flat bottom for ring PCB to rest in.
4x M2 mounting holes at 90 degree intervals on 50mm bolt circle centered on recess.
The cutout opening will be covered by the separate diffuser window piece.
Diffuser window recess: 1mm deep x 41mm diameter around cutout opening for window to sit flush.

Top of head — rotary encoder mount:
Circular shaft hole 7mm diameter.
Position: centered left-right on head top, 20mm back from front face top edge.
Recessed platform around hole: 25mm diameter x 2mm deep for encoder flange to sit in.
Encoder body cavity below platform extending into head interior: 20mm diameter x 16mm deep.

Top of head — character ears:
LEFT ear: solid hemisphere 20mm base diameter, 9mm tall.
Positioned 38mm left of head center on top surface.
RIGHT ear: solid hemisphere 20mm base diameter, 9mm tall.
Positioned 38mm right of head center on top surface.
Both ears blend smoothly into head surface with tangent-continuous blending, no visible seam.

Rear of head — camera slot:
Rectangular slot 24mm wide x 16mm tall on the flat rear face.
Slot depth 10mm, open channel for MIPI-CSI ribbon cable.
Centered horizontally on rear flat face, 20mm down from head top.

Head bottom — mating interface:
Flat annular ring: 110mm outer diameter, 88mm inner diameter, 4mm thick.
4x M3 heat-set insert holes: 5.5mm diameter x 5mm deep at 90 degree intervals on 96mm bolt circle.
2x alignment pins: 3mm diameter x 4mm tall at 0 and 180 degree positions on 80mm bolt circle.

=== PIECE 2: NECK COLLAR ===

Shape: hollow ring with double-curve vertical profile.
Top outer diameter: 118mm.
Middle outer diameter: 108mm — pinched at midpoint to create neck illusion.
Bottom outer diameter: 122mm.
Height: 20mm.
Wall thickness: 2mm throughout.
Top edge: 1mm chamfer, smooth.
Bottom edge: 1mm chamfer, smooth.
No fasteners — sits between head and body held by gravity and friction.
Interior is smooth and open to allow wiring to pass through from head to body.

=== PIECE 3: BODY BASE ===

External shape:
Rounded rectangle — 145mm wide, 110mm deep, 85mm tall.
Top surface gently domed upward 5mm at center for belly character effect.
Bottom face flat for stable desk placement.
All vertical exterior edges: 12mm fillet radius.
Top horizontal exterior edges: 8mm fillet radius.
Bottom horizontal exterior edges: 4mm fillet radius.
All exterior surface corners: minimum 6mm fillet.

Interior layout:
Interior floor at 0mm from body bottom.
Arduino UNO Q mounting shelf: raised platform 10mm above interior floor.
Shelf dimensions: 74mm x 58mm providing 2.5mm clearance around 68.85x53.34mm board.
4x M3 mounting standoffs at UNO Q standard corner hole positions.
Standoff height: 8mm above shelf, M3 threaded insert hole 6mm deep in each standoff.
Shelf positioned centered left-right, 12mm from rear interior wall.
Left side component zone: 40mm wide x 58mm deep x 65mm tall for relay and additional components.
Wire management channels: 8mm wide x 6mm deep grooves along all four interior base walls.
Total interior height clearance: 75mm from floor to interior top surface.

Front face — hidden capacitive touch zone (NO CUTOUT):
The wall in this area is thinned to exactly 1.5mm thick over a 30mm diameter circular zone.
On the exterior surface: decorative circular indent 24mm diameter x 1mm deep centered on the thin zone.
This indent looks like a belly button character detail and marks the touch target for the user.
On the interior surface: flat recessed platform 28mm diameter x 1mm deep flush against thinned zone.
On this interior platform: 2x M2 screw holes 18mm apart centered on platform for TTP223 mounting.
TTP223 module mounts face-forward against interior wall, sensing through the 1.5mm PLA wall.
Touch zone center position: centered horizontally on front face, 35mm up from body bottom exterior.

Front face — privacy LED hole:
Circular hole 6.2mm diameter for 5mm LED with small bezel.
8mm diameter x 1mm countersink for LED flange.
Position: 22mm from left exterior edge of body, 22mm down from body top exterior.

Front face — buzzer grille:
9-hole hexagonal pattern, each hole 2.5mm diameter.
Pattern fits within 16mm diameter circle.
Position: 22mm from right exterior edge of body, 22mm down from body top exterior.

Right side wall — port cutouts:
USB-C port cutout: 12mm wide x 6mm tall, corner radius 2mm.
Position: centered vertically on right wall, 20mm up from bottom exterior.
Qwiic JST connector cutout: 7mm wide x 5mm tall.
Position: same right wall, 36mm up from bottom exterior.

Left side wall — privacy switch:
Toggle switch rectangular cutout: 15mm wide x 9mm tall.
3mm recessed surround so switch body sits slightly inset and protected.
Position: centered vertically on left wall, 48mm up from bottom exterior.
Text embossed above cutout: the word PRIV raised 0.8mm, 6mm letter height, centered above cutout.

Rear wall — assembly access panel opening:
Rectangular opening: 88mm wide x 60mm tall.
Centered horizontally on rear wall, 8mm up from bottom.
1.5mm lip border all around opening interior edge for removable rear panel to seat against.
4x M2 screw holes in corners of lip, 5mm from each corner.

Body bottom face — stability features:
4x rubber foot recesses: 12mm diameter x 2.5mm deep at corners, 18mm inset from each edge.
Ventilation slot pattern centered on bottom face between foot recesses:
4 rows of 4 slots each. Each slot: 25mm x 4mm. Gap between slots: 4mm. Gap between rows: 7mm.
Pattern avoids foot recesses. Total vent array centered front-back and left-right.

Body top rim — mating interface:
Flat top rim 4mm wide around entire top perimeter.
4x M3 through-holes aligned with head heat-set inserts on 96mm bolt circle.
2x alignment pin receiver holes: 3.8mm diameter x 5mm deep on 80mm bolt circle.

=== PIECE 4: NEOPIXEL DIFFUSER WINDOW ===

Shape: circular disc.
Diameter: 41mm — fits into the 1mm deep x 41mm recess around the NeoPixel cutout in the head.
Thickness: 1.8mm.
Material recommendation in print notes: print in translucent natural white PLA or clear PLA.
The semi-transparent material diffuses NeoPixel LED dots into a smooth glowing ring effect.
No fasteners — press-fit into head recess, friction held, removable for maintenance.
Outer edge: 0.3mm chamfer for easy insertion.
Surface: perfectly flat and smooth on both faces, no texture.

=== SURFACE AND FINISH DETAILS ===
All exterior surfaces of head and body: fine stipple texture 0.25mm depth for matte appearance.
All component cavity interiors: smooth finish.
All text: 0.8mm raised, clean geometric sans-serif font.
Diffuser window: completely smooth, no texture.

=== PRINT SETTINGS RECOMMENDATION ===
Material: PLA for head, body, neck collar. Translucent natural white PLA for diffuser window.
Layer height: 0.2mm for structural pieces, 0.1mm for diffuser window.
Infill: 20% gyroid for head and body. 50% rectilinear for diffuser window.
Perimeters: 3 for most areas, 4 perimeters specifically around the 1.5mm touch zone wall.
Supports: required for head rear camera slot only, none elsewhere.
Print orientation: head upside down, body right-side up, collar upright, diffuser flat.
Bed temperature: 60C. Nozzle temperature: 210C for PLA.

=== OUTPUT FILES REQUIRED ===
4 separate STL files ready for FDM PLA printing:
1. desky_head.stl
2. desky_body.stl
3. desky_neck_collar.stl
4. desky_diffuser_window.stl
