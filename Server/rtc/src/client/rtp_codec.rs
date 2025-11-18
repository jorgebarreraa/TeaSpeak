use webrtc_lib::media::{Codec, CodecFeedback};
use webrtc_sdp::attribute_type::{SdpAttributeRtcpFbType, SdpAttributeFmtpParameters};
use lazy_static::lazy_static;

const DEFAULT_FMTP_PARAMETERS: SdpAttributeFmtpParameters = SdpAttributeFmtpParameters{
    packetization_mode: 0,
    level_asymmetry_allowed: false,
    profile_level_id: 0x0042_0010,
    max_fs: 0,
    max_cpb: 0,
    max_dpb: 0,
    max_br: 0,
    max_mbps: 0,
    usedtx: false,
    stereo: false,
    useinbandfec: false,
    cbr: false,
    max_fr: 0,
    maxplaybackrate: 48000,
    maxaveragebitrate: 0,
    ptime: 0,
    minptime: 0,
    maxptime: 0,
    encodings: Vec::new(),
    dtmf_tones: String::new(),
    rtx: None,
    unknown_tokens: Vec::new(),
};

pub const LOCAL_EXT_ID_AUDIO_LEVEL: u8 = 1;
pub const LOCAL_EXT_ID_PLAYOUT_DELAY: u8 = 2;

/* These MUST be the payloads used by the remote as well */
pub const OPUS_VOICE_PAYLOAD_TYPE: u8 = 111;
pub const OPUS_MUSIC_PAYLOAD_TYPE: u8 = 112;
pub const H264_PAYLOAD_TYPE: u8 = 126; /* 102 is standard for Google; 126 is standard for Firefox; */
pub const VP8_PAYLOAD_TYPE: u8 = 120; /* XX is standard for Google; 120 is standard for Firefox; */

fn opus_codec_mono() -> Codec {
    let mut parameters = DEFAULT_FMTP_PARAMETERS.clone();
    parameters.usedtx = true;
    parameters.useinbandfec = true;

    parameters.stereo = false;
    parameters.minptime = 20;
    parameters.ptime = 20;
    parameters.maxptime = 20;
    /* TODO: For the different codecs use parameters.max_mbps */

    /* The library does not allow any unknown parameters on the first hand so we've to me a bit hacky */
    parameters.unknown_tokens.push(format!(";sprop-stereo={}", 0));

    Codec{
        payload_type: OPUS_VOICE_PAYLOAD_TYPE,
        frequency: 48_000,
        codec_name: String::from("opus"),
        feedback: vec![
            CodecFeedback{ feedback_type: SdpAttributeRtcpFbType::Nack, parameter: String::new(), extra: String::new() },
            CodecFeedback{ feedback_type: SdpAttributeRtcpFbType::Remb, parameter: String::new(), extra: String::new() },
        ],
        channels: Some(2),
        parameters: Some(parameters)
    }
}

fn opus_codec_stereo() -> Codec {
    let mut parameters = DEFAULT_FMTP_PARAMETERS.clone();
    parameters.usedtx = true;
    parameters.useinbandfec = true;

    parameters.stereo = true;
    parameters.minptime = 20;
    parameters.ptime = 20;
    parameters.maxptime = 20;
    /* TODO: For the different codecs use parameters.max_mbps */

    /* The library does not allow any unknown parameters on the first hand so we've to me a bit hacky */
    parameters.unknown_tokens.push(format!(";sprop-stereo={}", 1));

    Codec{
        payload_type: OPUS_MUSIC_PAYLOAD_TYPE,
        frequency: 48_000,
        codec_name: String::from("opus"),
        feedback: vec![
            CodecFeedback{ feedback_type: SdpAttributeRtcpFbType::Nack, parameter: String::new(), extra: String::new() },
            CodecFeedback{ feedback_type: SdpAttributeRtcpFbType::Remb, parameter: String::new(), extra: String::new() },
        ],
        channels: Some(2),
        parameters: Some(parameters)
    }
}

pub fn h264_codec(max_framerate: u32) -> Codec {
    let mut parameters = DEFAULT_FMTP_PARAMETERS.clone();
    parameters.level_asymmetry_allowed = true;
    parameters.packetization_mode = 1;
    parameters.profile_level_id = 0x42001f;

    parameters.max_fr = max_framerate;

    Codec{
        payload_type: H264_PAYLOAD_TYPE,
        frequency: 90_000,
        codec_name: String::from("H264"),
        feedback: vec![
            CodecFeedback{ feedback_type: SdpAttributeRtcpFbType::Nack, parameter: String::new(), extra: String::new() },
            CodecFeedback{ feedback_type: SdpAttributeRtcpFbType::Nack, parameter: String::from("pli"), extra: String::new() },
            CodecFeedback{ feedback_type: SdpAttributeRtcpFbType::Remb, parameter: String::new(), extra: String::new() },
        ],
        channels: None,
        parameters: Some(parameters)
    }
}

pub fn vp8_codec() -> Codec {
    Codec{
        payload_type: VP8_PAYLOAD_TYPE,
        frequency: 90_000,
        codec_name: String::from("VP8"),
        feedback: vec![
            CodecFeedback{ feedback_type: SdpAttributeRtcpFbType::Nack, parameter: String::new(), extra: String::new() },
            CodecFeedback{ feedback_type: SdpAttributeRtcpFbType::Nack, parameter: String::from("pli"), extra: String::new() },
            CodecFeedback{ feedback_type: SdpAttributeRtcpFbType::Remb, parameter: String::new(), extra: String::new() },
        ],
        channels: None,
        parameters: None
    }
}

lazy_static! {
    pub static ref OPUS_CODEC_MONO: Codec = opus_codec_mono();
    pub static ref OPUS_CODEC_STEREO: Codec = opus_codec_stereo();
    pub static ref VP8_CODEC: Codec = vp8_codec();
}