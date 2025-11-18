use std::path::{PathBuf, Component, Path};

fn path_traversal(path: &Path) -> PathBuf {
    let mut stack = Vec::with_capacity(32);

    for component in path.components() {
        match component {
            Component::CurDir => {},
            Component::RootDir => {
                stack.clear();
                stack.push(Component::RootDir);
            },
            Component::ParentDir => {
                if stack.iter().find(|element| !matches!(element, Component::ParentDir | Component::RootDir)).is_some() {
                    stack.pop();
                } else {
                    stack.push(Component::ParentDir);
                }
            },
            Component::Normal(dir) => {
                stack.push(Component::Normal(dir));
            },
            Component::Prefix(_) => {}
        }
    }

    let mut buffer = PathBuf::with_capacity(255);
    for element in stack.iter() {
        buffer.push(element);
    }
    buffer
}

/// Normalizes the relative file path.
/// If the file path exceeds the root directory `None` will be returned.
/// If the path begins with `Component::RootDir` such will be stripped to prevent easy path appending.
pub fn normalize_user_path(path: &Path) -> Option<PathBuf> {
    let normalized_path = path_traversal(path);
    let mut components = normalized_path.components();

    match components.next() {
        Some(Component::Normal(_)) => Some(normalized_path),
        Some(Component::RootDir) => {
            let mut path = PathBuf::new();
            if let Some(next) = components.next() {
                if matches!(next, Component::Normal(_)) {
                    path.push(next);
                    components.for_each(|c| path.push(c));
                    Some(path)
                } else {
                    None
                }
            } else {
                Some(path)
            }
        }
        None => Some(PathBuf::new()),
        c => {
            println!("{:?}", c);
            None
        }
    }
}

#[cfg(test)]
mod test {
    use std::path::{Path, PathBuf};
    use crate::files::utils::{ path_traversal, normalize_user_path };

    #[test]
    fn test_abs_path() {
        assert_eq!(Path::new("test/abc/cde"), path_traversal(Path::new("test/abc/cde")));
        assert_eq!(Path::new("test/abc/cde"), path_traversal(Path::new("test/../test/abc/cde")));
        assert_eq!(Path::new("../test/abc/cde"), path_traversal(Path::new("../test/abc/cde")));
        assert_eq!(Path::new("../test/test/abc/cde"), path_traversal(Path::new("../test/test/abc/cde")));
        assert_eq!(Path::new("../"), path_traversal(Path::new("test/../../")));
        assert_eq!(Path::new("../../asd/"), path_traversal(Path::new("test/../../../test/../asd")));
        assert_eq!(Path::new("/../../asd/"), path_traversal(Path::new("/test/../../../test/../asd")));

        assert_eq!(normalize_user_path(Path::new("./test/")), Some(Path::new("test").to_path_buf()));
        assert_eq!(normalize_user_path(Path::new("../test/")), None);
        assert_eq!(normalize_user_path(Path::new("./test/../asd/")), Some(Path::new("asd").to_path_buf()));
        assert_eq!(normalize_user_path(Path::new("./test/../../asd/")), None);
        assert_eq!(normalize_user_path(Path::new("/")), Some(Path::new("").to_path_buf()));
        assert_eq!(normalize_user_path(Path::new("/../")), None);
        assert_eq!(normalize_user_path(Path::new("/asd/")), Some(Path::new("asd/").to_path_buf()));
        assert_eq!(normalize_user_path(Path::new("/asd/../asd2")), Some(Path::new("asd2/").to_path_buf()));
        assert_eq!(normalize_user_path(Path::new("/asd/..//asd2")), Some(Path::new("asd2/").to_path_buf()));
        assert_eq!(normalize_user_path(Path::new("/asd/../../asd")), None);
        assert_eq!(PathBuf::from("test/abc/").join(normalize_user_path(Path::new("/asd/")).unwrap()), Path::new("test/abc/asd/").to_path_buf());
    }
}