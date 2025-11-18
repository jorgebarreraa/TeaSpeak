use serde::{Serialize, Deserialize};

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct Config {
    pub bindings: Vec<ServerBinding>,
    pub fallback_certificates: FallbackCertificate,
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct FallbackCertificate {
    pub private_key: String,
    pub certificate: String,
}

#[derive(Debug, PartialEq, Serialize, Deserialize, Clone)]
pub struct ServerBinding {
    pub address: String,
    pub public_address: Option<String>,
    pub private_key: Option<String>,
    pub certificate: Option<String>,
}