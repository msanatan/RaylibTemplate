#ifndef GBM_ASSET_WATCHER_H
#define GBM_ASSET_WATCHER_H

// Polls at most every 250 ms and reports changes after a quiet interval.
// Only desktop Debug builds watch the source assets; elsewhere returns false.
bool HaveAssetsChanged();

#endif  // !GBM_ASSET_WATCHER_H
