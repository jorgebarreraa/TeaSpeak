use std::fmt::{Debug, Formatter};

pub trait GeneralExtension<'a> {
    fn value(&self, id: u8) -> Option<&'a [u8]>;
}


pub struct OneByteExtension<'a> {
    ext_buffer: &'a [u8]
}

impl<'a> OneByteExtension<'a> {
    pub fn new(extension: &'a [u8]) -> Self {
        OneByteExtension{
            ext_buffer: extension
        }
    }

    pub fn iter(&self) -> OneByteExtensionIterator<'a> {
        OneByteExtensionIterator{ ext_buffer: self.ext_buffer, index: 0 }
    }
}

impl<'a> GeneralExtension<'a> for OneByteExtension<'a> {
    fn value(&self, id: u8) -> Option<&'a [u8]> {
        self.iter().find(|e| e.0 == id).map(|e| e.1)
    }
}

impl Debug for OneByteExtension<'_> {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("OneByteExtension")
            .field("ids", &self.iter().map(|e| e.0).collect::<Vec<_>>())
            .finish()
    }
}

pub struct OneByteExtensionIterator<'a> {
    ext_buffer: &'a [u8],
    index: usize
}

impl<'a> Iterator for OneByteExtensionIterator<'a> {
    type Item = (u8, &'a [u8]);

    fn next(&mut self) -> Option<Self::Item> {
        while self.index < self.ext_buffer.len() && self.ext_buffer[self.index] == 0 {
            self.index += 1;
        }

        if self.index >= self.ext_buffer.len() {
            return None;
        }

        let info = self.ext_buffer[self.index];
        let length = (info & 0xF) as usize;
        let id = info >> 4;
        if self.index + 2 + length > self.ext_buffer.len() {
            /* Broken header... */
            return None;
        }

        let result = (id, &self.ext_buffer[(self.index + 1)..(self.index + 2 + length)]);
        self.index += length + 2;

        Some(result)
    }
}

pub struct TwoByteExtension<'a> {
    ext_buffer: &'a [u8]
}

impl<'a> TwoByteExtension<'a> {
    pub fn new(extension: &'a [u8]) -> Self {
        TwoByteExtension {
            ext_buffer: extension
        }
    }

    pub fn iter(&self) -> TwoByteExtensionIterator<'a> {
        TwoByteExtensionIterator { ext_buffer: self.ext_buffer, index: 0 }
    }
}

impl<'a> GeneralExtension<'a> for TwoByteExtension<'a> {
    fn value(&self, id: u8) -> Option<&'a [u8]> {
        self.iter().find(|e| e.0 == id).map(|e| e.1)
    }
}

impl Debug for TwoByteExtension<'_> {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("TwoByteExtension")
            .field("ids", &self.iter().map(|e| e.0).collect::<Vec<_>>())
            .finish()
    }
}

pub struct TwoByteExtensionIterator<'a> {
    ext_buffer: &'a [u8],
    index: usize
}

impl<'a> Iterator for TwoByteExtensionIterator<'a> {
    type Item = (u8, &'a [u8]);

    fn next(&mut self) -> Option<Self::Item> {
        while self.index < self.ext_buffer.len() && self.ext_buffer[self.index] == 0 {
            self.index += 1;
        }

        if self.index + 1 >= self.ext_buffer.len() {
            return None;
        }

        let id = self.ext_buffer[self.index];
        let length = self.ext_buffer[self.index + 1] as usize;
        if self.index + 3 + length > self.ext_buffer.len() {
            /* Broken header... */
            return None;
        }

        let result = (id, &self.ext_buffer[(self.index + 2)..(self.index + 3 + length)]);
        self.index += length + 3;

        Some(result)
    }
}