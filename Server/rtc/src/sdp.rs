#![allow(unused)]
use webrtc_sdp::media_type::SdpMedia;
use webrtc_sdp::error::SdpParserError;
use webrtc_sdp::SdpSession;
use std::borrow::Cow;

#[derive(Debug, Clone)]
pub enum SdpCompressError {
    InvalidMode,
    JsonEncodeFailed,
    InvalidCodecFormatList,
    InconsistentCodecTypes,
    InconsistentTransportInfo,
    InconsistentExtensions,
    CodecInfoBuildFailed,
    MissingIceUfrag,
    FailedToBuildTransportInfo
}

pub fn compress_sdp(sdp: &SdpSession, mode: u8) -> Result<String, SdpCompressError> {
    if mode == 0 {
        mode0::compress_sdp(sdp)
    } else if mode == 1 {
        mode1::compress_sdp(sdp)
    } else {
        Err(SdpCompressError::InvalidMode)
    }
}

#[derive(Debug, Clone)]
pub enum SdpDecompressError {
    InvalidMode,
    Unimplemented,
    SdpParseError(SdpParserError)
}

pub fn decompress_sdp(sdp: Cow<str>, mode: u8) -> Result<SdpSession, SdpDecompressError> {
    if mode == 0 {
        mode0::decompress_sdp(sdp)
    } else if mode == 1 {
        mode1::decompress_sdp(sdp)
    } else {
        Err(SdpDecompressError::InvalidMode)
    }
}

/// No SDP compression at all
mod mode0 {
    use webrtc_sdp::SdpSession;
    use crate::sdp::{SdpCompressError, SdpDecompressError};
    use std::borrow::Cow;

    pub fn compress_sdp(sdp: &SdpSession) -> Result<String, SdpCompressError> {
        Ok(sdp.to_string())
    }

    pub fn decompress_sdp(sdp: Cow<str>) -> Result<SdpSession, SdpDecompressError> {
        let sdp = sdp.into_owned(); /* if we're not doing this it panics... */
        webrtc_sdp::parse_sdp(sdp.as_str(), false)
            .map_err(|err| SdpDecompressError::SdpParseError(err))
    }
}

/// Compress SDP as JSON
mod mode1 {
    use crate::sdp::{SdpCompressError, SdpDecompressError};
    use webrtc_sdp::{SdpSession, SdpOrigin};
    use serde::{Deserialize, Serialize};
    use std::borrow::Cow;
    use webrtc_sdp::media_type::{SdpMediaValue, SdpMedia, SdpFormatList};
    use std::collections::HashMap;
    use webrtc_sdp::attribute_type::{SdpAttributeRtcpFb, SdpAttributeFmtpParameters, SdpAttributeType, SdpAttribute, SdpAttributePayloadType, SdpAttributeExtmap};

    #[derive(Serialize, Deserialize, PartialEq, Debug)]
    #[serde(tag = "type")]
    enum CodecList {
        /// Custom codes have been defined.
        /// The tuple contains a list with all codec related attributes.
        Custom{ codecs: CodecInfo },
        Inherited
    }

    #[derive(Serialize, Deserialize, PartialEq, Debug)]
    struct CodecInfo {
        payload_type: u32,
        codec_name: String,
        frequency: u32,
        #[serde(skip_serializing_if = "Option::is_none")]
        #[serde(default)]
        channels: Option<u32>,

        feedback: Vec<String>,
        #[serde(skip_serializing_if = "Option::is_none")]
        #[serde(default)]
        parameters: Option<String>
    }

    #[derive(Serialize, Deserialize, PartialEq, Debug)]
    struct IceTransport {
        setup: String,
        /* ufrag: String, */
        password: String,
        fingerprint: String,

        #[serde(skip_serializing_if = "Vec::is_empty")]
        #[serde(default)]
        options: Vec<String>,

        #[serde(skip_serializing_if = "Vec::is_empty")]
        #[serde(default)]
        candidates: Vec<String>
    }

    #[derive(Serialize, Deserialize)]
    struct SessionInfo {
        /* Using a String here since JavaScript does not support 64bit numbers */
        version: String,
        origin: String,
        #[serde(skip_serializing_if = "Option::is_none")]
        #[serde(default)]
        session: Option<String>,
        #[serde(skip_serializing_if = "Option::is_none")]
        #[serde(default)]
        connection: Option<String>,
        attribute: Vec<String>,

        media: Vec<MediaInfo>,
        codecs: HashMap<u32, CodecInfo>,
        extensions: HashMap<u16, String>,

        ice: HashMap<String, IceTransport>
    }

    #[derive(Serialize, Deserialize)]
    #[serde(tag = "type")]
    enum MediaInfo {
        Compressed{
            media: String,
            #[serde(skip_serializing_if = "Option::is_none")]
            #[serde(default)]
            mid: Option<String>,
            codecs: Vec<u32>,
            extensions: Vec<u16>,
            transport: String,
            ssrcs: HashMap<u32, HashMap<String, Option<String>>>
        },
        Raw{
            sdp: String
        }
    }

    fn generate_codec_info(line: &SdpMedia, payload_type: u32) -> Option<CodecInfo> {
        let map = line.get_attributes_of_type(SdpAttributeType::Rtpmap)
            .iter()
            .map(|e| if let SdpAttribute::Rtpmap(map) = e { map } else { panic!() })
            .find(|e| e.payload_type as u32 == payload_type);

        if map.is_none() {
            return None
        }

        let map = map.unwrap();
        let mut codec = CodecInfo {
            payload_type,
            frequency: map.frequency,
            codec_name: map.codec_name.clone(),
            channels: map.channels.clone(),
            feedback: Vec::new(),
            parameters: None
        };

        for feedback in line.get_attributes_of_type(SdpAttributeType::Rtcpfb)
            .iter()
            .map(|e| if let SdpAttribute::Rtcpfb(map) = e { map } else { panic!() })
            .filter(|e| if let SdpAttributePayloadType::PayloadType(payload) = e.payload_type { payload as u32 == payload_type } else { true }) {
            codec.feedback.push(feedback.to_string());
        }

        let parameters = line.get_attributes_of_type(SdpAttributeType::Fmtp)
            .iter()
            .map(|e| if let SdpAttribute::Fmtp(map) = e { map } else { panic!() })
            .find(|e| e.payload_type as u32 == payload_type);
        if let Some(parameters) = parameters {
            codec.parameters = Some(parameters.parameters.to_string());
        }

        Some(codec)
    }

    fn generate_transport_info(line: &SdpMedia) -> Option<IceTransport> {
        let password = if let Some(SdpAttribute::IcePwd(pwd)) = line.get_attribute(SdpAttributeType::IcePwd) {
            pwd.clone()
        } else {
            return None;
        };

        let fingerprint = if let Some(SdpAttribute::Fingerprint(fingerprint)) = line.get_attribute(SdpAttributeType::Fingerprint) {
            fingerprint.to_string()
        } else {
            return None;
        };

        let options = if let Some(SdpAttribute::IceOptions(options)) = line.get_attribute(SdpAttributeType::IceOptions) {
            options.clone()
        } else {
            return None;
        };

        let setup = if let Some(SdpAttribute::Setup(setup)) = line.get_attribute(SdpAttributeType::Setup) {
            setup.to_string()
        } else {
            return None;
        };

        let candidates = line.get_attributes_of_type(SdpAttributeType::Candidate)
            .iter()
            .map(|e| e.to_string())
            .collect();

        Some(IceTransport{
            setup,
            fingerprint,
            options,
            password,
            candidates
        })
    }

    pub fn compress_sdp(sdp: &SdpSession) -> Result<String, SdpCompressError> {
        let mut info = SessionInfo{
            version: sdp.version.to_string(),
            origin: sdp.origin.to_string(),
            session: sdp.session.as_ref().filter(|e| **e != "-").map(|e| e.clone()),
            connection: sdp.connection.as_ref().map(|e| e.to_string()),
            attribute: sdp.attribute.iter().map(|e| e.to_string()).collect(),

            media: Vec::with_capacity(sdp.media.len()),
            codecs: HashMap::new(),
            extensions: HashMap::new(),
            ice: HashMap::new()
        };

        /* The ICE transport */
        for line in sdp.media.iter() {
            let username = if let Some(SdpAttribute::IceUfrag(str)) = line.get_attribute(SdpAttributeType::IceUfrag) {
                Ok(str.clone())
            } else {
                Err(SdpCompressError::MissingIceUfrag)
            }?;

            let tinfo = generate_transport_info(line)
                .ok_or(SdpCompressError::FailedToBuildTransportInfo)?;

            if let Some(reg_tinfo) = info.ice.get_mut(&username) {
                if reg_tinfo.candidates.is_empty() && !tinfo.candidates.is_empty() {
                    /* libwebrtc only adds ice candidates to one line */
                    reg_tinfo.candidates = tinfo.candidates.clone();
                }

                /*
                /* We currently cant really compare them since the ice candidates may be different */
                if *reg_tinfo != tinfo {
                    println!("{:?}\n{:?}", reg_tinfo, &tinfo);
                    return Err(SdpCompressError::InconsistentTransportInfo);
                }
                */
            } else {
                info.ice.insert(username, tinfo);
            }
        }

        /* setup the codes */
        for line in sdp.media.iter() {
            if line.get_type() == &SdpMediaValue::Application {
                /* Application does not contain any codecs */
                continue;
            }

            if let SdpFormatList::Integers(codecs) = line.get_formats() {
                for codec_id in codecs.iter() {
                    let generated_info = generate_codec_info(line, *codec_id)
                        .ok_or(SdpCompressError::CodecInfoBuildFailed)?;

                    if let Some(codec) = info.codecs.get(codec_id) {
                        if *codec != generated_info {
                            println!("{:?}\n{:?}", codec, &generated_info);
                            return Err(SdpCompressError::InconsistentCodecTypes);
                        }
                    } else {
                        info.codecs.insert(*codec_id, generated_info);
                    }
                }
            } else {
                return Err(SdpCompressError::InvalidCodecFormatList);
            }
        }

        /* setup the extensions */
        for line in sdp.media.iter() {
            for ext in line.get_attributes_of_type(SdpAttributeType::Extmap)
                .iter()
                .map(|e| if let SdpAttribute::Extmap(map) = e { map } else { panic!() }) {
                let value = ext.to_string();
                if let Some(val) = info.extensions.get(&ext.id) {
                    if *val != value {
                        return Err(SdpCompressError::InconsistentExtensions);
                    }
                } else {
                    info.extensions.insert(ext.id, value);
                }
            }
        }

        for line in sdp.media.iter() {
            if line.get_type() == &SdpMediaValue::Application {
                info.media.push(MediaInfo::Raw {
                    sdp: line.to_string()
                });
            } else {
                let codecs = if let SdpFormatList::Integers(codecs) = line.get_formats() {
                    Ok(codecs.clone())
                } else {
                    return Err(SdpCompressError::InvalidCodecFormatList);
                }?;

                let username = if let Some(SdpAttribute::IceUfrag(str)) = line.get_attribute(SdpAttributeType::IceUfrag) {
                    Ok(str.clone())
                } else {
                    Err(SdpCompressError::MissingIceUfrag)
                }?;

                let mut extensions = line.get_attributes_of_type(SdpAttributeType::Extmap)
                    .iter()
                    .map(|e| if let SdpAttribute::Extmap(map) = e { map } else { panic!() })
                    .map(|e| e.id)
                    .collect::<Vec<_>>();
                extensions.sort();

                let mid = line.get_attribute(SdpAttributeType::Mid)
                    .map(|e| if let SdpAttribute::Mid(id) = e { id.clone() } else { panic!() });

                /* TODO: SSRC Group */
                let mut ssrcs = HashMap::new();
                for ssrc in line.get_attributes_of_type(SdpAttributeType::Ssrc).iter()
                    .map(|e| if let SdpAttribute::Ssrc(ssrc) = e { ssrc } else { panic!() }) {
                    let mut map = if let Some(map) = ssrcs.get_mut(&ssrc.id) {
                        map
                    } else {
                        ssrcs.insert(ssrc.id, HashMap::new());
                        ssrcs.get_mut(&ssrc.id).unwrap()
                    };

                    if let Some(attribute) = &ssrc.attribute {
                        map.insert(attribute.clone(), ssrc.value.clone());
                    }
                }

                info.media.push(MediaInfo::Compressed {
                    codecs,
                    media: line.get_type().to_string(),
                    extensions,
                    transport: username,
                    mid,
                    ssrcs
                });
            }
        }

        serde_json::to_string(&info).map_err(|_| SdpCompressError::JsonEncodeFailed)
    }

    pub fn decompress_sdp(_sdp: Cow<str>) -> Result<SdpSession, SdpDecompressError> {
        /* TODO! */
        Err(SdpDecompressError::Unimplemented)
    }

    #[cfg(test)]
    mod test {
        use crate::sdp::mode1::compress_sdp;

        #[test]
        fn test_compress() {
            let session = webrtc_sdp::parse_sdp(LONG_SDP, false).expect("failed to parse dummy sdp");
            println!("{}", compress_sdp(&session).unwrap());
        }

        const LONG_SDP: &'static str = r#"v=0
o=- 4451494552622545706 4 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1 2 3 4 5 6 7 8
a=msid-semantic: WMS
m=audio 62156 UDP/TLS/RTP/SAVPF 111 112 103 104 9 0 8 106 105 13 110 112 113 126
c=IN IP4 192.168.40.135
a=rtcp:9 IN IP4 0.0.0.0
a=candidate:1905690388 1 udp 2122260223 192.168.40.135 62156 typ host generation 0 network-id 1
a=candidate:1929993293 1 udp 2122194687 192.168.237.161 62157 typ host generation 0 network-id 2
a=candidate:2999745851 1 udp 2122129151 192.168.56.1 62158 typ host generation 0 network-id 3
a=candidate:1058372580 1 tcp 1518280447 192.168.40.135 9 typ host tcptype active generation 0 network-id 1
a=candidate:1032495293 1 tcp 1518214911 192.168.237.161 9 typ host tcptype active generation 0 network-id 2
a=candidate:4233069003 1 tcp 1518149375 192.168.56.1 9 typ host tcptype active generation 0 network-id 3
a=ice-ufrag:bN5b
a=ice-pwd:VG5bAKB/tcxNibiF1u3Tut2L
a=ice-options:trickle
a=fingerprint:sha-256 6B:EC:D9:2C:61:70:80:ED:EA:0F:29:47:E9:4D:30:11:A3:19:8B:7F:D8:B8:D9:E4:F5:AA:7C:45:D3:5F:67:64
a=setup:actpass
a=mid:0
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=sendrecv
a=msid:- 5a03cca0-2876-4629-837a-6c433cbe22cc
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtcp-fb:111 transport-cc
a=fmtp:111 minptime=1;stereo=1;useinbandfec=1
a=rtpmap:119 opus/48000/2
a=rtcp-fb:119 transport-cc
a=fmtp:119 minptime=1;stereo=1;useinbandfec=1
a=rtpmap:103 ISAC/16000
a=rtpmap:104 ISAC/32000
a=rtpmap:9 G722/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:106 CN/32000
a=rtpmap:105 CN/16000
a=rtpmap:13 CN/8000
a=rtpmap:110 telephone-event/48000
a=rtpmap:112 telephone-event/32000
a=rtpmap:113 telephone-event/16000
a=rtpmap:126 telephone-event/8000
a=maxptime:20
a=ssrc:4068673781 cname:Ep6aTFpFF0B3oPFP
a=ssrc:4068673781 msid:- 5a03cca0-2876-4629-837a-6c433cbe22cc
a=ssrc:4068673781 mslabel:-
a=ssrc:4068673781 label:5a03cca0-2876-4629-837a-6c433cbe22cc
m=audio 9 UDP/TLS/RTP/SAVPF 111 112 103 104 9 0 8 106 105 13 110 112 113 126
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:bN5b
a=ice-pwd:VG5bAKB/tcxNibiF1u3Tut2L
a=ice-options:trickle
a=fingerprint:sha-256 6B:EC:D9:2C:61:70:80:ED:EA:0F:29:47:E9:4D:30:11:A3:19:8B:7F:D8:B8:D9:E4:F5:AA:7C:45:D3:5F:67:64
a=setup:actpass
a=mid:1
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=sendrecv
a=msid:- 8b821557-8e7c-4088-844a-849e31022248
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtcp-fb:111 transport-cc
a=fmtp:111 minptime=1;stereo=1;useinbandfec=1
a=rtpmap:119 opus/48000/2
a=rtcp-fb:119 transport-cc
a=fmtp:119 minptime=1;stereo=1;useinbandfec=1
a=rtpmap:103 ISAC/16000
a=rtpmap:104 ISAC/32000
a=rtpmap:9 G722/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:106 CN/32000
a=rtpmap:105 CN/16000
a=rtpmap:13 CN/8000
a=rtpmap:110 telephone-event/48000
a=rtpmap:112 telephone-event/32000
a=rtpmap:113 telephone-event/16000
a=rtpmap:126 telephone-event/8000
a=maxptime:20
a=ssrc:2569047651 cname:Ep6aTFpFF0B3oPFP
a=ssrc:2569047651 msid:- 8b821557-8e7c-4088-844a-849e31022248
a=ssrc:2569047651 mslabel:-
a=ssrc:2569047651 label:8b821557-8e7c-4088-844a-849e31022248
m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 102 121 127 120 125 107 108 109 124 119 123 118 114 115 116
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:bN5b
a=ice-pwd:VG5bAKB/tcxNibiF1u3Tut2L
a=ice-options:trickle
a=fingerprint:sha-256 6B:EC:D9:2C:61:70:80:ED:EA:0F:29:47:E9:4D:30:11:A3:19:8B:7F:D8:B8:D9:E4:F5:AA:7C:45:D3:5F:67:64
a=setup:actpass
a=mid:2
a=extmap:14 urn:ietf:params:rtp-hdrext:toffset
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:13 urn:3gpp:video-orientation
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:12 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
a=extmap:11 http://www.webrtc.org/experiments/rtp-hdrext/video-content-type
a=extmap:7 http://www.webrtc.org/experiments/rtp-hdrext/video-timing
a=extmap:8 http://www.webrtc.org/experiments/rtp-hdrext/color-space
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=sendrecv
a=msid:- c3677774-365b-41d6-95ff-5d2764af6a0a
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:96 VP8/90000
a=rtcp-fb:96 nack
a=rtcp-fb:96 nack pli
a=rtcp-fb:96 ccm fir
a=rtcp-fb:96 transport-cc
a=rtpmap:97 rtx/90000
a=fmtp:97 apt=96
a=rtpmap:98 VP9/90000
a=rtcp-fb:98 goog-remb
a=rtcp-fb:98 transport-cc
a=rtcp-fb:98 ccm fir
a=rtcp-fb:98 nack
a=rtcp-fb:98 nack pli
a=fmtp:98 profile-id=0
a=rtpmap:99 rtx/90000
a=fmtp:99 apt=98
a=rtpmap:100 VP9/90000
a=rtcp-fb:100 goog-remb
a=rtcp-fb:100 transport-cc
a=rtcp-fb:100 ccm fir
a=rtcp-fb:100 nack
a=rtcp-fb:100 nack pli
a=fmtp:100 profile-id=2
a=rtpmap:101 rtx/90000
a=fmtp:101 apt=100
a=rtpmap:102 H264/90000
a=rtcp-fb:102 goog-remb
a=rtcp-fb:102 transport-cc
a=rtcp-fb:102 ccm fir
a=rtcp-fb:102 nack
a=rtcp-fb:102 nack pli
a=fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42001f
a=rtpmap:121 rtx/90000
a=fmtp:121 apt=102
a=rtpmap:127 H264/90000
a=rtcp-fb:127 goog-remb
a=rtcp-fb:127 transport-cc
a=rtcp-fb:127 ccm fir
a=rtcp-fb:127 nack
a=rtcp-fb:127 nack pli
a=fmtp:127 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42001f
a=rtpmap:120 rtx/90000
a=fmtp:120 apt=127
a=rtpmap:125 H264/90000
a=rtcp-fb:125 goog-remb
a=rtcp-fb:125 transport-cc
a=rtcp-fb:125 ccm fir
a=rtcp-fb:125 nack
a=rtcp-fb:125 nack pli
a=fmtp:125 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
a=rtpmap:107 rtx/90000
a=fmtp:107 apt=125
a=rtpmap:108 H264/90000
a=rtcp-fb:108 goog-remb
a=rtcp-fb:108 transport-cc
a=rtcp-fb:108 ccm fir
a=rtcp-fb:108 nack
a=rtcp-fb:108 nack pli
a=fmtp:108 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f
a=rtpmap:109 rtx/90000
a=fmtp:109 apt=108
a=rtpmap:124 H264/90000
a=rtcp-fb:124 goog-remb
a=rtcp-fb:124 transport-cc
a=rtcp-fb:124 ccm fir
a=rtcp-fb:124 nack
a=rtcp-fb:124 nack pli
a=fmtp:124 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=4d001f
a=rtpmap:119 rtx/90000
a=fmtp:119 apt=124
a=rtpmap:123 H264/90000
a=rtcp-fb:123 goog-remb
a=rtcp-fb:123 transport-cc
a=rtcp-fb:123 ccm fir
a=rtcp-fb:123 nack
a=rtcp-fb:123 nack pli
a=fmtp:123 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=64001f
a=rtpmap:118 rtx/90000
a=fmtp:118 apt=123
a=rtpmap:114 red/90000
a=rtpmap:115 rtx/90000
a=fmtp:115 apt=114
a=rtpmap:116 ulpfec/90000
a=ssrc-group:FID 3859623491 3834947611
a=ssrc:3859623491 cname:Ep6aTFpFF0B3oPFP
a=ssrc:3859623491 msid:- c3677774-365b-41d6-95ff-5d2764af6a0a
a=ssrc:3859623491 mslabel:-
a=ssrc:3859623491 label:c3677774-365b-41d6-95ff-5d2764af6a0a
a=ssrc:3834947611 cname:Ep6aTFpFF0B3oPFP
a=ssrc:3834947611 msid:- c3677774-365b-41d6-95ff-5d2764af6a0a
a=ssrc:3834947611 mslabel:-
a=ssrc:3834947611 label:c3677774-365b-41d6-95ff-5d2764af6a0a
m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 102 121 127 120 125 107 108 109 124 119 123 118 114 115 116
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:bN5b
a=ice-pwd:VG5bAKB/tcxNibiF1u3Tut2L
a=ice-options:trickle
a=fingerprint:sha-256 6B:EC:D9:2C:61:70:80:ED:EA:0F:29:47:E9:4D:30:11:A3:19:8B:7F:D8:B8:D9:E4:F5:AA:7C:45:D3:5F:67:64
a=setup:actpass
a=mid:3
a=extmap:14 urn:ietf:params:rtp-hdrext:toffset
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:13 urn:3gpp:video-orientation
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:12 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
a=extmap:11 http://www.webrtc.org/experiments/rtp-hdrext/video-content-type
a=extmap:7 http://www.webrtc.org/experiments/rtp-hdrext/video-timing
a=extmap:8 http://www.webrtc.org/experiments/rtp-hdrext/color-space
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=sendrecv
a=msid:- e244f59d-ba3e-4298-960b-d2778421c985
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:96 VP8/90000
a=rtcp-fb:96 nack
a=rtcp-fb:96 nack pli
a=rtcp-fb:96 ccm fir
a=rtcp-fb:96 transport-cc
a=rtpmap:97 rtx/90000
a=fmtp:97 apt=96
a=rtpmap:98 VP9/90000
a=rtcp-fb:98 goog-remb
a=rtcp-fb:98 transport-cc
a=rtcp-fb:98 ccm fir
a=rtcp-fb:98 nack
a=rtcp-fb:98 nack pli
a=fmtp:98 profile-id=0
a=rtpmap:99 rtx/90000
a=fmtp:99 apt=98
a=rtpmap:100 VP9/90000
a=rtcp-fb:100 goog-remb
a=rtcp-fb:100 transport-cc
a=rtcp-fb:100 ccm fir
a=rtcp-fb:100 nack
a=rtcp-fb:100 nack pli
a=fmtp:100 profile-id=2
a=rtpmap:101 rtx/90000
a=fmtp:101 apt=100
a=rtpmap:102 H264/90000
a=rtcp-fb:102 goog-remb
a=rtcp-fb:102 transport-cc
a=rtcp-fb:102 ccm fir
a=rtcp-fb:102 nack
a=rtcp-fb:102 nack pli
a=fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42001f
a=rtpmap:121 rtx/90000
a=fmtp:121 apt=102
a=rtpmap:127 H264/90000
a=rtcp-fb:127 goog-remb
a=rtcp-fb:127 transport-cc
a=rtcp-fb:127 ccm fir
a=rtcp-fb:127 nack
a=rtcp-fb:127 nack pli
a=fmtp:127 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42001f
a=rtpmap:120 rtx/90000
a=fmtp:120 apt=127
a=rtpmap:125 H264/90000
a=rtcp-fb:125 goog-remb
a=rtcp-fb:125 transport-cc
a=rtcp-fb:125 ccm fir
a=rtcp-fb:125 nack
a=rtcp-fb:125 nack pli
a=fmtp:125 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
a=rtpmap:107 rtx/90000
a=fmtp:107 apt=125
a=rtpmap:108 H264/90000
a=rtcp-fb:108 goog-remb
a=rtcp-fb:108 transport-cc
a=rtcp-fb:108 ccm fir
a=rtcp-fb:108 nack
a=rtcp-fb:108 nack pli
a=fmtp:108 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f
a=rtpmap:109 rtx/90000
a=fmtp:109 apt=108
a=rtpmap:124 H264/90000
a=rtcp-fb:124 goog-remb
a=rtcp-fb:124 transport-cc
a=rtcp-fb:124 ccm fir
a=rtcp-fb:124 nack
a=rtcp-fb:124 nack pli
a=fmtp:124 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=4d001f
a=rtpmap:119 rtx/90000
a=fmtp:119 apt=124
a=rtpmap:123 H264/90000
a=rtcp-fb:123 goog-remb
a=rtcp-fb:123 transport-cc
a=rtcp-fb:123 ccm fir
a=rtcp-fb:123 nack
a=rtcp-fb:123 nack pli
a=fmtp:123 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=64001f
a=rtpmap:118 rtx/90000
a=fmtp:118 apt=123
a=rtpmap:114 red/90000
a=rtpmap:115 rtx/90000
a=fmtp:115 apt=114
a=rtpmap:116 ulpfec/90000
a=ssrc-group:FID 2975446128 1648735555
a=ssrc:2975446128 cname:Ep6aTFpFF0B3oPFP
a=ssrc:2975446128 msid:- e244f59d-ba3e-4298-960b-d2778421c985
a=ssrc:2975446128 mslabel:-
a=ssrc:2975446128 label:e244f59d-ba3e-4298-960b-d2778421c985
a=ssrc:1648735555 cname:Ep6aTFpFF0B3oPFP
a=ssrc:1648735555 msid:- e244f59d-ba3e-4298-960b-d2778421c985
a=ssrc:1648735555 mslabel:-
a=ssrc:1648735555 label:e244f59d-ba3e-4298-960b-d2778421c985
m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 126
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:bN5b
a=ice-pwd:VG5bAKB/tcxNibiF1u3Tut2L
a=ice-options:trickle
a=fingerprint:sha-256 6B:EC:D9:2C:61:70:80:ED:EA:0F:29:47:E9:4D:30:11:A3:19:8B:7F:D8:B8:D9:E4:F5:AA:7C:45:D3:5F:67:64
a=setup:actpass
a=mid:4
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=recvonly
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtcp-fb:111 transport-cc
a=fmtp:111 minptime=1;stereo=1;useinbandfec=1
a=rtpmap:103 ISAC/16000
a=rtpmap:104 ISAC/32000
a=rtpmap:9 G722/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:106 CN/32000
a=rtpmap:105 CN/16000
a=rtpmap:13 CN/8000
a=rtpmap:110 telephone-event/48000
a=rtpmap:112 telephone-event/32000
a=rtpmap:113 telephone-event/16000
a=rtpmap:126 telephone-event/8000
a=maxptime:20
m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 126
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:bN5b
a=ice-pwd:VG5bAKB/tcxNibiF1u3Tut2L
a=ice-options:trickle
a=fingerprint:sha-256 6B:EC:D9:2C:61:70:80:ED:EA:0F:29:47:E9:4D:30:11:A3:19:8B:7F:D8:B8:D9:E4:F5:AA:7C:45:D3:5F:67:64
a=setup:actpass
a=mid:5
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=recvonly
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtcp-fb:111 transport-cc
a=fmtp:111 minptime=1;stereo=1;useinbandfec=1
a=rtpmap:103 ISAC/16000
a=rtpmap:104 ISAC/32000
a=rtpmap:9 G722/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:106 CN/32000
a=rtpmap:105 CN/16000
a=rtpmap:13 CN/8000
a=rtpmap:110 telephone-event/48000
a=rtpmap:112 telephone-event/32000
a=rtpmap:113 telephone-event/16000
a=rtpmap:126 telephone-event/8000
a=maxptime:20
m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 126
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:bN5b
a=ice-pwd:VG5bAKB/tcxNibiF1u3Tut2L
a=ice-options:trickle
a=fingerprint:sha-256 6B:EC:D9:2C:61:70:80:ED:EA:0F:29:47:E9:4D:30:11:A3:19:8B:7F:D8:B8:D9:E4:F5:AA:7C:45:D3:5F:67:64
a=setup:actpass
a=mid:6
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=recvonly
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtcp-fb:111 transport-cc
a=fmtp:111 minptime=1;stereo=1;useinbandfec=1
a=rtpmap:103 ISAC/16000
a=rtpmap:104 ISAC/32000
a=rtpmap:9 G722/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:106 CN/32000
a=rtpmap:105 CN/16000
a=rtpmap:13 CN/8000
a=rtpmap:110 telephone-event/48000
a=rtpmap:112 telephone-event/32000
a=rtpmap:113 telephone-event/16000
a=rtpmap:126 telephone-event/8000
a=maxptime:20
m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 126
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:bN5b
a=ice-pwd:VG5bAKB/tcxNibiF1u3Tut2L
a=ice-options:trickle
a=fingerprint:sha-256 6B:EC:D9:2C:61:70:80:ED:EA:0F:29:47:E9:4D:30:11:A3:19:8B:7F:D8:B8:D9:E4:F5:AA:7C:45:D3:5F:67:64
a=setup:actpass
a=mid:7
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=recvonly
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtcp-fb:111 transport-cc
a=fmtp:111 minptime=1;stereo=1;useinbandfec=1
a=rtpmap:103 ISAC/16000
a=rtpmap:104 ISAC/32000
a=rtpmap:9 G722/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:106 CN/32000
a=rtpmap:105 CN/16000
a=rtpmap:13 CN/8000
a=rtpmap:110 telephone-event/48000
a=rtpmap:112 telephone-event/32000
a=rtpmap:113 telephone-event/16000
a=rtpmap:126 telephone-event/8000
a=maxptime:20
m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 126
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:bN5b
a=ice-pwd:VG5bAKB/tcxNibiF1u3Tut2L
a=ice-options:trickle
a=fingerprint:sha-256 6B:EC:D9:2C:61:70:80:ED:EA:0F:29:47:E9:4D:30:11:A3:19:8B:7F:D8:B8:D9:E4:F5:AA:7C:45:D3:5F:67:64
a=setup:actpass
a=mid:8
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=recvonly
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtcp-fb:111 transport-cc
a=fmtp:111 minptime=1;stereo=1;useinbandfec=1
a=rtpmap:103 ISAC/16000
a=rtpmap:104 ISAC/32000
a=rtpmap:9 G722/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:106 CN/32000
a=rtpmap:105 CN/16000
a=rtpmap:13 CN/8000
a=rtpmap:110 telephone-event/48000
a=rtpmap:112 telephone-event/32000
a=rtpmap:113 telephone-event/16000
a=rtpmap:126 telephone-event/8000
a=maxptime:20
"#;

        const REAL_SDP: &'static str = r#"v=0
o=- 4451494552622545706 2 IN IP4 127.0.0.1
s=-
t=0 0
a=msid-semantic:  WMS
a=group:BUNDLE 0 1 2 3
m=audio 9 UDP/TLS/RTP/SAVPF 111 112
c=IN IP4 0.0.0.0
a=rtpmap:111 opus/48000/2
a=rtpmap:119 opus/48000/2
a=fmtp:111 minptime=1;maxptime=20;useinbandfec=1;stereo=1
a=fmtp:119 minptime=1;maxptime=20;useinbandfec=1;stereo=1
a=rtcp:9 IN IP4 0.0.0.0
a=rtcp-fb:111 transport-cc
a=rtcp-fb:112 transport-cc
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=setup:actpass
a=mid:0
a=msid:- 5a03cca0-2876-4629-837a-6c433cbe22cc
a=sendrecv
a=ice-ufrag:bN5b
a=ice-pwd:VG5bAKB/tcxNibiF1u3Tut2L
a=fingerprint:sha-256 6B:EC:D9:2C:61:70:80:ED:EA:0F:29:47:E9:4D:30:11:A3:19:8B:7F:D8:B8:D9:E4:F5:AA:7C:45:D3:5F:67:64
a=ice-options:trickle
a=ssrc:2222222222
a=ssrc:4068673781 cname:Ep6aTFpFF0B3oPFP
a=ssrc:4068673781 msid:- 5a03cca0-2876-4629-837a-6c433cbe22cc
a=ssrc:4068673781 mslabel:-
a=ssrc:4068673781 label:5a03cca0-2876-4629-837a-6c433cbe22cc
a=rtcp-mux
m=audio 9 UDP/TLS/RTP/SAVPF 111 112
c=IN IP4 0.0.0.0
a=rtpmap:111 opus/48000/2
a=rtpmap:119 opus/48000/2
a=fmtp:111 minptime=1;maxptime=20;useinbandfec=1;stereo=1
a=fmtp:119 minptime=1;maxptime=20;useinbandfec=1;stereo=1
a=rtcp:9 IN IP4 0.0.0.0
a=rtcp-fb:111 transport-cc
a=rtcp-fb:112 transport-cc
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=setup:actpass
a=mid:1
a=msid:- 8b821557-8e7c-4088-844a-849e31022248
a=sendrecv
a=ice-ufrag:bN5b
a=ice-pwd:VG5bAKB/tcxNibiF1u3Tut2L
a=fingerprint:sha-256 6B:EC:D9:2C:61:70:80:ED:EA:0F:29:47:E9:4D:30:11:A3:19:8B:7F:D8:B8:D9:E4:F5:AA:7C:45:D3:5F:67:64
a=ice-options:trickle
a=ssrc:2569047651 cname:Ep6aTFpFF0B3oPFP
a=ssrc:2569047651 msid:- 8b821557-8e7c-4088-844a-849e31022248
a=ssrc:2569047651 mslabel:-
a=ssrc:2569047651 label:8b821557-8e7c-4088-844a-849e31022248
a=rtcp-mux
m=video 9 UDP/TLS/RTP/SAVPF 96
c=IN IP4 0.0.0.0
a=rtpmap:96 VP8/90000
a=rtcp:9 IN IP4 0.0.0.0
a=rtcp-fb:96 nack
a=rtcp-fb:96 nack pli
a=rtcp-fb:96 ccm fir
a=rtcp-fb:96 transport-cc
a=extmap:14 urn:ietf:params:rtp-hdrext:toffset
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:13 urn:3gpp:video-orientation
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:12 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
a=extmap:11 http://www.webrtc.org/experiments/rtp-hdrext/video-content-type
a=extmap:7 http://www.webrtc.org/experiments/rtp-hdrext/video-timing
a=extmap:8 http://www.webrtc.org/experiments/rtp-hdrext/color-space
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=setup:actpass
a=mid:2
a=msid:- c3677774-365b-41d6-95ff-5d2764af6a0a
a=sendrecv
a=ice-ufrag:bN5b
a=ice-pwd:VG5bAKB/tcxNibiF1u3Tut2L
a=fingerprint:sha-256 6B:EC:D9:2C:61:70:80:ED:EA:0F:29:47:E9:4D:30:11:A3:19:8B:7F:D8:B8:D9:E4:F5:AA:7C:45:D3:5F:67:64
a=ice-options:trickle
a=ssrc:3859623491 cname:Ep6aTFpFF0B3oPFP
a=ssrc:3859623491 msid:- c3677774-365b-41d6-95ff-5d2764af6a0a
a=ssrc:3859623491 mslabel:-
a=ssrc:3859623491 label:c3677774-365b-41d6-95ff-5d2764af6a0a
a=ssrc:3834947611 cname:Ep6aTFpFF0B3oPFP
a=ssrc:3834947611 msid:- c3677774-365b-41d6-95ff-5d2764af6a0a
a=ssrc:3834947611 mslabel:-
a=ssrc:3834947611 label:c3677774-365b-41d6-95ff-5d2764af6a0a
a=ssrc-group:FID 3859623491 3834947611
a=rtcp-mux
a=rtcp-rsize
m=video 9 UDP/TLS/RTP/SAVPF 96
c=IN IP4 0.0.0.0
a=rtpmap:96 VP8/90000
a=rtcp:9 IN IP4 0.0.0.0
a=rtcp-fb:96 nack
a=rtcp-fb:96 nack pli
a=rtcp-fb:96 ccm fir
a=rtcp-fb:96 transport-cc
a=extmap:14 urn:ietf:params:rtp-hdrext:toffset
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:13 urn:3gpp:video-orientation
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:12 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
a=extmap:11 http://www.webrtc.org/experiments/rtp-hdrext/video-content-type
a=extmap:7 http://www.webrtc.org/experiments/rtp-hdrext/video-timing
a=extmap:8 http://www.webrtc.org/experiments/rtp-hdrext/color-space
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=setup:actpass
a=mid:3
a=msid:- e244f59d-ba3e-4298-960b-d2778421c985
a=sendrecv
a=ice-ufrag:bN5b
a=ice-pwd:VG5bAKB/tcxNibiF1u3Tut2L
a=fingerprint:sha-256 6B:EC:D9:2C:61:70:80:ED:EA:0F:29:47:E9:4D:30:11:A3:19:8B:7F:D8:B8:D9:E4:F5:AA:7C:45:D3:5F:67:64
a=ice-options:trickle
a=ssrc:2975446128 cname:Ep6aTFpFF0B3oPFP
a=ssrc:2975446128 msid:- e244f59d-ba3e-4298-960b-d2778421c985
a=ssrc:2975446128 mslabel:-
a=ssrc:2975446128 label:e244f59d-ba3e-4298-960b-d2778421c985
a=ssrc:1648735555 cname:Ep6aTFpFF0B3oPFP
a=ssrc:1648735555 msid:- e244f59d-ba3e-4298-960b-d2778421c985
a=ssrc:1648735555 mslabel:-
a=ssrc:1648735555 label:e244f59d-ba3e-4298-960b-d2778421c985
a=ssrc-group:FID 2975446128 1648735555
a=rtcp-mux
a=rtcp-rsize"#;

        const DUMMY_SDP: &'static str = r#"v=0
o=- 4451494552622545706 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1 2 3
a=msid-semantic: WMS
m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 126
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:bN5b
a=ice-pwd:VG5bAKB/tcxNibiF1u3Tut2L
a=ice-options:trickle
a=fingerprint:sha-256 6B:EC:D9:2C:61:70:80:ED:EA:0F:29:47:E9:4D:30:11:A3:19:8B:7F:D8:B8:D9:E4:F5:AA:7C:45:D3:5F:67:64
a=setup:actpass
a=mid:0
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=sendrecv
a=msid:- 5a03cca0-2876-4629-837a-6c433cbe22cc
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtcp-fb:111 transport-cc
a=fmtp:111 minptime=10;useinbandfec=1
a=rtpmap:103 ISAC/16000
a=rtpmap:104 ISAC/32000
a=rtpmap:9 G722/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:106 CN/32000
a=rtpmap:105 CN/16000
a=rtpmap:13 CN/8000
a=rtpmap:110 telephone-event/48000
a=rtpmap:112 telephone-event/32000
a=rtpmap:113 telephone-event/16000
a=rtpmap:126 telephone-event/8000
a=ssrc:4068673781 cname:Ep6aTFpFF0B3oPFP
a=ssrc:4068673781 msid:- 5a03cca0-2876-4629-837a-6c433cbe22cc
a=ssrc:4068673781 mslabel:-
a=ssrc:4068673781 label:5a03cca0-2876-4629-837a-6c433cbe22cc
m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 126
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:bN5b
a=ice-pwd:VG5bAKB/tcxNibiF1u3Tut2L
a=ice-options:trickle
a=fingerprint:sha-256 6B:EC:D9:2C:61:70:80:ED:EA:0F:29:47:E9:4D:30:11:A3:19:8B:7F:D8:B8:D9:E4:F5:AA:7C:45:D3:5F:67:64
a=setup:actpass
a=mid:1
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=sendrecv
a=msid:- 8b821557-8e7c-4088-844a-849e31022248
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtcp-fb:111 transport-cc
a=fmtp:111 minptime=10;useinbandfec=1
a=rtpmap:103 ISAC/16000
a=rtpmap:104 ISAC/32000
a=rtpmap:9 G722/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:106 CN/32000
a=rtpmap:105 CN/16000
a=rtpmap:13 CN/8000
a=rtpmap:110 telephone-event/48000
a=rtpmap:112 telephone-event/32000
a=rtpmap:113 telephone-event/16000
a=rtpmap:126 telephone-event/8000
a=ssrc:2569047651 cname:Ep6aTFpFF0B3oPFP
a=ssrc:2569047651 msid:- 8b821557-8e7c-4088-844a-849e31022248
a=ssrc:2569047651 mslabel:-
a=ssrc:2569047651 label:8b821557-8e7c-4088-844a-849e31022248
m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 102 121 127 120 125 107 108 109 124 119 123 118 114 115 116
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:bN5b
a=ice-pwd:VG5bAKB/tcxNibiF1u3Tut2L
a=ice-options:trickle
a=fingerprint:sha-256 6B:EC:D9:2C:61:70:80:ED:EA:0F:29:47:E9:4D:30:11:A3:19:8B:7F:D8:B8:D9:E4:F5:AA:7C:45:D3:5F:67:64
a=setup:actpass
a=mid:2
a=extmap:14 urn:ietf:params:rtp-hdrext:toffset
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:13 urn:3gpp:video-orientation
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:12 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
a=extmap:11 http://www.webrtc.org/experiments/rtp-hdrext/video-content-type
a=extmap:7 http://www.webrtc.org/experiments/rtp-hdrext/video-timing
a=extmap:8 http://www.webrtc.org/experiments/rtp-hdrext/color-space
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=sendrecv
a=msid:- c3677774-365b-41d6-95ff-5d2764af6a0a
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:96 VP8/90000
a=rtcp-fb:96 goog-remb
a=rtcp-fb:96 transport-cc
a=rtcp-fb:96 ccm fir
a=rtcp-fb:96 nack
a=rtcp-fb:96 nack pli
a=rtpmap:97 rtx/90000
a=fmtp:97 apt=96
a=rtpmap:98 VP9/90000
a=rtcp-fb:98 goog-remb
a=rtcp-fb:98 transport-cc
a=rtcp-fb:98 ccm fir
a=rtcp-fb:98 nack
a=rtcp-fb:98 nack pli
a=fmtp:98 profile-id=0
a=rtpmap:99 rtx/90000
a=fmtp:99 apt=98
a=rtpmap:100 VP9/90000
a=rtcp-fb:100 goog-remb
a=rtcp-fb:100 transport-cc
a=rtcp-fb:100 ccm fir
a=rtcp-fb:100 nack
a=rtcp-fb:100 nack pli
a=fmtp:100 profile-id=2
a=rtpmap:101 rtx/90000
a=fmtp:101 apt=100
a=rtpmap:102 H264/90000
a=rtcp-fb:102 goog-remb
a=rtcp-fb:102 transport-cc
a=rtcp-fb:102 ccm fir
a=rtcp-fb:102 nack
a=rtcp-fb:102 nack pli
a=fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42001f
a=rtpmap:121 rtx/90000
a=fmtp:121 apt=102
a=rtpmap:127 H264/90000
a=rtcp-fb:127 goog-remb
a=rtcp-fb:127 transport-cc
a=rtcp-fb:127 ccm fir
a=rtcp-fb:127 nack
a=rtcp-fb:127 nack pli
a=fmtp:127 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42001f
a=rtpmap:120 rtx/90000
a=fmtp:120 apt=127
a=rtpmap:125 H264/90000
a=rtcp-fb:125 goog-remb
a=rtcp-fb:125 transport-cc
a=rtcp-fb:125 ccm fir
a=rtcp-fb:125 nack
a=rtcp-fb:125 nack pli
a=fmtp:125 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
a=rtpmap:107 rtx/90000
a=fmtp:107 apt=125
a=rtpmap:108 H264/90000
a=rtcp-fb:108 goog-remb
a=rtcp-fb:108 transport-cc
a=rtcp-fb:108 ccm fir
a=rtcp-fb:108 nack
a=rtcp-fb:108 nack pli
a=fmtp:108 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f
a=rtpmap:109 rtx/90000
a=fmtp:109 apt=108
a=rtpmap:124 H264/90000
a=rtcp-fb:124 goog-remb
a=rtcp-fb:124 transport-cc
a=rtcp-fb:124 ccm fir
a=rtcp-fb:124 nack
a=rtcp-fb:124 nack pli
a=fmtp:124 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=4d001f
a=rtpmap:119 rtx/90000
a=fmtp:119 apt=124
a=rtpmap:123 H264/90000
a=rtcp-fb:123 goog-remb
a=rtcp-fb:123 transport-cc
a=rtcp-fb:123 ccm fir
a=rtcp-fb:123 nack
a=rtcp-fb:123 nack pli
a=fmtp:123 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=64001f
a=rtpmap:118 rtx/90000
a=fmtp:118 apt=123
a=rtpmap:114 red/90000
a=rtpmap:115 rtx/90000
a=fmtp:115 apt=114
a=rtpmap:116 ulpfec/90000
a=ssrc-group:FID 3859623491 3834947611
a=ssrc:3859623491 cname:Ep6aTFpFF0B3oPFP
a=ssrc:3859623491 msid:- c3677774-365b-41d6-95ff-5d2764af6a0a
a=ssrc:3859623491 mslabel:-
a=ssrc:3859623491 label:c3677774-365b-41d6-95ff-5d2764af6a0a
a=ssrc:3834947611 cname:Ep6aTFpFF0B3oPFP
a=ssrc:3834947611 msid:- c3677774-365b-41d6-95ff-5d2764af6a0a
a=ssrc:3834947611 mslabel:-
a=ssrc:3834947611 label:c3677774-365b-41d6-95ff-5d2764af6a0a
m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 102 121 127 120 125 107 108 109 124 119 123 118 114 115 116
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:bN5b
a=ice-pwd:VG5bAKB/tcxNibiF1u3Tut2L
a=ice-options:trickle
a=fingerprint:sha-256 6B:EC:D9:2C:61:70:80:ED:EA:0F:29:47:E9:4D:30:11:A3:19:8B:7F:D8:B8:D9:E4:F5:AA:7C:45:D3:5F:67:64
a=setup:actpass
a=mid:3
a=extmap:14 urn:ietf:params:rtp-hdrext:toffset
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:13 urn:3gpp:video-orientation
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:12 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
a=extmap:11 http://www.webrtc.org/experiments/rtp-hdrext/video-content-type
a=extmap:7 http://www.webrtc.org/experiments/rtp-hdrext/video-timing
a=extmap:8 http://www.webrtc.org/experiments/rtp-hdrext/color-space
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=sendrecv
a=msid:- e244f59d-ba3e-4298-960b-d2778421c985
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:96 VP8/90000
a=rtcp-fb:96 goog-remb
a=rtcp-fb:96 transport-cc
a=rtcp-fb:96 ccm fir
a=rtcp-fb:96 nack
a=rtcp-fb:96 nack pli
a=rtpmap:97 rtx/90000
a=fmtp:97 apt=96
a=rtpmap:98 VP9/90000
a=rtcp-fb:98 goog-remb
a=rtcp-fb:98 transport-cc
a=rtcp-fb:98 ccm fir
a=rtcp-fb:98 nack
a=rtcp-fb:98 nack pli
a=fmtp:98 profile-id=0
a=rtpmap:99 rtx/90000
a=fmtp:99 apt=98
a=rtpmap:100 VP9/90000
a=rtcp-fb:100 goog-remb
a=rtcp-fb:100 transport-cc
a=rtcp-fb:100 ccm fir
a=rtcp-fb:100 nack
a=rtcp-fb:100 nack pli
a=fmtp:100 profile-id=2
a=rtpmap:101 rtx/90000
a=fmtp:101 apt=100
a=rtpmap:102 H264/90000
a=rtcp-fb:102 goog-remb
a=rtcp-fb:102 transport-cc
a=rtcp-fb:102 ccm fir
a=rtcp-fb:102 nack
a=rtcp-fb:102 nack pli
a=fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42001f
a=rtpmap:121 rtx/90000
a=fmtp:121 apt=102
a=rtpmap:127 H264/90000
a=rtcp-fb:127 goog-remb
a=rtcp-fb:127 transport-cc
a=rtcp-fb:127 ccm fir
a=rtcp-fb:127 nack
a=rtcp-fb:127 nack pli
a=fmtp:127 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42001f
a=rtpmap:120 rtx/90000
a=fmtp:120 apt=127
a=rtpmap:125 H264/90000
a=rtcp-fb:125 goog-remb
a=rtcp-fb:125 transport-cc
a=rtcp-fb:125 ccm fir
a=rtcp-fb:125 nack
a=rtcp-fb:125 nack pli
a=fmtp:125 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
a=rtpmap:107 rtx/90000
a=fmtp:107 apt=125
a=rtpmap:108 H264/90000
a=rtcp-fb:108 goog-remb
a=rtcp-fb:108 transport-cc
a=rtcp-fb:108 ccm fir
a=rtcp-fb:108 nack
a=rtcp-fb:108 nack pli
a=fmtp:108 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f
a=rtpmap:109 rtx/90000
a=fmtp:109 apt=108
a=rtpmap:124 H264/90000
a=rtcp-fb:124 goog-remb
a=rtcp-fb:124 transport-cc
a=rtcp-fb:124 ccm fir
a=rtcp-fb:124 nack
a=rtcp-fb:124 nack pli
a=fmtp:124 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=4d001f
a=rtpmap:119 rtx/90000
a=fmtp:119 apt=124
a=rtpmap:123 H264/90000
a=rtcp-fb:123 goog-remb
a=rtcp-fb:123 transport-cc
a=rtcp-fb:123 ccm fir
a=rtcp-fb:123 nack
a=rtcp-fb:123 nack pli
a=fmtp:123 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=64001f
a=rtpmap:118 rtx/90000
a=fmtp:118 apt=123
a=rtpmap:114 red/90000
a=rtpmap:115 rtx/90000
a=fmtp:115 apt=114
a=rtpmap:116 ulpfec/90000
a=ssrc-group:FID 2975446128 1648735555
a=ssrc:2975446128 cname:Ep6aTFpFF0B3oPFP
a=ssrc:2975446128 msid:- e244f59d-ba3e-4298-960b-d2778421c985
a=ssrc:2975446128 mslabel:-
a=ssrc:2975446128 label:e244f59d-ba3e-4298-960b-d2778421c985
a=ssrc:1648735555 cname:Ep6aTFpFF0B3oPFP
a=ssrc:1648735555 msid:- e244f59d-ba3e-4298-960b-d2778421c985
a=ssrc:1648735555 mslabel:-
a=ssrc:1648735555 label:e244f59d-ba3e-4298-960b-d2778421c985
"#;
    }
}