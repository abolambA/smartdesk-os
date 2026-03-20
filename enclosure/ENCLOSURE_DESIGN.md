# SmartDesk OS — Enclosure Design Reference v4
# "Desky" character — Baymax-inspired friendly desk robot
# Material: PLA
# Privacy switch REMOVED — long press on TTP223 handles camera toggle
# Insert size confirmed: M3 × 4mm L × 5mm OD → holes sized at 5.2mm diameter
# Screw sizes: M3×10mm (head-body), M3×6mm (board mount), M2×6mm (display/ring/panel)

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
| Privacy LED 5mm              | 5mm dia                              |
| Coin vibration motor         | 10mm dia × 3mm thick                 |
| Relay module                 | 32mm × 26mm × 17mm                   |
| USB-C port on UNO Q          | 9mm wide × 3.5mm tall                |

## Touch Interaction (no physical switch needed)
- Single tap: start/stop session
- Long press 2 seconds: toggle camera on/off (replaces physical privacy switch)
- Double tap: cycle display mode (alternative to rotary encoder)

## Screws and Inserts Summary
- Inserts: M3 × 4mm L × 5mm OD brass heat-set → enclosure holes 5.2mm diameter × 5mm deep
- M3 × 10mm screws: head-to-body connection (4 screws)
- M3 × 6mm screws: UNO Q board to standoffs (4 screws)
- M2 × 6mm screws: display mount, NeoPixel ring, rear panel (12 screws)

## Measure before printing
1. Your ST7789 PCB — 60×36.5mm or 58×35mm?
2. USB-C port height on your UNO Q
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
The enclosure consists of 4 separate printed pieces: HEAD shell, BODY base, NECK COLLAR, and NEOPIXEL DIFFUSER WINDOW.
Connections use M3 brass heat-set inserts with OD 5mm. All insert holes are 5.2mm diameter x 5mm deep.
Head-to-body screws: M3 x 10mm. Board mounting screws: M3 x 6mm. Small component screws: M2 x 6mm.
There is NO privacy toggle switch — camera is controlled via long press on the capacitive touch sensor.

=== COMPONENTS TO FIT INSIDE ===
Arduino UNO Q PCB: 68.85 x 53.34 x 12mm tall
ST7789 2 inch SPI display module: 60 x 36.5 x 3.5mm thick, active area 40.8 x 30.6mm
NeoPixel 12-LED ring: 37mm outer diameter, 23mm inner diameter, 7mm thick
KY-040 rotary encoder: 19mm body diameter, 6mm shaft diameter, 20mm body height
TTP223 capacitive touch module: 24 x 24 x 3mm — mounts behind thin wall, NO external cutout
BH1750 light sensor module: 21 x 18 x 3mm
DHT22 temperature sensor: 15 x 12 x 5mm
Passive piezo buzzer: 12mm diameter x 9mm tall
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
38mm wide arc centered horizontally, 10mm below display cutout bottom edge.
Arc curves upward 4mm at its center forming a smile shape.

Left side of head — NeoPixel ring mount:
Circular cutout 39mm diameter through the left side wall.
Center of cutout: 32mm back from front face edge, vertically centered on head sphere.
Behind cutout on interior: cylindrical recess 8mm deep with flat bottom for ring PCB.
4x M2 mounting holes at 90 degree intervals on 50mm bolt circle centered on recess.
Diffuser window recess: 1mm deep x 41mm diameter around cutout opening for window to sit flush.

Top of head — rotary encoder mount:
Circular shaft hole 7mm diameter.
Position: centered left-right, 20mm back from front face top edge.
Recessed platform: 25mm diameter x 2mm deep for encoder flange.
Encoder body cavity below platform: 20mm diameter x 16mm deep.

Top of head — character ears:
LEFT ear: solid hemisphere 20mm base diameter, 9mm tall, 38mm left of center.
RIGHT ear: solid hemisphere 20mm base diameter, 9mm tall, 38mm right of center.
Both ears blend smoothly into head surface with tangent-continuous blending.

Rear of head — camera slot:
Rectangular slot 24mm wide x 16mm tall on flat rear face.
Slot depth 10mm, open channel for MIPI-CSI ribbon cable.
Centered horizontally, 20mm down from head top.

Head bottom — mating interface:
Flat annular ring: 110mm outer diameter, 88mm inner diameter, 4mm thick.
4x M3 heat-set insert holes: 5.2mm diameter x 5mm deep at 90 degree intervals on 96mm bolt circle.
2x alignment pins: 3mm diameter x 4mm tall at 0 and 180 degree positions on 80mm bolt circle.

=== PIECE 2: NECK COLLAR ===

Shape: hollow ring with double-curve vertical profile.
Top outer diameter: 118mm.
Middle outer diameter: 108mm — pinched at midpoint.
Bottom outer diameter: 122mm.
Height: 20mm. Wall thickness: 2mm.
No fasteners — friction and gravity held, removable.
Interior open for wiring passage.

=== PIECE 3: BODY BASE ===

External shape:
Rounded rectangle — 145mm wide, 110mm deep, 85mm tall.
Top surface gently domed 5mm upward at center.
Bottom face flat.
All vertical exterior edges: 12mm fillet radius.
Top horizontal exterior edges: 8mm fillet radius.
Bottom horizontal exterior edges: 4mm fillet radius.

Interior layout:
Interior floor at 0mm.
Arduino UNO Q mounting shelf: raised platform 10mm above floor.
Shelf dimensions: 74mm x 58mm.
4x M3 mounting standoffs at UNO Q corner hole positions, 8mm tall, M3 insert hole 5.2mm x 5mm deep.
Shelf centered left-right, 12mm from rear interior wall.
Left component zone: 40mm wide x 58mm deep x 65mm tall for relay, BH1750, DHT22.
Wire management channels: 8mm wide x 6mm deep grooves along all interior base walls.
Interior height clearance: 75mm.

Front face — hidden capacitive touch zone (NO CUTOUT):
Wall thinned to exactly 1.5mm over a 30mm diameter circular zone.
Exterior surface: decorative circular indent 24mm diameter x 1mm deep — belly button character detail.
Interior surface: flat platform 28mm diameter x 1mm deep for TTP223 mounting.
2x M2 screw holes 18mm apart centered on platform.
Touch zone center: centered horizontally, 35mm up from body bottom exterior.

Front face — privacy LED hole:
Circular hole 6.2mm diameter.
8mm diameter x 1mm countersink for LED flange.
Position: 22mm from left exterior edge, 22mm down from body top exterior.

Front face — buzzer grille:
9-hole hexagonal pattern, each hole 2.5mm diameter, pattern within 16mm circle.
Position: 22mm from right exterior edge, 22mm down from body top exterior.

Right side wall — port cutouts:
USB-C cutout: 12mm wide x 6mm tall, 2mm corner radius, 20mm up from bottom exterior.
Qwiic JST cutout: 7mm wide x 5mm tall, 36mm up from bottom exterior.

Rear wall — assembly access panel:
Rectangular opening: 88mm wide x 60mm tall.
Centered horizontally, 8mm from bottom.
1.5mm lip border all around for removable rear panel.
4x M2 screw holes in corners of lip, 5mm from each corner.

Body bottom face:
4x rubber foot recesses: 12mm diameter x 2.5mm deep, 18mm inset from each edge.
Ventilation slots: 4 rows of 4 slots, each 25mm x 4mm, 4mm gap between slots, 7mm between rows.
Pattern centered on bottom, avoiding foot recesses.

Body top rim — mating interface:
Flat rim 4mm wide around perimeter.
4x M3 through-holes on 96mm bolt circle for M3 x 10mm screws to pass through into head inserts.
2x alignment pin receiver holes: 3.8mm diameter x 5mm deep on 80mm bolt circle.

=== PIECE 4: NEOPIXEL DIFFUSER WINDOW ===

Shape: circular disc.
Diameter: 41mm — press-fits into 41mm x 1mm recess on head left side.
Thickness: 1.8mm.
Print in translucent natural white PLA or clear PLA for LED diffusion.
Perfectly flat and smooth both faces, no texture.
Outer edge: 0.3mm chamfer for easy insertion.

=== SURFACE FINISH ===
All exterior head and body surfaces: 0.25mm stipple matte texture.
All component cavities: smooth finish.
All text: 0.8mm raised, geometric sans-serif font.
Diffuser window: completely smooth, no texture.

=== PRINT SETTINGS ===
Material: PLA for head, body, collar. Translucent PLA for diffuser window only.
Layer height: 0.2mm structural, 0.1mm diffuser window.
Infill: 20% gyroid structural, 50% rectilinear diffuser.
Perimeters: 3 standard, 4 at 1.5mm touch zone wall area.
Supports: head rear camera slot only.
Print orientation: head upside down, body right-side up, collar upright, diffuser flat.
Bed: 60C. Nozzle: 210C.

=== OUTPUT FILES REQUIRED ===
4 STL files:
1. desky_head.stl
2. desky_body.stl
3. desky_neck_collar.stl
4. desky_diffuser_window.stl
