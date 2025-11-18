use lazy_static::lazy_static;
use std::sync::Mutex;
use tokio::task::JoinHandle;
use std::future::Future;

pub struct SingleEventLoopInstance {
    instance: glib::MainLoop
}

impl SingleEventLoopInstance {
    fn new() -> Self {
        let context = glib::MainContext::new();
        let instance = glib::MainLoop::new(Some(&context), false);

        let instance2 = instance.clone();
        std::thread::spawn(move || {
            if !instance2.get_context().acquire() {
                panic!("failed to acquire event loop context");
            }
            instance2.run();
        });

        SingleEventLoopInstance {
            instance
        }
    }

    pub fn event_loop(&mut self) -> glib::MainLoop {
        self.instance.clone()
    }
}

lazy_static! {
    pub static ref MAIN_GIO_EVENT_LOOP: Mutex<SingleEventLoopInstance> =
                                            Mutex::new(SingleEventLoopInstance::new());
}

struct GlobalTaskThreadPool {
    runtime: tokio::runtime::Runtime
}

impl GlobalTaskThreadPool {
    fn new() -> Self {
        /* TODO: Configurable threads */
        let runtime = tokio::runtime::Builder::new()
            .threaded_scheduler()
            .enable_all()
            .core_threads(4)
            .max_threads(4)
            .thread_name("rtc-broadcast")
            .build().unwrap();

        GlobalTaskThreadPool {
            runtime,
        }
    }
}
lazy_static! {
    static ref TASK_THREAD_POOL: GlobalTaskThreadPool = GlobalTaskThreadPool::new();
}

pub fn execute_task<T>(task: T) -> JoinHandle<T::Output>
    where
        T: Future + Send + 'static,
        T::Output: Send + 'static,
{
    TASK_THREAD_POOL.runtime.spawn(task)
}

pub fn enter_tasks<F, R>(f: F) -> R
    where
        F: FnOnce() -> R,
{
    TASK_THREAD_POOL.runtime.enter(f)
}