use lazy_static::lazy_static;
use tokio::runtime::EnterGuard;

struct GlobalTaskThreadPool {
    runtime: tokio::runtime::Runtime
}

impl GlobalTaskThreadPool {
    fn new() -> Self {
        /* TODO: Configurable threads */
        let runtime = tokio::runtime::Builder::new_multi_thread()
            .worker_threads(4)
            .enable_all()
            .thread_name("file-server")
            .build().unwrap();

        GlobalTaskThreadPool {
            runtime,
        }
    }
}
lazy_static! {
    static ref TASK_THREAD_POOL: GlobalTaskThreadPool = GlobalTaskThreadPool::new();
}

/*
pub fn execute_task<T>(task: T) -> JoinHandle<T::Output>
    where
        T: Future + Send + 'static,
        T::Output: Send + 'static,
{
    TASK_THREAD_POOL.runtime.spawn(task)
}
*/

pub fn runtime_enter_guard() -> EnterGuard<'static> {
    TASK_THREAD_POOL.runtime.enter()
}