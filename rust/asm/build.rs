fn main() {
    println!("cargo:rerun-if-changed=src/add.S");
    cc::Build::new().file("src/main_aarch64.S").compile("add");
}
