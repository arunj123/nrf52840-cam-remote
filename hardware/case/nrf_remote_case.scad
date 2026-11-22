// ==============================================================================
// nRF52840 BLE CAMERA REMOTE — PARAMETRIC OPTION A POCKET CLICKER ENCLOSURE
// ==============================================================================
// Design Style: Option A — Pocket Clicker (2x Cherry MX Keys on Top + Side
//               Bare EC11 Thumbwheel Roller + Keychain Loop)
//
// Hardware Envelope (Direct-Wired, Zero Perfboard, No External Reset Key):
//   - Pro Micro / SuperMini nRF52840 (17.9 x 32.8 mm PCB, 33.8 mm w/ USB-C)
//     * Underside wire trenches + 5.0 mm USB-C-end rail relief for B+/B- pads
//     * C3 2.4 GHz chip antenna (+X end) kept clear of metal battery & screws
//   - Cylindrical 3.7V Li-Ion Cell (Dia 13.2 x 42.6 mm; bay Dia 13.8 x 46.6 mm)
//   - 2x Plate-Mount Cherry MX Mechanical Switches + Relegendable Keycaps
//     * Snap into drop-in 1.50 mm mx_switch_plate (14.15 x 14.15 mm cutouts)
//     * Protected by low-filament 3.0 mm top key-well bezel
//   - Bare EC11 Rotary Encoder (12.0 x 12.5 x 6.5 mm body, M7 bushing, Dia 6 mm shaft)
//     * Clamped vertically by ec11_retainer_plate + 4 top_shell hold-down columns
//     * Custom 3D-printed knurled thumbwheel roller peeks 3.0 mm out +X side wall
//       and exposes top face for direct axial Z click (Mute / Profile Switch)
//   - 12.0 mm x 8.5 mm Piezo Buzzer in top-ceiling C-clip w/ 3 crush ribs & vents
//   - Fasteners: 4x M1.6 x 5mm or 6mm Thin Flat Wafer Head (or DIN 912 Hex Socket)
//   - Optimized for Anycubic Kobra 3 v2 (0.4 mm nozzle, 0.20 mm layers, 0% supports)
// ==============================================================================

/* [View & Export Selection] */
// Select preview or individual export part:
VIEW_PART = "print_all"; // [all: Assembled Translucent Inspection, exploded: Exploded Assembly, section_y: Longitudinal Cutaway, section_x: Transverse Cutaway, top_shell: Top Shell (Print Z=0), bottom_tray: Bottom Tray (Print Z=0), mx_plate: MX Switch Plate (Print Z=0), ec11_retainer: EC11 Retainer (Print Z=0), thumbwheel: Thumbwheel Roller (Print Z=0), print_all: Complete Print Plate (Z=0), test_coupons: Fit Calibration Coupons (Z=0), hardware_only: Internal Hardware Mockup]

// Show internal hardware mockup in assembly and cutaway views:
SHOW_HARDWARE = true;

/* [1. Outer Enclosure & Print Parameters (Anycubic Kobra 3 v2)] */
CASE_L          = 70.00;  // Outer length X (mm)
CASE_W          = 48.00;  // Outer width Y (mm)
CASE_H          = 23.00;  // Outer height Z (mm)
CORNER_R        = 4.50;   // Outer vertical corner fillet radius (mm)
WALL            = 2.20;   // Outer perimeter wall thickness (5 perimeters @ 0.44mm)
BOTTOM_FLOOR    = 1.60;   // Bottom tray floor thickness (8 layers @ 0.20mm)
TOP_CEILING     = 2.00;   // Top shell solid roof thickness outside key-well (mm)
Z_SPLIT         = 4.80;   // Split plane height between bottom_tray and top_shell (mm)
LIP_H           = 1.80;   // Interlocking perimeter rabbet lip height (mm)
FIT_TOL         = 0.20;   // Standard FDM slip-fit clearance (mm)
JOIN_D          = 0.30;   // Manifold solid overlap into walls/floors (mm)
SEAM_GAP        = 0.05;   // Axial mating gap for clean non-intersecting assembly (mm)

/* [2. Pro Micro / SuperMini nRF52840 MCU (Direct-Wired, No Headers)] */
MCU_PCB_W       = 17.90;  // Measured PCB width (mm)
MCU_PCB_L       = 32.80;  // Measured PCB length (mm)
MCU_TOTAL_L     = 33.80;  // Length to USB-C metal tip (1.00 mm overhang)
MCU_PCB_T       = 1.60;   // Measured PCB thickness (mm)
MCU_USBC_W      = 9.00;   // USB-C receptacle width (mm)
MCU_USBC_H      = 3.20;   // USB-C receptacle height above PCB top (mm)
MCU_TRENCH_H    = 1.60;   // Underside wire/solder joint trench depth (mm)
MCU_RAIL_RELIEF = 5.00;   // Rail omission from USB-C end for B+/B- solder blobs (mm)
MCU_CY          = 16.80;  // MCU centerline Y (aligned with MX switch center)

/* [3. Cylindrical 3.7V Li-Ion Battery Cell] */
BAT_DIA         = 13.20;  // Measured cell diameter (mm)
BAT_LEN         = 42.60;  // Measured cell length bottom-to-positive PCB cap (mm)
BAT_WIRE_RELIEF = 4.00;   // Extra -X space for red/black wire bend toward MCU (mm)
BAT_BAY_CLR     = 0.60;   // Total diametral bay clearance (Dia 13.80 mm)
BAT_CY          = 39.00;  // Battery bay centerline Y (mm)

/* [4. 2x Cherry MX Switches & Drop-In Plate] */
MX_CUTOUT       = 14.15;  // Plate-mount MX switch cutout size (tuned for Kobra 0.4mm nozzle)
MX_PLATE_T      = 1.50;   // Drop-in MX switch snap-plate thickness (mm)
MX_PITCH        = 20.00;  // Center-to-center key pitch (mm)
KEY1_CX         = 13.50;  // Trigger 1 (Main Shutter) X center (mm)
KEY2_CX         = 33.50;  // Trigger 2 (Secondary Action) X center (mm)
KEY_CY          = 16.80;  // Key Y centerline (mm)
KEYCAP_W        = 18.20;  // Relegendable MX keycap outer skirt width (mm)
KEYWELL_DEPTH   = 3.00;   // Protective bezel lip height above switch plate (mm)
KEYWELL_L       = 39.80;  // Top key-well cutout X length (mm)
KEYWELL_W       = 19.40;  // Top key-well cutout Y width (mm)

/* [5. Bare EC11 Rotary Encoder & 3D-Printed Thumbwheel Roller] */
EC11_BODY_W             = 12.00; // EC11 metal housing width X (mm)
EC11_BODY_L             = 12.50; // EC11 metal housing length Y (mm)
EC11_BODY_H             = 6.50;  // EC11 metal housing height Z (mm)
EC11_BUSHING_DIA        = 7.00;  // M7 threaded collar outer diameter (mm)
EC11_BUSHING_H          = 5.00;  // M7 threaded collar height above body (mm)
EC11_SHAFT_DIA          = 6.00;  // Knurled split shaft diameter (mm)
EC11_SHAFT_ABOVE_BODY   = 12.50; // Total bushing + shaft height above EC11 body (mm)
EC11_WIRE_CLR           = 2.20;  // Underside solder pin & wire routing clearance (mm)
ENC_CX                  = 61.30; // Encoder center X (mm)
ENC_CY                  = 16.50; // Encoder center Y (mm)
WHEEL_DIA               = 23.40; // Thumbwheel outer diameter (peeks 3.0 mm past X=70)
WHEEL_H                 = 5.60;  // Thumbwheel outer ring thickness (mm)
WHEEL_TOP_Z             = 24.00; // Resting top face of thumbwheel (1.0 mm proud for click)
WHEEL_FLUTES            = 36;    // Number of V-knurl flutes around thumbwheel rim
WHEEL_SHAFT_FIT         = 6.05;  // Press-fit shaft bore diameter (w/ relief slot)

/* [6. 12mm Piezo Buzzer] */
BUZZ_DIA        = 12.00;  // Cylindrical buzzer body diameter (mm)
BUZZ_H          = 8.50;   // Cylindrical buzzer body height (mm)
BUZZ_PIN_PITCH  = 7.10;   // Bottom lead pitch (mm)
BUZZ_CX         = 56.60;  // Buzzer center X (mm)
BUZZ_CY         = 37.80;  // Buzzer center Y (mm)

/* [7. M1.6 Fasteners, Keychain Loop & LED Pinhole] */
M16_PILOT_DIA   = 1.45;   // Self-tapping pilot hole in top_shell bosses (mm)
M16_CLEAR_DIA   = 1.95;   // Through-hole shank clearance in bottom_tray (mm)
M16_CB_DIA      = 3.80;   // Universal counterbore (fits 3.6mm Wafer & 3.0mm DIN912)
M16_CB_DEPTH    = 1.80;   // Counterbore shelf depth (leaves 3.00 mm shank -> 2.0/3.0 mm engage)
BOSS_OD         = 4.30;   // Screw boss outer diameter (mm) — clears mating lip cleanly
KEYRING_DIA     = 5.00;   // Keychain loop bore diameter (mm)
KEYRING_WALL    = 3.20;   // Keychain loop solid perimeter wall thickness (mm)
LED_HOLE_DIA    = 1.80;   // Subtle LED light-bleed pinhole diameter (mm)
LED_HOLE_X      = 12.50;  // LED pinhole X (front rim clear of FL/F1/plate)
LED_HOLE_Y      = 4.60;   // LED pinhole Y (mm)

/* [Hidden] */
$fn = $preview ? 24 : 48;
EPS = 0.02;

// ==============================================================================
// DERIVED COORDINATES (SINGLE SOURCE OF TRUTH)
// ==============================================================================
INNER_L         = CASE_L - 2 * WALL;
INNER_W         = CASE_W - 2 * WALL;
INNER_R         = max(0.5, CORNER_R - WALL);

// Rabbet lip parameters (0.20 mm radial slip clearance; zero inward protrusion!)
SKIRT_OUTER_T   = (WALL - FIT_TOL) / 2;              // 1.00 mm outer skirt on top_shell
LIP_INNER_T     = WALL - SKIRT_OUTER_T - FIT_TOL;    // 1.00 mm inner lip on bottom_tray

// 4x M1.6 Boss Coordinates (0.20 mm clear of Z=4.8..6.6 mating lip)
BOSS_FL         = [4.55, 4.55];
BOSS_FR         = [47.20, 4.55];
BOSS_ML         = [4.65, 29.30];
BOSS_RR         = [65.45, 43.45];
BOSS_PTS        = [BOSS_FL, BOSS_FR, BOSS_ML, BOSS_RR];
BOSS_TOP_Z      = CASE_H - 1.50;                     // 21.50 mm (1.50 mm solid top skin)
PILOT_STOP_Z    = Z_SPLIT + 5.80;                    // 10.60 mm (accepts 5mm/6mm/8mm M1.6)

// MCU Coordinates
MCU_POCKET_L    = MCU_PCB_L + 0.60;                  // 33.40 mm (X = 2.20..35.60)
MCU_POCKET_W    = MCU_PCB_W + 0.60;                  // 18.50 mm (Y = 7.55..26.05)
MCU_X0          = WALL;                              // 2.20 mm (USB-C tip at 1.20 mm)
MCU_X1          = MCU_X0 + MCU_POCKET_L;             // 35.60 mm
MCU_Y0          = MCU_CY - MCU_POCKET_W / 2;         // 7.55 mm
MCU_PCB_Z0      = BOTTOM_FLOOR + MCU_TRENCH_H;       // 3.20 mm
MCU_PCB_Z1      = MCU_PCB_Z0 + MCU_PCB_T;            // 4.80 mm (= Z_SPLIT)
USBC_OPEN_W     = MCU_USBC_W + 0.60;                 // 9.60 mm (Y = 12.00..21.60)
USBC_OPEN_Z1    = MCU_PCB_Z1 + MCU_USBC_H + 0.40;    // 8.40 mm

// Battery Bay Coordinates
BAT_BAY_DIA     = BAT_DIA + BAT_BAY_CLR;             // 13.80 mm
BAT_BAY_R       = BAT_BAY_DIA / 2;                   // 6.90 mm
BAT_BAY_LEN     = BAT_LEN + BAT_WIRE_RELIEF;         // 46.60 mm
BAT_X0          = WALL + 0.20;                       // 2.40 mm
BAT_X1          = BAT_X0 + BAT_BAY_LEN;              // 49.00 mm
BAT_CELL_X0     = BAT_X0 + BAT_WIRE_RELIEF;          // 6.40 mm
BAT_ZC          = BOTTOM_FLOOR + 0.40 + BAT_BAY_R;   // 8.90 mm (Z = 2.00..15.80)

// MX Plate & Key-Well Coordinates
PLATE_Z1        = CASE_H - KEYWELL_DEPTH;            // 20.00 mm
PLATE_Z0        = PLATE_Z1 - MX_PLATE_T;             // 18.50 mm
PLATE_PKT_L     = KEYWELL_L + 2 * 1.40;              // 42.60 mm (X = 2.20..44.80)
PLATE_PKT_W     = KEYWELL_W + 2 * 1.40;              // 22.20 mm (Y = 5.70..27.90)
PLATE_L         = PLATE_PKT_L - 2 * FIT_TOL;         // 42.20 mm
PLATE_W         = PLATE_PKT_W - 2 * FIT_TOL;         // 21.80 mm
PLATE_CX        = (KEY1_CX + KEY2_CX) / 2;           // 23.50 mm
PLATE_CLAMP_PTS = [[17.50, 6.45], [34.50, 6.45], [17.50, 27.15], [34.50, 27.15]];

// Bare EC11 & Thumbwheel Coordinates
EC11_PKT_W      = EC11_BODY_W + 0.50;                // 12.50 mm (X = 55.05..67.55)
EC11_PKT_L      = EC11_BODY_L + 0.50;                // 13.00 mm (Y = 10.00..23.00)
EC11_BODY_Z0    = BOTTOM_FLOOR + EC11_WIRE_CLR;      // 3.80 mm
EC11_BODY_Z1    = EC11_BODY_Z0 + EC11_BODY_H;        // 10.30 mm
EC11_BUSH_Z1    = EC11_BODY_Z1 + EC11_BUSHING_H;     // 15.30 mm
EC11_SHAFT_Z1   = EC11_BODY_Z1 + EC11_SHAFT_ABOVE_BODY; // 22.80 mm
RET_T           = 2.00;                              // ec11_retainer_plate thickness
RET_Z0          = EC11_BODY_Z1;                      // 10.30 mm
RET_Z1          = RET_Z0 + RET_T;                    // 12.30 mm
WHEEL_Z1        = WHEEL_TOP_Z;                       // 24.00 mm
WHEEL_Z0        = WHEEL_Z1 - WHEEL_H;                // 18.40 mm
WHEEL_HUB_Z0    = EC11_BUSH_Z1 + 0.90;               // 16.20 mm (0.9mm rest / 0.4mm click clr)
WHEEL_WIN_DIA   = WHEEL_DIA + 1.40;                  // 24.80 mm (0.7mm radial clearance)
WHEEL_WALL_ARCH_Z0 = WHEEL_Z0 - 1.20;                // 17.20 mm (clears 0.5mm click + 0.7mm margin)

// 4 Clamping Tower Coordinates for EC11 Retainer (All strictly outside WHEEL_WIN_DIA & walls!)
RET_CLAMP_PTS   = [[51.60, 5.60], [50.50, 27.20], [65.30, 3.50], [66.20, 29.90]];

// Buzzer C-Clip Coordinates
BUZZ_CCLIP_ID   = BUZZ_DIA + 2 * FIT_TOL;            // 12.40 mm
BUZZ_CCLIP_OD   = BUZZ_CCLIP_ID + 1.80;              // 14.20 mm
BUZZ_Z1         = CASE_H - TOP_CEILING;              // 21.00 mm
BUZZ_Z0         = BUZZ_Z1 - (BUZZ_H - 0.30);         // 12.80 mm

// ==============================================================================
// COMPILE-TIME DESIGN-RULE ASSERTIONS
// ==============================================================================
ENGAGE_5MM = (M16_CB_DEPTH + 5.0) - Z_SPLIT;
ENGAGE_6MM = (M16_CB_DEPTH + 6.0) - Z_SPLIT;
assert(M16_CB_DEPTH >= 1.65, "Counterbore shallower than DIN912 M1.6 head");
assert(ENGAGE_5MM >= 2.00, str("M1.6x5mm thread engagement < 2.0 mm: ", ENGAGE_5MM));
assert(ENGAGE_6MM <= (PILOT_STOP_Z - Z_SPLIT) - 0.4, "M1.6x6mm screw bottoms out in pilot");
assert(BOSS_TOP_Z <= CASE_H - 1.20, "Pilot boss risks poking top surface");

DIST_RR_BUZZ = norm([BOSS_RR[0] - BUZZ_CX, BOSS_RR[1] - BUZZ_CY]);
assert(DIST_RR_BUZZ - BUZZ_CCLIP_OD/2 - BOSS_OD/2 >= 0.90,
       str("RR screw boss too close to buzzer C-clip: ", DIST_RR_BUZZ));
assert((BUZZ_CX - BUZZ_CCLIP_OD/2) - BAT_X1 >= 0.40, "Buzzer C-clip hits battery bay");
assert((BUZZ_CY - BUZZ_CCLIP_OD/2) - (ENC_CY + WHEEL_WIN_DIA/2) >= 1.30,
       "Buzzer C-clip hits thumbwheel window");

DIST_ML_BAT_Y = (BAT_CY - BAT_BAY_R) - (BOSS_ML[1] + BOSS_OD/2);
assert(DIST_ML_BAT_Y >= 0.30, str("ML screw boss hits battery bay: ", DIST_ML_BAT_Y));
assert(CASE_W - (BAT_CY + BAT_BAY_R) >= 1.80, "Outer wall behind battery too thin");

assert(BOSS_FL[1] + BOSS_OD/2 <= KEY_CY - KEYWELL_W/2 - 0.2, "FL boss clips key-well");
assert(BOSS_ML[1] - BOSS_OD/2 >= KEY_CY + KEYWELL_W/2 + 0.2, "ML boss clips key-well");
assert(BOSS_FR[0] - BOSS_OD/2 >= PLATE_CX + PLATE_PKT_L/2, "FR boss enters MX plate pocket");
assert(PLATE_Z0 - 8.00 >= USBC_OPEN_Z1 + 1.50, "Switch pins risk touching USB-C top");

assert(ENC_CX + EC11_PKT_W/2 <= CASE_L - WALL - FIT_TOL + 0.01, "EC11 cradle hits +X inner wall");
assert(ENC_CX + WHEEL_DIA/2 - CASE_L >= 2.50, "Thumbwheel peek < 2.5 mm");
assert(WHEEL_Z0 - WHEEL_WALL_ARCH_Z0 >= 1.00, "Insufficient thumbwheel click travel");
assert(WHEEL_HUB_Z0 - EC11_BUSH_Z1 >= 0.70, "Thumbwheel hub bottoms out on M7 bushing");
assert(WHEEL_Z1 - EC11_SHAFT_Z1 >= 1.00, "Thumbwheel top skin < 1.0 mm over shaft");
for (p = RET_CLAMP_PTS) {
    assert(norm([p[0] - ENC_CX, p[1] - ENC_CY]) - 1.1 >= WHEEL_WIN_DIA/2 + 0.05,
           str("Retainer clamp post enters thumbwheel window: ", p));
}

// ==============================================================================
// TOP-LEVEL VIEW DISPATCH
// ==============================================================================
if (VIEW_PART == "all") {
    color([0.22, 0.24, 0.28, 0.75]) top_shell_world();
    color([0.30, 0.33, 0.38, 0.85]) bottom_tray();
    color([0.85, 0.52, 0.16]) mx_switch_plate_world();
    color([0.85, 0.52, 0.16]) ec11_retainer_world();
    color([0.16, 0.17, 0.20]) thumbwheel_world();
    if (SHOW_HARDWARE) hardware_all();

} else if (VIEW_PART == "exploded") {
    color([0.22, 0.24, 0.28]) translate([0, 0, 26]) top_shell_world();
    color([0.16, 0.17, 0.20]) translate([0, 0, 44]) thumbwheel_world();
    color([0.85, 0.52, 0.16]) translate([0, 0, 14]) mx_switch_plate_world();
    color([0.85, 0.52, 0.16]) translate([0, 0, 10]) ec11_retainer_world();
    color([0.30, 0.33, 0.38]) bottom_tray();
    if (SHOW_HARDWARE) translate([0, 0, 4]) hardware_all();

} else if (VIEW_PART == "section_y") {
    difference() {
        union() {
            color([0.22, 0.24, 0.28]) top_shell_world();
            color([0.30, 0.33, 0.38]) bottom_tray();
            color([0.85, 0.52, 0.16]) mx_switch_plate_world();
            color([0.85, 0.52, 0.16]) ec11_retainer_world();
            color([0.16, 0.17, 0.20]) thumbwheel_world();
            if (SHOW_HARDWARE) hardware_all();
        }
        translate([-15, -10, -5]) cube([CASE_L + 30, KEY_CY + 10, CASE_H + 30]);
    }

} else if (VIEW_PART == "section_x") {
    difference() {
        union() {
            color([0.22, 0.24, 0.28]) top_shell_world();
            color([0.30, 0.33, 0.38]) bottom_tray();
            color([0.85, 0.52, 0.16]) mx_switch_plate_world();
            color([0.85, 0.52, 0.16]) ec11_retainer_world();
            color([0.16, 0.17, 0.20]) thumbwheel_world();
            if (SHOW_HARDWARE) hardware_all();
        }
        translate([-15, -10, -5]) cube([PLATE_CX + 15, CASE_W + 20, CASE_H + 30]);
    }

} else if (VIEW_PART == "top_shell") {
    top_shell_print();
} else if (VIEW_PART == "bottom_tray") {
    bottom_tray();
} else if (VIEW_PART == "mx_plate") {
    mx_switch_plate_print();
} else if (VIEW_PART == "ec11_retainer") {
    ec11_retainer_print();
} else if (VIEW_PART == "thumbwheel") {
    thumbwheel_print();
} else if (VIEW_PART == "print_all") {
    print_plate_all();
} else if (VIEW_PART == "test_coupons") {
    test_coupons_print();
} else if (VIEW_PART == "hardware_only") {
    hardware_all();
}

// ==============================================================================
// UTILITY GEOMETRY HELPERS (FAST 2D EXTRUSIONS — ZERO SLOW 3D HULLS)
// ==============================================================================
module rounded_rect_2d(l, w, r) {
    offset(r = r) offset(delta = -r) square([l, w], center = false);
}

module rounded_rect_2d_c(l, w, r) {
    translate([-l/2, -w/2])
        rounded_rect_2d(l, w, r);
}

module rounded_box(l, w, h, r) {
    linear_extrude(height = h, convexity = 4)
        rounded_rect_2d(l, w, r);
}

module rounded_box_c(l, w, h, r) {
    linear_extrude(height = h, convexity = 4)
        rounded_rect_2d_c(l, w, r);
}

module stadium_2d(l, w) {
    hull() {
        if (l >= w) {
            for (sx = [-1, 1])
                translate([sx * (l - w) / 2, 0]) circle(d = w);
        } else {
            for (sy = [-1, 1])
                translate([0, sy * (w - l) / 2]) circle(d = l);
        }
    }
}

module chamfered_cylinder(d, h, c_bot = 0.4, c_top = 0.0) {
    union() {
        if (c_bot > 0)
            cylinder(d1 = d - 2*c_bot, d2 = d, h = c_bot + EPS);
        translate([0, 0, c_bot])
            cylinder(d = d, h = h - c_bot - c_top);
        if (c_top > 0)
            translate([0, 0, h - c_top - EPS])
                cylinder(d1 = d, d2 = d - 2*c_top, h = c_top + EPS);
    }
}

module wafer_din912_counterbore(cb_d, clear_d, cb_depth, total_h) {
    taper_h = (cb_d - clear_d) / 2;
    translate([0, 0, -0.1]) {
        cylinder(d = clear_d, h = total_h + 0.2);
        cylinder(d = cb_d, h = cb_depth + 0.1);
        translate([0, 0, cb_depth + 0.1])
            cylinder(d1 = cb_d, d2 = clear_d, h = taper_h);
    }
}

// Keychain loop lug at Rear-Left (-X, +Y) corner (bore strictly outside X = 0!)
KEYCHAIN_LUG_X = -4.20;
KEYCHAIN_LUG_Y = CASE_W - 7.80;

module keychain_lug_solid(h = 6.50) {
    hull() {
        translate([KEYCHAIN_LUG_X, KEYCHAIN_LUG_Y, 0])
            cylinder(d = KEYRING_DIA + 2 * KEYRING_WALL, h = h);
        translate([CORNER_R, CASE_W - 7.80, 0])
            cylinder(r = CORNER_R, h = h);
    }
}

module keychain_lug_bore(h = 6.50) {
    translate([KEYCHAIN_LUG_X, KEYCHAIN_LUG_Y, -0.5])
        cylinder(d = KEYRING_DIA, h = h + 1.0);
    translate([KEYCHAIN_LUG_X, KEYCHAIN_LUG_Y, -0.1])
        cylinder(d1 = KEYRING_DIA + 1.0, d2 = KEYRING_DIA, h = 0.6);
    translate([KEYCHAIN_LUG_X, KEYCHAIN_LUG_Y, h - 0.5])
        cylinder(d1 = KEYRING_DIA, d2 = KEYRING_DIA + 1.0, h = 0.6);
}

// ==============================================================================
// 1. TOP SHELL (World orientation Z = Z_SPLIT + SEAM_GAP .. CASE_H)
// ==============================================================================
module top_shell_world() {
    z_bot = Z_SPLIT + SEAM_GAP; // 4.85 mm (clean 0.05 mm axial seam clearance)
    lug_h = 6.50;
    lug_z0 = CASE_H - lug_h;    // 16.50..23.00 mm (prints flat on bed Z_print = 0..6.50!)

    difference() {
        union() {
            // A. Outer shell + rabbet skirt + ceiling
            difference() {
                union() {
                    rounded_box(CASE_L, CASE_W, CASE_H, CORNER_R);
                    translate([0, 0, lug_z0])
                        keychain_lug_solid(lug_h);
                }

                // Cut everything below Z_SPLIT + SEAM_GAP
                translate([-20, -20, -5])
                    cube([CASE_L + 40, CASE_W + 40, z_bot + 5]);

                // Main internal cavity up to ceiling (Z = z_bot..21.00)
                translate([WALL, WALL, z_bot - 0.1])
                    rounded_box(INNER_L, INNER_W, CASE_H - TOP_CEILING - z_bot + 0.1, INNER_R);

                // Support-free 3-step rabbet socket (0.40 mm steps = 1 perimeter each, fast CGAL!)
                translate([SKIRT_OUTER_T, SKIRT_OUTER_T, z_bot - 0.1])
                    rounded_box(CASE_L - 2*SKIRT_OUTER_T, CASE_W - 2*SKIRT_OUTER_T,
                                LIP_H + 0.35, CORNER_R - SKIRT_OUTER_T);
                translate([SKIRT_OUTER_T + 0.40, SKIRT_OUTER_T + 0.40, z_bot + LIP_H + 0.25])
                    rounded_box(CASE_L - 2*(SKIRT_OUTER_T + 0.40), CASE_W - 2*(SKIRT_OUTER_T + 0.40),
                                0.45, CORNER_R - SKIRT_OUTER_T - 0.40);
                translate([SKIRT_OUTER_T + 0.80, SKIRT_OUTER_T + 0.80, z_bot + LIP_H + 0.65])
                    rounded_box(CASE_L - 2*(SKIRT_OUTER_T + 0.80), CASE_W - 2*(SKIRT_OUTER_T + 0.80),
                                0.45, CORNER_R - SKIRT_OUTER_T - 0.80);

                // 1. Key-well bezel cutout (Z = 21.50..23.00) + fast 45-deg tapered ramp to Z = 20.05
                translate([PLATE_CX, KEY_CY, PLATE_Z1 + 1.45])
                    rounded_box_c(KEYWELL_L, KEYWELL_W, KEYWELL_DEPTH + 1.0, 1.40);
                translate([PLATE_CX, KEY_CY, PLATE_Z1 + 0.05])
                    linear_extrude(height = 1.42, convexity = 4,
                                   scale = [KEYWELL_L / PLATE_PKT_L, KEYWELL_W / PLATE_PKT_W])
                        rounded_rect_2d_c(PLATE_PKT_L, PLATE_PKT_W, 1.40);

                // 2. MX Switch Plate seating pocket (Z = 18.50..20.05)
                translate([PLATE_CX, KEY_CY, PLATE_Z0])
                    rounded_box_c(PLATE_PKT_L, PLATE_PKT_W, MX_PLATE_T + SEAM_GAP + 0.02, 1.40);

                // 3. Thumbwheel top cutout (Dia 24.80 mm) & clean full-height +X wall U-slot
                //    (sealed from below by bottom_tray's matching dust-shield sill -> zero overhang!)
                translate([ENC_CX, ENC_CY, WHEEL_WALL_ARCH_Z0])
                    cylinder(d = WHEEL_WIN_DIA, h = CASE_H - WHEEL_WALL_ARCH_Z0 + 1.0);
                translate([CASE_L - WALL - 0.30, ENC_CY - 10.30, z_bot - 0.1])
                    cube([WALL + 5.0, 20.60, CASE_H - z_bot + 1.2]);
            }

            // B. 4x M1.6 Blind Screw Bosses + Corner Gussets (gussets start above rabbet lip!)
            for (p = BOSS_PTS) {
                translate([p[0], p[1], z_bot])
                    cylinder(d = BOSS_OD, h = BOSS_TOP_Z - z_bot);
            }
            gusset_z0 = z_bot + LIP_H + 0.45;
            translate([BOSS_FR[0] - 1.2, WALL - JOIN_D, gusset_z0])
                cube([2.4, BOSS_FR[1] - WALL + JOIN_D, BOSS_TOP_Z - gusset_z0]);
            translate([WALL - JOIN_D, BOSS_ML[1] - 1.2, gusset_z0])
                cube([BOSS_ML[0] - WALL + JOIN_D, 2.4, BOSS_TOP_Z - gusset_z0]);

            // C. Overhead Battery Retention Saddles (U-shaped valleys -> zero overhangs!)
            for (bx = [15.0, 38.0]) {
                translate([bx, BAT_CY, 0])
                    difference() {
                        translate([-2.0, -BAT_BAY_R - 1.6, BAT_ZC])
                            cube([4.0, 2 * (BAT_BAY_R + 1.6), CASE_H - TOP_CEILING - BAT_ZC + JOIN_D]);
                        translate([0, 0, BAT_ZC])
                            rotate([0, 90, 0])
                                cylinder(r = BAT_BAY_R, h = 6.0, center = true);
                    }
            }

            // D. Piezo Buzzer Inverted C-Clip Cradle + 3 Crush Ribs (Z = 12.80..21.00)
            translate([BUZZ_CX, BUZZ_CY, BUZZ_Z0]) {
                difference() {
                    cylinder(d = BUZZ_CCLIP_OD, h = BUZZ_Z1 - BUZZ_Z0 + JOIN_D);
                    translate([0, 0, -0.1])
                        cylinder(d = BUZZ_CCLIP_ID, h = BUZZ_Z1 - BUZZ_Z0 + 0.5);
                    rotate([0, 0, -135])
                        translate([-4.8, 0, -0.1])
                            cube([9.6, BUZZ_CCLIP_OD, BUZZ_Z1 - BUZZ_Z0 + 0.5]);
                }
                for (ang = [15, 115, 305]) {
                    rotate([0, 0, ang]) {
                        translate([BUZZ_DIA/2 - 0.15, -0.5, 1.10])
                            cube([0.65, 1.0, BUZZ_Z1 - BUZZ_Z0 - 1.10 + JOIN_D]);
                        hull() {
                            translate([BUZZ_DIA/2 - 0.15, -0.5, 1.05])
                                cube([0.65, 1.0, 0.10]);
                            translate([BUZZ_DIA/2 + 0.15, -0.5, 0.00])
                                cube([0.35, 1.0, 0.10]);
                        }
                    }
                }
            }

            // E. 4x Vertical Clamping Columns holding ec11_retainer_plate at Z = 12.35..21.00
            for (p = RET_CLAMP_PTS) {
                translate([p[0], p[1], RET_Z1 + SEAM_GAP])
                    cylinder(d = 2.20, h = CASE_H - TOP_CEILING - (RET_Z1 + SEAM_GAP) + JOIN_D);
            }
        }

        // --- SUBTRACTIONS THROUGH TOP SHELL ---

        // 1. Keychain bore & edge chamfers
        translate([0, 0, lug_z0])
            keychain_lug_bore(lug_h);

        // 2. 4x M1.6 Blind Pilot Holes
        for (p = BOSS_PTS) {
            translate([p[0], p[1], z_bot - 0.1])
                cylinder(d = M16_PILOT_DIA, h = PILOT_STOP_Z - z_bot + 0.1);
            translate([p[0], p[1], z_bot - 0.05])
                cylinder(d1 = M16_PILOT_DIA + 0.60, d2 = M16_PILOT_DIA, h = 0.35);
        }

        // 3. USB-C Arch Cutout on -X Wall + Plug Boot Chamfer
        translate([-0.1, MCU_CY - USBC_OPEN_W/2, z_bot - 0.1])
            cube([WALL + 0.2, USBC_OPEN_W, USBC_OPEN_Z1 - z_bot + 0.1]);
        hull() {
            translate([-0.1, MCU_CY - (USBC_OPEN_W + 2.8)/2, z_bot - 0.1])
                cube([0.2, USBC_OPEN_W + 2.8, USBC_OPEN_Z1 - z_bot + 1.5]);
            translate([0.9, MCU_CY - USBC_OPEN_W/2, z_bot - 0.1])
                cube([0.1, USBC_OPEN_W, USBC_OPEN_Z1 - z_bot + 0.1]);
        }

        // 4. 3x Buzzer Acoustic Vent Slots on Top Ceiling
        for (dy = [-2.6, 0, 2.6]) {
            translate([BUZZ_CX, BUZZ_CY + dy, CASE_H - TOP_CEILING - 0.2])
                linear_extrude(height = TOP_CEILING + 0.4)
                    stadium_2d(8.00, 1.30);
        }

        // 5. Subtle LED Light-Bleed Pinhole
        translate([LED_HOLE_X, LED_HOLE_Y, CASE_H - TOP_CEILING - 0.2])
            cylinder(d = LED_HOLE_DIA, h = TOP_CEILING + 0.4);
        translate([LED_HOLE_X, LED_HOLE_Y, CASE_H - 0.45])
            cylinder(d1 = LED_HOLE_DIA, d2 = LED_HOLE_DIA + 0.80, h = 0.55);
    }
}

module top_shell_print() {
    translate([0, CASE_W, CASE_H])
        rotate([180, 0, 0])
            top_shell_world();
}

// ==============================================================================
// 2. BOTTOM TRAY (World & Print orientation identical: floor down at Z = 0)
// ==============================================================================
module bottom_tray() {
    difference() {
        union() {
            // A. Outer Shell (Z = 0..Z_SPLIT) + Interlocking Rabbet Lip (Z = Z_SPLIT..Z_SPLIT+LIP_H)
            difference() {
                union() {
                    rounded_box(CASE_L, CASE_W, Z_SPLIT, CORNER_R);
                    translate([WALL - LIP_INNER_T, WALL - LIP_INNER_T, Z_SPLIT - JOIN_D])
                        rounded_box(CASE_L - 2*(WALL - LIP_INNER_T),
                                    CASE_W - 2*(WALL - LIP_INNER_T),
                                    LIP_H + JOIN_D,
                                    CORNER_R - (WALL - LIP_INNER_T));
                }
                translate([WALL, WALL, BOTTOM_FLOOR])
                    rounded_box(INNER_L, INNER_W, Z_SPLIT + LIP_H + 1.0, INNER_R);

                // Notch mating lip cleanly across -X USB-C port slot & +X thumbwheel dust sill
                translate([-0.5, MCU_CY - USBC_OPEN_W/2 - 0.2, Z_SPLIT - 0.05])
                    cube([WALL + 1.0, USBC_OPEN_W + 0.4, LIP_H + 0.5]);
                translate([CASE_L - WALL - 0.4, ENC_CY - 10.50, Z_SPLIT - 0.05])
                    cube([WALL + 1.0, 21.00, LIP_H + 0.5]);
            }

            // A2. +X Wall Dust-Shield Sill rising under Thumbwheel (Z = 1.60..17.00, 0.20mm Y clear)
            translate([CASE_L - WALL, ENC_CY - 10.10, BOTTOM_FLOOR - JOIN_D])
                cube([WALL, 20.20, (WHEEL_WALL_ARCH_Z0 - 0.20) - BOTTOM_FLOOR + JOIN_D]);

            // B. 4x Screw Boss Landing Pads + Solid Wall Tie-Blocks (eliminates tangent non-manifoldness!)
            for (p = BOSS_PTS) {
                translate([p[0], p[1], 0])
                    cylinder(d = BOSS_OD, h = Z_SPLIT);
            }
            // Anchor FL, FR, ML, RR solid pads into adjacent perimeter walls
            translate([WALL - JOIN_D, BOSS_FL[1] - 1.5, 0])
                cube([BOSS_FL[0] - WALL + JOIN_D, 3.0, Z_SPLIT]);
            translate([BOSS_FR[0] - 1.5, WALL - JOIN_D, 0])
                cube([3.0, BOSS_FR[1] - WALL + JOIN_D, Z_SPLIT]);
            translate([WALL - JOIN_D, BOSS_ML[1] - 1.5, 0])
                cube([BOSS_ML[0] - WALL + JOIN_D, 3.0, Z_SPLIT]);
            translate([BOSS_RR[0] - 1.5, BOSS_RR[1], 0])
                cube([3.0, CASE_W - WALL - BOSS_RR[1] + JOIN_D, Z_SPLIT]);

            // C. Pro Micro nRF52840 Cradle (Underside wire trenches + 5mm B+/B- rail relief!)
            for (sy = [-1, 1]) {
                ry = (sy < 0) ? (MCU_Y0 - 1.40) : (MCU_Y0 + MCU_POCKET_W);
                translate([MCU_X0 + MCU_RAIL_RELIEF, ry, BOTTOM_FLOOR - JOIN_D])
                    cube([MCU_POCKET_L - MCU_RAIL_RELIEF + 0.80, 1.40, (Z_SPLIT - BOTTOM_FLOOR) + 0.40 + JOIN_D]);
            }
            translate([MCU_X1, MCU_CY - 4.00, BOTTOM_FLOOR - JOIN_D])
                cube([1.60, 8.00, (Z_SPLIT - BOTTOM_FLOOR) + 0.40 + JOIN_D]);
            translate([MCU_X0 + 1.50, MCU_CY - 4.20, BOTTOM_FLOOR - JOIN_D])
                cube([MCU_POCKET_L - 2.50, 8.40, MCU_TRENCH_H + JOIN_D]);
            for (sy = [-1, 1]) {
                py = (sy < 0) ? (MCU_Y0 + 2.50) : (MCU_Y0 + MCU_POCKET_W - 4.30);
                translate([MCU_X0 + 0.20, py, BOTTOM_FLOOR - JOIN_D])
                    cube([3.00, 1.80, MCU_TRENCH_H + JOIN_D]);
            }

            // D. Cylindrical Battery Bay Cradle (2 curved saddle ribs clipped inside inner +Y wall)
            for (bx = [12.0, 41.0]) {
                translate([bx, BAT_CY, BOTTOM_FLOOR - JOIN_D])
                    difference() {
                        translate([-2.50, -BAT_BAY_R - 1.20, 0])
                            cube([5.00, (BAT_BAY_R + 1.20) + (CASE_W - WALL - FIT_TOL - BAT_CY),
                                  (Z_SPLIT - BOTTOM_FLOOR) + 0.80 + JOIN_D]);
                        translate([0, 0, BAT_ZC - BOTTOM_FLOOR + JOIN_D])
                            rotate([0, 90, 0])
                                cylinder(r = BAT_BAY_R, h = 6.0, center = true);
                    }
            }
            translate([BAT_X1, BAT_CY - 4.50, BOTTOM_FLOOR - JOIN_D])
                cube([1.60, 9.00, (Z_SPLIT - BOTTOM_FLOOR) + 2.50 + JOIN_D]);

            // E. 4x Vertical Plate-Support Posts under mx_switch_plate (Z = 1.60..18.45)
            for (p = PLATE_CLAMP_PTS) {
                translate([p[0] - 1.60, p[1] - 0.70, BOTTOM_FLOOR - JOIN_D])
                    cube([3.20, 1.40, (PLATE_Z0 - SEAM_GAP) - BOTTOM_FLOOR + JOIN_D]);
                translate([p[0] - 2.00, p[1] - 1.00, BOTTOM_FLOOR - JOIN_D])
                    cube([4.00, 2.00, 3.00 + JOIN_D]);
            }

            // F. Bare EC11 Anti-Rotation U-Cradle (solid manifold L-corners, clear of +X wall!)
            ec11_x0 = ENC_CX - EC11_PKT_W / 2; // 55.05
            ec11_x1 = ENC_CX + EC11_PKT_W / 2; // 67.55 (<= 67.60 inner wall clear!)
            ec11_y0 = ENC_CY - EC11_PKT_L / 2; // 10.00
            ec11_y1 = ENC_CY + EC11_PKT_L / 2; // 23.00
            cradle_h = (EC11_BODY_Z1 - SEAM_GAP) - BOTTOM_FLOOR + JOIN_D;

            // Continuous -Y side wall + solid -X corner wrap
            translate([ec11_x0 - 1.50, ec11_y0 - 1.50, BOTTOM_FLOOR - JOIN_D])
                cube([EC11_PKT_W + 1.50, 1.50, cradle_h]);
            // Continuous +Y side wall + solid -X corner wrap
            translate([ec11_x0 - 1.50, ec11_y1, BOTTOM_FLOOR - JOIN_D])
                cube([EC11_PKT_W + 1.50, 1.50, cradle_h]);
            // Two -X corner return tabs overlapping cleanly into -Y/+Y walls (open center for wires!)
            translate([ec11_x0 - 1.50, ec11_y0 - 0.20, BOTTOM_FLOOR - JOIN_D])
                cube([1.50, 3.20, cradle_h]);
            translate([ec11_x0 - 1.50, ec11_y1 - 3.00, BOTTOM_FLOOR - JOIN_D])
                cube([1.50, 3.20, cradle_h]);
            // 4 corner seating pads overlapping 0.30 mm into cradle walls (zero non-manifold seams!)
            for (sx = [-1, 1], sy = [-1, 1]) {
                px = (sx < 0) ? (ec11_x0 - 0.30) : (ec11_x1 - 2.20);
                py = (sy < 0) ? (ec11_y0 - 0.30) : (ec11_y1 - 2.20);
                translate([px, py, BOTTOM_FLOOR - JOIN_D])
                    cube([2.50, 2.50, EC11_WIRE_CLR + JOIN_D]);
            }
        }

        // --- SUBTRACTIONS ---
        for (p = BOSS_PTS) {
            translate([p[0], p[1], 0])
                wafer_din912_counterbore(M16_CB_DIA, M16_CLEAR_DIA, M16_CB_DEPTH, Z_SPLIT + 0.5);
        }
    }
}

// ==============================================================================
// 3. DROP-IN CHERRY MX SWITCH PLATE (1.50 mm, Prints Flat at Z = 0)
// ==============================================================================
module mx_switch_plate_print() {
    difference() {
        union() {
            translate([PLATE_CX, KEY_CY, 0]) {
                rounded_box_c(PLATE_L, PLATE_W, MX_PLATE_T - 0.75, 1.20);
                translate([0, 0, MX_PLATE_T - 0.76])
                    linear_extrude(height = 0.76, convexity = 4,
                                   scale = [(PLATE_L - 1.40) / PLATE_L, (PLATE_W - 1.40) / PLATE_W])
                        rounded_rect_2d_c(PLATE_L, PLATE_W, 1.20);
            }
        }

        for (kx = [KEY1_CX, KEY2_CX]) {
            translate([kx - MX_CUTOUT/2, KEY_CY - MX_CUTOUT/2, -0.2])
                cube([MX_CUTOUT, MX_CUTOUT, MX_PLATE_T + 0.4]);
        }

        // Clean stepped corner reliefs clearing FL & ML screw bosses (no thin feather tips)
        translate([2.0, 5.5, -0.2]) cube([5.2, 1.7, MX_PLATE_T + 0.4]);
        translate([2.0, 26.6, -0.2]) cube([5.2, 1.5, MX_PLATE_T + 0.4]);
    }
}

module mx_switch_plate_world() {
    translate([0, 0, PLATE_Z0])
        mx_switch_plate_print();
}

// ==============================================================================
// 4. BARE EC11 RETAINER PLATE (2.00 mm, Prints Flat at Z = 0)
// ==============================================================================
module ec11_retainer_print() {
    difference() {
        union() {
            // Central hub plate covering EC11 body, stopping 0.5mm short of +X inner wall (67.8)
            translate([60.65, ENC_CY, 0])
                rounded_box_c(13.30, 15.60, RET_T, 1.50);
            for (p = RET_CLAMP_PTS) {
                hull() {
                    translate([ENC_CX, ENC_CY, 0])
                        cylinder(d = 9.40, h = RET_T);
                    translate([p[0], p[1], 0])
                        cylinder(d = 2.40, h = RET_T);
                }
            }
        }
        translate([ENC_CX, ENC_CY, -0.2])
            cylinder(d = EC11_BUSHING_DIA + 0.40, h = RET_T + 0.4);
    }
}

module ec11_retainer_world() {
    translate([0, 0, RET_Z0])
        ec11_retainer_print();
}

// ==============================================================================
// 5. CUSTOM 3D-PRINTED KNURLED THUMBWHEEL ROLLER (Fast 2D Extruded Knurl)
// ==============================================================================
module thumbwheel_print() {
    hub_h = WHEEL_Z0 - WHEEL_HUB_Z0;
    total_h = WHEEL_Z1 - WHEEL_HUB_Z0;
    bore_depth = total_h - (WHEEL_Z1 - EC11_SHAFT_Z1);

    difference() {
        union() {
            // Single intersection of chamfered cylinder & 2D knurled disc (ultra-fast CGAL!)
            intersection() {
                chamfered_cylinder(WHEEL_DIA, WHEEL_H, c_bot = 0.60, c_top = 0.60);
                linear_extrude(height = WHEEL_H + 0.2, convexity = 6)
                    difference() {
                        circle(d = WHEEL_DIA + 0.2);
                        for (i = [0 : WHEEL_FLUTES - 1]) {
                            rotate([0, 0, i * (360 / WHEEL_FLUTES)])
                                translate([WHEEL_DIA / 2 + 0.25, 0])
                                    rotate([0, 0, 45])
                                        square([1.15, 1.15], center = true);
                        }
                    }
            }

            // Inner shaft hub sleeve
            translate([0, 0, WHEEL_H - 0.50])
                chamfered_cylinder(10.60, hub_h + 0.50, c_bot = 0.0, c_top = 0.40);
        }

        // Tactile click dimple on outer top face (Z_print = 0)
        translate([0, 0, -0.1])
            cylinder(d1 = 7.00, d2 = 5.20, h = 0.45);

        // Underside weight-saving pocket around hub
        translate([0, 0, 1.80])
            difference() {
                cylinder(d = WHEEL_DIA - 4.40, h = WHEEL_H);
                cylinder(d = 10.60, h = WHEEL_H + 0.5);
            }

        // Press-fit EC11 shaft bore (leaves 1.2mm solid top cap)
        translate([0, 0, total_h - bore_depth])
            cylinder(d = WHEEL_SHAFT_FIT, h = bore_depth + 0.2);

        // Anti-split stress-relief micro-slot across hub collar
        translate([0, -0.25, total_h - bore_depth + 1.0])
            cube([6.0, 0.50, bore_depth]);
    }
}

module thumbwheel_world() {
    translate([ENC_CX, ENC_CY, WHEEL_Z1])
        rotate([180, 0, 0])
            thumbwheel_print();
}

// ==============================================================================
// 6. COMBINED SINGLE-JOB PRINT PLATE & CALIBRATION COUPONS
// ==============================================================================
module print_plate_all() {
    translate([0, 0, 0]) bottom_tray();
    translate([CASE_L + 18, 0, 0]) top_shell_print();
    translate([0, CASE_W + 10, 0]) mx_switch_plate_print();
    translate([55, CASE_W + 10, 0]) ec11_retainer_print();
    translate([95, CASE_W + 21, 0]) thumbwheel_print();
}

module test_coupons_print() {
    difference() {
        union() {
            rounded_box(46.0, 26.0, 4.8, 2.5);
            translate([38.0, 19.0, 0]) cylinder(d = BOSS_OD, h = 8.0);
        }
        translate([3.0, 5.9, -0.2]) cube([MX_CUTOUT, MX_CUTOUT, 5.2]);
        translate([2.0, 4.9, 0]) cube([MX_CUTOUT + 2.0, MX_CUTOUT + 2.0, 3.3]);
        translate([26.0, 13.0, -0.2]) cylinder(d = EC11_BUSHING_DIA + 0.40, h = 5.2);
        translate([38.0, 7.0, 0]) wafer_din912_counterbore(M16_CB_DIA, M16_CLEAR_DIA, M16_CB_DEPTH, 5.0);
        translate([38.0, 19.0, -0.2]) cylinder(d = M16_PILOT_DIA, h = 8.5);
    }
    translate([58.0, 13.0, 0])
        difference() {
            cylinder(d = 12.0, h = 6.0);
            translate([0, 0, 1.2]) cylinder(d = WHEEL_SHAFT_FIT, h = 5.0);
        }
}

// ==============================================================================
// 7. INTERNAL HARDWARE MOCKUPS (FOR ASSEMBLY, CUTAWAY & CSG COLLISION TESTS)
// ==============================================================================
module hardware_mcu() {
    color([0.12, 0.14, 0.16])
        translate([WALL + 0.30, MCU_CY - MCU_PCB_W/2, MCU_PCB_Z0 + 0.02])
            cube([MCU_PCB_L, MCU_PCB_W, MCU_PCB_T]);
    color([0.75, 0.77, 0.80])
        translate([WALL + 0.30 - 1.00, MCU_CY - MCU_USBC_W/2, MCU_PCB_Z1 + 0.02])
            cube([7.50, MCU_USBC_W, MCU_USBC_H]);
    color([0.85, 0.20, 0.20])
        translate([WALL + 0.30 + MCU_PCB_L - 3.5, MCU_CY - 2.0, MCU_PCB_Z1 + 0.02])
            cube([3.2, 4.0, 1.1]);
}

module hardware_battery() {
    color([0.92, 0.48, 0.10])
        translate([BAT_CELL_X0 - 0.02, BAT_CY, BAT_ZC])
            rotate([0, 90, 0])
                cylinder(d = BAT_DIA, h = BAT_LEN);
}

module hardware_mx_switches() {
    for (kx = [KEY1_CX, KEY2_CX]) {
        color([0.85, 0.65, 0.65])
            translate([kx - 6.8, KEY_CY - 6.8, PLATE_Z1 - 5.0])
                cube([13.6, 13.6, 5.0]);
        color([0.85, 0.65, 0.65])
            translate([kx, KEY_CY, PLATE_Z1 - 8.3])
                cylinder(d = 3.9, h = 3.3);
        color([0.85, 0.65, 0.65])
            translate([kx - 6.8, KEY_CY - 6.8, PLATE_Z1])
                cube([13.6, 13.6, 5.1]);
        color([0.15, 0.15, 0.17])
            translate([kx, KEY_CY, PLATE_Z1 + 3.0])
                linear_extrude(height = 8.0, convexity = 4, scale = 14.0 / KEYCAP_W)
                    square([KEYCAP_W, KEYCAP_W], center = true);
    }
}

module hardware_ec11() {
    color([0.75, 0.77, 0.80])
        translate([ENC_CX - EC11_BODY_W/2, ENC_CY - EC11_BODY_L/2, EC11_BODY_Z0 + 0.02])
            cube([EC11_BODY_W, EC11_BODY_L, EC11_BODY_H]);
    color([0.55, 0.58, 0.62])
        translate([ENC_CX, ENC_CY, EC11_BODY_Z1 + 0.02])
            cylinder(d = EC11_BUSHING_DIA, h = EC11_BUSHING_H);
    color([0.80, 0.82, 0.85])
        translate([ENC_CX, ENC_CY, EC11_BUSH_Z1 + 0.02])
            cylinder(d = EC11_SHAFT_DIA, h = EC11_SHAFT_ABOVE_BODY - EC11_BUSHING_H - 0.04);
}

module hardware_buzzer() {
    color([0.15, 0.15, 0.17])
        translate([BUZZ_CX, BUZZ_CY, BUZZ_Z1 - BUZZ_H])
            cylinder(d = BUZZ_DIA, h = BUZZ_H);
}

module hardware_all() {
    hardware_mcu();
    hardware_battery();
    hardware_mx_switches();
    hardware_ec11();
    hardware_buzzer();
}
