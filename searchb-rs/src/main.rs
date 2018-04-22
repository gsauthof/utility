// 2018, Georg Sauthoff <mail@gms.tf>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// How to build:
// cargo build --release

extern crate memmap;
extern crate twoway;


fn mmap(filename: &str) -> Result<memmap::Mmap, std::io::Error>
{
    let file = std::fs::File::open(filename)?;
    let p = unsafe { memmap::Mmap::map(&file)? };
    Ok(p)
}

/*
fn naive_find_bytes(t : &[u8], q : &[u8]) -> Option<usize> {
    t.windows(q.len()).position(|w| w == q)
}
*/

fn real_main(args : & mut std::env::Args) -> Result<Option<usize>, String> {
    args.next();
    let qfilename = args.next().ok_or("query filename missing")?;
    let filename  = args.next().ok_or("target filename missing")?;
    let q = match mmap(&qfilename) {
      Ok(x) => x,
      Err(ref e) if e.kind() == std::io::ErrorKind::InvalidInput
              => return Ok(Some(0)),
      Err(e)  => return Err(e.to_string())
    };
    let t = match mmap(&filename) {
      Ok(x) => x,
      Err(ref e) if e.kind() == std::io::ErrorKind::InvalidInput
              => return Ok(None),
      Err(e)  => return Err(e.to_string())
    };
    Ok(twoway::find_bytes(&t, &q)) // equivalent to below notation
    //Ok(twoway::find_bytes(&t[..], &q[..]))
    //Ok(naive_find_bytes(&t, &q))
}

fn main() {
    std::process::exit(
        match real_main(&mut std::env::args()) {
            Ok(t)  => match t {
                Some(n) => { println!("{}", n); 0 },
                None    => 1
            },
            Err(e) => {eprintln!("error: {}", e); 1 }
    })
}
