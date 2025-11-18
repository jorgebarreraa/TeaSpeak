use std::collections::BTreeMap;
use crate::client::{Client};
use crate::threads::{MAIN_GIO_EVENT_LOOP, execute_task};
use std::sync::{Mutex, Arc};
use std::os::raw::c_void;
use crate::channel::Channel;
use parking_lot::RwLock;
use std::ops::DerefMut;
use num_enum::{ IntoPrimitive, TryFromPrimitive };

#[derive(PartialEq, PartialOrd, Debug, TryFromPrimitive, IntoPrimitive)]
#[repr(u32)]
pub enum ChannelAssignResult {
    Success = 0x00,
    ClientUnknown = 0x01,
    TargetChannelUnknown = 0x02
}

/* TODO: Allow N main contexts and distribute clients evenly */
pub struct Server {
    clients: BTreeMap<u32, Arc<Mutex<Client>>>,
    client_id_index: u32,

    channels: BTreeMap<u32, Arc<RwLock<Channel>>>,
    channel_id_index: u32,

    client_channel_assignments: BTreeMap<u32, u32>,

    main_loop: glib::MainLoop
}

impl Server {
    pub fn new() -> Server {
        Server {
            clients: BTreeMap::new(),
            client_id_index: 1,

            channels: BTreeMap::new(),
            channel_id_index: 1,

            client_channel_assignments: BTreeMap::new(),

            main_loop: MAIN_GIO_EVENT_LOOP.lock().unwrap().event_loop()
        }
    }

    pub fn create_client(&mut self, data: *mut c_void) -> (u32, Arc<Mutex<Client>>) {
        if self.client_id_index == 0 { self.client_id_index += 1; }
        let client_id = self.client_id_index;
        let client = Client::new(client_id, self.main_loop.get_context(), data);

        self.client_id_index = self.client_id_index.wrapping_add(1);
        self.clients.insert(client_id, client.clone());

        (client_id, client)
    }

    pub fn find_client(&self, client_id: u32) -> Option<&Arc<Mutex<Client>>> {
        self.clients.get(&client_id)
    }

    pub fn destroy_client(&mut self, client_id: u32) -> bool {
        self.assign_channel(client_id, 0);
        if let Some(_client) = self.clients.remove(&client_id) {
            /* Do more shutdown actions. Currently nothing required */
            true
        } else {
            false
        }
    }

    pub fn find_channel(&self, channel_id: u32) -> Option<&Arc<RwLock<Channel>>> {
        self.channels.get(&channel_id)
    }

    pub fn create_channel(&mut self) -> (u32, Arc<RwLock<Channel>>) {
        if self.channel_id_index == 0 { self.channel_id_index += 1; }
        let channel_id = self.channel_id_index;
        self.channel_id_index = self.channel_id_index.wrapping_add(1);

        let channel = Channel::new(channel_id);
        self.channels.insert(channel_id, channel.clone());

        {
            let channel = Arc::downgrade(&channel);
            execute_task(async move {
                if let Some(channel) = channel.upgrade() {
                    channel.write().execute();
                }
            });
        }

        (channel_id, channel)
    }

    pub fn assign_channel(&mut self, client_id: u32, channel_id: u32) -> ChannelAssignResult {
        let client = self.find_client(client_id).map(|e| e.clone());
        if !client.is_some() {
            return ChannelAssignResult::ClientUnknown;
        }
        let client = client.unwrap();

        let channel = self.find_channel(channel_id).map(|e| e.clone());
        if !channel.is_some() && channel_id != 0 {
            return ChannelAssignResult::TargetChannelUnknown;
        }

        /* remove client from his old channel */
        if let Some(channel_id) = self.client_channel_assignments.remove(&client_id) {
            if let Some(channel) = self.find_channel(channel_id) {
                let mut channel = channel.write();
                channel.unregister_client(client_id, false);
            }
        }

        if let Some(channel) = channel {
            self.client_channel_assignments.insert(client_id, channel_id);

            let mut channel = channel.write();
            let mut owned_client = client.lock().unwrap();
            channel.register_client(client.clone(), owned_client.deref_mut().deref_mut());
        }

        ChannelAssignResult::Success
    }

    pub fn get_channel_id(&self, client_id: u32) -> Option<u32> {
        self.client_channel_assignments.get(&client_id).map(|e| *e)
    }

    pub fn get_channel(&self, client_id: u32) -> Option<&Arc<RwLock<Channel>>> {
        self.client_channel_assignments.get(&client_id)
            .map(|id| self.find_channel(*id))
            .flatten()
    }

    pub fn destroy_channel(&mut self, channel_id: u32) -> bool {
        if let Some(_channel) = self.channels.remove(&channel_id) {
            let clients = self.client_channel_assignments.iter()
                .filter(|(_, chan_id)| **chan_id == channel_id)
                .map(|(_, chan_id)| *chan_id)
                .collect::<Vec<_>>();

            for client_id in clients {
                self.assign_channel(client_id, 0);
            }

            true
        } else {
            false
        }
    }
}