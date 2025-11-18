use openssl::ssl::{SslMethod, SslContext};
use std::path::Path;
use openssl::hash::MessageDigest;
use openssl::asn1::Asn1Time;
use openssl::bn::BigNum;
use openssl::error::ErrorStack;
use std::fs::File;
use std::io::Read;
use std::sync::Mutex;
use std::collections::BTreeMap;

#[derive(Debug)]
enum CertificateError {
    IoError(std::io::Error),
    SslError(ErrorStack)
}

fn load_ssl_certificate(key_path: &str, certificate_path: &str) -> std::result::Result<SslContext, (String, CertificateError)> {
    let mut buffer = Vec::new();

    let mut private_key = File::open(key_path)
        .map_err(|error| ("Failed to open private key".to_owned(), CertificateError::IoError(error)))?;

    private_key.read_to_end(&mut buffer)
        .map_err(|error| ("Failed to read private key".to_owned(), CertificateError::IoError(error)))?;

    let private_key = openssl::pkey::PKey::private_key_from_pem(&buffer)
        .map_err(|error| ("Failed to import private key".to_owned(), CertificateError::SslError(error)))?;

    let mut certificate = File::open(certificate_path)
        .map_err(|error| ("Failed to open certificate".to_owned(), CertificateError::IoError(error)))?;

    certificate.read_to_end(&mut buffer)
        .map_err(|error| ("Failed to read certificate".to_owned(), CertificateError::IoError(error)))?;

    let certificate = openssl::x509::X509::from_pem(&buffer)
        .map_err(|error| ("Failed to import certificate".to_owned(), CertificateError::SslError(error)))?;

    let ctx = {
        let mut builder = openssl::ssl::SslContext::builder(SslMethod::tls())
            .map_err(|error| ("failed to create a new ssl builder".to_owned(), CertificateError::SslError(error)))?;

        builder.set_private_key(&private_key)
            .map_err(|error| ("Failed to set private key".to_owned(), CertificateError::SslError(error)))?;

        builder.set_certificate(&certificate)
            .map_err(|error| ("Failed to set certificate".to_owned(), CertificateError::SslError(error)))?;

        builder.check_private_key()
            .map_err(|error| ("Failed to validate private key".to_owned(), CertificateError::SslError(error)))?;

        builder.build()
    };

    Ok(ctx)
}

fn generate_ssl_components(key_path: &str, certificate_path: &str, save_generated: bool) -> std::result::Result<SslContext, (String, CertificateError)> {
    let private_key = openssl::rsa::Rsa::generate_with_e(4096, &BigNum::from_u32(0x10001u32).unwrap())
        .map_err(|error| ("failed to generate rsa key".to_owned(), CertificateError::SslError(error)))?;

    let private_key = openssl::pkey::PKey::from_rsa(private_key)
        .map_err(|error| ("failed to generate private key from rsa key".to_owned(), CertificateError::SslError(error)))?;

    let certificate = {
        let subject = {
            let mut builder = openssl::x509::X509NameBuilder::new().unwrap();
            builder.append_entry_by_text("CN", "TestCertificate").unwrap();
            builder.build()
        };

        let mut cert_builder = openssl::x509::X509::builder().unwrap();
        cert_builder.set_pubkey(&private_key).unwrap();
        cert_builder.set_version(0).unwrap();
        cert_builder.set_subject_name(&subject).unwrap();
        cert_builder.set_issuer_name(&subject).unwrap();
        cert_builder.set_not_before(&Asn1Time::from_unix(0).unwrap()).unwrap();
        cert_builder.set_not_after(&Asn1Time::days_from_now(14).unwrap()).unwrap();
        cert_builder.sign(&private_key, MessageDigest::sha1()).unwrap();
        cert_builder.build()
    };

    if save_generated {
        let private_key_path = Path::new(key_path);
        if let Some(parent) = private_key_path.parent() {
            std::fs::create_dir_all(parent)
                .map_err(|error| ("failed to create private key file directories".to_owned(), CertificateError::IoError(error)))?;
        }

        let certificate_path = Path::new(certificate_path);
        if let Some(parent) = certificate_path.parent() {
            std::fs::create_dir_all(parent)
                .map_err(|error| ("failed to certificate file directories".to_owned(), CertificateError::IoError(error)))?;
        }

        let certificate = certificate.to_pem()
            .map_err(|error| ("failed to export certificate".to_owned(), CertificateError::SslError(error)))?;

        let private_key = private_key.private_key_to_pem_pkcs8()
            .map_err(|error| ("failed to export the private key".to_owned(), CertificateError::SslError(error)))?;

        std::fs::write(private_key_path, private_key)
            .map_err(|error| ("failed to write private key".to_owned(), CertificateError::IoError(error)))?;

        std::fs::write(certificate_path, certificate)
            .map_err(|error| ("failed to write certificate".to_owned(), CertificateError::IoError(error)))?;
    }

    let ctx = {
        let mut builder = openssl::ssl::SslContext::builder(SslMethod::tls())
            .map_err(|error| ("failed to create a new ssl builder".to_owned(), CertificateError::SslError(error)))?;

        builder.set_private_key(&private_key)
            .map_err(|error| ("Failed to set private key".to_owned(), CertificateError::SslError(error)))?;

        builder.set_certificate(&certificate)
            .map_err(|error| ("Failed to set certificate".to_owned(), CertificateError::SslError(error)))?;

        builder.check_private_key()
            .map_err(|error| ("Failed to validate private key".to_owned(), CertificateError::SslError(error)))?;

        builder.build()
    };

    Ok(ctx)
}

pub struct CertificateCache {
    context_cache: BTreeMap<String, SslContext>
}

impl CertificateCache {
    pub fn new() -> Self {
        CertificateCache{
            context_cache: BTreeMap::new()
        }
    }

    pub fn get_context(&mut self, key_path: &str, certificate_path: &str) -> Option<SslContext> {
        let joined_path = format!("{}--{}", key_path, certificate_path);
        return if let Some(context) = self.context_cache.get(&joined_path) {
            Some(context.clone())
        } else {
            let context = {
                if Path::new(key_path).exists() || Path::new(certificate_path).exists() {
                    match load_ssl_certificate(key_path, certificate_path) {
                        Ok(value) => value,
                        Err(error) => {
                            /* TODO: Error! */
                            println!("{}: {:?}", error.0, error.1);
                            return None;
                        }
                    }
                } else {
                    match generate_ssl_components(key_path, certificate_path, true) {
                        Ok(value) => value,
                        Err(error) => {
                            /* TODO: Error! */
                            println!("{}: {:?}", error.0, error.1);
                            return None;
                        }
                    }
                }
            };

            self.context_cache.insert(joined_path, context.clone());
            Some(context)
        }
    }
}

lazy_static::lazy_static! {
    pub static ref CERTIFICATE_CACHE: Mutex<CertificateCache> = Mutex::new(CertificateCache::new());
}