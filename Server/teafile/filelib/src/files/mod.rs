use std::path::{Path, PathBuf};
use std::fs;
use slog::*;
use std::time::SystemTime;
use tokio::io::{ErrorKind};
use crate::files::utils::normalize_user_path;
use std::pin::Pin;
use std::task::{Context, Poll};
use std::collections::HashMap;
use std::sync::{Weak, RwLock, Arc};
use crate::files::io::{UploadingFileInternals, DownloadFileInternals, DefaultUploadTarget, StreamingDownloadTarget, NormalDownloadTarget};
use std::fmt::Debug;

mod utils;
mod io;

#[derive(Debug, Clone)]
pub enum FilePath {
    Channel(u64, String),
    Icon(String),
    Avatar(String)
}

#[derive(Debug, Clone)]
pub enum DirectoryCreateResult {
    Success,
    InvalidPath,
    PathAlreadyExists,
    IoError
}

#[derive(Debug, Clone)]
pub enum DirectoryQueryResult {
    Success(Vec<PathInfo>),

    InvalidPath,
    PathDoesNotExists,
    PathIsNotADirectory,
    IoError
}

#[derive(Debug, Clone)]
pub enum PathQueryResult {
    Success(PathInfo),

    InvalidPath,
    PathDoesNotExists,
    UnknownFileType,
}

#[derive(Debug, Clone)]
pub enum PathInfo {
    Directory(PathInfoDirectory),
    File(PathInfoFile)
}

#[derive(Debug, Clone)]
pub struct PathInfoFile {
    pub name: String,
    pub modify_timestamp: std::time::SystemTime,
    pub size: u64,
}

#[derive(Debug, Clone)]
pub struct PathInfoDirectory {
    pub name: String,
    pub modify_timestamp: std::time::SystemTime,
    pub empty: bool
}

#[derive(Debug, Clone, PartialEq)]
pub enum PathDeleteResult {
    Success,

    InvalidPath,
    PathDoesNotExists,

    PathLocked,
    IoError
}

#[derive(Debug, Clone)]
pub enum PathRenameResult {
    Success,

    PathInvalidType,

    InvalidSourcePath,
    InvalidTargetPath,

    SourceNotFound,
    SourcePathLocked,
    TargetPathAlreadyExists,

    IoError
}

#[derive(Debug)]
pub enum UploadWriteResult {
    Success(usize),
    IoError(std::io::Error),
    WriteExceedsSize(/* written */ usize, /* dropped */ usize),
    TransferFinished
}

pub trait UploadTarget : Debug + Send {
    fn file_path(&self) -> PathBuf;
    fn total_expected_bytes(&self) -> u64;
    fn current_bytes(&self) -> u64;

    fn write(&mut self, buf: &[u8]) -> UploadWriteResult;

    fn finish_upload(&mut self);
    /// Abort the upload process.
    fn abort_upload(&mut self);
}

pub trait DownloadTarget : Debug + Send + Unpin {
    fn file_path(&self) -> PathBuf;
    fn current_bytes(&self) -> u64;
    fn total_expected_bytes(&self) -> u64;

    fn poll_read(
        self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        buf: &mut [u8],
    ) -> Poll<std::io::Result<usize>>;
}

#[derive(Debug)]
pub enum TargetInitializeError {
    InvalidPath,
    FileNotFound,
    FileInUse,
    FileNotAccessible,
    InternalIoError
}

impl TargetInitializeError {
    pub fn from_io_error(error: &std::io::Error) -> Option<TargetInitializeError> {
        match error.kind() {
            ErrorKind::NotFound => Some(TargetInitializeError::FileNotFound),
            ErrorKind::PermissionDenied => Some(TargetInitializeError::FileNotAccessible),
            _ => None
        }
    }
}

#[derive(Debug)]
pub enum UploadResult {
    Success(Box<dyn UploadTarget>),

    InvalidPath,
    FileNotFound,
    FileInUse,
    FileNotAccessible,
    IoError
}

#[derive(Debug)]
pub enum DownloadResult {
    Success(Box<dyn DownloadTarget>),

    InvalidPath,
    FileNotFound,
    FileInUse,
    FileNotAccessible,
    IoError
}

pub struct VirtualFileSystem {
    logger: slog::Logger,
    root_path: PathBuf,

    file_uploads: HashMap<PathBuf, Weak<RwLock<UploadingFileInternals>>>,
    file_downloads: HashMap<PathBuf, Weak<DownloadFileInternals>>,
}

impl VirtualFileSystem {
    pub fn new(logger: slog::Logger, root_path: PathBuf) -> Self {
        VirtualFileSystem {
            logger,
            root_path,

            file_uploads: HashMap::new(),
            file_downloads: HashMap::new()
        }
    }

    pub fn root_path(&self) -> &PathBuf {
        &self.root_path
    }

    pub fn generate_absolut_path(&self, path: &FilePath) -> Option<PathBuf> {
        let user_path = match path {
            FilePath::Channel(_, path) => normalize_user_path(Path::new(&path)),
            FilePath::Icon(path) => normalize_user_path(Path::new(&path)),
            FilePath::Avatar(path) => normalize_user_path(Path::new(&path)),
        };

        let user_path = match user_path{
            Some(path) => path,
            None => return None
        };

        Some(match path {
            FilePath::Channel(channel_id, _) => self.root_path.join(format!("channel_{}", channel_id)).join(user_path),
            FilePath::Avatar(_) => self.root_path.join("avatar").join(user_path),
            FilePath::Icon(_) => self.root_path.join("icons").join(user_path)
        })
    }

    fn directory_locked(&self, absolute_path: &Path) -> bool {
        for (key, value) in self.file_downloads.iter() {
            if key.starts_with(absolute_path) && value.upgrade().is_some() {
                return true;
            }
        }

        for (key, value) in self.file_uploads.iter() {
            if key.starts_with(absolute_path) && value.upgrade().is_some() {
                return true;
            }
        }

        false
    }

    fn file_locked(&self, absolute_path: &Path) -> bool {
        if let Some(entry) = self.file_uploads.get(absolute_path) {
            return if entry.upgrade().is_some() {
                true
            } else {
                false
            }
        }

        if let Some(entry) = self.file_downloads.get(absolute_path) {
            return if entry.upgrade().is_some() {
                true
            } else {
                false
            }
        }

        false
    }

    pub fn create_directories(&self, paths: &[FilePath]) -> Vec<DirectoryCreateResult> {
        let mut result = Vec::with_capacity(paths.len());

        for path in paths.iter() {
            if !matches!(path, FilePath::Channel(_, _)) {
                result.push(DirectoryCreateResult::InvalidPath);
                continue;
            }

            let absolute_path = match self.generate_absolut_path(path) {
                Some(path) => path,
                None => {
                    result.push(DirectoryCreateResult::InvalidPath);
                    continue;
                }
            };

            if let Err(error) = fs::create_dir_all(&absolute_path) {
                if error.kind() == ErrorKind::AlreadyExists {
                    result.push(DirectoryCreateResult::PathAlreadyExists);
                    continue;
                }

                warn!(self.logger, "Failed to create directories for {}", absolute_path.to_string_lossy());
                result.push(DirectoryCreateResult::IoError);
                continue;
            }

            result.push(DirectoryCreateResult::Success);
        }

        result
    }

    pub fn query_path_info(&self, paths: &[FilePath]) -> Vec<PathQueryResult> {
        let mut result = Vec::with_capacity(paths.len());
        for path in paths.iter() {
            let absolute_path = match self.generate_absolut_path(path) {
                Some(path) => path,
                None => {
                    result.push(PathQueryResult::InvalidPath);
                    continue;
                }
            };

            result.push(self.query_path_info_(&absolute_path));
        }

        result
    }

    fn query_path_info_(&self, absolute_path: &Path) -> PathQueryResult {
        let metadata = match fs::metadata(&absolute_path) {
            Err(error) => {
                trace!(self.logger, "Failed to get metadata for file {}: {}", absolute_path.to_string_lossy(), error);
                return PathQueryResult::PathDoesNotExists;
            },
            Ok(data) => data
        };

        /* TODO: Locked and currently uploading files */
        if metadata.file_type().is_dir() {
            let not_empty = absolute_path.read_dir().map(|mut reader| reader.next().is_some()).unwrap_or(true);

            PathQueryResult::Success(PathInfo::Directory(PathInfoDirectory{
                name: absolute_path.file_name().map(|value| value.to_string_lossy().into_owned()).unwrap_or(String::new()),
                modify_timestamp: metadata.modified().unwrap_or(SystemTime::UNIX_EPOCH),
                empty: not_empty
            }))
        } else if metadata.file_type().is_file() {
            PathQueryResult::Success(PathInfo::File(PathInfoFile{
                name: absolute_path.file_name().map(|value| value.to_string_lossy().into_owned()).unwrap_or(String::new()),
                size: metadata.len(),
                modify_timestamp: metadata.modified().unwrap_or(SystemTime::UNIX_EPOCH)
            }))
        } else {
            warn!(self.logger, "Queried file which isn't a directory nor a file: {}", absolute_path.to_string_lossy());
            PathQueryResult::PathDoesNotExists
        }
    }

    pub fn query_directory_entries(&self, path: &FilePath) -> DirectoryQueryResult {
        let absolute_path = match self.generate_absolut_path(path) {
            Some(path) => path,
            None => return DirectoryQueryResult::InvalidPath
        };

        if !absolute_path.is_dir() {
            return DirectoryQueryResult::PathIsNotADirectory;
        }

        let iterator = match absolute_path.read_dir() {
            Ok(iterator) => iterator,
            Err(error) => {
                warn!(self.logger, "Failed to read directory {}: {}", absolute_path.to_string_lossy(), error);
                return DirectoryQueryResult::IoError;
            }
        };

        let files = iterator
            .filter_map(|entry| match entry {
                Ok(entry) => Some(entry),
                Err(error) => {
                    warn!(self.logger, "An error occurred while reading directory {}: {}", absolute_path.to_string_lossy(), error);
                    None
                }
            })
            .filter_map(|entry| match self.query_path_info_(&entry.path()) {
                PathQueryResult::Success(info) => Some(info),
                error => {
                    warn!(self.logger, "Failed to query file path info for directory entry {}: {:?}", entry.path().to_string_lossy(), error);
                    None
                }
            })
            .collect::<Vec<_>>();

        DirectoryQueryResult::Success(files)
    }

    pub fn delete_file(&self, paths: &[FilePath]) -> Vec<PathDeleteResult> {
        let mut result = Vec::with_capacity(paths.len());

        for path in paths.iter() {
            let absolute_path = match self.generate_absolut_path(path) {
                Some(path) => path,
                None => {
                    result.push(PathDeleteResult::InvalidPath);
                    continue;
                }
            };

            let metadata = match fs::metadata(&absolute_path) {
                Err(_) => {
                    result.push(PathDeleteResult::PathDoesNotExists);
                    continue;
                },
                Ok(data) => data
            };

            if metadata.is_file() {
                if self.file_locked(&absolute_path) {
                    result.push(PathDeleteResult::PathLocked);
                    continue;
                }

                if let Err(error) = fs::remove_file(&absolute_path) {
                    warn!(self.logger, "Failed to delete file {}: {}", absolute_path.to_string_lossy(), error);
                    result.push(PathDeleteResult::IoError);
                } else {
                    result.push(PathDeleteResult::Success);
                }
                continue;
            } else if metadata.is_dir() {
                if self.directory_locked(&absolute_path) {
                    result.push(PathDeleteResult::PathLocked);
                    continue;
                }

                if let Err(error) = fs::remove_dir_all(&absolute_path) {
                    warn!(self.logger, "Failed to delete directory {}: {}", absolute_path.to_string_lossy(), error);
                    result.push(PathDeleteResult::IoError);
                } else {
                    result.push(PathDeleteResult::Success);
                }
                continue;
            } else {
                result.push(PathDeleteResult::PathDoesNotExists);
                continue;
            }
        }

        result
    }

    pub fn rename_file(&self, old_path: &FilePath, new_path: &FilePath) -> PathRenameResult {
        if !matches!(old_path, FilePath::Channel(_, _)) || !matches!(new_path, FilePath::Channel(_, _)) {
            return PathRenameResult::PathInvalidType;
        }

        let absolute_source = match self.generate_absolut_path(old_path) {
            Some(path) => path,
            None => return PathRenameResult::InvalidSourcePath,
        };

        let absolute_target = match self.generate_absolut_path(new_path) {
            Some(path) => path,
            None => return PathRenameResult::InvalidTargetPath,
        };

        if fs::metadata(&absolute_target).is_ok() {
            return PathRenameResult::TargetPathAlreadyExists;
        }

        let metadata_source = match fs::metadata(&absolute_source) {
            Err(_) => return PathRenameResult::SourceNotFound,
            Ok(data) => data
        };

        if metadata_source.is_dir() {
            if self.directory_locked(&absolute_source) {
                return PathRenameResult::SourcePathLocked;
            }
        } else if metadata_source.is_file() {
            if self.file_locked(&absolute_source) {
                return PathRenameResult::SourcePathLocked;
            }
        } else {
            return PathRenameResult::SourceNotFound;
        }

        /* create the required parent directories */
        if let Some(parent) = absolute_target.parent() {
            if !parent.is_dir() {
                if let Err(error) = fs::create_dir_all(&parent) {
                    warn!(self.logger, "Failed to create rename target parent directory {}: {}", parent.to_string_lossy(), error);
                    return PathRenameResult::IoError;
                }
            }
        }

        if let Err(error) = fs::rename(&absolute_source, &absolute_target) {
            if error.kind() == ErrorKind::NotFound {
                return PathRenameResult::SourceNotFound;
            }

            warn!(self.logger, "Failed to rename {} to {}: {}", absolute_source.to_string_lossy(), absolute_target.to_string_lossy(), error);
            return PathRenameResult::IoError;
        }

        PathRenameResult::Success
    }

    pub fn create_upload_target(&mut self, path: &FilePath, expected_bytes: u64) -> std::result::Result<Box<dyn UploadTarget>, TargetInitializeError> {
        let absolute_path = match self.generate_absolut_path(path) {
            Some(path) => path,
            None => return Err(TargetInitializeError::InvalidPath),
        };

        if let Some(entry) = self.file_uploads.get(&absolute_path) {
            if entry.upgrade().is_some() {
                return Err(TargetInitializeError::FileInUse);
            } else {
                self.file_uploads.remove(&absolute_path);
            }
        }

        if let Some(entry) = self.file_downloads.get(&absolute_path) {
            if entry.upgrade().is_some() {
                return Err(TargetInitializeError::FileInUse);
            } else {
                self.file_downloads.remove(&absolute_path);
            }
        }

        let upload_target = DefaultUploadTarget::new(
            self.logger.new(o!("file" => absolute_path.to_string_lossy().to_string())),
            absolute_path.clone(),
            expected_bytes
        );

        let upload_target = match upload_target {
            Ok(upload_target) => upload_target,
            Err(error) => {
                return Err({
                    if let Some(error) = TargetInitializeError::from_io_error(&error) {
                        error
                    } else {
                        warn!(self.logger, "Failed to open upload file {}: {}", absolute_path.to_string_lossy(), error);
                        TargetInitializeError::InternalIoError
                    }
                });
            }
        };

        self.file_uploads.insert(absolute_path, Arc::downgrade(upload_target.upload_handle()));

        Ok(Box::new(upload_target))
    }

    pub fn create_download_target(&mut self, path: &FilePath) -> std::result::Result<Box<dyn DownloadTarget>, TargetInitializeError> {
        let absolute_path = match self.generate_absolut_path(path) {
            Some(path) => path,
            None => return Err(TargetInitializeError::InvalidPath),
        };

        if let Some(entry) = self.file_uploads.get(&absolute_path) {
            if let Some(entry) = entry.upgrade() {
                return match StreamingDownloadTarget::new(entry) {
                    Ok(result) => Ok(Box::new(result)),
                    Err(error) => Err({
                        if let Some(error) = TargetInitializeError::from_io_error(&error) {
                            error
                        } else {
                            warn!(self.logger, "Failed to open streamed download file {}: {}", absolute_path.to_string_lossy(), error);
                            TargetInitializeError::InternalIoError
                        }
                    })
                };
            } else {
                self.file_uploads.remove(&absolute_path);
            }
        }

        let result = 'outer: {
            if let Some(entry) = self.file_downloads.get(&absolute_path) {
                if let Some(entry) = entry.upgrade() {
                    break 'outer NormalDownloadTarget::new_from_handle(entry);
                } else {
                    self.file_downloads.remove(&absolute_path);
                }
            }

            NormalDownloadTarget::new(self.logger.new(o!("file" => absolute_path.to_string_lossy().to_string())), absolute_path.clone())
        };

        match result {
            Ok(result) => Ok(Box::new(result)),
            Err(error) => Err({
                if let Some(error) = TargetInitializeError::from_io_error(&error) {
                    error
                } else {
                    warn!(self.logger, "Failed to open download file {}: {}", absolute_path.to_string_lossy(), error);
                    TargetInitializeError::InternalIoError
                }
            })
        }
    }
}

#[cfg(test)]
mod test {
    use std::path::{PathBuf};
    use crate::files::{VirtualFileSystem, FilePath, PathDeleteResult, UploadWriteResult};
    use crate::log::create_test_logger;

    #[test]
    fn test_file_path() {
        let server = VirtualFileSystem::new(create_test_logger(), PathBuf::from("./__TeaSpeak/virtualserver_test/"));
        println!("{:?}", server.generate_absolut_path(&FilePath::Icon("//icon_1234".to_owned())));
        println!("{:?}", server.query_path_info(&[FilePath::Icon("icon_1234".to_owned())]));
        println!("{:?}", server.query_path_info(&[FilePath::Icon("icon_1234".to_owned())]));
        println!("{:?}", server.create_directories(&[FilePath::Channel(1, "MyDirectory".to_owned())]));
    }

    #[test]
    fn test_file_locking() {
        let mut server = VirtualFileSystem::new(create_test_logger(), PathBuf::from("./__TeaSpeak/virtualserver_test/"));
        let mut upload = server.create_upload_target(&FilePath::Channel(1, "/my_path/test_icon".to_owned()), 11).expect("failed to create upload");

        assert_eq!(server.delete_file(&[FilePath::Channel(1, "/my_path/test_icon".to_owned())]).pop(), Some(PathDeleteResult::PathLocked));
        assert_eq!(server.delete_file(&[FilePath::Channel(1, "/my_path/test_icon1".to_owned())]).pop(), Some(PathDeleteResult::PathDoesNotExists));
        assert_eq!(server.delete_file(&[FilePath::Channel(1, "/my_path/".to_owned())]).pop(), Some(PathDeleteResult::PathLocked));
        assert_eq!(server.delete_file(&[FilePath::Channel(1, "/my_path/asd/../test_icon".to_owned())]).pop(), Some(PathDeleteResult::PathLocked));

        match upload.write(b"Hello Worl") {
            UploadWriteResult::Success(10) => {},
            result => panic!("unexpected write result {:?}", result)
        }
        match upload.write(b"dHello World") {
            UploadWriteResult::WriteExceedsSize(1, 11) => {},
            result => panic!("unexpected write result {:?}", result)
        }
        upload.finish_upload();
    }
}