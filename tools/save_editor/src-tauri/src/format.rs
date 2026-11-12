//! Native OGT4SAVE v1. See src/core/save.c and save_state.c; no game logic.
use serde::{Deserialize, Serialize};
const LIMIT: usize = 4 * 1024 * 1024;
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(tag = "kind", content = "value", rename_all = "lowercase")]
pub enum Node {
    Nil,
    Bool(bool),
    Int(String),
    Float(String),
    String(String),
    Array(Vec<Node>),
    Module(Vec<(String, Node)>),
    Map(Vec<(Node, Node)>),
}
struct Reader<'a> {
    data: &'a [u8],
    at: usize,
    nodes: usize,
}
type Result<T> = std::result::Result<T, String>;
impl Reader<'_> {
    fn bytes(&mut self, n: usize) -> Result<&[u8]> {
        let end = self.at.checked_add(n).ok_or("Invalid length")?;
        let b = self.data.get(self.at..end).ok_or("Truncated save")?;
        self.at = end;
        Ok(b)
    }
    fn count(&mut self) -> Result<usize> {
        Ok(u32::from_le_bytes(self.bytes(4)?.try_into().unwrap()) as usize)
    }
    fn string(&mut self) -> Result<String> {
        let n = self.count()?;
        if n > 65536 {
            return Err("String exceeds save limit".into());
        }
        let b = self.bytes(n)?;
        if b.contains(&0) {
            return Err("Embedded NUL in string".into());
        }
        String::from_utf8(b.to_vec())
            .map_err(|_| "Non-UTF8 string: this save cannot safely be edited".into())
    }
    fn node(&mut self, depth: usize) -> Result<Node> {
        self.nodes += 1;
        if depth > 20 || self.nodes > 200000 {
            return Err("Save nesting/node limit exceeded".into());
        }
        Ok(match self.bytes(1)?[0] {
            0 => Node::Nil,
            1 => match self.bytes(1)?[0] {
                0 => Node::Bool(false),
                1 => Node::Bool(true),
                _ => return Err("Invalid boolean".into()),
            },
            2 => Node::Int(i64::from_le_bytes(self.bytes(8)?.try_into().unwrap()).to_string()),
            3 => {
                let f = f64::from_le_bytes(self.bytes(8)?.try_into().unwrap());
                if !f.is_finite() {
                    return Err("Non-finite number".into());
                }
                Node::Float(f.to_string())
            }
            4 => Node::String(self.string()?),
            tag @ 5..=7 => {
                let n = self.count()?;
                if n > 100000 || n > self.data.len() - self.at {
                    return Err("Invalid collection length".into());
                }
                match tag {
                    5 => {
                        let mut v = Vec::new();
                        for _ in 0..n {
                            v.push(self.node(depth + 1)?);
                        }
                        Node::Array(v)
                    }
                    6 => {
                        let mut v: Vec<(String, Node)> = Vec::new();
                        for _ in 0..n {
                            let k = self.string()?;
                            if k.is_empty() || k.len() > 128 || v.iter().any(|(old, _)| old == &k) {
                                return Err("Invalid/duplicate member name".into());
                            }
                            v.push((k, self.node(depth + 1)?));
                        }
                        Node::Module(v)
                    }
                    _ => {
                        let mut v = Vec::new();
                        let mut keys = std::collections::HashSet::new();
                        for _ in 0..n {
                            let k = self.node(depth + 1)?;
                            if matches!(k, Node::Array(_) | Node::Module(_) | Node::Map(_)) {
                                return Err("Invalid map key".into());
                            }
                            // adhoc_vm.c values_equal compares all numeric key types via double.
                            let number = match &k {
                                Node::Int(s) => Some(s.parse::<i64>().unwrap() as f64),
                                Node::Float(s) => Some(s.parse::<f64>().unwrap()),
                                Node::Bool(b) => Some(if *b { 1.0 } else { 0.0 }),
                                _ => None,
                            };
                            let identity = if let Some(f) = number {
                                format!("number:{}", if f == 0.0 { 0 } else { f.to_bits() })
                            } else {
                                serde_json::to_string(&k).unwrap()
                            };
                            if !keys.insert(identity) {
                                return Err("Duplicate map key".into());
                            }
                            v.push((k, self.node(depth + 1)?));
                        }
                        Node::Map(v)
                    }
                }
            }
            _ => return Err("Unknown save type".into()),
        })
    }
}
fn text(out: &mut Vec<u8>, s: &str) {
    out.extend((s.len() as u32).to_le_bytes());
    out.extend(s.as_bytes());
}
fn encode(n: &Node, out: &mut Vec<u8>, depth: usize) -> Result<()> {
    if depth > 20 || out.len() > LIMIT {
        return Err("Save exceeds limits".into());
    }
    match n {
        Node::Nil => out.push(0),
        Node::Bool(b) => out.extend([1, *b as u8]),
        Node::Int(s) => {
            out.push(2);
            out.extend(
                s.parse::<i64>()
                    .map_err(|_| "Enter a signed 64-bit integer")?
                    .to_le_bytes(),
            );
        }
        Node::Float(s) => {
            let f = s.parse::<f64>().map_err(|_| "Enter a finite number")?;
            if !f.is_finite() {
                return Err("Enter a finite number".into());
            }
            out.push(3);
            out.extend(f.to_le_bytes());
        }
        Node::String(s) => {
            if s.len() > 65536 || s.contains('\0') {
                return Err("Invalid string".into());
            }
            out.push(4);
            text(out, s);
        }
        Node::Array(v) => {
            out.push(5);
            out.extend((v.len() as u32).to_le_bytes());
            for n in v {
                encode(n, out, depth + 1)?;
            }
        }
        Node::Module(v) => {
            out.push(6);
            out.extend((v.len() as u32).to_le_bytes());
            for (k, n) in v {
                text(out, k);
                encode(n, out, depth + 1)?;
            }
        }
        Node::Map(v) => {
            out.push(7);
            out.extend((v.len() as u32).to_le_bytes());
            for (k, n) in v {
                encode(k, out, depth + 1)?;
                encode(n, out, depth + 1)?;
            }
        }
    }
    Ok(())
}
pub fn validate(n: &Node) -> Result<Vec<u8>> {
    let mut out = Vec::new();
    encode(n, &mut out, 0)?;
    if out.len() > LIMIT {
        return Err("Save exceeds 4 MiB".into());
    }
    let mut r = Reader {
        data: &out,
        at: 0,
        nodes: 0,
    };
    r.node(0)?;
    let roots = match n {
        Node::Module(v) => v,
        _ => return Err("Profile must be a module".into()),
    };
    for (k, v) in roots {
        match k.as_str() {
            "username" => match v {
                Node::String(s)
                    if !s.trim_matches(' ').is_empty()
                        && s.len() <= 96
                        && s.chars().count() <= 24
                        && !s.chars().any(|c| c < ' ' || c == '\u{7f}') =>
                {
                    ()
                }
                _ => return Err("Player name: 1–24 characters, no control characters".into()),
            },
            "money" => {
                if !matches!(v, Node::Int(_)) {
                    return Err("Credits must be an integer".into());
                }
            }
            "withdrawn" | "withdrawnGT4p" => {
                if !matches!(v, Node::Bool(_)) && !matches!(v,Node::Int(s) if s=="0"||s=="1") {
                    return Err("Transfer flag must be 0 or 1".into());
                }
            }
            "major_menu_page" | "major_menu_widget" => (),
            "option" | "garage" | "calendar" | "race_record" | "license_record"
            | "course_record" | "machine_record" | "favorite_car" | "favorite_course"
            | "presents" | "championship" | "used_car" | "statistics" | "available"
            | "ps2_import" => {
                if !matches!(v, Node::Module(_)) {
                    return Err(format!("{k} must be a module"));
                }
            }
            _ => return Err(format!("Unknown profile root: {k}")),
        }
    }
    Ok(out)
}
pub fn unpack(b: &[u8]) -> Result<(Node, u64)> {
    if b.len() < 32 || &b[..8] != b"OGT4SAVE" {
        return Err("Choose a native Moon Engine .ogt4 profile (not a PS2 memory card)".into());
    }
    let word = |i| u32::from_le_bytes(b[i..i + 4].try_into().unwrap());
    if word(8) != 1 {
        return Err("Unsupported save version".into());
    }
    if word(12) as usize != b.len() - 32
        || b.len() > LIMIT + 32
        || word(28) != crc32fast::hash(&b[..28])
        || word(16) != crc32fast::hash(&b[32..])
    {
        return Err("Save length/checksum is invalid. Open a backup instead.".into());
    }
    let mut r = Reader {
        data: &b[32..],
        at: 0,
        nodes: 0,
    };
    let n = r.node(0)?;
    if r.at != b.len() - 32 {
        return Err("Unexpected trailing data".into());
    }
    validate(&n)?;
    Ok((n, u64::from_le_bytes(b[20..28].try_into().unwrap())))
}
pub fn pack(n: &Node, stamp: u64) -> Result<Vec<u8>> {
    let data = validate(n)?;
    let mut out = b"OGT4SAVE".to_vec();
    out.extend(1u32.to_le_bytes());
    out.extend((data.len() as u32).to_le_bytes());
    out.extend(crc32fast::hash(&data).to_le_bytes());
    out.extend(stamp.to_le_bytes());
    out.extend(crc32fast::hash(&out).to_le_bytes());
    out.extend(data);
    Ok(out)
}
#[cfg(test)]
mod tests {
    use super::*;
    fn profile() -> Node {
        Node::Module(vec![
            ("username".into(), Node::String("Driver".into())),
            ("money".into(), Node::Int("9007199254740993".into())),
            (
                "garage".into(),
                Node::Module(vec![(
                    "future_field".into(),
                    Node::Map(vec![(
                        Node::Int("-1".into()),
                        Node::Array(vec![Node::Nil, Node::Bool(true), Node::Float("-0".into())]),
                    )]),
                )]),
            ),
        ])
    }
    #[test]
    fn roundtrip_all_types_and_unknown_nested_data() {
        let n = profile();
        let b = pack(&n, 123).unwrap();
        let (read, t) = unpack(&b).unwrap();
        assert_eq!(n, read);
        assert_eq!(pack(&read, t).unwrap(), b);
    }
    #[test]
    fn corrupt_checksums_are_rejected() {
        let mut b = pack(&profile(), 0).unwrap();
        b[35] ^= 1;
        assert!(unpack(&b).is_err());
        b[8] = 2;
        assert!(unpack(&b).is_err());
    }
    #[test]
    fn invalid_types_names_and_duplicates_are_rejected() {
        for n in [
            Node::Module(vec![("money".into(), Node::Float("1".into()))]),
            Node::Module(vec![("username".into(), Node::String(" ".into()))]),
            Node::Module(vec![(
                "garage".into(),
                Node::Module(vec![(
                    "map".into(),
                    Node::Map(vec![
                        (Node::Bool(true), Node::Nil),
                        (Node::Int("1".into()), Node::Nil),
                    ]),
                )]),
            )]),
            Node::Module(vec![(
                "option".into(),
                Node::Module(vec![("x".into(), Node::Nil), ("x".into(), Node::Nil)]),
            )]),
        ] {
            assert!(pack(&n, 0).is_err());
        }
    }
    #[test]
    fn nonfinite_and_integer_overflow_are_rejected() {
        for n in [
            Node::Float("NaN".into()),
            Node::Int("9223372036854775808".into()),
        ] {
            assert!(encode(&n, &mut Vec::new(), 0).is_err());
        }
    }
}
