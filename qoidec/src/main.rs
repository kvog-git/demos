// ================================================================================================
// Basic Quite Ok Image (QOI) to PPM converter written to learn Rust
//
// ref: https://qoiformat.org/qoi-specification.pdf
// ref: https://github.com/phoboslab/qoi/blob/master/qoi.h
// ref: https://en.wikipedia.org/wiki/Netpbm#32-bit
//
// Changelog:
//     9/29/2026: Initial release
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

use std::env;
use std::fs;
use std::io::{self, Read, Seek, Write};
use std::process::ExitCode;

// Always RGBA32
type Pixel = (u8, u8, u8, u8);

// Always RGBA32
struct Frame {
    pub w:   u32,
    pub h:   u32,
    pub buf: Vec<Pixel>,
}

impl Frame {
    pub fn new(w: u32, h: u32) -> Self {
        Self {
            w:   w,
            h:   h,
            buf: Vec::with_capacity((w * h) as usize),
        }
    }
}

trait ByteReader {
    fn ru8(&mut self) -> io::Result<u8>;
    fn ru32be(&mut self) -> io::Result<u32>;
}

impl<R: Read> ByteReader for R {
    fn ru8(&mut self) -> io::Result<u8> {
        let mut buf = [0u8];
        self.read_exact(&mut buf)?;
        Ok(buf[0])
    }

    fn ru32be(&mut self) -> io::Result<u32> {
        let mut buf = [0u8; 4];
        self.read_exact(&mut buf)?;
        Ok(u32::from_be_bytes(buf))
    }
}

fn index_pos(p: Pixel) -> usize {
    let r = p.0 as u32;
    let g = p.1 as u32;
    let b = p.2 as u32;
    let a = p.3 as u32;
    return ((r * 3 + g * 5 + b * 7 + a * 11) % 64) as usize;
}

fn decode_qoi(inp: &mut (impl Read + Seek)) -> io::Result<Frame> {
    // magic was validated by caller
    inp.seek(io::SeekFrom::Current(4))?;

    let w = inp.ru32be()?;
    let h = inp.ru32be()?;
    assert!(w > 0 && h > 0);
    assert!((w as u64 * h as u64) < 400_000_000u64);

    inp.ru8()?; // don't care: channels
    inp.ru8()?; // don't care: color space

    let mut frame = Frame::new(w, h);
    let mut cur   = (0u8, 0u8, 0u8, 0xFFu8);
    let mut index = [(0u8, 0u8, 0u8, 0u8); 64];
    let mut run   = 0u8;

    for _ in 0..((w * h) as usize) {
        if run > 0 {
            run -= 1;
        } else {
            let b0 = inp.ru8()?;
            let pixel = match b0 {
                // QOI_OP_RGB
                0b11111110 => (inp.ru8()?, inp.ru8()?, inp.ru8()?, cur.3),
                // QOI_OP_RGBA
                0b11111111 => (inp.ru8()?, inp.ru8()?, inp.ru8()?, inp.ru8()?),
                // 2-bit ops
                _ => match b0 >> 6 {
                    // QOI_OP_INDEX
                    0b00 => index[(b0 & 0b00111111) as usize],
                    // QOI_OP_DIFF
                    0b01 => {
                        let dr = ((b0 >> 4) & 0b11) as i8 - 2;
                        let dg = ((b0 >> 2) & 0b11) as i8 - 2;
                        let db = ((b0 >> 0) & 0b11) as i8 - 2;
                        let r = cur.0.wrapping_add_signed(dr);
                        let g = cur.1.wrapping_add_signed(dg);
                        let b = cur.2.wrapping_add_signed(db);
                        (r, g, b, cur.3)
                    }
                    // QOI_OP_LUMA
                    0b10 => {
                        let b1 = inp.ru8()?;
                        let dg = (b0 & 0b111111) as i8 - 32;
                        let dr = ((b1 >> 4) & 0b1111) as i8 + dg - 8;
                        let db = ((b1 >> 0) & 0b1111) as i8 + dg - 8;
                        let r = cur.0.wrapping_add_signed(dr);
                        let g = cur.1.wrapping_add_signed(dg);
                        let b = cur.2.wrapping_add_signed(db);
                        (r, g, b, cur.3)
                    }
                    // QOI_OP_RUN
                    0b11 => {
                        run = b0 & 0b00111111;
                        cur
                    }
                    _ => unreachable!(),
                }
            };
            cur = pixel;
            index[index_pos(cur)] = cur;
        }
        frame.buf.push(cur);
    }

    Ok(frame)
}

fn encode_ppm(out: &mut impl Write, frame: Frame) -> io::Result<()> {
    out.write_all(b"P6\n")?;
    out.write_all(format!("{} {}\n", frame.w, frame.h).as_bytes())?;
    out.write_all(b"255\n")?;

    for p in frame.buf {
        // just drop the alpha channel lol
        /*
        let val =
            (p.0 as u32)       |
            (p.1 as u32) << 8  |
            (p.2 as u32) << 16;
        out.write_all(&val.to_le_bytes()[0..3])?;
        */
        out.write_all(&[p.0, p.1, p.2])?;
    }

    out.flush()?;

    Ok(())
}

fn main() -> ExitCode {
    let Some(inp_path) = env::args().nth(1) else {
        eprintln!("supply an input file");
        return ExitCode::FAILURE;
    };

    let Some(out_path) = env::args().nth(2) else {
        eprintln!("supply an output file");
        return ExitCode::FAILURE;
    };

    let inp = match fs::File::open(inp_path) {
        Ok(f)  => f,
        Err(e) => {
            eprintln!("failed to open input: {e}");
            return ExitCode::FAILURE;
        },
    };
    let mut inp = io::BufReader::new(inp);

    let out = match fs::File::create(out_path) {
        Ok(f)  => f,
        Err(e) => {
            eprintln!("failed to open output: {e}");
            return ExitCode::FAILURE;
        },
    };
    let mut out = io::BufWriter::new(out);

    let mut hdr = [0u8; 4];
    if inp.read_exact(&mut hdr).is_err() || &hdr != b"qoif" {
        eprintln!("invalid input file");
        return ExitCode::FAILURE;
    }
    inp.rewind().unwrap();

    let frame = match decode_qoi(&mut inp) {
        Ok(f)  => f,
        Err(e) => {
            eprintln!("error while decoding: {e}");
            return ExitCode::FAILURE;
        },
    };

    if let Err(e) = encode_ppm(&mut out, frame) {
        eprintln!("error while encoding: {e}");
        return ExitCode::FAILURE;
    }

    return ExitCode::SUCCESS;
}

