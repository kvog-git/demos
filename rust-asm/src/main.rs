// ================================================================================================
// Rust->assembly interop demo
//
// License:
//     SPDX-License-Identifier: 0BSD
//     Copyright (c) 2026 Hunter Kvalevog
//
//     Permission to use, copy, modify, and/or distribute this software for any
//     purpose with or without fee is hereby granted.
//
//     THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
//     WITH REGARD TO THIS SOFTWARE.
// ================================================================================================

#[repr(C)]
struct Vec3 {
    x: f32,
    y: f32,
    z: f32,
}

impl Vec3 {
    pub fn new(x: f32, y: f32, z: f32) -> Self {
        Self { x, y, z }
    }
}

unsafe extern "C" {
    fn add_u32(a: u32, b: u32) -> u32;
    fn dot_f32v3(a: *const Vec3, b: *const Vec3) -> f32;
}

fn test_add(a: u32, b: u32) {
    let r = unsafe { add_u32(a, b) };
    println!("{a} + {b} = {r}");
}

fn test_dot(a: &Vec3, b: &Vec3) {
    let r = unsafe { dot_f32v3(a, b) };
    println!("[{}, {}, {}] ⋅ [{}, {}, {}] = {}", a.x, a.y, a.z, b.x, b.y, b.z, r);
}

fn main() {
    test_add(1, 2);
    test_dot(&Vec3::new(1.0, 2.0, 3.0), &Vec3::new(4.0, 5.0, 6.0));
}
