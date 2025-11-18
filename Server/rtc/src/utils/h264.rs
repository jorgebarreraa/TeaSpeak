use std::io::{Cursor, Seek};
use byteorder::{ReadBytesExt, BigEndian};
use tokio::io::SeekFrom;

/* https://github.com/meetecho/janus-gateway/blob/a8e047077aaa21f8eac6cd2298e8ee872389d2e3/utils.c#L802 */
pub fn h264_is_keyframe(buffer: &[u8]) -> std::io::Result<bool> {
    let mut reader = Cursor::new(buffer);

    let nal_header = reader.read_u8()?;
    let packet_nal_type = nal_header & 0x1F;

    match packet_nal_type {
        7 => {
            /* Sequence parameter set */
            Ok(true)
        },
        28 | 29 => {
            let nal = reader.read_u8()? & 0x1F;
            Ok(nal == 7)
        },
        24 => {
            /* Single-Time Aggregation Packet */
            while let Ok(psize) = reader.read_u16::<BigEndian>() {
                let nal = reader.read_u8()? & 0x1F;
                if nal == 7 {
                    return Ok(true);
                }

                reader.seek(SeekFrom::Current(psize.into()))?;
            }

            Ok(false)
        },
        _ => Ok(false)
    }
}