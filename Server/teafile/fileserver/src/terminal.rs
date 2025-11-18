use crossterm::{
    event::{Event, EventStream, KeyCode},
    terminal::{disable_raw_mode, enable_raw_mode},
};
use crossterm::{QueueableCommand};
use crossterm::cursor::{ MoveToColumn };
use crossterm::style::{SetForegroundColor, Color, Print, SetAttributes, Attribute, Attributes};
use std::io::{stdout, stdin, Write, Cursor, BufWriter};
use parking_lot::Mutex;
use futures::future::AbortHandle;
use futures::StreamExt;
use std::ops::{DerefMut};
use std::time::SystemTime;
use chrono::{DateTime, Local};
use slog::Level;
use tokio::sync::mpsc;
use tokio::macros::support::{Poll};
use crossterm::event::{KeyModifiers, KeyEvent};
use tokio::sync::mpsc::error::TrySendError;
use std::convert::TryFrom;
use tokio::io::{ErrorKind, BufReader, AsyncBufReadExt};
use std::collections::VecDeque;
use crossterm::tty::IsTty;

lazy_static::lazy_static!{
    static ref TERMINAL: Mutex<Option<TerminalInstance>> = Mutex::new(None);
}

pub enum TerminalEvents {
    /// `^C^K` has been issued or the terminal has been gone.
    Terminate,
    Command(String),
}

pub fn setup() -> Result<(), String> {
    let mut terminal = TERMINAL.lock();
    if terminal.is_some() {
        return Err(format!("Terminal already initialized"));
    }

    let instance = match TerminalInstance::new() {
        Ok(instance) => instance,
        Err(error) => return Err(format!("{}", error))
    };

    *terminal = Some(instance);

    return Ok(());
}

pub fn shutdown() {
    let mut terminal = TERMINAL.lock().take();
    if let Some(terminal) = &mut terminal {
        terminal.stop();
    }
}

pub fn println(level: slog::Level, message: &str) {
    let mut terminal = TERMINAL.lock();
    if let Some(terminal) = terminal.deref_mut() {
        terminal.println(level, message);
    } else {
        drop(terminal);
        fallback_println(level, message);
    }
}

fn fallback_println(level: slog::Level, message: &str) {
    println!("[{}] {}", level.as_str(), message);
}

pub async fn next_event() -> TerminalEvents {
    let event = futures::future::poll_fn(|cx| {
        let mut terminal = TERMINAL.lock();
        if let Some(terminal) = terminal.deref_mut() {
            terminal.event_receiver.poll_recv(cx)
        } else {
            Poll::Ready(None)
        }
    }).await;
    event.unwrap_or(TerminalEvents::Terminate)
}

struct TerminalInstance {
    tty: bool,

    initialized: bool,
    abort: Option<AbortHandle>,
    event_receiver: mpsc::Receiver<TerminalEvents>,
    event_sender: mpsc::Sender<TerminalEvents>,

    control_issued: bool,

    prompt: TerminalPrompt,
    prompt_length: usize,
    prompt_prefix: String,

    terminal_width: usize
}

impl TerminalInstance {
    pub fn new() -> crossterm::Result<TerminalInstance> {
        let (event_sender, event_receiver) = mpsc::channel(32);

        let tty = stdin().is_tty();
        let abort_handle = {
            if tty {
                enable_raw_mode()?;

                let (future, abort_handle) = futures::future::abortable(TerminalInstance::tty_event_loop());
                tokio::spawn(future);
                abort_handle
            } else {
                println!("Terminal isn't a TTY. Falling back to simple terminal mode");
                let (future, abort_handle) = futures::future::abortable(TerminalInstance::normal_event_loop());
                tokio::spawn(future);
                abort_handle
            }
        };

        let (terminal_width, _) = crossterm::terminal::size().unwrap_or((120, 0));

        let mut terminal = TerminalInstance{
            tty,
            initialized: true,

            abort: Some(abort_handle),
            event_receiver,
            event_sender,

            control_issued: false,

            prompt: TerminalPrompt::new(terminal_width.into()),
            prompt_prefix: "> ".to_owned(),
            prompt_length: 0,

            terminal_width: terminal_width.into(),
        };
        /* We don't know what the current "prompt" is. So assume it has been set to anything. */
        terminal.prompt_length = terminal.terminal_width;
        terminal.print_prompt(true);
        Ok(terminal)
    }

    pub fn stopped(&self) -> bool {
        !self.initialized
    }

    pub fn stop(&mut self) {
        if !self.initialized {
            return;
        }
        self.initialized = false;

        if self.tty {
            if let Err(error) = disable_raw_mode() {
                self.println(Level::Critical, &format!("Failed to disable raw mode: {}", error));
            }
        }

        if let Some(handle) = self.abort.take() {
            handle.abort();
        }
    }

    async fn tty_event_loop() {
        let mut reader = EventStream::new();

        loop {
            let result = match reader.next().await {
                Some(result) => result,
                None => break,
            };

            let mut terminal = TERMINAL.lock();
            if let Some(terminal) = terminal.deref_mut() {
                match result {
                    Ok(event) => terminal.handle_input(event),
                    Err(error) => terminal.handle_error(error)
                }
            }
        }

        if TERMINAL.lock().is_some() {
            println(slog::Level::Critical, "Terminal event loop exited without a proper terminate!");
        }
    }

    async fn normal_event_loop() {
        let mut lines = BufReader::new(tokio::io::stdin()).lines();

        loop {
            let result = match lines.next_line().await {
                Ok(Some(line)) => Ok(TerminalEvents::Command(line)),
                Ok(None) => Ok(TerminalEvents::Terminate),
                Err(error) => Err(crossterm::ErrorKind::IoError(error))
            };

            let mut terminal = TERMINAL.lock();
            if let Some(terminal) = terminal.deref_mut() {
                match result {
                    Ok(event) => terminal.send_terminal_event(event),
                    Err(error) => terminal.handle_error(error)
                }
            }
        }
    }
}

const EMPTY_STR_BUFFER: &'static str = "                                                            ";
const STACK_BUFFER_SIZE: usize = 1024 * 16;

impl TerminalInstance {
    /// Print a new line into the terminal
    pub fn println(&mut self, level: slog::Level, message: &str) {
        let mut expected_message_length = message.len() + 10 + self.prompt_length + self.terminal_width;

        loop {
            let result = {
                if expected_message_length <= STACK_BUFFER_SIZE {
                    self.do_println_stack_buffer(level, message)
                } else {
                    self.do_println_heap_buffer(level, message)
                }
            };

            let prompt_length = match result {
                Ok(prompt_length) => prompt_length,
                Err(error) => {
                    match error {
                        crossterm::ErrorKind::IoError(error) => {
                            match error.kind() {
                                ErrorKind::WriteZero => {
                                    /* Buffer too small */
                                    expected_message_length = usize::max_value();
                                    continue;
                                },
                                _ => {
                                    eprintln!("Failed to write to terminal: {:?}", error);
                                }
                            }
                        },
                        error => {
                            eprintln!("Failed to write to terminal: {:?}", error);
                        }
                    }

                    return;
                }
            };

            self.prompt_length = prompt_length;
            break;
        }
    }

    fn do_println_heap_buffer(&mut self, level: slog::Level, message: &str) -> crossterm::Result<usize> {
        let mut writer = BufWriter::new(Vec::with_capacity(8192 * 4));

        let prompt_length = self.do_println(&mut writer, level, message)?;

        let buffer = writer.into_inner().unwrap();
        stdout().write_all(buffer.as_slice()).map_err(|err| crossterm::ErrorKind::IoError(err))?;
        let _ = stdout().flush();

        Ok(prompt_length)
    }

    /// Print a new line into the terminal without restoring the current prompt
    fn do_println_stack_buffer(&mut self, level: slog::Level, message: &str) -> crossterm::Result<usize> {
        let mut buffer: [u8; STACK_BUFFER_SIZE] = unsafe { std::mem::MaybeUninit::uninit().assume_init() };
        let mut writer = Cursor::new(buffer.as_mut());

        let prompt_length = self.do_println(&mut writer, level, message)?;

        let write_offset = writer.position() as usize;
        stdout().write_all(&buffer[0..write_offset]).map_err(|err| crossterm::ErrorKind::IoError(err))?;
        let _ = stdout().flush();

        Ok(prompt_length)
    }

    /// Println the log message and return the new prompt length
    fn do_println<T: Write + ?Sized>(&self, out_stream: &mut T, level: slog::Level, message: &str) -> crossterm::Result<usize> {
        self.do_print(out_stream, level, message)?;
        out_stream.write(b"\n").map_err(|err| crossterm::ErrorKind::IoError(err))?;
        if self.tty {
            self.do_print_prompt(out_stream, false)
        } else {
            Ok(0)
        }
    }

    /// Print the log message into the out_stream
    fn do_print<T: Write + ?Sized>(&self, out_stream: &mut T, level: slog::Level, message: &str) -> crossterm::Result<()> {
        let mut line_size = 0usize;

        let system_time = SystemTime::now();
        let datetime: DateTime<Local> = system_time.into();

        out_stream.queue(MoveToColumn(0))?;

        {
            let date = datetime.format("[%d/%m %T%.3f]").to_string();
            line_size += date.len();
            out_stream.queue(Print(date))?;
        }

        out_stream.queue(Print(" ["))?;
        line_size += 2;

        match level {
            Level::Trace    => {
                out_stream.queue(SetForegroundColor(Color::Blue))?;
                out_stream.queue(Print("TRACE"))?;
                out_stream.queue(SetForegroundColor(Color::Reset))?;
                line_size += 5;
            },
            Level::Debug    => {
                out_stream.queue(SetForegroundColor(Color::Blue))?;
                out_stream.queue(Print("DEBUG"))?;
                out_stream.queue(SetForegroundColor(Color::Reset))?;
                line_size += 5;
            },
            Level::Info     => {
                out_stream.queue(SetForegroundColor(Color::Yellow))?;
                out_stream.queue(Print("INFO "))?;
                out_stream.queue(SetForegroundColor(Color::Reset))?;
                line_size += 5;
            },
            Level::Warning  => {
                out_stream.queue(SetForegroundColor(Color::DarkYellow))?;
                out_stream.queue(Print("WARN "))?;
                out_stream.queue(SetForegroundColor(Color::Reset))?;
                line_size += 5;
            },
            Level::Error    => {
                out_stream.queue(SetForegroundColor(Color::Red))?;
                out_stream.queue(Print("ERROR"))?;
                out_stream.queue(SetForegroundColor(Color::Reset))?;
                line_size += 5;
            },
            Level::Critical => {
                out_stream.queue(SetForegroundColor(Color::Red))?;
                let mut attributes: Attributes = Default::default();
                attributes.set(Attribute::Bold);
                out_stream.queue(SetAttributes(attributes))?;
                out_stream.queue(Print("CRITICAL"))?;
                out_stream.queue(SetForegroundColor(Color::Reset))?;
                line_size += 8;

                let mut attributes: Attributes = Default::default();
                attributes.set(Attribute::Reset);
                out_stream.queue(SetAttributes(attributes))?;
            }
        };

        out_stream.queue(Print("] "))?;
        line_size += 3;

        line_size += message.len();
        out_stream.queue(Print(message))?;

        self.clear_prompt(out_stream, line_size)?;
        Ok(())
    }

    fn clear_prompt<T: Write + ?Sized>(&self, out_stream: &mut T, mut bytes_written: usize) -> crossterm::Result<()> {
        while bytes_written < self.prompt_length {
            let write_bytes = bytes_written.min(EMPTY_STR_BUFFER.len());
            out_stream.queue(Print(&EMPTY_STR_BUFFER[0..write_bytes]))?;
            bytes_written += write_bytes;
        }

        Ok(())
    }

    fn do_print_prompt<T: Write + ?Sized>(&self, out_stream: &mut T, clear_old: bool) -> crossterm::Result<usize> {
        let mut prompt_length = 0usize;

        out_stream.queue(MoveToColumn(0)).unwrap();
        out_stream.queue(Print(&self.prompt_prefix)).unwrap();

        prompt_length += self.prompt_prefix.len();

        if prompt_length < self.terminal_width {
            let write_bytes = (self.terminal_width - prompt_length).min(self.prompt.draw_buffer.len());
            out_stream.queue(Print(&self.prompt.draw_buffer[0..write_bytes])).unwrap();
            prompt_length += write_bytes;
        }

        if clear_old {
            self.clear_prompt(out_stream, prompt_length)?;
        }
        self.do_update_cursor(out_stream)?;

        Ok(prompt_length)
    }

    fn print_prompt(&mut self, clear_old: bool) {
        if !self.tty {
            return;
        }

        let mut buffer: [u8; 8192 * 4] = unsafe { std::mem::MaybeUninit::uninit().assume_init() };
        let mut writer = Cursor::new(buffer.as_mut());

        let prompt_length = match self.do_print_prompt(&mut writer, clear_old) {
            Ok(prompt_length) => prompt_length,
            Err(error) => {
                eprintln!("Failed to build terminal prompt: {:?}", error);
                return;
            }
        };

        let write_offset = writer.position() as usize;
        if let Err(error) = stdout().write_all(&buffer[0..write_offset]) {
            eprintln!("Failed to write terminal prompt to stdout: {:?}", error);
            return;
        }
        let _ = stdout().flush();

        self.prompt_length = prompt_length;
    }

    fn do_update_cursor<T: Write + ?Sized>(&self, out_stream: &mut T) -> crossterm::Result<()> {
        match u16::try_from(self.prompt_prefix.len() + self.prompt.draw_cursor) {
            Ok(offset) => {
                out_stream.queue(MoveToColumn(offset + 1))?;
            },
            Err(_) => {
                eprintln!("Terminal cursor offset overflow.");
            }
        };

        Ok(())
    }

    fn update_cursor(&self) {
        if !self.tty {
            return;
        }

        let mut buffer: [u8; 265] = unsafe { std::mem::MaybeUninit::uninit().assume_init() };
        let mut writer = Cursor::new(buffer.as_mut());

        self.do_update_cursor(&mut writer).expect("failed to write cursor position");

        let write_offset = writer.position() as usize;
        if let Err(error) = stdout().write_all(&buffer[0..write_offset]) {
            eprintln!("Failed to write terminal cursor to stdout: {:?}", error);
            return;
        }
        let _ = stdout().flush();
    }
}

impl Drop for TerminalInstance {
    fn drop(&mut self) {
        if self.initialized {
            println!("Terminal dropped without being stopped!");
        }
    }
}

impl TerminalInstance {
    fn send_terminal_event(&mut self, event: TerminalEvents) {
        if let Err(error) = self.event_sender.try_send(event) {
            let error = match error {
                TrySendError::Full(_) => "full",
                TrySendError::Closed(_) => "closed"
            };

            self.println(Level::Error, &format!("Failed to enqueue terminal event ({}). Dropping event.", error));
        }
    }

    fn handle_error(&mut self, error: crossterm::ErrorKind) {
        self.println(Level::Critical, &format!("Terminal received error: {}", error));
    }

    fn handle_input(&mut self, input: Event) {
        match input {
            Event::Key(event) => {
                //self.println(Level::Trace, &format!("Terminal received event: {:?}", event));
                if event.modifiers.contains(KeyModifiers::CONTROL) {
                    if event.code == KeyCode::Char('c') {
                        self.control_issued = true;
                        return;
                    } else if event.code == KeyCode::Char('k') && self.control_issued {
                        self.send_terminal_event(TerminalEvents::Terminate);
                        return;
                    }
                }

                if event.code == KeyCode::Enter {
                    let line = self.prompt.take_line();
                    if line.chars().find(|e| *e != ' ').is_some() {
                        self.send_terminal_event(TerminalEvents::Command(line));
                    }
                    self.print_prompt(true);
                    return;
                }

                self.control_issued = false;

                match self.prompt.handle_input(event) {
                    TerminalPromptAction::None => {},
                    TerminalPromptAction::Redraw => self.print_prompt(true),
                    TerminalPromptAction::MoveCursor => self.update_cursor()
                }
            },
            Event::Mouse(_event) => {},
            Event::Resize(width, _) => {
                let width = width as usize;
                self.println(Level::Trace, &format!("Terminal resized to {}", width));
                self.terminal_width = width;

                let prefix_length = self.prompt_prefix.len();
                if prefix_length >= width {
                    self.prompt.resize(0);
                } else {
                    self.prompt.resize(width - prefix_length);
                }

                self.print_prompt(true);
            }
        }
    }
}

enum TerminalPromptAction {
    None,
    Redraw,
    MoveCursor
}

struct TerminalPrompt {
    prompt_width: usize,

    draw_buffer: String,
    draw_cursor: usize,

    line_buffer: String,
    line_offset: usize,

    /// Started line buffer but aborted due to history switch.
    temp_line_buffer: Option<String>,

    history: VecDeque<String>,
    history_offset: Option<usize>
}

impl TerminalPrompt {
    pub fn new(prompt_width: usize) -> Self {
        TerminalPrompt{
            prompt_width,
            draw_buffer: String::new(),

            line_buffer: String::new(),
            line_offset: 0,

            draw_cursor: 0,

            temp_line_buffer: None,

            history: VecDeque::with_capacity(32),
            history_offset: None
        }
    }

    pub fn resize(&mut self, prompt_width: usize) {
        if self.prompt_width == prompt_width {
            return;
        }

        self.prompt_width = prompt_width;
        /* TODO: Adjust the draw buffer! */
    }

    pub fn handle_input(&mut self, input: KeyEvent) -> TerminalPromptAction {
        match input.code {
            KeyCode::Char(char) => {
                if char.len_utf8() > 1 {
                    /* We currently only support Ascii */
                    return TerminalPromptAction::None;
                }

                self.temp_line_buffer = None;
                self.history_offset = None;

                self.line_buffer.insert(self.line_offset, char);
                if let Some(position) = self.draw_buffer_position(self.line_offset) {
                    self.draw_buffer.insert(position, char);
                }

                self.line_offset += 1;
                self.draw_cursor += 1;
                TerminalPromptAction::Redraw
            },
            KeyCode::Backspace => {
                if self.line_offset == 0 {
                    return TerminalPromptAction::None;
                }

                self.temp_line_buffer = None;
                self.history_offset = None;

                self.line_buffer.remove(self.line_offset - 1);
                if let Some(position) = self.draw_buffer_position(self.line_offset - 1) {
                    self.draw_buffer.remove(position);
                }

                self.line_offset -= 1;
                self.draw_cursor -= 1;
                TerminalPromptAction::Redraw
            },
            KeyCode::Delete => {
                if self.line_offset >= self.line_buffer.len() {
                    return TerminalPromptAction::None;
                }

                self.temp_line_buffer = None;
                self.history_offset = None;

                self.line_buffer.remove(self.line_offset);
                if let Some(position) = self.draw_buffer_position(self.line_offset) {
                    self.draw_buffer.remove(position);
                }
                TerminalPromptAction::Redraw
            },
            KeyCode::Left => {
                return if self.line_offset > 0 {
                    self.line_offset -= 1;
                    self.draw_cursor -= 1;
                    TerminalPromptAction::MoveCursor
                } else {
                    TerminalPromptAction::None
                }
            },
            KeyCode::Right => {
                return if self.line_offset < self.line_buffer.len() {
                    self.line_offset += 1;
                    self.draw_cursor += 1;
                    TerminalPromptAction::MoveCursor
                } else {
                    TerminalPromptAction::None
                }
            },
            KeyCode::Up => {
                let history_offset = {
                    if let Some(position) = self.history_offset {
                        if position == 0 {
                            /* No more history (may ring the bell?) */
                            return TerminalPromptAction::None;
                        }

                        position - 1
                    } else if self.history.is_empty() {
                        return TerminalPromptAction::None;
                    } else {
                        if !self.line_buffer.is_empty() {
                            self.temp_line_buffer = Some(std::mem::take(&mut self.line_buffer));
                        }

                        self.history.len() - 1
                    }
                };

                self.history_offset = Some(history_offset);
                self.line_buffer = self.history[history_offset].clone();
                self.line_offset = self.line_buffer.len();
                self.update_draw_buffer();
                TerminalPromptAction::Redraw
            },
            KeyCode::Down => {
                let history_offset = {
                    if let Some(position) = self.history_offset {
                        if position + 1 >= self.history.len() {
                            None
                        } else {
                            Some(position + 1)
                        }
                    } else {
                        /* We never went into "history" mode */
                        return TerminalPromptAction::None;
                    }
                };

                let history_string = {
                    if let Some(history_offset) = history_offset {
                        self.history_offset = Some(history_offset);
                        self.history[history_offset].clone()
                    } else if let Some(temp_buffer) = self.temp_line_buffer.take() {
                        self.history_offset = None;
                        temp_buffer
                    } else {
                        self.history_offset = None;
                        "".to_owned()
                    }
                };

                self.line_buffer = history_string;
                self.line_offset = self.line_buffer.len();
                self.update_draw_buffer();
                TerminalPromptAction::Redraw
            },
            _ => {
                TerminalPromptAction::None
            }
        }
    }

    pub fn take_line(&mut self) -> String {
        self.draw_buffer.clear();
        self.draw_cursor = 0;
        self.line_offset = 0;
        let result = std::mem::take(&mut self.line_buffer);

        self.temp_line_buffer = None;

        self.history.push_back(result.clone());
        while self.history.len() > 16 {
            self.history.pop_front();
        }
        self.history_offset = None;

        result
    }

    fn draw_buffer_position(&self, line_offset: usize) -> Option<usize> {
        Some(line_offset)
    }

    fn update_draw_buffer(&mut self) {
        self.draw_buffer = self.line_buffer.clone();
        self.draw_cursor = self.line_offset;
    }
}