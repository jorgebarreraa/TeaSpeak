use std::sync::{Arc};
use std::sync::atomic::{AtomicU64, Ordering, AtomicUsize};

pub struct PendingQuotaSummary {
    parent: Option<Arc<PendingQuotaSummary>>,

    total_bytes: AtomicU64,
    realized_bytes: AtomicU64,
}

impl PendingQuotaSummary {
    pub fn new(parent: Option<Arc<PendingQuotaSummary>>, base_quota: (u64, u64)) -> Self {
        debug_assert!(base_quota.0 >= base_quota.1);

        PendingQuotaSummary {
            parent,
            total_bytes: AtomicU64::new(base_quota.0),
            realized_bytes: AtomicU64::new(base_quota.1)
        }
    }

    /// Collect info about the current status.
    /// (total_bytes, realized_bytes).
    /// Note: realized_bytes will always be lower than total_bytes
    pub fn info(&self) -> (u64, u64) {
        /* Firstly load realized bytes so we've a lower realized_bytes than total_bytes */
        let realized_bytes = self.realized_bytes.load(Ordering::Relaxed);
        let total_bytes = self.total_bytes.load(Ordering::Relaxed);
        (total_bytes, realized_bytes)
    }

    pub fn realized_bytes(&self) -> u64 {
        self.realized_bytes.load(Ordering::Relaxed)
    }

    pub fn total_bytes(&self) -> u64 {
        self.total_bytes.load(Ordering::Relaxed)
    }

    fn increase_realized_bytes(&self, bytes: u64) {
        self.realized_bytes.fetch_add(bytes, Ordering::Relaxed);
        debug_assert!(self.total_bytes.load(Ordering::Relaxed) >= self.realized_bytes.load(Ordering::Relaxed));

        if let Some(parent) = &self.parent {
            parent.increase_realized_bytes(bytes);
        }
    }

    fn add_quota(&self, quota: &PendingQuota) {
        self.total_bytes.fetch_add(quota.total_bytes.load(Ordering::Relaxed), Ordering::Relaxed);
        self.realized_bytes.fetch_add(quota.realized_bytes.load(Ordering::Relaxed), Ordering::Relaxed);

        if let Some(parent) = &self.parent {
            parent.add_quota(quota);
        }
    }

    fn remove_quota(&self, quota: &PendingQuota) {
        let unrealized_bytes = quota.total_bytes.load(Ordering::Relaxed) - quota.realized_bytes.load(Ordering::Relaxed);
        self.total_bytes.fetch_sub(unrealized_bytes, Ordering::Relaxed);

        if let Some(parent) = &self.parent {
            parent.remove_quota(quota);
        }
    }
}

pub struct PendingQuota {
    parent: Arc<PendingQuotaSummary>,
    total_bytes: AtomicU64,
    realized_bytes: AtomicU64,
}

impl PendingQuota {
    pub fn new(total_bytes: u64, parent: Arc<PendingQuotaSummary>) -> Self {
        let result = PendingQuota {
            parent,
            total_bytes: AtomicU64::new(total_bytes),
            realized_bytes: AtomicU64::new(0)
        };

        result.parent.add_quota(&result);
        result
    }

    pub fn increase_realized_bytes(&self, bytes: u64) {
        self.realized_bytes.fetch_add(bytes, Ordering::Relaxed);
        debug_assert!(self.total_bytes.load(Ordering::Relaxed) >= self.realized_bytes.load(Ordering::Relaxed));

        self.parent.increase_realized_bytes(bytes);
    }
}

impl Drop for PendingQuota {
    fn drop(&mut self) {
        self.parent.remove_quota(self);
    }
}

pub struct RunningTransferSummary {
    parent: Option<Arc<RunningTransferSummary>>,
    transfer_count: AtomicUsize,
}

impl RunningTransferSummary {
    pub fn new(parent: Option<Arc<RunningTransferSummary>>) -> Self {
        RunningTransferSummary{
            parent,
            transfer_count: AtomicUsize::new(0)
        }
    }

    pub fn running_transfers(&self) -> usize {
        self.transfer_count.load(Ordering::Relaxed)
    }

    fn register_transfer(&self) {
        self.transfer_count.fetch_add(1,Ordering::Relaxed);

        let mut parent = &self.parent;
        while let Some(parent_ref) = parent {
            parent_ref.transfer_count.fetch_add(1,Ordering::Relaxed);
            parent = &parent_ref.parent;
        }
    }

    fn unregister_transfer(&self) {
        self.transfer_count.fetch_sub(1,Ordering::Relaxed);

        let mut parent = &self.parent;
        while let Some(parent_ref) = parent {
            parent_ref.transfer_count.fetch_sub(1,Ordering::Relaxed);
            parent = &parent_ref.parent;
        }
    }
}

pub struct RunningTransfer {
    parent: Arc<RunningTransferSummary>
}

impl RunningTransfer {
    pub fn new(parent: Arc<RunningTransferSummary>) -> Self {
        parent.register_transfer();
        RunningTransfer{
            parent
        }
    }
}

impl Drop for RunningTransfer {
    fn drop(&mut self) {
        self.parent.unregister_transfer();
    }
}