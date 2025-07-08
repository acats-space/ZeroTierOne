use std::env;
use std::fs;
use std::io::Write;
use std::path::Path;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 3 {
        eprintln!("Usage: {} <input.json> <output.c>", args[0]);
        std::process::exit(1);
    }
    let input_path = &args[1];
    let output_path = &args[2];

    let data = fs::read_to_string(input_path).expect("Failed to read input file");
    // Minify JSON
    let minified = match serde_json::from_str::<serde_json::Value>(&data) {
        Ok(json) => serde_json::to_string(&json).unwrap_or(data.clone()),
        Err(_) => data.clone(),
    };

    let escaped = minified
        .replace('\\', "\\\\")
        .replace('"', "\\\"")
        .replace('\n', "\\n")
        .replace('\r', "");

    let header = "#include \"diagnostic_schema_embed.h\"\n\n";
    let array_decl = format!(
        "const char ZT_DIAGNOSTIC_SCHEMA_JSON[] = \"{}\";\n",
        escaped
    );
    let len_decl = "const unsigned int ZT_DIAGNOSTIC_SCHEMA_JSON_LEN = sizeof(ZT_DIAGNOSTIC_SCHEMA_JSON) - 1;\n";

    let mut out = fs::File::create(output_path).expect("Failed to create output file");
    out.write_all(header.as_bytes()).unwrap();
    out.write_all(array_decl.as_bytes()).unwrap();
    out.write_all(len_decl.as_bytes()).unwrap();
} 