// ================================================================================================
// Basic PNG decoder
//
// ref: https://www.w3.org/TR/png-3/
// ref: www.rfc-editor.org/info/rfc1950/
//
// Changelog:
//    ??/??/2026: Initial release
//
// License:
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
struct Frame
{
    pub w:   u32,
    pub h:   u32,
    pub buf: Vec<Pixel>,
}

impl Frame
{
    pub fn new(w: u32, h: u32) -> Self
    {
        Self { w, h, buf: Vec::with_capacity((w * h) as usize) }
    }
}

trait ByteReader
{
    fn ru8(&mut self) -> io::Result<u8>;
    fn ru32be(&mut self) -> io::Result<u32>;
}

impl<R: Read> ByteReader for R
{
    fn ru8(&mut self) -> io::Result<u8>
    {
        let mut buf = [0u8];
        self.read_exact(&mut buf)?;
        Ok(buf[0])
    }

    fn ru32be(&mut self) -> io::Result<u32>
    {
        let mut buf = [0u8; 4];
        self.read_exact(&mut buf)?;
        Ok(u32::from_be_bytes(buf))
    }
}

struct PngReader
{
    next_chunk_pos : u64,
}

impl PngReader
{
    fn new(inp: &mut impl Seek) -> io::Result<Self>
    {
        Ok(Self { next_chunk_pos: inp.stream_position()? })
    }

    fn next_chunk(&mut self, inp: &mut (impl Read + Seek)) -> io::Result<(u32, u32)>
    {
        inp.seek(io::SeekFrom::Start(self.next_chunk_pos))?;
        let chunk_len = inp.ru32be()?;
        let chunk_tag = inp.ru32be()?;
        self.next_chunk_pos = inp.stream_position()? + chunk_len as u64 + 4u64; // skip crc
        Ok((chunk_tag, chunk_len))
    }
}

const TAG_IDAT: u32 = u32::from_be_bytes(*b"IDAT");
const TAG_IEND: u32 = u32::from_be_bytes(*b"IEND");
const TAG_IHDR: u32 = u32::from_be_bytes(*b"IHDR");

fn tag_string(tag: u32) -> String
{
    let bytes = tag.to_be_bytes();
    String::from_utf8_lossy(&bytes).to_string()
}

fn decode_png(inp: &mut (impl Read + Seek)) -> io::Result<Frame>
{
    // magic was validated by caller
    inp.seek(io::SeekFrom::Current(8))?;

    let mut png = PngReader::new(inp)?;

    // IHDR is guaranteed to be first (5.6)
    if png.next_chunk(inp)?.0 != TAG_IHDR {
        panic!("PNG must start with IHDR");
    }

    // (11.2.1)
    let ihdr_w           = inp.ru32be()?;
    let ihdr_h           = inp.ru32be()?;
    let ihdr_depth       = inp.ru8()?;          assert!(ihdr_depth == 8);
    let ihdr_color       = inp.ru8()?;          assert!(ihdr_color == 2);
    let ihdr_compression = inp.ru8()?;          assert!(ihdr_compression == 0);
    let ihdr_filter      = inp.ru8()?;          assert!(ihdr_filter == 0);
    let ihdr_interlace   = inp.ru8()?;          assert!(ihdr_interlace == 0);

    let mut idat: Vec<u8> = Vec::new();

    // Collect all IDAT chunks into one buffer
    loop {
        let (tag, len) = png.next_chunk(inp)?;
        println!("+{}", tag_string(tag));
        match tag {
            TAG_IDAT => { inp.take(len as u64).read_to_end(&mut idat)?; },
            TAG_IEND => break,
            _        => { }
        };
    }

    println!("compressed data size: {} bytes", idat.len());

    let mut frame = Frame::new(ihdr_w, ihdr_h);

    // (10.1)

    Ok(frame)
}

fn encode_ppm(out: &mut impl Write, frame: Frame) -> io::Result<()>
{
    out.write_all(b"P6\n")?;
    out.write_all(format!("{} {}\n", frame.w, frame.h).as_bytes())?;
    out.write_all(b"255\n")?;

    for p in frame.buf {
        // PPM does not support alpha, just blend to black
        let a = p.3 as u16;
        let blend = |c: u8| ((c as u16 * a + 127) / 255) as u8;
        out.write_all(&[blend(p.0), blend(p.1), blend(p.2)])?;
    }

    out.flush()?;

    Ok(())
}

fn main() -> ExitCode
{
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

    let mut hdr = [0u8; 8];
    if inp.read_exact(&mut hdr).is_err() || hdr != *b"\x89PNG\r\n\x1A\n" {
        eprintln!("invalid input file");
        return ExitCode::FAILURE;
    }
    inp.rewind().unwrap();

    let frame = match decode_png(&mut inp) {
        Ok(f)  => f,
        Err(e) => {
            eprintln!("error while decoding: {e}");
            return ExitCode::FAILURE;
        },
    };

    let out = match fs::File::create(out_path) {
        Ok(f)  => f,
        Err(e) => {
            eprintln!("failed to open output: {e}");
            return ExitCode::FAILURE;
        },
    };
    let mut out = io::BufWriter::new(out);

    if let Err(e) = encode_ppm(&mut out, frame) {
        eprintln!("error while encoding: {e}");
        return ExitCode::FAILURE;
    }

    ExitCode::SUCCESS
}

