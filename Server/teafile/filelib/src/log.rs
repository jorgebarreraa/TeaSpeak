#[cfg(test)]
mod test {
    use slog_async::Async;
    use slog_term::{ TermDecorator, FullFormat };
    use slog::*;

    pub fn create_test_logger() -> Logger {
        let decorator = TermDecorator::new().build();
        let drain = FullFormat::new(decorator).build().fuse();
        let drain = Async::new(drain).build().fuse();

        Logger::root(drain, o!())
    }
}

#[cfg(test)]
pub fn create_test_logger() -> slog::Logger {
    test::create_test_logger()
}