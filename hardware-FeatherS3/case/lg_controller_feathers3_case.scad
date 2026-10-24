// Case for JanM321 esphome-lg-controller (hardware-FeatherS3 carrier)
// with an Unexpected Maker FeatherS3[D] plugged into its sockets.
//
// Carrier: from lg_hvac_esp32.kicad_pcb
//   22.86 x 50.80 mm, 2.54 mm corners, 4x M2.5 on the Feather pattern
//   JST-XH 3-pin side-entry (J1) on the USB end
// FeatherS3[D]: from UnexpectedMaker/esp32s3 series_d/3d models/FeatherS3D.STEP
//   22.86 x 51.26 mm PCB; runs 0.46 mm past the carrier at the far end
//   3 holes only; antenna corner has a 1 mm radius and no hole
//   USB-C centred across the board, overhangs the edge by 1.53 mm
//   tallest part 6.5 mm above PCB underside (JST-PH battery)
//
// Coordinates: X across the board, Y along it, Y max = USB/XH end,
// origin at the carrier corner on the 16-pin socket side.
//
// part = "base" | "lid" | "print" (both laid out) | "assembly" (preview)

part = "assembly";

/* ---------- Carrier ---------- */
pcb_w   = 22.86;
pcb_l   = 50.80;
pcb_t   = 1.6;
pcb_r   = 2.54;
carrier_holes = [[2.54, 2.54], [20.32, 2.54], [2.54, 48.26], [20.32, 48.26]];

/* ---------- FeatherS3[D] (in carrier coordinates) ---------- */
s3_y0     = -0.46;      // far end overhang past the carrier
s3_t      = 1.0;
s3_r      = 2.54;
s3_r_ant  = 1.0;        // antenna corner (x max, y min)
s3_holes  = [[2.54, 48.26], [20.32, 48.26], [2.54, 2.08]];

/* ---------- Stack heights (measure yours) ---------- */
standoff_h = 3.5;   // under carrier, clears through-hole leads
socket_h   = 8.5;   // female header body on carrier
pin_spacer = 2.5;   // male header plastic under the S3[D]
top_clear  = 6.5;   // above S3[D] PCB top (JST-PH is 5.6)

/* ---------- Case ---------- */
wall     = 2.0;
floor_t  = 2.0;
top_t    = 2.0;
clr      = 0.5;     // board-to-wall gap
lip_h    = 3.0;     // lid/base overlap
fit_tol  = 0.15;    // per side on the lip
snap     = 0.3;     // snap ridge height, 0 to disable

screw_hole = 2.2;   // M2.5 self-tapping; 3.6 for heat-set inserts
boss_d     = 5.5;

/* ---------- Openings (USB/XH end) ---------- */
xh_x   = 11.73;     // JST-XH centre (KiCad)
xh_w   = 12.0;
xh_h   = 7.0;       // above carrier top
usb_x  = 11.46;     // USB-C centre (STEP)
usb_zc = 1.7;       // centre height above S3[D] PCB top
usb_w  = 13.0;      // sized for a cable overmould
usb_h  = 7.0;

/* ---------- Extras ---------- */
lid_posts  = true;
mount_ears = true;
ear_hole   = 3.5;

$fn = 48;

/* ---------- Derived ---------- */
z_carrier     = floor_t + standoff_h;
z_carrier_top = z_carrier + pcb_t;
z_s3          = z_carrier_top + socket_h + pin_spacer;
z_s3_top      = z_s3 + s3_t;
split_z       = z_s3_top;
out_h         = z_s3_top + top_clear + top_t;
s3_l          = pcb_l - s3_y0;

/* ---------- 2D profiles ---------- */
module carrier_2d() { offset(pcb_r) offset(-pcb_r) square([pcb_w, pcb_l]); }

module s3_2d() {
    translate([0, s3_y0])
    hull() {
        for (c = [[s3_r, s3_l - s3_r], [pcb_w - s3_r, s3_l - s3_r], [s3_r, s3_r]])
            translate(c) circle(s3_r);
        translate([pcb_w - s3_r_ant, s3_r_ant]) circle(s3_r_ant);
    }
}

module cavity_2d() { offset(clr) union() { carrier_2d(); s3_2d(); } }

// inset > 0 shrinks towards the cavity, inset = wall is the cavity itself
module outline(inset, h) {
    linear_extrude(h) offset(wall - inset) cavity_2d();
}

/* ---------- Bounding box (for ears/layout) ---------- */
bx0 = -clr - wall;
by0 = s3_y0 - clr - wall;
out_w = pcb_w + 2 * (clr + wall);
out_l = pcb_l - s3_y0 + 2 * (clr + wall);

/* ---------- Cutters ---------- */
module rounded_slot(w, h, r) {
    rotate([-90, 0, 0])
        linear_extrude(wall * 3 + 4)
            offset(r) offset(-r) square([w, h], center = true);
}

module cutouts() {
    end_y = pcb_l - 1;
    translate([xh_x, end_y, z_carrier_top - 0.5 + (xh_h + 0.5) / 2])
        rounded_slot(xh_w, xh_h + 0.5, 1);
    translate([usb_x, end_y, z_s3_top + usb_zc])
        rounded_slot(usb_w, usb_h, 2);
}

module snap_ridges(extra = 0) {
    z = split_z - lip_h / 2;
    for (x = [-clr - wall / 2 + 0.01, pcb_w + clr + wall / 2 - 0.01])
        translate([x, pcb_l * 0.25, z])
            rotate([-90, 0, 0]) cylinder(r = snap + extra, h = pcb_l * 0.5, $fn = 16);
}

/* ---------- Base ---------- */
module base() {
    difference() {
        union() {
            difference() {
                outline(0, split_z);
                translate([0, 0, floor_t]) outline(wall, split_z);
                translate([0, 0, split_z - lip_h])
                    difference() {
                        outline(-2, lip_h + 1);
                        outline(wall / 2, lip_h + 1);
                    }
            }
            if (snap > 0) snap_ridges();
            for (h = carrier_holes)
                translate([h[0], h[1], 0]) cylinder(d = boss_d, h = z_carrier);
            if (mount_ears) ears();
        }
        for (h = carrier_holes)
            translate([h[0], h[1], 1.0]) cylinder(d = screw_hole, h = z_carrier);
        cutouts();
    }
}

module ears() {
    ear_l = 9; ear_w = 14; ear_t = 3;
    for (end = [[by0, 1], [by0 + out_l, 0]])
        translate([pcb_w / 2, end[0], 0])
            mirror([0, end[1], 0])
                difference() {
                    hull() {
                        translate([-ear_w / 2, -1, 0]) cube([ear_w, 1, ear_t]);
                        translate([0, ear_l - ear_w / 2 + 2, 0])
                            cylinder(d = ear_w * 0.75, h = ear_t);
                    }
                    translate([0, ear_l - ear_w / 2 + 2, -0.1]) {
                        cylinder(d = ear_hole, h = ear_t + 1);
                        translate([0, 0, ear_t - ear_hole / 2 + 0.1])
                            cylinder(d1 = ear_hole, d2 = ear_hole * 2, h = ear_hole / 2 + 0.01);
                    }
                }
}

/* ---------- Lid (modelled in place, flipped for printing) ---------- */
module lid() {
    difference() {
        union() {
            translate([0, 0, split_z])
                difference() {
                    outline(0, out_h - split_z);
                    translate([0, 0, -0.01]) outline(wall, out_h - split_z - top_t);
                }
            translate([0, 0, split_z - lip_h])
                difference() {
                    outline(0, lip_h);
                    outline(wall / 2 + fit_tol, lip_h + 1);
                }
            if (lid_posts)
                for (h = s3_holes)
                    translate([h[0], h[1], 0]) {
                        translate([0, 0, z_s3_top + 0.1])
                            cylinder(d = 3.8, h = out_h - top_t - z_s3_top);
                        translate([0, 0, z_s3_top - 1.2])
                            cylinder(d = 2.0, h = 1.4);
                    }
        }
        if (snap > 0) snap_ridges(fit_tol + 0.05);
        cutouts();
    }
}

/* ---------- Preview boards ---------- */
module ghost_boards() {
    color("green", 0.6) translate([0, 0, z_carrier]) linear_extrude(pcb_t) carrier_2d();
    color("black", 0.6) translate([0, 0, z_s3]) linear_extrude(s3_t) s3_2d();
    color("silver") translate([usb_x - 4.47, pcb_l - 6.37, z_s3_top]) cube([8.94, 7.9, 3.2]);
    color("white") translate([xh_x - 4.95, pcb_l - 11.9, z_carrier_top]) cube([9.9, 11.5, 6]);
}

/* ---------- Output ---------- */
module lid_flipped() { translate([0, 2 * by0 + out_l, out_h]) rotate([180, 0, 0]) lid(); }

if (part == "base") base();
else if (part == "lid") lid_flipped();
else if (part == "print") { base(); translate([out_w + 25, 0, 0]) lid_flipped(); }
else { base(); %lid(); ghost_boards(); }
