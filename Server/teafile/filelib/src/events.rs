use parking_lot::Mutex;
use std::sync::{Arc, Weak};
use std::ops::Deref;
use std::ops::Fn;
use futures::Stream;
use futures::task::Context;
use tokio::macros::support::{Pin, Poll};
use tokio::sync::mpsc;

/// Return `true` if the listener should receive further events
/// and `false` if it should be unregistered.
type EventListener<Payload> = dyn Fn(&Payload) -> bool + Sync + Send;

struct EventsInner<Payload> {
    registered_handler: Mutex<Vec<Arc<EventListener<Payload>>>>
}

impl<Payload> EventsInner<Payload> {
    fn register_listener(&self, callback: Arc<EventListener<Payload>>) {
        self.registered_handler.lock().push(callback);
    }

    fn unregister_listener(&self, callback: &Arc<EventListener<Payload>>) {
        self.registered_handler.lock().drain_filter(|entry| {
            (*entry).deref() as *const _ == (*callback).deref() as *const _
        });
    }
}

pub struct EventEmitter<Payload> {
    inner: Arc<EventsInner<Payload>>,
    registered_handler_copy: Vec<Arc<EventListener<Payload>>>,
}

impl<Payload> EventEmitter<Payload> {
    fn new(inner: Arc<EventsInner<Payload>>) -> Self {
        EventEmitter{
            inner,
            registered_handler_copy: Vec::new()
        }
    }

    pub fn fire(&mut self, event: Payload) {
        let registered_handler = self.inner.registered_handler.lock();
        self.registered_handler_copy.clone_from(registered_handler.deref());
        drop(registered_handler);

        let removed_handler = self.registered_handler_copy.iter()
            .filter(|handler| !handler(&event))
            .collect::<Vec<_>>();

        if !removed_handler.is_empty() {
            let mut registered_handler = self.inner.registered_handler.lock();
            registered_handler.drain_filter(|entry| {
                removed_handler.iter()
                    .find(|rentry| (**rentry).deref() as *const _ == (*entry).deref() as *const _)
                    .is_some()
            });
        }
    }
}

impl<Payload> Clone for EventEmitter<Payload> {
    fn clone(&self) -> Self {
        EventEmitter::new(self.inner.clone())
    }
}

impl<Payload: Send + Sync + 'static> EventEmitter<Payload> {
    pub fn fire_later(&mut self, handle: Option<tokio::runtime::Handle>, event: Payload) {
        let inner = self.inner.clone();
        let event = Box::new(event);

        let handle = handle.unwrap_or_else(|| tokio::runtime::Handle::current());
        handle.spawn(async move {
            Self::new(inner).fire(*event);
        });
    }
}

#[derive(Clone)]
pub struct EventRegistry<Payload>(Arc<EventsInner<Payload>>);

impl<Payload: 'static> EventRegistry<Payload> {
    pub fn new() -> (EventRegistry<Payload>, EventEmitter<Payload>) {
        let inner = Arc::new(EventsInner{
            registered_handler: Mutex::new(Vec::with_capacity(32)),
        });

        (EventRegistry(inner.clone()), EventEmitter::new(inner))
    }

    pub fn hook(&self, callback: Box<dyn Fn(&Payload) -> () + Sync + Send>) -> HookHandle {
        let listener: Arc<EventListener<Payload>> = Arc::new(move |event| {
            callback(event);
            true
        });
        self.0.register_listener(listener.clone());
        HookHandle::new(Arc::downgrade(&self.0), listener)
    }

    pub fn register_listener(&self, callback: Arc<EventListener<Payload>>) {
        self.0.register_listener(callback);
    }

    pub fn unregister_listener(&self, callback: &Arc<EventListener<Payload>>) {
        self.0.unregister_listener(callback);
    }
}

impl<Payload: Clone + Send + Sync + 'static> EventRegistry<Payload> {
    pub fn event_stream(&self) -> EventStream<Payload> {
        let (sender, receiver) = tokio::sync::mpsc::unbounded_channel();
        let hook = self.hook(box move |event| {
            let _ = sender.send(Box::new(event.clone()));
        });
        EventStream(hook, receiver)
    }
}

pub struct HookHandle(Weak<EventsInner<()>>, Option<Arc<EventListener<()>>>);

impl HookHandle {
    fn new<Payload>(inner: Weak<EventsInner<Payload>>, listener: Arc<EventListener<Payload>>) -> Self {
        /*
         * Safety: The Payload will only be used behind a `Arc<_>`.
         *         We never access any field which requires the real signature of Payload.
         */
        let inner = unsafe {
            std::mem::transmute::<_, Weak<EventsInner<()>>>(inner)
        };

        /*
         * Safety: The Payload will only be used behind a `Arc<_>`.
         *         We never access any field which requires the real signature of Payload.
         */
        let listener = unsafe {
            std::mem::transmute::<_, Arc<EventListener<_>>>(listener)
        };

        HookHandle(inner, Some(listener))
    }
}

impl Default for HookHandle {
    fn default() -> Self {
        HookHandle(Default::default(), None)
    }
}

impl Drop for HookHandle {
    fn drop(&mut self) {
        if let Some(handle) = self.1.take() {
            if let Some(inner) = self.0.upgrade() {
                inner.unregister_listener(&handle);
            }
        }
    }
}

pub struct EventStream<Payload: Sync + Send + 'static>(HookHandle, mpsc::UnboundedReceiver<Box<Payload>>);

impl<Payload: Sync + Send + 'static> Stream for EventStream<Payload> {
    type Item = Payload;

    fn poll_next(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<Self::Item>> {
        match self.1.poll_recv(cx) {
            Poll::Pending => Poll::Pending,
            Poll::Ready(Some(event)) => Poll::Ready(Some(*event)),
            Poll::Ready(None) => Poll::Ready(None)
        }
    }
}

#[cfg(test)]
mod test {
    use crate::events::EventRegistry;
    use std::sync::Arc;
    use tokio::runtime::Runtime;
    use futures::StreamExt;

    #[derive(Clone, Debug)]
    enum BasicEvents {
        EventA,
        EventB,
        EventC
    }

    #[test]
    fn basic_functionality() {
        let (registry, mut emitter) = EventRegistry::<BasicEvents>::new();

        std::mem::forget(registry.hook(box |_event| {
            println!("Having new event on hook 1");
        }));

        let hook2 = registry.hook(box |_event| {
            println!("Having new event on hook 2");
        });

        registry.register_listener(Arc::new(|event| {
            println!("Received event 1: {:?}", event);
            false
        }));

        registry.register_listener(Arc::new(|event| {
            println!("Received event 2: {:?}", event);
            true
        }));

        emitter.fire(BasicEvents::EventA);
        emitter.fire(BasicEvents::EventB);
        emitter.fire(BasicEvents::EventC);

        drop(hook2);
        emitter.fire(BasicEvents::EventA);
    }

    #[test]
    fn test_stream() {
        let rt = Runtime::new().unwrap();
        let (registry, mut emitter) = EventRegistry::<BasicEvents>::new();

        let mut stream = registry.event_stream();
        let task = rt.spawn(async move {
            println!("Received event: {:?}", stream.next().await);
        });

        emitter.fire(BasicEvents::EventA);
        rt.block_on(task).unwrap();
    }
}